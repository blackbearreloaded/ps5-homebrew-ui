# Themes

A *design* (see [DESIGNS.md](DESIGNS.md)) is a bespoke screen. A *theme* is
the opposite idea: one set of standard widgets that can wear any design
language, the way a web page changes when you swap its CSS framework.

The kit has both because both are needed. A launcher's home screen deserves a
bespoke design. Its settings page, a dialog or a tool does not: it wants good
standard widgets in a consistent style. `ui::Theme` and `ui::Painter` give you
those, in thirty styles, all drawn by the same OpenGL renderer as everything
else.

Open the **Theme Lab** design in the app and press **L2 / R2** to restyle one
working screen through all of them.

## Using a theme

```cpp
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

const ui::Theme &theme = ui::themes()[0];          // or find one by id
frame.backdrop = theme.backdrop;                   // the page behind the widgets
frame.backdrop.time = clock_;

ui::Painter paint(frame.scene, context_.fonts, theme, frame.glass_texture);
paint.panel({96, 280, 1080, 600});
paint.heading("Settings", 136, 350, 44);
paint.button({136, 400, 236, 64}, "Continue", ui::ButtonKind::primary, look);
paint.toggle({508, 524, 88, 46}, on_amount, look);
paint.slider({300, 680, 720, 44}, value, look);
```

Widgets are stateless and take *animated* values:

- `ui::Look{focus, press, disabled}`: how focused (0..1) and how pressed
  (0..1) the widget is. You ease them (a `tween::Spring` for focus, a
  `ui::Pulse` for press); the painter draws them.
- a toggle is given how "on" it is (0..1), a slider its position (0..1), tabs
  the selected index as a float (1.4 is between tabs 1 and 2).

Ease those values with the theme's own motion tokens
(`value.update(dt, theme.omega, theme.damping)` on a `tween::Bounce`) and play
cues in the theme's sound set (`event.set = theme.sounds`): then Candy
bounces, Classic and Pixel snap, Clay glides, each in its own voice. Motion
and sound are part of a design language, not an afterthought.

Glass themes need the blurred backdrop: draw the widgets in `frame.overlay`
and set `frame.glass = true` (see `src/concepts/themes.cpp`).

## The widgets

| Painter call | Widget |
| --- | --- |
| `panel(rect)` | Card, dialog body |
| `button(rect, label, kind, look)` | `primary`, `secondary`, `ghost` |
| `toggle(rect, value, look)` | Switch |
| `checkbox(rect, value, look)` | Check box; the mark draws itself as `value` rises |
| `radio(rect, value, look)` | Radio button |
| `slider(rect, value, look)` | Track, fill and thumb |
| `progress(rect, value)` | Progress bar (blocks in bevelled and pixel themes) |
| `tabs(rect, labels, active, look)` | Segmented control, boxed tabs or underlined tabs, by theme |
| `field(rect, text, caret, look)` | Text field (a box or an underline, by theme) |
| `chip(rect, label, selected, look)` | Chip, tag, badge |
| `row(rect, label, value, selected, look)` | List row |
| `focus_ring(rect, radius, amount)` | The theme's focus indicator, for your own widgets |
| `surface`, `well`, `fill`, `stroke` | The building blocks, for widgets of your own |
| `heading`, `label`, `body` | Text in the theme's faces, capitals and tracking |

Build your own widgets from `surface()` (raised) and `well()` (sunken): they
pick up every theme automatically.

## What a theme is made of

`ui::Theme` (`src/ui/theme.hpp`) is plain data:

| Group | Tokens |
| --- | --- |
| Page | `backdrop`, `page`, `page_text`, `page_text_muted` |
| Palette | `surface`, `surface_high`, `text`, `text_muted`, `primary`, `on_primary`, `secondary`, `on_secondary`, `accent`, `outline`, `focus`, `shadow`, `light` |
| Shape | `style`, `corner`, `radius`, `radius_card`, `border`, `button_border`, `pill_chips`, `pill_switches`, `shadow_offset`, `shadow_blur`, `focus_width`, `focus_gap`, `underline_fields` |
| Type | `heading`, `label`, `caps`, `tracking` |
| Motion and sound | `omega`, `damping`, `sounds`, `dark` |

Two tokens do most of the work.

**`style`** is how a surface is built:

| `SurfaceStyle` | Construction | Themes |
| --- | --- | --- |
| `flat` | A fill and, with a border, a hairline. Cards may still float on a shadow | most web looks |
| `soft` | A fill on a soft drop shadow that shrinks as the surface is pressed | Daisy, Material, Layers, Candy |
| `hard` | Thick outline and a solid offset shadow; pressing moves the body into the shadow | Brutal |
| `neumorphic` | The page's own colour, raised by a light shadow up-left and a dark one down-right; pressed, it turns into a dent | Clay |
| `bevel` | Light top-left edges, dark bottom-right edges; swapped when pressed | Classic |
| `gloss` | Vertical gradient, glassy highlight over the top half, dark edge | Gloss |
| `glass` | The blurred screen behind, a tint, a hairline of light | Acrylic |
| `outline` | A stroke only | Blueprint, Contrast |
| `glow` | Dark fill, bright stroke, coloured light around it | Hazard |
| `pixel` | Notched outline, a darker band inside the bottom-right edge | Pixel |
| `sketch` | Four slightly crooked pen strokes over a fill that is not quite square | Sketch |

**`corner`** is `round` (radius 0 is square, 100 is a pill), `chamfer` (cut at
45 degrees) or `pixel` (one square notch).

## The gallery

Ten themes are design languages in their own right. Twenty are modelled on
well-known web frameworks: their colours, corner radii, border widths, shadows
and type weights were read from the frameworks' own component pages and scaled
from desktop to television size (about 1.6 times: a 4 px web radius is 7 here).
They are homages rebuilt with this kit's shapes. No framework code, font or
asset is used, and the names of those projects belong to their owners.

<!-- BEGIN:themes -->
<!-- END:themes -->

## Adding a theme

1. In `src/ui/theme.cpp`, write a function that starts from `web_light()` or
   `web_dark()` and overrides what differs, and add it to the array in
   `themes()` (and raise the array's size).
2. Run `tools/host-snapshots.sh build/snapshots themes` and look at
   `build/snapshots/NN-themes-<id>.png`.
3. Check contrast at a distance: body text against `surface`, `on_primary`
   against `primary`, and the focus ring against both the control and the page.

If the look needs a construction the existing styles cannot give, add a
`SurfaceStyle` and handle it in `Painter::surface`, `well` and `focus_ring`
(`src/ui/widgets.cpp`); every widget then gains it at once.

## Honest limits

- **Fonts.** The kit ships four faces. A theme picks among them (plus the
  built-in 5x7 pixel face), so a framework's own typeface is approximated.
- **Scale.** Web controls are about 36 px tall and read at arm's length. Here
  they are 58 to 64 px and read from a sofa; proportions were kept, sizes were
  not.
- **States.** Focus, pressed and disabled are drawn. Hover does not exist on a
  console.
- **One widget set.** These are the controls a console UI needs. There is no
  data table, tree, date picker or menu bar.
