# Performance on the console

A menu that drops frames feels broken however good it looks. The designs here
are built to hold 60 frames per second at 4K with a wide margin, and the tour
measures it (see [CONSOLE_VALIDATION.md](CONSOLE_VALIDATION.md)). This page
says what keeps a UI fast on PS5 through ps5-opengl, and what was measured.

## Where the time goes

A UI frame has three costs:

1. **Recording** (CPU): the design computes rectangles and appends instances.
   A screen is a few hundred to a few thousand 96-byte instances; this takes
   well under a millisecond.
2. **Submitting** (CPU, driver): one buffer upload and one draw call per run.
   This is the cost to watch: each draw call has a fixed price in the driver.
3. **Shading** (GPU): every instance is a quad whose fragment shader evaluates
   one small signed distance function. Backdrops shade every pixel of the
   screen once; the glass blur shades a 480 x 270 target five times.

## Rules that keep it fast

**Keep runs long.** A run is a stretch of instances that share a clip
rectangle and a texture; each run is one draw call.

- Text does not break a run: the font atlases sit on texture units of their
  own (`GlBatch::create_font_texture` returns a slot handle).
- An *image* starts a run and the next image with another texture starts
  another. Draw images of the same texture together; a grid of 24 different
  covers is 24 runs, which is fine.
- A clip rectangle starts a run. Clip a region (a scrolling list), not each
  row.
- `polygon` uses a second program and always starts a run. Prefer the SDF
  shapes (`triangle` with an angle, `arc`, `line`); they are also the only
  ones with anti-aliased edges.

**One mip level per texture.** Textures with mip chains leave ps5-opengl's
fast path. Every texture the kit creates sets `GL_TEXTURE_MAX_LEVEL` to 0.

**Orphan the instance buffer.** The batch re-specifies its buffer storage
before filling it each frame (`glBufferData(..., nullptr)` then
`glBufferSubData`). Reusing the same storage makes the driver wait for the
GPU to finish reading the previous frame.

**Never read the display surface back.** `glReadPixels` from framebuffer 0 is
a slow CPU path. The tour's pictures are drawn a second time into a small
off-screen target and read from there.

**Glass is cheap, but not free.** A frame that asks for glass replays the
backdrop and the scene into a 480 x 270 target and runs four blur passes.
That is a small fraction of a 4K frame, but ask for it only while a panel
that uses it is visible (`frame.glass = sheet_.value > 0.01f`).

**Backdrops are per-pixel.** At 4K the backdrop shader runs 8.3 million times
per frame. Keep a mode to a few dozen instructions, with constant loop
bounds, and no textures.

**Do nothing slow on the frame.** No file reads, no decoding, no shader
compilation after start-up. The sample covers are rendered once at launch;
fonts are baked offline; sounds are decoded at launch.

**Allocate at start-up.** `DrawList` keeps its vectors between frames, so
recording allocates nothing once the lists have grown. Avoid building many
`std::string`s per frame; format numbers with `snprintf` into a stack buffer.

**Frame time, not frame count.** Animation advances by measured time, clamped
to 50 ms, so a hitch (a system notification, a screenshot) costs one late
frame and no visible jump.

## Start-up

Start-up is not free, and it is paid again every time an app closes its
display and opens it again. Measured in ProsperoLight's launcher (four
programs, four fonts, twenty sounds, 3840 x 2160) on 2026-10-02:

| Step | Time after the launcher began to open |
| --- | --- |
| Display open (EGL, 4K surface) | 0.36 s |
| `Renderer::init`: the four programs | 3.6 s (each about 0.5 s, whatever its size) |
| Four fonts read and uploaded | 3.9 s |
| Sounds, controller, the screens built | 4.0 s |
| First frame presented | 5.8 s (the first `present` alone took 1.75 s) |

The log says how long each program took (`[HUI] program batch2d built in 561
ms`). Drawing every screen once before the first frame did not help: the
first draw costs the same whatever it draws (1.74 s, then 2 to 4 ms for each
further screen). The OpenGL runtime's shader cache did not shorten any of it.
These numbers were taken with an unbuffered log on `/data`, where every line
the runtime prints while compiling costs tens of milliseconds, so part of
each step is log writing; how much is not yet measured.

What follows from it:

- **Keep the splash picture until the first frame.** The OpenGL runtime hides
  it when the display opens, which leaves the screen black for all of the
  above. The build links with `--wrap=sceSystemServiceHideSplashScreen`,
  `src/runtime/runtime_shims.c` holds the request back, and
  `sys::hide_splash_screen()` (called after the first swap) lets it through.
- **Do not close the display for something short.** Reopening costs the whole
  table again.

## Budget

A 60 Hz frame is 16.67 ms. The app's log prints frame statistics every 600
frames (`[HUI] frames=600 avg=... p99=... max=... draws=... shapes=...`), and
the Pulse Dashboard design plots them live. Turn on "Show performance
overlay" in the Control Room design to see the numbers on any screen.

As a guide for a 4K UI on this renderer:

| Quantity | Comfortable | Look again |
| --- | --- | --- |
| Draw calls per frame | under 60 | over 150 |
| Instances per frame | under 6,000 | over 20,000 |
| Glass captures per frame | 0 or 1 | 2 (the info panel over a design that uses glass) |
| Texture uploads per frame | 0 | any |

## Measured

<!-- BEGIN:measured -->
Measured on a PS5 on 2026-10-02 with `tools/console-tour.py` (build `044d909`,
every design's whole tour, 39,956 frames, about eleven minutes):

- Output 3840 x 2160 at 60 Hz, `"4.6 (Core Profile) Mesa 26.2.0" renderer="PS5 AGC"`.
- **Every design held 60 frames per second**: averages of 16.67 to 16.69 ms and
  no frame over 21.0 ms, including frosted glass, the theme changes and the
  Component Library's 74 draw calls.
- 266 pictures were taken on the console; the twelve compared by eye with the PC
  renders (designs, themes and component pages) match them.
- Audio: no output errors; six stream underruns, all during start-up.
- Heap: about 34 MB live at the end, no failed allocation.
- The app closed itself through the system; no crash report.

| # | Design | Frames | Average | Worst frame | Most draw calls | Most shapes |
| --- | --- | --- | --- | --- | --- | --- |
| 01 | `aurora` | 552 | 16.69 ms | 19.55 ms | 40 | 746 |
| 02 | `paper` | 697 | 16.68 ms | 20.15 ms | 46 | 1,108 |
| 03 | `neon` | 798 | 16.68 ms | 20.25 ms | 10 | 433 |
| 04 | `editorial` | 657 | 16.68 ms | 19.85 ms | 14 | 1,575 |
| 05 | `carousel` | 776 | 16.68 ms | 20.34 ms | 37 | 377 |
| 06 | `radial` | 656 | 16.69 ms | 20.49 ms | 24 | 434 |
| 07 | `hud` | 1,140 | 16.68 ms | 19.94 ms | 17 | 715 |
| 08 | `dashboard` | 756 | 16.68 ms | 19.92 ms | 30 | 1,816 |
| 09 | `player` | 857 | 16.68 ms | 19.52 ms | 16 | 501 |
| 10 | `keyboard` | 750 | 16.68 ms | 19.87 ms | 9 | 610 |
| 11 | `constellation` | 792 | 16.69 ms | 20.00 ms | 9 | 762 |
| 12 | `terminal` | 900 | 16.68 ms | 19.98 ms | 7 | 2,308 |
| 13 | `store` | 922 | 16.68 ms | 20.20 ms | 61 | 842 |
| 14 | `trophies` | 840 | 16.68 ms | 20.41 ms | 21 | 1,784 |
| 15 | `files` | 1,420 | 16.68 ms | 20.31 ms | 17 | 1,484 |
| 16 | `inventory` | 776 | 16.69 ms | 19.42 ms | 11 | 1,257 |
| 17 | `boot` | 1,168 | 16.68 ms | 20.00 ms | 63 | 827 |
| 18 | `settings` | 843 | 16.68 ms | 19.85 ms | 13 | 761 |
| 19 | `themes` | 9,463 | 16.68 ms | 20.76 ms | 11 | 1,198 |
| 20 | `components` | 14,895 | 16.68 ms | 21.02 ms | 74 | 2,438 |
| 21 | `toolbox` | 298 | 16.67 ms | 20.20 ms | 10 | 942 |

"Most draw calls" and "most shapes" are the peaks seen during that design's
tour. A frame over 16.7 ms here is the frame in which a picture was saved.
<!-- END:measured -->
