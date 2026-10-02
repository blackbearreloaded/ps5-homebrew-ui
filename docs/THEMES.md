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
| # | Theme | Design language | Recipe |
| --- | --- | --- | --- |
| 01 | [Acrylic](#acrylic) | Frosted glass | Blurred translucent panels, hairline light edges, small radii |
| 02 | [Brutal](#brutal) | Neo-brutalism | Square corners, thick black outlines, hard offset shadows, loud flats |
| 03 | [Clay](#clay) | Neumorphism | Everything is the page colour, raised and pressed by two shadows |
| 04 | [Tiles](#tiles) | Flat | No depth at all: square colour blocks, big type, capitals |
| 05 | [Gloss](#gloss) | Skeuomorphic | Gradients, glassy highlights and dark edges: buttons that look pressable |
| 06 | [Classic](#classic) | Bevelled desktop | Grey panels with a light and a dark edge; nothing eases, everything clicks |
| 07 | [Blueprint](#blueprint) | Wireframe | Strokes only, one colour, monospaced capitals on a drafting grid |
| 08 | [Hazard](#hazard) | Sci-fi console | Cut corners, lit strokes and glow on near-black |
| 09 | [Candy](#candy) | Playful pastel | Fully round, pastel, coloured shadows, everything bounces |
| 10 | [Contrast](#contrast) | High contrast | Black, white and one signal colour; thick strokes; nothing subtle |
| 11 | [Pixel](#pixel) | 8-bit pixel art · after NES.css | Notched black outlines, a darker band inside each button, a bitmap font |
| 12 | [Soft](#soft) | Modern soft · after Mantine | Blue on white, 8 px corners, tinted "light" buttons, no shadows |
| 13 | [Daisy](#daisy) | Clean and customisable · after daisyUI | Indigo, pink and teal on white; small radii; a hint of depth under buttons |
| 14 | [Pico](#pico) | Minimal and classless · after Pico.css | Large calm type, azure actions, soft layered card shadows |
| 15 | [Enterprise](#ant) | Structured enterprise · after Ant Design | Dense, orderly, hairline borders, 6 px corners, one confident blue |
| 16 | [Chakra](#chakra) | Modern and accessible · after Chakra UI | Teal on white, grey ghost buttons, soft cards, a blue focus halo |
| 17 | [Fresh](#fresh) | Modern flat · after Nuxt UI | Slate night, one vivid green, hairline rings, pill badges |
| 18 | [Neutral](#neutral) | Neutral modern · after Shoelace | Zinc greys, sky blue, 4 px corners, a wide translucent focus ring |
| 19 | [Material](#paper) | Material Design · after Propeller | Raised sheets with real shadows, capitals, underlined fields, a pink accent |
| 20 | [Humane](#humane) | Clean and readable · after Semantic UI | Soft grey buttons with bold labels, white segments, 4 px corners |
| 21 | [Standard](#standard) | General purpose · after Bootstrap 5 | The familiar default: medium corners, blue and grey buttons, a wide focus halo |
| 22 | [Pill](#pill) | High-contrast dashboard · after Preline UI | Near-black, white and blue; every control is a smooth pill |
| 23 | [Admin](#admin) | Dark admin · after Flowbite | Blue-grey night panels, rounded 8 px controls, a four pixel focus ring |
| 24 | [Friendly](#friendly) | Friendly and soft · after Bulma | Turquoise, generous round boxes floating on a long soft shadow |
| 25 | [Crisp](#crisp) | Minimal and square · after UIkit | Square edges, small capitals, grey text, one bright blue |
| 26 | [Layers](#layers) | Material Design · after Materialize | Teal and coral, flat layers stacked by elevation, capitals, underlined fields |
| 27 | [Utility](#utility) | Utilitarian · after Foundation | Sharp square blocks, plain borders, no decoration at all |
| 28 | [Sketch](#sketch) | Hand-drawn paper · after PaperCSS | Crooked pen lines, tinted fills and soft shadows: a UI on a notepad |
| 29 | [Featherweight](#light) | Ultra-light · after Milligram | Almost nothing: thin grey rules and small purple capitals with wide tracking |
| 30 | [Code](#code) | Developer tools · after Primer | Ink-blue night, hairline borders, a green call to action, blue focus |

<a id="acrylic"></a>

### 01 &middot; Acrylic

*Frosted glass.* Blurred translucent panels, hairline light edges, small radii. Theme id `acrylic`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/acrylic.jpg" alt="Acrylic"></td>
<td width="50%" valign="top"><img src="media/themes/acrylic.webp" alt="Acrylic in motion"></td>
</tr>
</table>

<a id="brutal"></a>

### 02 &middot; Brutal

*Neo-brutalism.* Square corners, thick black outlines, hard offset shadows, loud flats. Theme id `brutal`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/brutal.jpg" alt="Brutal"></td>
<td width="50%" valign="top"><img src="media/themes/brutal.webp" alt="Brutal in motion"></td>
</tr>
</table>

<a id="clay"></a>

### 03 &middot; Clay

*Neumorphism.* Everything is the page colour, raised and pressed by two shadows. Theme id `clay`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/clay.jpg" alt="Clay"></td>
<td width="50%" valign="top"><img src="media/themes/clay.webp" alt="Clay in motion"></td>
</tr>
</table>

<a id="tiles"></a>

### 04 &middot; Tiles

*Flat.* No depth at all: square colour blocks, big type, capitals. Theme id `tiles`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/tiles.jpg" alt="Tiles"></td>
<td width="50%" valign="top"><img src="media/themes/tiles.webp" alt="Tiles in motion"></td>
</tr>
</table>

<a id="gloss"></a>

### 05 &middot; Gloss

*Skeuomorphic.* Gradients, glassy highlights and dark edges: buttons that look pressable. Theme id `gloss`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/gloss.jpg" alt="Gloss"></td>
<td width="50%" valign="top"><img src="media/themes/gloss.webp" alt="Gloss in motion"></td>
</tr>
</table>

<a id="classic"></a>

### 06 &middot; Classic

*Bevelled desktop.* Grey panels with a light and a dark edge; nothing eases, everything clicks. Theme id `classic`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/classic.jpg" alt="Classic"></td>
<td width="50%" valign="top"><img src="media/themes/classic.webp" alt="Classic in motion"></td>
</tr>
</table>

<a id="blueprint"></a>

### 07 &middot; Blueprint

*Wireframe.* Strokes only, one colour, monospaced capitals on a drafting grid. Theme id `blueprint`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/blueprint.jpg" alt="Blueprint"></td>
<td width="50%" valign="top"><img src="media/themes/blueprint.webp" alt="Blueprint in motion"></td>
</tr>
</table>

<a id="hazard"></a>

### 08 &middot; Hazard

*Sci-fi console.* Cut corners, lit strokes and glow on near-black. Theme id `hazard`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/hazard.jpg" alt="Hazard"></td>
<td width="50%" valign="top"><img src="media/themes/hazard.webp" alt="Hazard in motion"></td>
</tr>
</table>

<a id="candy"></a>

### 09 &middot; Candy

*Playful pastel.* Fully round, pastel, coloured shadows, everything bounces. Theme id `candy`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/candy.jpg" alt="Candy"></td>
<td width="50%" valign="top"><img src="media/themes/candy.webp" alt="Candy in motion"></td>
</tr>
</table>

<a id="contrast"></a>

### 10 &middot; Contrast

*High contrast.* Black, white and one signal colour; thick strokes; nothing subtle. Theme id `contrast`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/contrast.jpg" alt="Contrast"></td>
<td width="50%" valign="top"><img src="media/themes/contrast.webp" alt="Contrast in motion"></td>
</tr>
</table>

<a id="pixel"></a>

### 11 &middot; Pixel

*8-bit pixel art · after NES.css.* Notched black outlines, a darker band inside each button, a bitmap font. Theme id `pixel`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/pixel.jpg" alt="Pixel"></td>
<td width="50%" valign="top"><img src="media/themes/pixel.webp" alt="Pixel in motion"></td>
</tr>
</table>

<a id="soft"></a>

### 12 &middot; Soft

*Modern soft · after Mantine.* Blue on white, 8 px corners, tinted "light" buttons, no shadows. Theme id `soft`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/soft.jpg" alt="Soft"></td>
<td width="50%" valign="top"><img src="media/themes/soft.webp" alt="Soft in motion"></td>
</tr>
</table>

<a id="daisy"></a>

### 13 &middot; Daisy

*Clean and customisable · after daisyUI.* Indigo, pink and teal on white; small radii; a hint of depth under buttons. Theme id `daisy`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/daisy.jpg" alt="Daisy"></td>
<td width="50%" valign="top"><img src="media/themes/daisy.webp" alt="Daisy in motion"></td>
</tr>
</table>

<a id="pico"></a>

### 14 &middot; Pico

*Minimal and classless · after Pico.css.* Large calm type, azure actions, soft layered card shadows. Theme id `pico`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/pico.jpg" alt="Pico"></td>
<td width="50%" valign="top"><img src="media/themes/pico.webp" alt="Pico in motion"></td>
</tr>
</table>

<a id="ant"></a>

### 15 &middot; Enterprise

*Structured enterprise · after Ant Design.* Dense, orderly, hairline borders, 6 px corners, one confident blue. Theme id `ant`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/ant.jpg" alt="Enterprise"></td>
<td width="50%" valign="top"><img src="media/themes/ant.webp" alt="Enterprise in motion"></td>
</tr>
</table>

<a id="chakra"></a>

### 16 &middot; Chakra

*Modern and accessible · after Chakra UI.* Teal on white, grey ghost buttons, soft cards, a blue focus halo. Theme id `chakra`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/chakra.jpg" alt="Chakra"></td>
<td width="50%" valign="top"><img src="media/themes/chakra.webp" alt="Chakra in motion"></td>
</tr>
</table>

<a id="fresh"></a>

### 17 &middot; Fresh

*Modern flat · after Nuxt UI.* Slate night, one vivid green, hairline rings, pill badges. Theme id `fresh`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/fresh.jpg" alt="Fresh"></td>
<td width="50%" valign="top"><img src="media/themes/fresh.webp" alt="Fresh in motion"></td>
</tr>
</table>

<a id="neutral"></a>

### 18 &middot; Neutral

*Neutral modern · after Shoelace.* Zinc greys, sky blue, 4 px corners, a wide translucent focus ring. Theme id `neutral`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/neutral.jpg" alt="Neutral"></td>
<td width="50%" valign="top"><img src="media/themes/neutral.webp" alt="Neutral in motion"></td>
</tr>
</table>

<a id="paper"></a>

### 19 &middot; Material

*Material Design · after Propeller.* Raised sheets with real shadows, capitals, underlined fields, a pink accent. Theme id `paper`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/paper.jpg" alt="Material"></td>
<td width="50%" valign="top"><img src="media/themes/paper.webp" alt="Material in motion"></td>
</tr>
</table>

<a id="humane"></a>

### 20 &middot; Humane

*Clean and readable · after Semantic UI.* Soft grey buttons with bold labels, white segments, 4 px corners. Theme id `humane`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/humane.jpg" alt="Humane"></td>
<td width="50%" valign="top"><img src="media/themes/humane.webp" alt="Humane in motion"></td>
</tr>
</table>

<a id="standard"></a>

### 21 &middot; Standard

*General purpose · after Bootstrap 5.* The familiar default: medium corners, blue and grey buttons, a wide focus halo. Theme id `standard`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/standard.jpg" alt="Standard"></td>
<td width="50%" valign="top"><img src="media/themes/standard.webp" alt="Standard in motion"></td>
</tr>
</table>

<a id="pill"></a>

### 22 &middot; Pill

*High-contrast dashboard · after Preline UI.* Near-black, white and blue; every control is a smooth pill. Theme id `pill`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/pill.jpg" alt="Pill"></td>
<td width="50%" valign="top"><img src="media/themes/pill.webp" alt="Pill in motion"></td>
</tr>
</table>

<a id="admin"></a>

### 23 &middot; Admin

*Dark admin · after Flowbite.* Blue-grey night panels, rounded 8 px controls, a four pixel focus ring. Theme id `admin`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/admin.jpg" alt="Admin"></td>
<td width="50%" valign="top"><img src="media/themes/admin.webp" alt="Admin in motion"></td>
</tr>
</table>

<a id="friendly"></a>

### 24 &middot; Friendly

*Friendly and soft · after Bulma.* Turquoise, generous round boxes floating on a long soft shadow. Theme id `friendly`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/friendly.jpg" alt="Friendly"></td>
<td width="50%" valign="top"><img src="media/themes/friendly.webp" alt="Friendly in motion"></td>
</tr>
</table>

<a id="crisp"></a>

### 25 &middot; Crisp

*Minimal and square · after UIkit.* Square edges, small capitals, grey text, one bright blue. Theme id `crisp`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/crisp.jpg" alt="Crisp"></td>
<td width="50%" valign="top"><img src="media/themes/crisp.webp" alt="Crisp in motion"></td>
</tr>
</table>

<a id="layers"></a>

### 26 &middot; Layers

*Material Design · after Materialize.* Teal and coral, flat layers stacked by elevation, capitals, underlined fields. Theme id `layers`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/layers.jpg" alt="Layers"></td>
<td width="50%" valign="top"><img src="media/themes/layers.webp" alt="Layers in motion"></td>
</tr>
</table>

<a id="utility"></a>

### 27 &middot; Utility

*Utilitarian · after Foundation.* Sharp square blocks, plain borders, no decoration at all. Theme id `utility`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/utility.jpg" alt="Utility"></td>
<td width="50%" valign="top"><img src="media/themes/utility.webp" alt="Utility in motion"></td>
</tr>
</table>

<a id="sketch"></a>

### 28 &middot; Sketch

*Hand-drawn paper · after PaperCSS.* Crooked pen lines, tinted fills and soft shadows: a UI on a notepad. Theme id `sketch`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/sketch.jpg" alt="Sketch"></td>
<td width="50%" valign="top"><img src="media/themes/sketch.webp" alt="Sketch in motion"></td>
</tr>
</table>

<a id="light"></a>

### 29 &middot; Featherweight

*Ultra-light · after Milligram.* Almost nothing: thin grey rules and small purple capitals with wide tracking. Theme id `light`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/light.jpg" alt="Featherweight"></td>
<td width="50%" valign="top"><img src="media/themes/light.webp" alt="Featherweight in motion"></td>
</tr>
</table>

<a id="code"></a>

### 30 &middot; Code

*Developer tools · after Primer.* Ink-blue night, hairline borders, a green call to action, blue focus. Theme id `code`.

<table>
<tr>
<td width="50%" valign="top"><img src="media/themes/code.jpg" alt="Code"></td>
<td width="50%" valign="top"><img src="media/themes/code.webp" alt="Code in motion"></td>
</tr>
</table>
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

- **Fonts.** The kit ships six faces. A theme picks among them, so a
  framework's own typeface is approximated, except where the typeface *is*
  the look: Pixel uses Press Start 2P, the face NES.css itself uses, and
  Sketch uses Patrick Hand, as PaperCSS does. (`ui/pixel_font.hpp` also
  draws a 5 x 7 face from plain rectangles, for apps that ship no pixel font.)
- **Scale.** Web controls are about 36 px tall and read at arm's length. Here
  they are 58 to 64 px and read from a sofa; proportions were kept, sizes were
  not.
- **States.** Focus, pressed and disabled are drawn. Hover does not exist on a
  console.
- **One widget set.** These are the controls a console UI needs. There is no
  data table, tree, date picker or menu bar.
