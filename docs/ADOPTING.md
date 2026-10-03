# Adopting the kit in your own app

You do not have to fork this app to use what is in it. The kit is a set of
plain C++20 files with no dependencies beyond OpenGL 4.5+, the C++ standard
library and `stb_vorbis` (music only). There are three ways in, from lightest
to fullest.

## 1. Read it and write your own

The ideas are the valuable part: one instanced SDF shader, springs for
everything that moves, a cue vocabulary instead of file names, a tour that
photographs the app. [CRAFT.md](CRAFT.md) is independent of this code.

## 2. Take the kit

Copy these directories into your project and add them to your build:

| Directory | You get | Needs |
| --- | --- | --- |
| `src/gfx/` | Draw list, GL batch, SDF fonts, backdrops, renderer, canvas | OpenGL headers, `platform/ps5/system.hpp` for logging |
| `src/ui/` | Fonts, controller glyphs, motion helpers, themes, widgets, pixel font, confetti | `gfx/`, `core/tween.hpp`, `audio/cues.hpp` (for the sound set enum) |
| `src/ui/components/` | The component library: lists, grids, carousels, tabs, menus, dialogs, sheets, toasts, forms, indicators ([COMPONENTS.md](COMPONENTS.md)) | the rest of `src/ui/`, `core/input.hpp` |
| `src/core/` | Input model, tweens and springs, settings, save files | nothing |
| `src/audio/` | Mixer, cue vocabulary and sound bank, music player | `core/save_file` (file reading), `third_party/stb` |
| `src/platform/ps5/` | EGL display, controller, audio output thread, system services | ps5-opengl SDK, PS5 payload SDK |
| `assets/fonts/`, `assets/audio/` | Baked fonts and the two sound sets | see the licences in THIRD_PARTY_NOTICES.md |

The only coupling to this app is the logging function `sys::log`. Keep
`platform/ps5/system.cpp` or provide your own five functions (`log`,
`monotonic_us`, `sleep_us`, `park`, `quit`); `host/platform_host.cpp` is a
twenty-line example.

The smallest program that draws with the kit:

```cpp
#include "gfx/renderer.hpp"
#include "ui/fonts.hpp"

gfx::Renderer renderer;
renderer.init();                                   // after the GL context exists

gfx::Font face;                                    // one baked font
std::string bytes;
save::read_file("/app0/assets/fonts/inter-regular.huifont", &bytes);
face.load(bytes);
ui::FontRef regular{&face, renderer.batch().create_font_texture(face)};

gfx::DrawList list;
for (;;)
{
    list.clear();
    list.shadow({660, 420, 600, 240}, 28, 40, gfx::Color::rgb(0x000000, 0.5f));
    list.rounded_rect({660, 400, 600, 240}, 28, gfx::Color::rgb(0x1c2140));
    ui::text(list, regular, "Hello, console", 960, 532, 44, gfx::Color::rgb(0xffffff),
             gfx::Align::center);

    gfx::BackdropSpec sky;
    sky.mode = gfx::BackdropMode::aurora;
    sky.colors[0] = gfx::Color::rgb(0x0b1026);
    sky.colors[1] = gfx::Color::rgb(0x1b1f4a);
    sky.colors[2] = gfx::Color::rgb(0x3a5bd9);
    sky.colors[3] = gfx::Color::rgb(0xc04bd6);
    sky.time = seconds;

    renderer.begin();
    renderer.backdrop(sky);
    renderer.draw(list);
    renderer.present(0, surface_width, surface_height);
    swap_buffers();
}
```

`src/main.cpp` is the full version of that loop with input, sound, settings
and frame statistics; it is about 350 lines and worth copying as a starting
point.

On another platform with desktop OpenGL 4.5 or newer the kit needs only a GL
context (`gfx::set_glsl_prefix` picks the `#version` line).
`host/snapshot_main.cpp` shows the Linux and Mesa version. OpenGL ES is not
supported as is: the batch draws with `glDrawArraysInstancedBaseInstance`,
which ES lacks without an extension.

## 3. Start from this app

Fork the repository when you want everything: the PS5 build and packaging,
the shell, the tour, the tests and CI.

1. `make init TITLE_ID=PPSA12345 APP_NAME="My App"` sets the identity in
   `sce_sys/param.json`.
2. Delete the designs you do not want from `src/concepts/` and their lines
   from `concepts.hpp` and `registry.cpp`. With one design left, the L1/R1
   switcher simply has nothing to switch to; remove the shell's banner in
   `app/shell.cpp` if you want no trace of it.
3. Replace `src/demo/` with your content, the fonts in `third_party/fonts/`
   (then `make fonts`), the sounds in `assets/audio/sfx/<set>/` and the icon
   in `sce_sys/`.
4. Keep the tour. It is your regression test and your screenshot generator.

## Things to carry over even if you take no code

- Lay out in a fixed virtual canvas and scale to the surface.
- Measure animation time from frame start to frame start, and clamp it.
- Accumulate input edges across every controller sample of a frame.
- Never load from disk on the frame; show a placeholder that fades to the
  content.
- Keep textures at a single mip level and orphan dynamic buffers before
  refilling them (see [PERFORMANCE.md](PERFORMANCE.md)).
- End the app by asking the system to close it
  (`sceSystemServiceLoadExec("exit", NULL)`), never with `exit()`.
- Give the app a scripted, self-ending run mode for hardware tests.

## What the first adoption taught

ProsperoRadio (57,793 stations from a public catalogue) was the first app
rebuilt on the kit. What it ran into, so the next one does not:

- **Text from the outside world is not ASCII.** The baked fonts cover
  printable ASCII and a few signs; station names arrive in Arabic, Cyrillic,
  Chinese and accented Latin, and drew as blanks. The app replaced
  `gfx/font.*` with ProsperoEden's engine (the console's fonts from
  `/preinst/common/font`, HarfBuzz shaping, right-to-left ordering) and added
  an upload of the atlas rows that changed each frame. Budget for this before
  showing names you did not write. The first glyph of a new script costs one
  frame of about 110 ms (the font file is read then): warm the scripts you
  expect behind the loading screen.
- **A big list needs a grid over a count.** A `CardItem` per station is tens
  of megabytes and a long `set_items()`; use `GridView::set_count()` and draw
  cells from pages of your own data
  ([collections.md](components/collections.md#a-grid-over-a-data-source)).
- **Jumps in a long list**: `JumpBar` beside the grid, `set_focus_at_top()`
  for its target, `hui::HeldStep` so held L2 / R2 keep paging. Work out where
  each letter starts in the list's own order with one query, not per jump.
  SQLite's `NOCASE` folds to lower case, so `[`, `_` and `@` sort before `a`:
  bucket by the folded character, or the marks point one section off.
- **Keep the splash until the first frame.** Renderer and font start-up took
  about half a second of black; linking with
  `--wrap=sceSystemServiceHideSplashScreen` and releasing it after the first
  present removes it.
- **Small writes to `/data` are slow** (about 5 ms a page with SQLite). Build
  a database in memory and write it once; never write on the frame. Save
  settings a moment after the last change, not on every step of a slider.
- **Toasts are enough for news the app has for the player** (a finished
  refresh, a newer release): `ToastStack::push` with its own duration, pushed
  once the loading screen is gone.
- **ShadowMountPlus stages `param.json` and the home-screen art once.** After
  changing them, replace the staged copies too, or the console keeps showing
  and checking the old ones.

## What the second adoption taught

ProsperoLight (a Sunshine client) replaced its SDL and RmlUi launcher with one
built on the kit, next to a video presenter of its own that drives AGC
directly. All of this was seen on a console unless it says otherwise.

- **Two renderers can take turns with the display.** The launcher closes its
  display (`renderer.release()`, `display.close()`), the stream opens the
  video output for itself, and the launcher opens again afterwards. `sceAgcInit`
  may run once per process: the OpenGL runtime does it, so the second renderer
  must skip its own. Every reopening pays the whole start-up again (see
  [Start-up](PERFORMANCE.md#start-up)).
- **A picture of a screen for someone else to show.** To keep the connecting
  screen up while the other presenter owns the display, the launcher draws it
  once more into a 1920 x 1080 texture attached to a framebuffer and reads it
  with `glReadPixels`: 36 ms on the console. The stream converts it to video
  frames and draws the progress bar on it. For that the app draws its own bar
  (`LoadingScreenStyle::indicator = LoadingIndicator::none`), so both sides
  can continue the same one.
- **An elevated app has no `/app0`.** Once a title has filesystem access its
  root is the console's: assets load from the install folder
  (`/data/homebrew/<TITLE_ID>` or `/mnt/sandbox/<TITLE_ID>_000/app0`). Every
  kit call takes its path from the app, so nothing in the kit changes; do not
  hard-code `/app0` in your own code.
- **The shader cache needs `mkstemp`.** Setting `PS5_SHADER_CACHE_DIR` made the
  app stop at its first shader: the SDK binds `mkstemp` (and `isatty`) to
  `libScePosixForWebKit`, which a native title does not load.
  `src/runtime/runtime_shims.c` now defines both. With them the cache works
  (18 of 22 shaders read back on the second launch) but start-up was not
  measurably shorter, so the kit's app does not turn it on.
- **Statistics lines and a slow log.** The OpenGL runtime prints about forty
  lines of statistics to the standard streams every 10,000 draws (about every
  nine seconds in a launcher). With the log in the title's own storage that
  costs nothing you can see. The elevated app had moved its unbuffered log to
  `/data`, where each write took 40 to 70 ms: the launcher froze for 1.15 s at
  the same frame numbers on every run (`renderer.present` measured, frame 528,
  1055, 1581). Keep the log in the sandbox, or buffer it and flush it from a
  thread of its own (not yet verified on a console).
- **Do not touch the standard streams' descriptors.** A build that sent
  `stdout` into a pipe with `dup2(pipe, fileno(stdout))` wrote nothing, ended
  at start, and the console restarted. `fileno` is a header macro that reads
  the C library's `FILE`, and the console's library lays it out differently.
  Use `freopen` and `setvbuf` only.
- **Network requests belong on a thread with a stack you chose.** Console
  threads run first-in first-out at one priority and are never time-sliced,
  and a thread's default stack is small. The launcher's worker (TLS requests
  to the PC) gets a 1 MiB stack, and the screen's thread a core of its own
  while the launcher runs.
- **Names from other people's PCs** needed more than ASCII here too, but not
  a shaping engine: `tools/font-baker` takes a `european` glyph set (accented
  Latin, Greek and Cyrillic) with a 2048 atlas.
- **A crash report pays for itself.** A signal handler that writes the fault
  address as an offset into the build's ELF, the registers and the code
  addresses found on the stack turns "it closed" into a function name.
  ProsperoEden and ProsperoLight each carry one; the kit's app does not yet.

## Licence

The code is GPL-3.0-or-later (see [LICENSE](../LICENSE)). The fonts and other
third-party parts carry their own terms, listed in
[THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md). The sound effects and
music are the author's own and are distributed under the project licence.
