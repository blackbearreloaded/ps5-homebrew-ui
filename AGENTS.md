# Instructions for AI agents

This file is for coding agents (and people in a hurry). It says what this
repository is, how to work in it, and what "done" means here. The detailed
guides are linked where you need them; read those instead of guessing.

## What this is

A reference for building console-grade user interfaces for PS5 homebrew with
OpenGL 4.6:

- a **kit** (`src/gfx`, `src/ui`, `src/core`, `src/audio`): an instanced
  signed-distance-field renderer, procedural backdrops, frosted glass, baked
  fonts, springs and easing, controller glyphs, a mixer with a cue vocabulary
  and two sets of sound effects, and a themeable widget set;
- **designs** (`src/concepts/*.cpp`): complete screens, one file each, switched
  with L1/R1 in one native app;
- **themes** (`src/ui/theme.cpp`): design languages as data, applied to one
  widget set (`ui::Painter`);
- a **tour** that drives the app without a person, used for pictures, tests
  and console validation.

Everything on screen is drawn through OpenGL by the kit. Do not add another
rendering path.

## Read before you write

| Task | Read |
| --- | --- |
| Any UI work | [docs/CRAFT.md](docs/CRAFT.md): the quality bar and the checklist |
| A new screen | [docs/BUILDING_A_DESIGN.md](docs/BUILDING_A_DESIGN.md), then `src/concepts/aurora.cpp` |
| What can I draw or call? | [docs/KIT.md](docs/KIT.md), then the header itself |
| Standard widgets, a new theme | [docs/THEMES.md](docs/THEMES.md), `src/ui/widgets.hpp`, `src/concepts/themes.cpp` |
| Sound, rumble | [docs/SOUND.md](docs/SOUND.md) |
| Backgrounds | [docs/BACKDROPS.md](docs/BACKDROPS.md) |
| How the pieces fit | [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) |
| Using the kit elsewhere | [docs/ADOPTING.md](docs/ADOPTING.md) |
| Frame rate | [docs/PERFORMANCE.md](docs/PERFORMANCE.md) |
| A console run | [docs/CONSOLE_VALIDATION.md](docs/CONSOLE_VALIDATION.md) |
| Building, packaging, deploying | [docs/platform/](docs/platform/) |

## The loop

Work on a PC first. A console is only for the final check.

```bash
tools/host-snapshots.sh build/snapshots <design id>   # build for the PC, run the tour, write PNGs
make test-unit                                        # GoogleTest suite, sanitizers on
make                                                  # PS5 app folder in dist/
make lint                                             # format, static analysis, metadata
```

1. Change code.
2. Render the design and **look at the pictures** in `build/snapshots/`. A
   build that compiles proves nothing about a UI. Crop and enlarge anything
   you are unsure about.
3. Run the tests.
4. Build for the PS5.

`tools/host-snapshots.sh` takes `all` or one design id. `HUI_STRIP=<from>,<to>`
writes the frames of switching between two designs. `tools/render-media.sh`
regenerates everything under `docs/media`, and `tools/gen-docs.py` the
galleries in the README and the docs.

## Rules

**Code**

- C++20. The PS5 build has no exceptions and no RTTI: no `try`, `throw`,
  `dynamic_cast` or `typeid`.
- No warnings. Tests compile with `-Wall -Wextra -Wpedantic -Werror` and run
  under AddressSanitizer and UndefinedBehaviorSanitizer.
- Format with `make format` (clang-format, the repository's `.clang-format`).
- Every source file starts with the three header lines the existing files
  have (name and purpose, copyright, SPDX identifier). `make lint` checks it.
- `concept` is a C++20 keyword. Name variables `design`.
- Comments explain *why*: the technique, the constraint, the trap. They do
  not narrate the code.

**Designs**

- One file per design in `src/concepts/`, registered in `concepts.hpp` and
  `registry.cpp`. A design never edits the kit to suit itself; if the kit
  lacks something general, add it to the kit as its own change.
- `update()` owns all state and animation, driven by `dt`. `draw()` is `const`
  and pure: it can run more than once per frame.
- L1, R1 and the touchpad belong to the shell. Use `Action::confirm` and
  `Action::back`, never the physical buttons.
- Lay out in the 1920 x 1080 virtual canvas, inside the safe area (96 px at
  the sides). Text is positioned by its baseline.
- Every action has a cue; list ends refuse softly and stay silent on a held
  direction; `settings.reduced_motion` is honoured.
- Content is invented. No real product names, brands, logos or artwork.

**Assets**

- Sound effects: reuse the two sets in `assets/audio/sfx/`. Do not generate
  or add new ones unless the owner asks.
- Fonts: only faces whose licence is in `third_party/fonts/`. Bake with
  `make fonts`.
- `docs/media` is generated. Do not edit files there by hand.

**Tests**

- `tests/unit/concepts_test.cpp` runs every registered design through its
  tour and 600 random inputs. A new design must pass it and bring behaviour
  tests of its own (`tests/unit/aurora_test.cpp` is the pattern).
- Tests need no OpenGL. Keep GL out of anything `tests/unit/sources.txt`
  lists.

**Git**

- Small commits with one-line imperative subjects ("Add the storefront
  design"). No body, no trailers.
- Never commit build output, console evidence, addresses of machines,
  personal paths or credentials. `make lint` scans for the usual leaks.

## Console work

Read [docs/CONSOLE_VALIDATION.md](docs/CONSOLE_VALIDATION.md) first. In short:

- A request to build or change something is not permission to launch on a
  console. Ask the owner before each run, say what it does and how long it
  takes.
- One run at a time, a handful per session. Never chain unattended runs.
- Use `tools/console-tour.py`. It verifies what it uploads, lets the app end
  itself, never kills and never retries.
- If the console stops answering, stop. Report what you saw. Do not retry.
- Never touch console settings. Take the console's lock if it is shared.

## What "done" means

- The pictures look right and you have looked at each one.
- `make test-unit` passes; `make` builds without warnings from your files.
- The checklist at the end of [docs/CRAFT.md](docs/CRAFT.md) holds.
- Docs that mention what you changed are updated; generated galleries are
  regenerated.
- You say plainly what you did not verify. Sound by ear, controller feel and
  console frame rate are *not verified* until someone has actually checked
  them, and a PC snapshot never counts as a console result.

## Pitfalls that have already cost time here

- Shell heredocs mangle backslashes. Write source files with an editor tool,
  not through `cat <<EOF`.
- `list.glow()` fills its interior: draw it *under* the thing it lights.
- Springs need a `snap()` at start-up, or the first frame animates in from
  zero.
- A `static` local inside `draw()` is shared by every instance and by both
  passes of a transition. Keep state in members.
- Text given a top coordinate instead of a baseline lands about one text
  height too high.
- `std::memcpy` with a null pointer is undefined even for zero bytes (a font
  with no kerning pairs found that one).
- A stale result file from an earlier console run can answer for a new one:
  the runner compares modification times for that reason.
- Killing a native title that is still rendering has been followed by lost
  consoles. The app ends itself with `sceSystemServiceLoadExec("exit", NULL)`;
  `exit()` and `_Exit()` end in the system's crash reporter.
