# The kit: API reference

Everything a design draws, animates and plays goes through a few small
headers under `src/`. This page is the map. The headers themselves are short
and commented; read them when you need exact signatures.

| Need | Header |
| --- | --- |
| Shapes, text, images, clipping, transforms | `gfx/draw_list.hpp` |
| Procedural backgrounds and post overlays | `gfx/backdrop.hpp` |
| Frame composition and frosted glass | `gfx/renderer.hpp` |
| Fonts and text helpers | `ui/fonts.hpp`, `gfx/font.hpp` |
| Controller glyphs and hint rows | `ui/glyphs.hpp` |
| Standard widgets in thirty themes | `ui/theme.hpp`, `ui/widgets.hpp` ([THEMES.md](THEMES.md)) |
| Whole components: lists, grids, dialogs, forms, indicators | `ui/components.hpp` ([COMPONENTS.md](COMPONENTS.md)) |
| Sounds and rumble a screen asks for | `ui/feedback.hpp` |
| Hard-edged bitmap text | `ui/pixel_font.hpp` |
| Easing curves and springs | `core/tween.hpp` |
| Focus rings, scrolling, pulses, colour springs | `ui/motion.hpp` |
| Controller input | `core/input.hpp` |
| Sound cues | `audio/cues.hpp` |
| The design interface | `app/concept.hpp` |
| Sample content (titles, covers) | `demo/catalog.hpp` |

## Coordinates and colour

- The canvas is **1920 x 1080 virtual pixels**, origin top-left, y down
  (`gfx::kVirtualWidth`, `gfx::kVirtualHeight`). It is scaled to the surface.
- `gfx::Rect{x, y, w, h}` with helpers `cx()`, `cy()`, `inset(amount)`.
- `gfx::Color{r, g, b, a}` in 0..1, straight alpha. `Color::rgb(0x7cf0c8)`,
  `Color::rgb(0x000000, 0.5f)`, `c.with_alpha(0.6f)`, `gfx::mix(a, b, t)`.
- **Angles** are radians, clockwise from 12 o'clock.

## DrawList: what you can draw

A `DrawList` records one frame of 2D drawing. Later calls draw on top.

| Call | Draws |
| --- | --- |
| `rounded_rect(r, radius, fill)` | Filled rectangle; radius 0 is square, half the height is a pill |
| `gradient_rect(r, radius, top, bottom)` | Vertical gradient |
| `gradient_rect_h(r, radius, left, right)` | Horizontal gradient |
| `bordered_rect(r, radius, fill, border, border_color)` | Fill plus an inner border; a transparent fill gives an outline |
| `rotated_rect(r, radius, angle, fill)` | Rectangle turned about its centre |
| `chamfer_rect(r, cut, fill, border, border_color)` | Rectangle with corners cut at 45 degrees |
| `circle(cx, cy, radius, fill)` / `ring(cx, cy, radius, thickness, color)` | Disc / outline |
| `arc(cx, cy, radius, thickness, start, sweep, color, round_caps)` | Ring sector: gauges, spinners, radial menus. `radius` is the outer edge |
| `line(x1, y1, x2, y2, thickness, color)` | Segment with round caps |
| `triangle(r, fill, outline, angle)` | Triangle pointing up, or turned by `angle` (1.5708 points right): chevrons, play icons, arrowheads. `outline > 0` strokes it |
| `star(cx, cy, radius, fill, outline)` | Five-pointed star |
| `shadow(r, radius, softness, color)` | Soft dark falloff; offset the rect down to "lift" a card |
| `glow(r, radius, spread, color)` | The same falloff used as coloured light |
| `image(texture, r, uv, tint, radius)` | Texture, tinted, optional rounded corners |
| `image_gradient(texture, r, uv, top, bottom, radius)` | The same with a tint that blends top to bottom: fades and reflections |
| `glass(texture, r, radius, tint)` | The blurred copy of what is behind `r` (frosted panels) |
| `polygon(xy, count, fill)` | Filled polygon; edges are not anti-aliased, keep it small |
| `text(...)` | One line of text; prefer `ui::text` below |

All shapes are anti-aliased in the shader and stay sharp at any scale.

### State: clip, transform, opacity

```cpp
list.push_clip(rect);               // scissor; nests by intersection
list.push_transform(scale, origin_x, origin_y, dx, dy);  // zooms and slides
list.push_opacity(alpha);           // multiplies every alpha drawn inside
...
list.pop_opacity(); list.pop_transform(); list.pop_clip();
```

Transforms and opacity are applied when a shape is recorded, so they cost
nothing at draw time and nest freely. They are how whole groups fade, slide
and zoom: wrap the group, animate the three numbers.

Clipping is rectangular (a scissor). Each distinct clip rectangle starts a new
draw call, so clip regions, not individual items.

### Images

`image(texture, rect, uv, tint, radius)`:

- textures you upload with `GlBatch::create_texture(width, height, rgba)` use
  `gfx::kFullUv`;
- textures you rendered into (the sample covers, a `gfx::Canvas`) are stored
  bottom row first: use `gfx::kCanvasUv`.

The sample covers are `context.catalog[i].cover`.

## Text

```cpp
ui::text(list, fonts.semibold, "Resume", x, baseline, 28, color);
ui::text(list, fonts.semibold, "SETTINGS", x, baseline, 18, color, gfx::Align::left, 3.0f); // tracked
ui::text(list, fonts.regular, value, right_edge, baseline, 24, color, gfx::Align::right);
ui::paragraph(list, fonts.regular, blurb, x, first_baseline, 26, width, line_height, color, max_lines);
fonts.regular.measure("text", 24);            // width in virtual pixels
fonts.regular.font->fit(text, 24, max_width); // ellipsis if too long
fonts.regular.font->wrap(text, 24, width);    // lines
ui::upper("Continue playing");                // ASCII capitals for tracked labels
```

`context.fonts` holds six faces:

| Member | Face | Use |
| --- | --- | --- |
| `regular` | Inter Regular | Body text |
| `semibold` | Inter SemiBold | Titles, labels, buttons |
| `display` | Montserrat Medium | Hero titles, wordmarks |
| `mono` | DejaVu Sans Mono | Changing numbers, code, terminals |
| `pixel` | Press Start 2P | An 8 x 8 bitmap face; use sizes that are multiples of 8 (or 4) so its pixels stay square |
| `hand` | Patrick Hand | Handwriting, for sketched or playful screens |

Text is positioned by its **baseline**. Fonts are baked signed-distance
fields: any size is sharp, and one atlas per face is all the memory they use.
The atlases sit on texture units of their own (up to `gfx::kFontSlots`, six), so text never interrupts
a run of shapes: a screen full of mixed text and shapes is one draw call.
The glyph set is printable ASCII plus `· × © ° – — • … ← ↑ → ↓ ✓` (and, in
DejaVu Sans Mono, `█ ● ▲ ▶ ▼ ◀`); a face that lacks a symbol simply skips it,
and curly quotes are not baked at all. Check `font->has_glyph(codepoint)` before
relying on a symbol; draw icons from shapes instead of hunting for glyphs.

## Backdrops

A design sets `frame.backdrop` to choose what the full-screen shader paints
behind it, and optionally `frame.post` for an overlay on top of everything.

```cpp
frame.backdrop.mode = gfx::BackdropMode::aurora;
frame.backdrop.colors[0] = ...;   // up to four colours, meaning depends on the mode
frame.backdrop.params[0] = ...;   // up to four parameters
frame.backdrop.time = clock_;     // seconds; freeze it to stop the motion
```

See [BACKDROPS.md](BACKDROPS.md) for every mode with pictures and parameters.

## Frosted glass

```cpp
frame.glass = true;                                    // ask for the blurred copy
frame.overlay.glass(frame.glass_texture, panel, 40, white);   // paint it
frame.overlay.rounded_rect(panel, 40, tint.with_alpha(0.6f)); // tint
frame.overlay.bordered_rect(panel, 40, clear, 1.5f, white.with_alpha(0.2f)); // light edge
```

The blur holds the backdrop and `frame.scene`. Only things drawn in
`frame.overlay` can use it. It costs one extra low-resolution pass, only on
frames that ask for it.

## Motion

`core/tween.hpp`:

| Tool | Use |
| --- | --- |
| `tween::Spring` | A value that chases a target, critically damped. `omega` 20 snappy, 12 default, 6 soft |
| `tween::Bounce` | The same with overshoot; `kick(impulse)` for flicks |
| `tween::Timer` | A one-shot 0..1 timeline: `start(seconds)`, `update(dt)`, `progress()` |
| `cubic_out`, `quint_out`, `expo_out` | Arrive |
| `cubic_in` | Leave |
| `cubic_in_out`, `smoothstep` | Move within the screen |
| `back_out`, `elastic_out` | Pop, celebrate |
| `ping` | 0 to 1 to 0 |
| `stagger(elapsed, index, step, duration)` | Entrance progress of the nth item |
| `lerp`, `inverse_lerp`, `clamp01` | Arithmetic |

`ui/motion.hpp`:

| Tool | Use |
| --- | --- |
| `ui::SpringRect` | A rectangle that glides to the focus: `snap`, `target`, `update`, `value` |
| `ui::SpringColor` | A colour that eases to a target |
| `ui::Scroller` | `reveal(start, end, view, margin, limit)` keeps the focus on screen; draw at `-offset()` |
| `ui::Pulse` | Flash to 1 and decay: `trigger()`, `update(dt, rate)`, `value` |
| `ui::shake(pulse, time)` | Horizontal offset of a refusal nudge |
| `ui::breathe(time)` | Slow 0..1 for idle glows |
| `ui::pan_for_x(x)` | Stereo position for a sound made at virtual x |

## Input

`update()` receives an `InputFrame` (see `core/input.hpp`):

| Field | Meaning |
| --- | --- |
| `nav` | `Direction::up/down/left/right/none` from the D-pad or left stick; fires on press, then repeats while held |
| `nav_repeat` | True when `nav` is a repeat, not a fresh press |
| `is_pressed(Action::confirm)` | Went down this frame |
| `is_held(Action::west)` | Is down |
| `nav_from_stick` | The step came from the left stick, not the D-pad (for screens that also use the stick as an analog control) |
| `stick_x`, `stick_y` | Left stick, -1..1, right and down positive, after a radial dead zone |
| `stick2_x`, `stick2_y` | Right stick |
| `trigger_l`, `trigger_r` | Analog triggers, 0..1 |

Actions: `confirm` (Cross), `back` (Circle), `north` (Triangle), `west`
(Square), `jump_prev` / `jump_next` (L2 / R2), `menu` (Options), `l3`, `r3`.
Confirm and back follow the player's button-swap setting. **L1, R1 and the
touchpad belong to the shell**: a design never sees them.

## Controller glyphs and hints

```cpp
const ui::Hint hints[] = {{ui::Button::cross, "Select"}, {ui::Button::l2, "Tab", ui::Button::r2}};
ui::draw_hints(list, fonts, ui::GlyphStyle::dark(), hints, 2, 1824, true);  // right-aligned at x
float width = ui::measure_hints(fonts, hints, 2);                           // for a plate behind it
ui::draw_button(list, fonts, style, ui::Button::triangle, x, cy, 40);       // one glyph
```

`GlyphStyle::dark()` suits dark pages, `light()` light ones, and
`mono(ink, body)` draws every glyph in one colour. `HintLayout` sets the
glyph size, text size, the row's centre line and the label face.

## Feedback: sound and rumble

```cpp
feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(x));   // cue, pitch, pan, gain
feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
feedback.rumble(0.25f, 0.05f);                              // strength 0..1, seconds
```

Cues are described in `audio/cues.hpp`; [SOUND.md](SOUND.md) explains the two
sound sets and which cues each records.

## The design interface

See [BUILDING_A_DESIGN.md](BUILDING_A_DESIGN.md). In short: derive from
`app::Concept`, implement `info()`, `enter()`, `update()` and `draw()`, and
register a factory.

## What the kit does not do

Stated plainly, so you do not look for it:

- **No retained widget tree or layout engine.** Screens are immediate mode:
  you compute rectangles and draw. For a controller-driven UI of fixed
  resolution this is less code, not more.
- **No rotation of text or images**, only of rounded rectangles and
  triangles. No non-uniform scale: `push_transform` scales both axes alike.
- **`glow` and `shadow` are filled.** They light the area under a shape as
  well as around it: draw them first, then the shape on top.
- **No rounded clipping.** Clips are rectangles; rounded images use the
  `radius` argument instead.
- **No text shaping.** Left-to-right text in the baked glyph set only: a
  character outside it draws as nothing. For other scripts see how
  ProsperoEden shapes text with HarfBuzz and system fonts, and what
  ProsperoRadio needed to adopt it ([ADOPTING.md](ADOPTING.md)).
- **No image decoder.** Upload RGBA pixels; decode files with a library of
  your choice (stb_image is a single header).
