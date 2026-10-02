<div align="center">

# ps5-homebrew-ui

**Console-grade user interfaces for PS5 homebrew, drawn with OpenGL.**

A small rendering, motion and sound kit, a themeable widget set, and one
native app that holds a gallery of complete, working UI designs.
Press **L1** / **R1** to switch between them.

<img src="docs/media/switcher.webp" width="860" alt="L1 and R1 cycling through every design in the app">

<!-- BEGIN:counts -->
**21 designs** &middot; **30 themes** &middot; **99 reusable components**
<!-- END:counts -->
&middot; **67 sound effects** in two sets &middot; **one OpenGL 4.6 shader** behind all of it

[Designs](docs/DESIGNS.md) &middot;
[Themes](docs/THEMES.md) &middot;
[Components](docs/COMPONENTS.md) &middot;
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
| **Type and glyphs** | Six baked distance-field fonts, sharp at any size. Every DualSense button drawn from shapes, in any colour scheme. |
| **Widgets and themes** | Buttons, switches, sliders, tabs, fields, chips, lists and dialogs in thirty design languages, from frosted glass to neo-brutalism to 8-bit. |
| **Components** | A library of reusable pieces that own their focus, motion and sound: lists, grids, carousels, tabs, menus, dialogs, sheets, toasts, forms, progress, badges, counters and more. Each is restyled by any theme, tuned through a style struct and extended through slots. |
| **Designs** | Complete screens, each in one file: a home screen, a library, a storefront, a HUD with a pause menu, a radial menu, an on-screen keyboard, a settings screen that really works, and more. |
| **A tour** | The app can drive itself. The same scripted run renders every picture in these docs on a PC, runs the unit tests, and validates a build on the console. |
| **A PS5 build** | Reproducible native build and packaging, from [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate), rendering through [ps5-opengl](https://github.com/blackbearreloaded/ps5-opengl). |

## The designs

Each one is a different answer to "what should this screen feel like?", with
its own layout, palette, motion and interaction pattern. Click a picture for
its clip, its techniques and its source.

<!-- BEGIN:designs -->
<table>
<tr>
<td width="50%" valign="top"><a href="docs/DESIGNS.md#aurora"><img src="docs/media/designs/aurora.jpg" alt="Aurora Shelf"></a><br><b>01 &middot; Aurora Shelf</b><br><sub>A console home screen: hero panel, cover shelves, frosted details</sub></td>
<td width="50%" valign="top"><a href="docs/DESIGNS.md#paper"><img src="docs/media/designs/paper.jpg" alt="Paper Library"></a><br><b>02 &middot; Paper Library</b><br><sub>A game shelf of paper cards that travel when sorted, filtered or picked up</sub></td>
</tr>
<tr>
<td width="50%" valign="top"><a href="docs/DESIGNS.md#neon"><img src="docs/media/designs/neon.jpg" alt="Neon Arcade"></a><br><b>03 &middot; Neon Arcade</b><br><sub>A synthwave racer's main menu: neon sign, gliding tube, live previews</sub></td>
<td width="50%" valign="top"><a href="docs/DESIGNS.md#editorial"><img src="docs/media/designs/editorial.jpg" alt="Editorial"></a><br><b>04 &middot; Editorial</b><br><sub>A magazine's weekly selection: big type on paper, and an article behind every row</sub></td>
</tr>
<tr>
<td width="50%" valign="top"><a href="docs/DESIGNS.md#carousel"><img src="docs/media/designs/carousel.jpg" alt="Cover Wheel"></a><br><b>05 &middot; Cover Wheel</b><br><sub>A carousel with weight: scrub it, let it coast, open a cover</sub></td>
<td width="50%" valign="top"><a href="docs/DESIGNS.md#radial"><img src="docs/media/designs/radial.jpg" alt="Radial Dial"></a><br><b>06 &middot; Radial Dial</b><br><sub>An in-game item wheel: aim with the stick, equip with one press</sub></td>
</tr>
<tr>
<td width="50%" valign="top"><a href="docs/DESIGNS.md#hud"><img src="docs/media/designs/hud.jpg" alt="Field HUD"></a><br><b>07 &middot; Field HUD</b><br><sub>An in-game HUD over a moving world, and the pause menu behind Options</sub></td>
<td width="50%" valign="top"><a href="docs/DESIGNS.md#dashboard"><img src="docs/media/designs/dashboard.jpg" alt="Pulse Dashboard"></a><br><b>08 &middot; Pulse Dashboard</b><br><sub>A bento grid of live data tiles that expand into detail views</sub></td>
</tr>
<tr>
<td width="50%" valign="top"><a href="docs/DESIGNS.md#player"><img src="docs/media/designs/player.jpg" alt="Now Playing"></a><br><b>09 &middot; Now Playing</b><br><sub>A music player: breathing artwork, a visualizer, a scrubber and a glass queue</sub></td>
<td width="50%" valign="top"><a href="docs/DESIGNS.md#keyboard"><img src="docs/media/designs/keyboard.jpg" alt="First Run"></a><br><b>10 &middot; First Run</b><br><sub>A setup wizard: avatar, a name typed on a controller keyboard, a warm welcome</sub></td>
</tr>
<tr>
<td width="50%" valign="top"><a href="docs/DESIGNS.md#constellation"><img src="docs/media/designs/constellation.jpg" alt="Constellation"></a><br><b>11 &middot; Constellation</b><br><sub>A skill tree as a star map: free 2D focus, a gliding camera, progression</sub></td>
<td width="50%" valign="top"><a href="docs/DESIGNS.md#terminal"><img src="docs/media/designs/terminal.jpg" alt="Phosphor"></a><br><b>12 &middot; Phosphor</b><br><sub>A monochrome CRT terminal: character grid, glow, typed text</sub></td>
</tr>
<tr>
<td width="50%" valign="top"><a href="docs/DESIGNS.md#store"><img src="docs/media/designs/store.jpg" alt="Storefront"></a><br><b>13 &middot; Storefront</b><br><sub>A shop window: featured banner, product pages, a cart and a checkout</sub></td>
<td width="50%" valign="top"><a href="docs/DESIGNS.md#trophies"><img src="docs/media/designs/trophies.jpg" alt="Trophy Room"></a><br><b>14 &middot; Trophy Room</b><br><sub>An achievements cabinet: metal medals, counting numbers, an unlock with ceremony</sub></td>
</tr>
<tr>
<td width="50%" valign="top"><a href="docs/DESIGNS.md#files"><img src="docs/media/designs/files.jpg" alt="File Browser"></a><br><b>15 &middot; File Browser</b><br><sub>A file manager: aligned columns, folders that keep your place, visible results</sub></td>
<td width="50%" valign="top"><a href="docs/DESIGNS.md#inventory"><img src="docs/media/designs/inventory.jpg" alt="Satchel"></a><br><b>16 &middot; Satchel</b><br><sub>An inventory you handle: lift, carry, swap, stack and equip, with a comparing tooltip</sub></td>
</tr>
<tr>
<td width="50%" valign="top"><a href="docs/DESIGNS.md#boot"><img src="docs/media/designs/boot.jpg" alt="Launch Sequence"></a><br><b>17 &middot; Launch Sequence</b><br><sub>Before the menu: studio splash, title, profiles and an honest loading screen</sub></td>
<td width="50%" valign="top"><a href="docs/DESIGNS.md#settings"><img src="docs/media/designs/settings.jpg" alt="Control Room"></a><br><b>18 &middot; Control Room</b><br><sub>A settings screen with real controls: sliders, switches, steppers, a dialog</sub></td>
</tr>
<tr>
<td width="50%" valign="top"><a href="docs/DESIGNS.md#themes"><img src="docs/media/designs/themes.jpg" alt="Theme Lab"></a><br><b>19 &middot; Theme Lab</b><br><sub>One screen in thirty design languages: L2 and R2 restyle every widget</sub></td>
<td width="50%" valign="top"><a href="docs/DESIGNS.md#components"><img src="docs/media/designs/components.jpg" alt="Component Library"></a><br><b>20 &middot; Component Library</b><br><sub>Reusable lists, grids, dialogs, forms and indicators, restyled by thirty themes</sub></td>
</tr>
<tr>
<td width="50%" valign="top"><a href="docs/DESIGNS.md#toolbox"><img src="docs/media/designs/toolbox.jpg" alt="Toolbox"></a><br><b>21 &middot; Toolbox</b><br><sub>The kit on one screen: shapes, type, motion, glyphs and every sound</sub></td>
</tr>
</table>
<!-- END:designs -->

## The themes

The same working screen in thirty design languages. Ten are styles in their
own right; twenty are modelled on well-known web frameworks, with colours,
radii, borders and shadows measured from their own component pages. In the
app, open **Theme Lab** and press **L2** / **R2**.

<!-- BEGIN:themes -->
<table>
<tr>
<td width="33%" valign="top"><a href="docs/THEMES.md#acrylic"><img src="docs/media/themes/acrylic.jpg" alt="Acrylic"></a><br><b>01 &middot; Acrylic</b><br><sub>Frosted glass</sub></td>
<td width="33%" valign="top"><a href="docs/THEMES.md#brutal"><img src="docs/media/themes/brutal.jpg" alt="Brutal"></a><br><b>02 &middot; Brutal</b><br><sub>Neo-brutalism</sub></td>
<td width="33%" valign="top"><a href="docs/THEMES.md#clay"><img src="docs/media/themes/clay.jpg" alt="Clay"></a><br><b>03 &middot; Clay</b><br><sub>Neumorphism</sub></td>
</tr>
<tr>
<td width="33%" valign="top"><a href="docs/THEMES.md#tiles"><img src="docs/media/themes/tiles.jpg" alt="Tiles"></a><br><b>04 &middot; Tiles</b><br><sub>Flat</sub></td>
<td width="33%" valign="top"><a href="docs/THEMES.md#gloss"><img src="docs/media/themes/gloss.jpg" alt="Gloss"></a><br><b>05 &middot; Gloss</b><br><sub>Skeuomorphic</sub></td>
<td width="33%" valign="top"><a href="docs/THEMES.md#classic"><img src="docs/media/themes/classic.jpg" alt="Classic"></a><br><b>06 &middot; Classic</b><br><sub>Bevelled desktop</sub></td>
</tr>
<tr>
<td width="33%" valign="top"><a href="docs/THEMES.md#blueprint"><img src="docs/media/themes/blueprint.jpg" alt="Blueprint"></a><br><b>07 &middot; Blueprint</b><br><sub>Wireframe</sub></td>
<td width="33%" valign="top"><a href="docs/THEMES.md#hazard"><img src="docs/media/themes/hazard.jpg" alt="Hazard"></a><br><b>08 &middot; Hazard</b><br><sub>Sci-fi console</sub></td>
<td width="33%" valign="top"><a href="docs/THEMES.md#candy"><img src="docs/media/themes/candy.jpg" alt="Candy"></a><br><b>09 &middot; Candy</b><br><sub>Playful pastel</sub></td>
</tr>
<tr>
<td width="33%" valign="top"><a href="docs/THEMES.md#contrast"><img src="docs/media/themes/contrast.jpg" alt="Contrast"></a><br><b>10 &middot; Contrast</b><br><sub>High contrast</sub></td>
<td width="33%" valign="top"><a href="docs/THEMES.md#pixel"><img src="docs/media/themes/pixel.jpg" alt="Pixel"></a><br><b>11 &middot; Pixel</b><br><sub>8-bit pixel art · after NES.css</sub></td>
<td width="33%" valign="top"><a href="docs/THEMES.md#soft"><img src="docs/media/themes/soft.jpg" alt="Soft"></a><br><b>12 &middot; Soft</b><br><sub>Modern soft · after Mantine</sub></td>
</tr>
<tr>
<td width="33%" valign="top"><a href="docs/THEMES.md#daisy"><img src="docs/media/themes/daisy.jpg" alt="Daisy"></a><br><b>13 &middot; Daisy</b><br><sub>Clean and customisable · after daisyUI</sub></td>
<td width="33%" valign="top"><a href="docs/THEMES.md#pico"><img src="docs/media/themes/pico.jpg" alt="Pico"></a><br><b>14 &middot; Pico</b><br><sub>Minimal and classless · after Pico.css</sub></td>
<td width="33%" valign="top"><a href="docs/THEMES.md#ant"><img src="docs/media/themes/ant.jpg" alt="Enterprise"></a><br><b>15 &middot; Enterprise</b><br><sub>Structured enterprise · after Ant Design</sub></td>
</tr>
<tr>
<td width="33%" valign="top"><a href="docs/THEMES.md#chakra"><img src="docs/media/themes/chakra.jpg" alt="Chakra"></a><br><b>16 &middot; Chakra</b><br><sub>Modern and accessible · after Chakra UI</sub></td>
<td width="33%" valign="top"><a href="docs/THEMES.md#fresh"><img src="docs/media/themes/fresh.jpg" alt="Fresh"></a><br><b>17 &middot; Fresh</b><br><sub>Modern flat · after Nuxt UI</sub></td>
<td width="33%" valign="top"><a href="docs/THEMES.md#neutral"><img src="docs/media/themes/neutral.jpg" alt="Neutral"></a><br><b>18 &middot; Neutral</b><br><sub>Neutral modern · after Shoelace</sub></td>
</tr>
<tr>
<td width="33%" valign="top"><a href="docs/THEMES.md#paper"><img src="docs/media/themes/paper.jpg" alt="Material"></a><br><b>19 &middot; Material</b><br><sub>Material Design · after Propeller</sub></td>
<td width="33%" valign="top"><a href="docs/THEMES.md#humane"><img src="docs/media/themes/humane.jpg" alt="Humane"></a><br><b>20 &middot; Humane</b><br><sub>Clean and readable · after Semantic UI</sub></td>
<td width="33%" valign="top"><a href="docs/THEMES.md#standard"><img src="docs/media/themes/standard.jpg" alt="Standard"></a><br><b>21 &middot; Standard</b><br><sub>General purpose · after Bootstrap 5</sub></td>
</tr>
<tr>
<td width="33%" valign="top"><a href="docs/THEMES.md#pill"><img src="docs/media/themes/pill.jpg" alt="Pill"></a><br><b>22 &middot; Pill</b><br><sub>High-contrast dashboard · after Preline UI</sub></td>
<td width="33%" valign="top"><a href="docs/THEMES.md#admin"><img src="docs/media/themes/admin.jpg" alt="Admin"></a><br><b>23 &middot; Admin</b><br><sub>Dark admin · after Flowbite</sub></td>
<td width="33%" valign="top"><a href="docs/THEMES.md#friendly"><img src="docs/media/themes/friendly.jpg" alt="Friendly"></a><br><b>24 &middot; Friendly</b><br><sub>Friendly and soft · after Bulma</sub></td>
</tr>
<tr>
<td width="33%" valign="top"><a href="docs/THEMES.md#crisp"><img src="docs/media/themes/crisp.jpg" alt="Crisp"></a><br><b>25 &middot; Crisp</b><br><sub>Minimal and square · after UIkit</sub></td>
<td width="33%" valign="top"><a href="docs/THEMES.md#layers"><img src="docs/media/themes/layers.jpg" alt="Layers"></a><br><b>26 &middot; Layers</b><br><sub>Material Design · after Materialize</sub></td>
<td width="33%" valign="top"><a href="docs/THEMES.md#utility"><img src="docs/media/themes/utility.jpg" alt="Utility"></a><br><b>27 &middot; Utility</b><br><sub>Utilitarian · after Foundation</sub></td>
</tr>
<tr>
<td width="33%" valign="top"><a href="docs/THEMES.md#sketch"><img src="docs/media/themes/sketch.jpg" alt="Sketch"></a><br><b>28 &middot; Sketch</b><br><sub>Hand-drawn paper · after PaperCSS</sub></td>
<td width="33%" valign="top"><a href="docs/THEMES.md#light"><img src="docs/media/themes/light.jpg" alt="Featherweight"></a><br><b>29 &middot; Featherweight</b><br><sub>Ultra-light · after Milligram</sub></td>
<td width="33%" valign="top"><a href="docs/THEMES.md#code"><img src="docs/media/themes/code.jpg" alt="Code"></a><br><b>30 &middot; Code</b><br><sub>Developer tools · after Primer</sub></td>
</tr>
</table>
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
- **Assemble from components.** [docs/COMPONENTS.md](docs/COMPONENTS.md): a
  list, a grid, a form or a dialog is a member variable, three calls per
  frame and a style struct.
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
| [COMPONENTS.md](docs/COMPONENTS.md) | The component library: the five rules, customising, every component with its knobs, slots, events and cues |
| [COMPONENT_INDEX.md](docs/COMPONENT_INDEX.md) | One table of every component: class, header, what it is for |
| [KIT.md](docs/KIT.md) | API reference: shapes, text, backdrops, glass, motion, input, feedback |
| [BUILDING_A_DESIGN.md](docs/BUILDING_A_DESIGN.md) | Step by step, with a complete skeleton, the tour, tests and pitfalls |
| [SOUND.md](docs/SOUND.md) | The cue vocabulary, the two sound sets, levels, adding recordings |
| [BACKDROPS.md](docs/BACKDROPS.md) | The procedural backgrounds and post overlays |
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | The layers, one frame, where every file lives |
| [ADOPTING.md](docs/ADOPTING.md) | Using the kit in your own app |
| [PERFORMANCE.md](docs/PERFORMANCE.md) | What keeps a UI at 60 frames per second on the console, and what was measured |
| [CONSOLE_VALIDATION.md](docs/CONSOLE_VALIDATION.md) | The self-driving tour run and the rules for console work |
| [docs/platform/](docs/platform/) | Building, packaging, deploying, the runtime shim, troubleshooting |
| [AGENTS.md](AGENTS.md) | For AI coding agents: where to look for what, the rules, how to add things, console facts, pitfalls (`CLAUDE.md` points to it) |

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
Validated on a PS5 on 2026-10-02 (build `044d909`): the app's self-driving
tour ran all 21 designs, the 30 themes and every Component Library page at
3840 x 2160. **Every design held 60 frames per second** (16.68 ms average, no
frame over 21.0 ms), a sample of the 266 pictures taken on the console matches
the PC renders, and the app closed itself cleanly. Numbers per design are in
[docs/PERFORMANCE.md](docs/PERFORMANCE.md#measured).

Not verified by a person yet: how the sounds and the rumble feel, and
navigation with a controller in hand (the tour injects its input).
<!-- END:validated -->

## Repository layout

```
src/concepts/     the designs, one file each
src/ui/           fonts, glyphs, motion helpers, themes, widgets
src/ui/components the component library
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
