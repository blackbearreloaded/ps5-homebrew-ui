# Instructions for AI agents

This file is for coding agents (and people in a hurry). It says what this
repository is, where to look, how to work in it, and what "done" means here.
The detailed guides are linked where you need them; read those instead of
guessing.

## What this is

A reference for building console-grade user interfaces for PS5 homebrew with
OpenGL 4.6, and one native app ("Homebrew UI Lab") that shows all of it.

<!-- BEGIN:counts -->
Right now: **21 designs**, **30 themes**, **99 components**.
<!-- END:counts -->

It has four levels. Enter at the highest one that does the job:

| Level | What it gives you | Where | Guide |
| --- | --- | --- | --- |
| **Components** | Whole pieces that own their focus, values, motion and sound: lists, grids, tabs, menus, dialogs, notifications, forms, pickers, a keyboard, tables, charts, media controls, HUD pieces, layout and focus navigation | `src/ui/components/` | [docs/COMPONENTS.md](docs/COMPONENTS.md), [docs/COMPONENT_INDEX.md](docs/COMPONENT_INDEX.md) |
| **Widgets and themes** | `ui::Painter`: stateless themed controls; `ui::Theme`: a design language as data | `src/ui/widgets.hpp`, `src/ui/theme.cpp` | [docs/THEMES.md](docs/THEMES.md) |
| **The kit** | Shapes, text, images, glass, backdrops, springs, input, the mixer and its cue vocabulary | `src/gfx`, `src/ui`, `src/core`, `src/audio` | [docs/KIT.md](docs/KIT.md) |
| **Designs** | Complete screens, one file each, switched with L1/R1 in the app | `src/concepts/*.cpp` | [docs/DESIGNS.md](docs/DESIGNS.md) |

Everything on screen is drawn through OpenGL by the kit. Do not add another
rendering path.

## If the task is...

| Task | Do this |
| --- | --- |
| "Build me a settings screen / library / store / menu" | Start a design ([docs/BUILDING_A_DESIGN.md](docs/BUILDING_A_DESIGN.md)) and assemble it from components. Find them in [docs/COMPONENT_INDEX.md](docs/COMPONENT_INDEX.md). `src/concepts/components/*_page.cpp` are worked examples, one per group. |
| "I need a widget that does X" | Search [docs/COMPONENT_INDEX.md](docs/COMPONENT_INDEX.md), open the header (it starts with a usage example), then the group's guide in `docs/components/` for every knob, slot, event and cue. |
| "Make it look like Y" | Pick or write a `ui::Theme` ([docs/THEMES.md](docs/THEMES.md)); assign it to `component.style.theme`. Then turn the component's own knobs. |
| "Move the focus between things" | By hand, by edge exits, or with `ui::FocusGroup`: [docs/COMPONENTS.md](docs/COMPONENTS.md), [docs/components/layout.md](docs/components/layout.md). |
| "Add a new component" | The checklist under *Adding things* below. |
| "Use this in my own app" | [docs/ADOPTING.md](docs/ADOPTING.md): copy `src/gfx`, `src/ui`, `src/core`, `src/audio` and the assets. |
| "Why is it slow / is it fast enough" | [docs/PERFORMANCE.md](docs/PERFORMANCE.md) (rules and numbers measured on a console). |
| "Run it on the console" | [docs/CONSOLE_VALIDATION.md](docs/CONSOLE_VALIDATION.md) and *Console work* below. Ask the owner first. |

Other guides: [docs/CRAFT.md](docs/CRAFT.md) (the quality bar and its
checklist: read it before any UI work), [docs/SOUND.md](docs/SOUND.md),
[docs/BACKDROPS.md](docs/BACKDROPS.md),
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md),
[docs/platform/](docs/platform/) (building, packaging, deploying).

## Repository map

```
src/ui/components/        the component library (one header + source per component)
src/ui/components.hpp     includes every component
src/ui/                   fonts, glyphs, motion helpers, themes, Painter widgets
src/gfx/                  draw list, GL batch, fonts, backdrops, renderer
src/audio/, src/core/     mixer, cues, music; input, springs, settings, save files
src/concepts/             the designs; components/ holds the gallery's pages
src/app/                  the shell (L1/R1 switch), the tour, the design interface
src/platform/ps5/         display, controller, audio output, system services
host/                     PC renderer (Mesa): pictures, clips, manifest
tests/unit/               GoogleTest suite, no OpenGL needed
tools/                    build, snapshots, media, docs, console validation
assets/                   baked fonts, two sound-effect sets, music
sce_sys/                  what the PS5 home screen shows: icon, backgrounds, music, param.json
docs/                     the guides; docs/media is generated
```

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

## Local preview: see the UI without a console

The app's interface builds for a PC and renders off-screen through Mesa's
software renderer. It is the same UI code and the same shaders as on the
PS5; pictures taken on a console match these. There is no window and no
sound: you get **PNG frames** (and optionally short clips), which is exactly
what an agent needs. Use it after every visual change.

Needs: Linux or WSL with `clang`, `ninja`, the Mesa EGL and GL development
packages (`libegl-dev` and `libgl-dev` on Debian and Ubuntu), and Python 3. Clips also need `ffmpeg` with libwebp; `tools/render-media.sh` needs
Pillow. No console, no GPU, no PS5 toolchain.

```bash
# Every picture of one design (its entrance, then one per named tour step)
tools/host-snapshots.sh build/snapshots aurora

# Every design: about 300 pictures in about a minute
tools/host-snapshots.sh build/snapshots all

# The Component Library: every page and variant picture, and every page
# again in the Brutal, Pixel and Sketch themes
tools/host-snapshots.sh build/snapshots components

# Another size (the default is 1920 x 1080): 4K as on the console
tools/host-snapshots.sh build/snapshots-4k aurora 3840 2160
```

- Usage: `tools/host-snapshots.sh [output dir] [design id|all] [width height]`.
  Design ids are the file names in `src/concepts/` (`aurora`, `store`,
  `themes`, `components`, ...).
- Output: `<output dir>/NN-<design id>.png` after the entrance animation and
  `NN-<design id>-<step name>.png` for each named step; `NN` is the design's
  place in the switcher. The command prints one line per picture with its
  shape and draw-call counts and the GL error (it must be `0x0`).
- A compile error stops it and prints the errors; the full log is
  `build/host-snapshots/build.log`.
- **Then open the PNGs and look at them.** To check a detail, crop and enlarge
  (Python with Pillow is enough). Compare states side by side with a contact
  sheet.

What a picture shows is decided by the screen's `tour()`: a list of
`app::TourStep` (wait, input, optional picture name; see
`docs/BUILDING_A_DESIGN.md`). To photograph a state that has no picture yet,
add a step with a `capture` name, or temporarily change the tour while you
work and restore it before you finish. Gallery pages
(`src/concepts/components/*_page.cpp`) have a `tour()` of their own; to see a
page in another theme, the gallery's tour already photographs every page in
Brutal, Pixel and Sketch, and Options (`Action::menu`) cycles the theme.

Other outputs of the same program:

| Command | Result |
| --- | --- |
| `HUI_STRIP=aurora,paper tools/host-snapshots.sh out` | The frames of the L1/R1 switch between two designs |
| `HUI_REEL=out/clips tools/host-snapshots.sh out/scratch aurora 640 360` | An animated WebP of the whole tour |
| `tools/render-media.sh all` | Everything under `docs/media` (pictures and clips) |
| `HUI_MANIFEST=build/manifest.json tools/host-snapshots.sh build/snapshots aurora` then `python3 tools/gen-docs.py build/manifest.json` | The galleries, the component catalogue, the component index and the counts |
| `HUI_ICON=icon.png tools/host-snapshots.sh out` | A plain 512 x 512 icon drawn by the renderer |

What the preview cannot tell you, and must be reported as not verified until
checked on hardware: frame rate, how the sounds and the rumble feel, and
navigation with a controller in hand.

## Rules

**Code**

- C++20. The PS5 build has no exceptions and no RTTI: no `try`, `throw`,
  `dynamic_cast` or `typeid`. `std::function` and the containers are fine.
- No warnings. Tests compile with `-Wall -Wextra -Wpedantic -Werror` and run
  under AddressSanitizer and UndefinedBehaviorSanitizer; `make lint` runs
  clang-tidy with warnings as errors.
- Format with `make format` (clang-format, the repository's `.clang-format`).
- Every source file starts with the three header lines the existing files
  have (name and purpose, copyright, SPDX identifier). `make lint` checks it.
- `concept` is a C++20 keyword. Name variables `design`.
- Comments explain *why*: the technique, the constraint, the trap. They do
  not narrate the code.

**Components**

- Before hand-rolling a list, a grid, tabs, a dialog, a form, a keyboard, a
  chart or a progress indicator, use the one in `src/ui/components/`.
  Customise through its style struct and slots; if a knob is missing, add the
  knob.
- The five rules every component follows: public `style` (a `ui::Theme` plus
  its own knobs, changeable at any time without losing state),
  `set_bounds(rect)`, `handle(input, feedback)` returning a `ui::Event`,
  `update(dt)` every frame, const `draw(canvas)`.
- Only the component that has the focus receives `handle()` in a frame.
  "Has the focus" is `set_active(bool)` (or `set_focused(bool)` where
  `set_active(index)` selects something).
- A component draws through `ui::Painter` with `style.theme` (no colour
  literals), moves at `style.omega()` / `style.damping()`, plays
  `style.sounds` through `ui::play_cue`, refuses softly at edges with
  `ui::refuse` (silent on a held direction), honours `style.reduced_motion`,
  and never includes `app/`, `concepts/` or `demo/`.
- Text on a panel uses `theme.text`; text drawn straight on the page uses
  `paint.page_text()`. They differ in some themes.
- Overlays play a cue as they appear, so `open(feedback)` and
  `close(feedback)` take the feedback. Draw them after everything else.
- "Frosted" needs the frame's blurred copy (`canvas.glass`), which exists
  only when the screen asked for it (`frame.glass = true`) and holds what was
  drawn *before* it; with `canvas.glass == 0` components fall back to solid
  panels.
- It must look right in all thirty themes: glass, hard shadows, bevels,
  neumorphic, pixel (a wide bitmap face) and sketch are the ones that break
  first.

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
- Music: every `.ogg` in `assets/audio/music/` joins the playlist (shuffled
  per launch, then looped). 48 kHz stereo, about -18 LUFS, peaks at or below
  -1 dBTP; `python3 tools/audio-check.py` checks it.
- Fonts: only faces whose licence is in `third_party/fonts/`. Bake with
  `make fonts`. A renderer has six font slots (`gfx::kFontSlots`).
- Home-screen assets live in `sce_sys/`: `icon0.png` (512 x 512), `pic0.dds`
  (shown while the title is selected), `pic1.dds` (shown while it starts),
  `snd0.at9` (music while selected, under 2 MiB). `tools/validate-assets.sh`
  checks them. These are the owner's artwork and music: do not replace them
  unasked.
- `docs/media` is generated. Do not edit files there by hand.
- Anything third-party goes into `THIRD_PARTY_NOTICES.md` with its licence.

**Tests**

- `tests/unit/concepts_test.cpp` runs every registered design through its
  tour and 600 random inputs. A new design must pass it and bring behaviour
  tests of its own (`tests/unit/aurora_test.cpp` is the pattern).
- A component brings `tests/unit/components_<group>_test.cpp` built on
  `ComponentFixture`: behaviour (navigation, edges, events, cues, values) and
  "draws in every theme and variant".
- Tests need no OpenGL. Keep GL out of anything `tests/unit/sources.txt`
  lists.

**Git**

- Small commits with one-line imperative subjects ("Add the storefront
  design"). No body, no trailers.
- Never commit build output, console evidence, addresses of machines,
  personal paths or credentials. `make lint` scans for the usual leaks.

## Adding things

**A component**

1. `src/ui/components/<name>.hpp` and `.cpp`, namespace `hui::ui`, a style
   struct deriving from `ui::ComponentStyle`, a usage example in the header
   comment. `list.hpp` / `list.cpp` are the reference.
2. Add the header to `src/ui/components.hpp`. The umbrella test
   (`tests/unit/components_umbrella_test.cpp`) then catches a name that
   clashes with another component's.
3. Show it on a gallery page (`src/concepts/components/<group>_page.cpp`,
   contract in `page.hpp`): interactive, with variants on Square that prove
   the knobs, and tour steps with named pictures.
4. Tests, as above.
5. Document it in `docs/components/<group>.md` under a level-two heading that
   is exactly the class name (`## DatePicker`); the catalogue, the index and
   the count are generated from those headings. Other sections use `###`.
6. Render it (`tools/host-snapshots.sh build/snapshots components`) and look
   at it in at least Acrylic, Brutal, Classic, Pixel, a light web theme and
   Sketch.

**A new gallery page (a new group)**: a factory in `page.hpp`, an entry in
`kPages` in `src/concepts/components.cpp`, and the group in
`COMPONENT_GROUPS` in `tools/gen-docs.py`.

**A theme**: a function in `src/ui/theme.cpp` added to `themes()`; see
[docs/THEMES.md](docs/THEMES.md). Check contrast and the focus ring.

**A design**: [docs/BUILDING_A_DESIGN.md](docs/BUILDING_A_DESIGN.md).

## Console work

Read [docs/CONSOLE_VALIDATION.md](docs/CONSOLE_VALIDATION.md) first. In short:

- A request to build or change something is not permission to launch on a
  console. Ask the owner before each run, say what it does and how long it
  takes. Uploading is a console action too.
- One run at a time, a handful per session. Never chain unattended runs.
- Use `tools/console-tour.py`. It verifies what it uploads by hash, waits for
  the title's registration, launches once, copies the evidence and waits for
  the app to close itself. It never kills and never retries.
- Opening and closing on demand: the launch helper opens the title; a line
  `quit - <token>` in `/data/homebrew/<TITLE_ID>/dev/request.txt` makes the
  running app close itself through the system (the app polls for it once a
  second; this path is implemented but has not been exercised on a console
  yet, so confirm it with the owner present the first time). Do not kill a
  title that is rendering.
- If the console stops answering, stop. Report what you saw. Do not retry.
- Never touch console settings. Take the console's lock if it is shared.

Facts about a title's sandbox that the tooling depends on:

- It has no `/data`. The app writes its log, pictures and report to its own
  storage (`/download0/hui/dev`), which is mounted only while it runs.
- From a PC that storage is `/mnt/sandbox/<TITLE_ID>_000/download0/hui/dev`:
  FTP can read it, not write it.
- The install folder `/data/homebrew/<TITLE_ID>` is writable from a PC and is
  `/app0` inside the app. Requests go in that way and are honoured once per
  token.
- The console registers a new title about ten seconds after its folder stops
  changing; the home-screen assets are copied once at that moment
  (`/user/app/<TITLE_ID>/sce_sys`, `/user/appmeta/<TITLE_ID>`), so replacing
  an icon or `snd0.at9` later means replacing those copies too.

## What "done" means

- The pictures look right and you have looked at each one.
- `make test-unit` passes; `make` builds without warnings from your files;
  `make lint` passes.
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
  `Painter::halo()` and `Painter::focus_ring()` light only the outside.
- Springs need a `snap()` at start-up, or the first frame animates in from
  zero.
- A `static` local inside `draw()` is shared by every instance and by both
  passes of a transition. Keep state in members.
- Text given a top coordinate instead of a baseline lands about one text
  height too high.
- A component that measures text needs fonts, which arrive in `draw()`. Lay
  out lazily there (or offer `measure(fonts)`), and remember that tests send
  input before any draw.
- `push_opacity` fades every shape separately: a surface with a shadow under
  it turns grey as it fades. Fade a veil of the page colour over it instead.
- Two components may want the same type name (`GroupLayout` happened). The
  umbrella test fails when they do; rename one.
- `std::memcpy` with a null pointer is undefined even for zero bytes (a font
  with no kerning pairs found that one).
- A long string literal split across two lines inside an array looks like a
  missing comma to the compiler (`-Wstring-concatenation`); keep each entry on
  one line.
- Float loop counters fail static analysis. Count steps with an `int`.
- Item structs with `std::string` members cannot be brace-initialised
  partially under `-Wextra -Werror`; fill them field by field.
- A title uploaded for the first time is launchable only after the console
  has registered it. Launching earlier shows "title not found".
- The console's FTP server lists only the current directory, has no `MDTM`
  and no `NLST`: change into a folder, then `MLSD`.
- Some console FTP servers have a `SELF` switch that returns decrypted
  executables. Leave it off when comparing an installed file with the build.
- Killing a native title that is still rendering has been followed by lost
  consoles. The app ends itself with `sceSystemServiceLoadExec("exit", NULL)`;
  `exit()` and `_Exit()` end in the system's crash reporter.
