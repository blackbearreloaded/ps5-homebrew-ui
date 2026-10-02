# Backdrops and post overlays

Behind every design runs one fragment shader over the whole screen
(`src/gfx/backdrop.cpp`). A design picks a mode, up to four colours and up to
four parameters; the shader does the rest. A moving, coloured backdrop is the
cheapest way to make a screen feel alive, and it costs one draw call.

```cpp
frame.backdrop.mode = gfx::BackdropMode::aurora;
frame.backdrop.colors[0] = Color::rgb(0x06222e);
frame.backdrop.colors[1] = Color::rgb(0x12708a);
frame.backdrop.colors[2] = Color::rgb(0x2bb0a0);
frame.backdrop.colors[3] = Color::rgb(0x7ff0d8);
frame.backdrop.time = clock_;   // seconds; do not advance it to freeze the motion
```

## Modes

| Mode | Look | c0 | c1 | c2 | c3 | Parameters |
| --- | --- | --- | --- | --- | --- | --- |
| `gradient` | Vertical gradient with one soft highlight | top | bottom | highlight | | p0, p1: highlight centre (0..1); p2: strength |
| `aurora` | Slow, domain-warped colour clouds | top | bottom | cloud | second cloud | |
| `grid` | Synthwave: perspective floor, striped sun, stars | sky top | horizon | grid lines | sun | |
| `stars` | Three layers of twinkling stars over a nebula | space top | space bottom | nebula | second nebula | p0, p1: camera position (parallax) |
| `waves` | Five layered sine ribbons | top | bottom | first ribbon | last ribbon | |
| `bokeh` | Soft discs drifting upward | top | bottom | disc | second disc | |
| `phosphor` | CRT glass: dark with a centre glow and faint noise | dark | glow | | | |
| `paper` | Warm paper with grain, lit from the top-left | top | bottom | light | | |
| `dots` | Dot matrix with a pulse travelling from the centre | top | bottom | dots | | |
| `vista` | Parallax ridges under a sky with a sun: a stand-in game world | sky | horizon haze | far ridge | near ridge | p0: scroll speed |

All modes add a little noise at the end, which hides the banding that slow
gradients show on TVs.

## Post overlays

`frame.post` runs the same shader after the scene and overlay, blended on top.

| Mode | Look | Parameters |
| --- | --- | --- |
| `scanlines` | CRT lines plus darkened corners | p0: line strength (0.15 to 0.3); p1: vignette strength |
| `vignette` | Darkened corners in colour c0 | p0: strength |

## Making the backdrop respond

The backdrop is most effective when it is not fixed:

- **Follow the focus.** Ease the four colours toward the palette of the
  focused item with `ui::SpringColor` (Aurora Shelf does this).
- **Follow a camera.** `stars` takes a camera position, so a map or a tree
  can pan against a parallax sky.
- **Calm down under overlays.** Dim the scene and let the glass blur it; do
  not also animate it faster.
- **Honour reduced motion.** Stop advancing `time`.

## Adding a mode

1. Add a value to `gfx::BackdropMode` (`src/gfx/backdrop_spec.hpp`); values of
   20 and above are post overlays.
2. Add a branch to the fragment shader in `src/gfx/backdrop.cpp`. `uv` is
   0..1 from the top-left, `centred` is aspect-corrected around the middle,
   `u_time` is seconds, `c0..c3` and `u_params` are the design's values.
3. Document it in the tables above and render it with `make host-snapshots`.

Keep a mode to a few dozen texture-free instructions per pixel: at 4K it runs
8.3 million times per frame. Loops must have constant bounds.
