<div align="center">

# ps5-homebrew-ui

**Console-grade user interfaces for PS5 homebrew, drawn with OpenGL.**

A small rendering, motion and sound kit, a themeable widget set, and one
native app that holds a gallery of complete, working UI designs.
Press **L1** / **R1** to switch between them.

<img src="docs/media/switcher.webp" width="860" alt="L1 and R1 cycling through every design in the app">

<!-- BEGIN:counts -->
<!-- END:counts -->
&middot; **67 sound effects** in two sets &middot; **one OpenGL 4.6 shader** behind all of it

[Designs](docs/DESIGNS.md) &middot;
[Themes](docs/THEMES.md) &middot;
[The craft](docs/CRAFT.md) &middot;
[Kit reference](docs/KIT.md) &middot;
[Build a design](docs/BUILDING_A_DESIGN.md) &middot;
[For AI agents](AGENTS.md)

</div>

## Why this exists

Most homebrew looks like a tool: a list, a cursor, default fonts. It does not
have to. The difference between that and something that feels like the
console's own software is a few dozen small decisions about motion, sound,
depth and focus, made the same way everywhere.

This repository writes those decisions down, gives you code that implements
them, and proves the point with finished screens you can run, read and take
apart. It is meant as a reference for people and for coding agents: enough
documentation to learn the craft, enough working code to copy from.

## What is in the box

| | |
| --- | --- |
| **A renderer** | One instanced signed-distance-field shader draws every shape, glyph and image, anti-aliased at any size, at 4K in a handful of draw calls. Procedural animated backdrops. Real frosted glass. |
| **Motion** | Springs for everything that moves, easing curves, staggered entrances, a focus highlight that glides. Frame-rate independent, interruptible. |
| **Sound** | A 32-voice mixer on its own thread, a vocabulary of 42 cues, two complete sets of recorded effects, stereo placement, pitch that carries meaning, music with ducking, controller rumble. |
| **Type and glyphs** | Four baked distance-field fonts, sharp at any size. Every DualSense button drawn from shapes, in any colour scheme. |
| **Widgets and themes** | Buttons, switches, sliders, tabs, fields, chips, lists and dialogs in thirty design languages, from frosted glass to neo-brutalism to 8-bit. |
| **Designs** | Complete screens, each in one file: a home screen, a library, a storefront, a HUD with a pause menu, a radial menu, an on-screen keyboard, a settings screen that really works, and more. |
| **A tour** | The app can drive itself. The same scripted run renders every picture in these docs on a PC, runs the unit tests, and validates a build on the console. |
| **A PS5 build** | Reproducible native build and packaging, from [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate), rendering through [ps5-opengl](https://github.com/blackbearreloaded/ps5-opengl). |

## The designs

Each one is a different answer to "what should this screen feel like?", with
its own layout, palette, motion and interaction pattern. Click a picture for
its clip, its techniques and its source.

<!-- BEGIN:designs -->
<!-- END:designs -->

## The themes

The same working screen in thirty design languages. Ten are styles in their
own right; twenty are modelled on well-known web frameworks, with colours,
radii, borders and shadows measured from their own component pages. In the
app, open **Theme Lab** and press **L2** / **R2**.

<!-- BEGIN:themes -->
<!-- END:themes -->

## Quick start

You need Linux or WSL with `clang-18`, `ninja`, `make` and `python3`. The
build fetches and verifies everything else.

```bash
make deps              # one-time: toolchain pieces and the OpenGL SDK, pinned and verified
make                   # build the PS5 app folder in dist/
make host-snapshots    # run every design on this PC and write PNGs to build/snapshots
make test              # unit tests (sanitizers on) and tooling tests
```

To see it on a console, copy `dist/<TITLE_ID>` to `/data/homebrew/` (or
`make deploy PS5_HOST=<address>`) and start **Homebrew UI Lab**.

| Button | Does |
| --- | --- |
| **L1 / R1** | Previous / next design |
| **Touchpad** | Info panel: what the design demonstrates and where its code is |
| Everything else | Belongs to the design on screen; its hint row says what |

## Use it

- **Learn the craft.** [docs/CRAFT.md](docs/CRAFT.md) is the short version of
  everything here: the rules, the numbers, and a checklist.
- **Build a screen.** [docs/BUILDING_A_DESIGN.md](docs/BUILDING_A_DESIGN.md)
  takes you from an empty file to a design with pictures and tests in an
  afternoon. You iterate on a PC; a full render takes seconds.
- **Style standard widgets.** [docs/THEMES.md](docs/THEMES.md): pick one of
  thirty themes or define your own as data.
- **Take the kit.** [docs/ADOPTING.md](docs/ADOPTING.md): which directories to
  copy, and the twenty-line program that draws with them.

## Documentation

| Guide | Covers |
| --- | --- |
| [CRAFT.md](docs/CRAFT.md) | What makes a console UI feel finished: the ten-foot rules, motion, sound, edges, depth, type, and the checklist |
| [DESIGNS.md](docs/DESIGNS.md) | Every design: clip, pictures, techniques, source |
| [THEMES.md](docs/THEMES.md) | The widget set, the theme tokens, all thirty themes, adding your own |
| [KIT.md](docs/KIT.md) | API reference: shapes, text, backdrops, glass, motion, input, feedback |
| [BUILDING_A_DESIGN.md](docs/BUILDING_A_DESIGN.md) | Step by step, with a complete skeleton, the tour, tests and pitfalls |
| [SOUND.md](docs/SOUND.md) | The cue vocabulary, the two sound sets, levels, adding recordings |
| [BACKDROPS.md](docs/BACKDROPS.md) | The procedural backgrounds and post overlays |
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | The layers, one frame, where every file lives |
| [ADOPTING.md](docs/ADOPTING.md) | Using the kit in your own app |
| [PERFORMANCE.md](docs/PERFORMANCE.md) | What keeps a UI at 60 frames per second on the console, and what was measured |
| [CONSOLE_VALIDATION.md](docs/CONSOLE_VALIDATION.md) | The self-driving tour run and the rules for console work |
| [docs/platform/](docs/platform/) | Building, packaging, deploying, the runtime shim, troubleshooting |
| [AGENTS.md](AGENTS.md) | Orientation and rules for AI coding agents |

## How it works, in one paragraph

Every frame, the active design computes rectangles in a 1920 x 1080 virtual
canvas and records shapes into a draw list: rounded and cut-corner
rectangles, arcs, lines, shadows, glows, glyphs, images. Clip, transform and
opacity are applied as it records. The renderer uploads the frame's instances
once and draws each run with one instanced call; a fragment shader evaluates
a signed distance function per shape, which is why everything is sharp at 4K
without multisampling. A second full-screen shader paints the animated
backdrop. When a design wants frosted glass, the frame so far is replayed
into a small target and blurred. There is no widget tree, no layout engine
and no second renderer: everything you see is OpenGL.
[ARCHITECTURE.md](docs/ARCHITECTURE.md) has the diagrams.

## Validated on hardware

<!-- BEGIN:validated -->
Not yet validated on a console for this revision.
<!-- END:validated -->

## Repository layout

```
src/concepts/     the designs, one file each
src/ui/           fonts, glyphs, motion helpers, themes, widgets
src/gfx/          draw list, GL batch, backdrops, renderer
src/audio/        mixer, cues, sound bank, music
src/core/         input, springs and easing, settings, save files
src/app/          the shell (L1/R1 switcher), the tour, the design interface
src/platform/     PS5 display, controller, audio output, system services
host/             PC renderer for pictures, clips and tests
assets/           baked fonts, sound effects, music
tests/            unit tests (no console or GPU needed)
tools/            build, snapshots, media, docs, console validation
docs/             the guides; docs/media is generated by the app itself
```

<!-- bbr-footer:start -->
<!-- Generated by ps5-homebrew-dev-protocol/scripts/readme-footer. Edit the template there, not here. -->

## Credits

Built with the [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk) by John Törnblom (ps5-payload-dev).
Third-party components, authors and licenses are listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## License

Copyright © 2026 BlackBearReloaded. Licensed under GPL-3.0-or-later; see [LICENSE](LICENSE). Third-party components keep their own licenses.

## Disclaimer

- **No affiliation.** This is an independent homebrew project. It is not
  affiliated with, endorsed by, or sponsored by Sony Interactive Entertainment.
  "PlayStation", "PS5" and related marks are trademarks of Sony Interactive
  Entertainment Inc. The web frameworks named in the theme gallery belong to their authors, who do not endorse this project.
- **No proprietary material.** No Sony SDK, firmware, encryption keys or
  decrypted system modules are included.
- **No warranty.** This project is provided "as is", without warranty of any
  kind, to the extent permitted by law. See sections 15 and 16 of the GPL.

- **Use at your own risk.** Running homebrew requires a modified console, which
  may void its warranty, breach the platform's terms of service, or cause data
  loss.
- **Legal use only.** Use it only with hardware, accounts and content you own.
  This project does not support or enable piracy.

## AI assistance

This project was developed with AI assistance from OpenAI and/or Anthropic tools.
<!-- bbr-footer:end -->
