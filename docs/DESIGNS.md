# The designs

Every design is a complete, working screen in one file under `src/concepts/`,
with its own layout, palette, motion character, interaction pattern and
sounds. In the app, **L1** and **R1** switch between them and the **touchpad**
opens a panel that says what the design demonstrates and where its code is.

They are meant to be read as much as looked at: each file opens with a comment
that lists what makes that design feel finished, and its constants (sizes,
radii, colours, timings) sit at the top. To build one of your own, start from
[BUILDING_A_DESIGN.md](BUILDING_A_DESIGN.md).

The pictures and clips below are rendered by the app itself running its tour
on a PC (`tools/render-media.sh`), with the same code that runs on the console.

<!-- BEGIN:designs -->
| # | Design | What it is | Sound set | Source |
| --- | --- | --- | --- | --- |
| 01 | [Aurora Shelf](#aurora) | A console home screen: hero panel, cover shelves, frosted details | glass | [`aurora.cpp`](../src/concepts/aurora.cpp) |
| 02 | [Paper Library](#paper) | A game shelf of paper cards that travel when sorted, filtered or picked up | paper | [`paper.cpp`](../src/concepts/paper.cpp) |
| 03 | [Neon Arcade](#neon) | A synthwave racer's main menu: neon sign, gliding tube, live previews | glass | [`neon.cpp`](../src/concepts/neon.cpp) |
| 04 | [Editorial](#editorial) | A magazine's weekly selection: big type on paper, and an article behind every row | paper | [`editorial.cpp`](../src/concepts/editorial.cpp) |
| 05 | [Cover Wheel](#carousel) | A carousel with weight: scrub it, let it coast, open a cover | glass | [`carousel.cpp`](../src/concepts/carousel.cpp) |
| 06 | [Radial Dial](#radial) | An in-game item wheel: aim with the stick, equip with one press | paper | [`radial.cpp`](../src/concepts/radial.cpp) |
| 07 | [Field HUD](#hud) | An in-game HUD over a moving world, and the pause menu behind Options | paper | [`hud.cpp`](../src/concepts/hud.cpp) |
| 08 | [Pulse Dashboard](#dashboard) | A bento grid of live data tiles that expand into detail views | glass | [`dashboard.cpp`](../src/concepts/dashboard.cpp) |
| 09 | [Now Playing](#player) | A music player: breathing artwork, a visualizer, a scrubber and a glass queue | glass | [`player.cpp`](../src/concepts/player.cpp) |
| 10 | [First Run](#keyboard) | A setup wizard: avatar, a name typed on a controller keyboard, a warm welcome | paper | [`keyboard.cpp`](../src/concepts/keyboard.cpp) |
| 11 | [Constellation](#constellation) | A skill tree as a star map: free 2D focus, a gliding camera, progression | paper | [`constellation.cpp`](../src/concepts/constellation.cpp) |
| 12 | [Phosphor](#terminal) | A monochrome CRT terminal: character grid, glow, typed text | paper | [`terminal.cpp`](../src/concepts/terminal.cpp) |
| 13 | [Storefront](#store) | A shop window: featured banner, product pages, a cart and a checkout | glass | [`store.cpp`](../src/concepts/store.cpp) |
| 14 | [Trophy Room](#trophies) | An achievements cabinet: metal medals, counting numbers, an unlock with ceremony | paper | [`trophies.cpp`](../src/concepts/trophies.cpp) |
| 15 | [File Browser](#files) | A file manager: aligned columns, folders that keep your place, visible results | paper | [`files.cpp`](../src/concepts/files.cpp) |
| 16 | [Satchel](#inventory) | An inventory you handle: lift, carry, swap, stack and equip, with a comparing tooltip | paper | [`inventory.cpp`](../src/concepts/inventory.cpp) |
| 17 | [Launch Sequence](#boot) | Before the menu: studio splash, title, profiles and an honest loading screen | glass | [`boot.cpp`](../src/concepts/boot.cpp) |
| 18 | [Control Room](#settings) | A settings screen with real controls: sliders, switches, steppers, a dialog | glass | [`settings.cpp`](../src/concepts/settings.cpp) |
| 19 | [Theme Lab](#themes) | One screen in thirty design languages: L2 and R2 restyle every widget | glass | [`themes.cpp`](../src/concepts/themes.cpp) |
| 20 | [Component Library](#components) | Reusable lists, grids, dialogs, forms and indicators, restyled by thirty themes | glass | [`components.cpp`](../src/concepts/components.cpp) |
| 21 | [Toolbox](#toolbox) | The kit on one screen: shapes, type, motion, glyphs and every sound | glass | [`toolbox.cpp`](../src/concepts/toolbox.cpp) |

<a id="aurora"></a>

## 01 &middot; Aurora Shelf

A console home screen: hero panel, cover shelves, frosted details.

<img src="media/designs/aurora.webp" width="640" alt="Aurora Shelf in motion">

**What it demonstrates**

- Backdrop colours eased toward the focused cover's palette (ui::SpringColor)
- Hero text and artwork cross-fade with a staggered slide on every focus change
- Spring-driven shelf scrolling, card growth and a gliding focus ring
- Frosted details sheet: Frame::glass blurs the screen behind the overlay
- Stereo-panned focus sounds, pitched row changes, refusal nudge at the ends

Source: [`src/concepts/aurora.cpp`](../src/concepts/aurora.cpp) &middot; tests: [`tests/unit/aurora_test.cpp`](../tests/unit/aurora_test.cpp) &middot; sound set: `glass`

<table>
<tr>
<td width="50%" valign="top"><img src="media/designs/aurora.jpg" alt="Aurora Shelf: aurora"></td>
<td width="50%" valign="top"><img src="media/designs/aurora-details.jpg" alt="Aurora Shelf: aurora-details"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/aurora-info.jpg" alt="Aurora Shelf: aurora-info"></td>
<td width="50%" valign="top"><img src="media/designs/aurora-library.jpg" alt="Aurora Shelf: aurora-library"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/aurora-shelf.jpg" alt="Aurora Shelf: aurora-shelf"></td>
</tr>
</table>

<a id="paper"></a>

## 02 &middot; Paper Library

A game shelf of paper cards that travel when sorted, filtered or picked up.

<img src="media/designs/paper.webp" width="640" alt="Paper Library in motion">

**What it demonstrates**

- Cards keyed by item with their own position springs: sorting and filtering make them travel
- Paper physics: lifted cards grow, drop a softer shadow and turn a sheet out from under them
- A gold focus ring (ui::SpringRect) that glides between cards and follows them in flight
- Spring scrolling in a clipped grid whose scrolled edges dissolve into the backdrop
- A paper dialog over a glass-blurred screen, with a gliding ink highlight and confetti
- Overshooting favourite sticker and finished stamp; a friendly note for an empty filter
- The paper sound set, panned and pitched by position, with soft refusals at the edges

Source: [`src/concepts/paper.cpp`](../src/concepts/paper.cpp) &middot; tests: [`tests/unit/paper_test.cpp`](../tests/unit/paper_test.cpp) &middot; sound set: `paper`

<table>
<tr>
<td width="50%" valign="top"><img src="media/designs/paper.jpg" alt="Paper Library: paper"></td>
<td width="50%" valign="top"><img src="media/designs/paper-details.jpg" alt="Paper Library: paper-details"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/paper-empty.jpg" alt="Paper Library: paper-empty"></td>
<td width="50%" valign="top"><img src="media/designs/paper-finished.jpg" alt="Paper Library: paper-finished"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/paper-sorted.jpg" alt="Paper Library: paper-sorted"></td>
<td width="50%" valign="top"><img src="media/designs/paper-travel.jpg" alt="Paper Library: paper-travel"></td>
</tr>
</table>

<a id="neon"></a>

## 03 &middot; Neon Arcade

A synthwave racer's main menu: neon sign, gliding tube, live previews.

<img src="media/designs/neon.webp" width="640" alt="Neon Arcade in motion">

**What it demonstrates**

- Neon from plain shapes: layered glows of falling alpha around a thin hot core
- Chromatic sign lettering: magenta and cyan copies either side of white, over a glow
- A focus tube on an underdamped spring: it overshoots, settles and follows the label width
- Previews that cross-fade, then assemble: bars fill, a ring sweeps, numbers count up
- Focus cue pitched down a major scale by menu position, panned to where it happened
- A second level that slides in, and a one-second launch moment with flash and zoom

Source: [`src/concepts/neon.cpp`](../src/concepts/neon.cpp) &middot; tests: [`tests/unit/neon_test.cpp`](../tests/unit/neon_test.cpp) &middot; sound set: `glass`

<table>
<tr>
<td width="50%" valign="top"><img src="media/designs/neon.jpg" alt="Neon Arcade: neon"></td>
<td width="50%" valign="top"><img src="media/designs/neon-career.jpg" alt="Neon Arcade: neon-career"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/neon-garage.jpg" alt="Neon Arcade: neon-garage"></td>
<td width="50%" valign="top"><img src="media/designs/neon-go.jpg" alt="Neon Arcade: neon-go"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/neon-quit.jpg" alt="Neon Arcade: neon-quit"></td>
<td width="50%" valign="top"><img src="media/designs/neon-tracks.jpg" alt="Neon Arcade: neon-tracks"></td>
</tr>
</table>

<a id="editorial"></a>

## 04 &middot; Editorial

A magazine's weekly selection: big type on paper, and an article behind every row.

<img src="media/designs/editorial.webp" width="640" alt="Editorial in motion">

**What it demonstrates**

- A focus without a box: ink weight, a spring-grown accent bar and an eased underline wipe
- Strict baseline grid on a light paper backdrop, with one accent and hairline rules
- Shared-element transition: the cover travels from the list into the article's colour plate
- Typeset article: drop cap, text poured into column pairs once, grid-locked spring scroll
- Page turns that cross-slide the text while the plate colour eases (ui::SpringColor)
- Bookmark ribbon dropped with an underdamped spring (tween::Bounce) and the mark cue

Source: [`src/concepts/editorial.cpp`](../src/concepts/editorial.cpp) &middot; tests: [`tests/unit/editorial_test.cpp`](../tests/unit/editorial_test.cpp) &middot; sound set: `paper`

<table>
<tr>
<td width="50%" valign="top"><img src="media/designs/editorial.jpg" alt="Editorial: editorial"></td>
<td width="50%" valign="top"><img src="media/designs/editorial-article.jpg" alt="Editorial: editorial-article"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/editorial-bookmark.jpg" alt="Editorial: editorial-bookmark"></td>
<td width="50%" valign="top"><img src="media/designs/editorial-continued.jpg" alt="Editorial: editorial-continued"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/editorial-list-end.jpg" alt="Editorial: editorial-list-end"></td>
<td width="50%" valign="top"><img src="media/designs/editorial-page-turn.jpg" alt="Editorial: editorial-page-turn"></td>
</tr>
</table>

<a id="carousel"></a>

## 05 &middot; Cover Wheel

A carousel with weight: scrub it, let it coast, open a cover.

<img src="media/designs/carousel.webp" width="640" alt="Cover Wheel in motion">

**What it demonstrates**

- A wheel with position and velocity: stick scrubbing, inertia, spring snap to a cover
- Depth without 3D: place, size and light from distance; covers drawn far to near
- Floor reflections: the cover texture with reversed v, squashed and veiled by a gradient
- Rubber-band ends: stretch while scrubbing, bounce when coasting, soft refusal on a press
- A caption that waits for the wheel to settle, so fast scrubbing never strobes text
- The centred cover lifts out of the row beside a frosted details panel (Frame::glass)

Source: [`src/concepts/carousel.cpp`](../src/concepts/carousel.cpp) &middot; tests: [`tests/unit/carousel_test.cpp`](../tests/unit/carousel_test.cpp) &middot; sound set: `glass`

<table>
<tr>
<td width="50%" valign="top"><img src="media/designs/carousel.jpg" alt="Cover Wheel: carousel"></td>
<td width="50%" valign="top"><img src="media/designs/carousel-details.jpg" alt="Cover Wheel: carousel-details"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/carousel-end.jpg" alt="Cover Wheel: carousel-end"></td>
<td width="50%" valign="top"><img src="media/designs/carousel-scrub.jpg" alt="Cover Wheel: carousel-scrub"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/carousel-settled.jpg" alt="Cover Wheel: carousel-settled"></td>
</tr>
</table>

<a id="radial"></a>

## 06 &middot; Radial Dial

An in-game item wheel: aim with the stick, equip with one press.

<img src="media/designs/radial.webp" width="640" alt="Radial Dial in motion">

**What it demonstrates**

- Stick-angle selection with hysteresis on angle and deflection: no flicker at a boundary
- Needle and focus rim on angle springs that always take the shortest way round
- Sectors from DrawList::arc; icons built from circles, lines, arcs, stars and triangles
- Detent ticks pitched by sector and panned by its position; ripple and rumble on equip
- World dimmed, desaturated and slowed under the wheel; frosted card through Frame::glass
- Wheel folds into a HUD dial with one transform; sectors rotate out and in when paging

Source: [`src/concepts/radial.cpp`](../src/concepts/radial.cpp) &middot; tests: [`tests/unit/radial_test.cpp`](../tests/unit/radial_test.cpp) &middot; sound set: `paper`

<table>
<tr>
<td width="50%" valign="top"><img src="media/designs/radial.jpg" alt="Radial Dial: radial"></td>
<td width="50%" valign="top"><img src="media/designs/radial-closed.jpg" alt="Radial Dial: radial-closed"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/radial-empty.jpg" alt="Radial Dial: radial-empty"></td>
<td width="50%" valign="top"><img src="media/designs/radial-equip.jpg" alt="Radial Dial: radial-equip"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/radial-signals.jpg" alt="Radial Dial: radial-signals"></td>
<td width="50%" valign="top"><img src="media/designs/radial-stick.jpg" alt="Radial Dial: radial-stick"></td>
</tr>
</table>

<a id="hud"></a>

## 07 &middot; Field HUD

An in-game HUD over a moving world, and the pause menu behind Options.

<img src="media/designs/hud.webp" width="640" alt="Field HUD in motion">

**What it demonstrates**

- Health bar with a delayed damage ghost: one value snaps, a slower spring drains behind it
- Radial cooldown sweeps (DrawList::arc as a pie) with monospaced seconds and a soft refusal
- A check mark that draws itself, a strike-through and a card that hands over by cross-fade
- Hit feedback in layers: popping numbers, a decaying HUD shake, a vignette, rumble
- A separate gameplay clock: pausing freezes cooldowns, compass, toasts and the world
- Pause menu on frosted glass over the blurred world, the HUD kept readable at 30 %

Source: [`src/concepts/hud.cpp`](../src/concepts/hud.cpp) &middot; tests: [`tests/unit/hud_test.cpp`](../tests/unit/hud_test.cpp) &middot; sound set: `paper`

<table>
<tr>
<td width="50%" valign="top"><img src="media/designs/hud.jpg" alt="Field HUD: hud"></td>
<td width="50%" valign="top"><img src="media/designs/hud-abilities.jpg" alt="Field HUD: hud-abilities"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/hud-down.jpg" alt="Field HUD: hud-down"></td>
<td width="50%" valign="top"><img src="media/designs/hud-hit.jpg" alt="Field HUD: hud-hit"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/hud-pause.jpg" alt="Field HUD: hud-pause"></td>
<td width="50%" valign="top"><img src="media/designs/hud-quit.jpg" alt="Field HUD: hud-quit"></td>
</tr>
</table>

<a id="dashboard"></a>

## 08 &middot; Pulse Dashboard

A bento grid of live data tiles that expand into detail views.

<img src="media/designs/dashboard.webp" width="640" alt="Pulse Dashboard in motion">

**What it demonstrates**

- Spatial focus navigation across tiles of mixed sizes, scored by one small function
- Shared-element expand and collapse: the tile's rectangle and label travel to the detail view
- One focus ring that glides and morphs to each tile's size and accent (ui::SpringRect)
- Charts that arrive: counting numbers, staggered bars, growing rings and segments
- Live telemetry with honest empty states, and a pause switch shown as a chip
- Focus sounds panned and pitched by the tile's position; soft refusals at the grid's edges

Source: [`src/concepts/dashboard.cpp`](../src/concepts/dashboard.cpp) &middot; tests: [`tests/unit/dashboard_test.cpp`](../tests/unit/dashboard_test.cpp) &middot; sound set: `glass`

<table>
<tr>
<td width="50%" valign="top"><img src="media/designs/dashboard.jpg" alt="Pulse Dashboard: dashboard"></td>
<td width="50%" valign="top"><img src="media/designs/dashboard-bars.jpg" alt="Pulse Dashboard: dashboard-bars"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/dashboard-expanding.jpg" alt="Pulse Dashboard: dashboard-expanding"></td>
<td width="50%" valign="top"><img src="media/designs/dashboard-focus.jpg" alt="Pulse Dashboard: dashboard-focus"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/dashboard-frames.jpg" alt="Pulse Dashboard: dashboard-frames"></td>
<td width="50%" valign="top"><img src="media/designs/dashboard-library.jpg" alt="Pulse Dashboard: dashboard-library"></td>
</tr>
</table>

<a id="player"></a>

## 09 &middot; Now Playing

A music player: breathing artwork, a visualizer, a scrubber and a glass queue.

<img src="media/designs/player.webp" width="640" alt="Now Playing in motion">

**What it demonstrates**

- One pseudo-spectrum (sines + a beat, fast attack, slow decay) drives bars, glow and icons
- Artwork as play state: 94 % and dimmed when paused, slide-through on track change
- A focus ring that glides between round buttons and locks onto the scrubber thumb
- Spring-chased scrubber with pitched, panned seek ticks; hold to repeat
- Frosted queue (Frame::glass) that slides away while the layout re-centres
- Backdrop and accents eased from the current album's palette (ui::SpringColor)

Source: [`src/concepts/player.cpp`](../src/concepts/player.cpp) &middot; tests: [`tests/unit/player_test.cpp`](../tests/unit/player_test.cpp) &middot; sound set: `glass`

<table>
<tr>
<td width="50%" valign="top"><img src="media/designs/player.jpg" alt="Now Playing: player"></td>
<td width="50%" valign="top"><img src="media/designs/player-paused.jpg" alt="Now Playing: player-paused"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/player-queue.jpg" alt="Now Playing: player-queue"></td>
<td width="50%" valign="top"><img src="media/designs/player-scrub.jpg" alt="Now Playing: player-scrub"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/player-track-change.jpg" alt="Now Playing: player-track-change"></td>
<td width="50%" valign="top"><img src="media/designs/player-wide.jpg" alt="Now Playing: player-wide"></td>
</tr>
</table>

<a id="keyboard"></a>

## 10 &middot; First Run

A setup wizard: avatar, a name typed on a controller keyboard, a warm welcome.

<img src="media/designs/keyboard.webp" width="640" alt="First Run in motion">

**What it demonstrates**

- On-screen keyboard: one ui::SpringRect highlight that morphs between key sizes
- Key caps coloured by how much of the gliding highlight covers them
- Row wrap-around: out through one edge, in through the other; wide keys keep the column
- Shortcuts on Square, Triangle, L2 and R2, with held-Square repeat from is_held and a timer
- Characters slide in and sink out; the caret glides; refusals shake the field and say why
- Steps slide and cross-fade in the direction of travel under a springing step indicator
- Confetti and a frosted welcome card, then the wizard loops with its state kept

Source: [`src/concepts/keyboard.cpp`](../src/concepts/keyboard.cpp) &middot; tests: [`tests/unit/keyboard_test.cpp`](../tests/unit/keyboard_test.cpp) &middot; sound set: `paper`

<table>
<tr>
<td width="50%" valign="top"><img src="media/designs/keyboard.jpg" alt="First Run: keyboard"></td>
<td width="50%" valign="top"><img src="media/designs/keyboard-empty.jpg" alt="First Run: keyboard-empty"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/keyboard-shift.jpg" alt="First Run: keyboard-shift"></td>
<td width="50%" valign="top"><img src="media/designs/keyboard-summary.jpg" alt="First Run: keyboard-summary"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/keyboard-typing.jpg" alt="First Run: keyboard-typing"></td>
<td width="50%" valign="top"><img src="media/designs/keyboard-welcome.jpg" alt="First Run: keyboard-welcome"></td>
</tr>
</table>

<a id="constellation"></a>

## 11 &middot; Constellation

A skill tree as a star map: free 2D focus, a gliding camera, progression.

<img src="media/designs/constellation.webp" width="640" alt="Constellation in motion">

**What it demonstrates**

- Free-form focus navigation: one scoring function, proven complete by a static_assert
- A spring camera inside push_transform, with the star backdrop moving in parallax
- Three zoom levels on a spring; line widths divided by the zoom so they stay crisp
- Unlocks that travel: light runs along the link, then the star ignites with a ripple
- A cue that climbs a scale as a branch fills, panned by where the star is on screen
- A frosted info card that cross-fades, explains refusals and hosts the refund prompt

Source: [`src/concepts/constellation.cpp`](../src/concepts/constellation.cpp) &middot; tests: [`tests/unit/constellation_test.cpp`](../tests/unit/constellation_test.cpp) &middot; sound set: `paper`

<table>
<tr>
<td width="50%" valign="top"><img src="media/designs/constellation.jpg" alt="Constellation: constellation"></td>
<td width="50%" valign="top"><img src="media/designs/constellation-overview.jpg" alt="Constellation: constellation-overview"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/constellation-refund.jpg" alt="Constellation: constellation-refund"></td>
<td width="50%" valign="top"><img src="media/designs/constellation-refusal.jpg" alt="Constellation: constellation-refusal"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/constellation-unlock.jpg" alt="Constellation: constellation-unlock"></td>
</tr>
</table>

<a id="terminal"></a>

## 12 &middot; Phosphor

A monochrome CRT terminal: character grid, glow, typed text.

<img src="media/designs/terminal.webp" width="640" alt="Phosphor in motion">

**What it demonstrates**

- A strict 100 x 25 character grid: every glyph, rule and bar is placed in cells
- Phosphor glow: each line drawn as a soft halo plus crisp text, under scanlines
- Inverse-video focus that snaps, with an afterglow ghost instead of a spring
- Pages redrawn by a row-by-row wipe over the fading ghost of the old page
- Typed boot sequence with rate-limited key sounds; any button skips it
- CRT power-off: a clip rectangle collapses the picture to a line, then a dot
- Theme colours eased with ui::SpringColor; glyphs drawn with GlyphStyle::mono

Source: [`src/concepts/terminal.cpp`](../src/concepts/terminal.cpp) &middot; tests: [`tests/unit/terminal_test.cpp`](../tests/unit/terminal_test.cpp) &middot; sound set: `paper`

<table>
<tr>
<td width="50%" valign="top"><img src="media/designs/terminal.jpg" alt="Phosphor: terminal"></td>
<td width="50%" valign="top"><img src="media/designs/terminal-archive.jpg" alt="Phosphor: terminal-archive"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/terminal-diagnostics.jpg" alt="Phosphor: terminal-diagnostics"></td>
<td width="50%" valign="top"><img src="media/designs/terminal-library.jpg" alt="Phosphor: terminal-library"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/terminal-poweroff.jpg" alt="Phosphor: terminal-poweroff"></td>
<td width="50%" valign="top"><img src="media/designs/terminal-shutdown.jpg" alt="Phosphor: terminal-shutdown"></td>
</tr>
</table>

<a id="store"></a>

## 13 &middot; Storefront

A shop window: featured banner, product pages, a cart and a checkout.

<img src="media/designs/store.webp" width="640" alt="Storefront in motion">

**What it demonstrates**

- Shared-element transition: the card's cover and its uv crop grow into the product page
- Featured banner: cross-fade plus a slow uv-rectangle drift, paused while focused
- Add to cart: an arc flight into the counter, which bumps and rolls its number
- Frosted cart drawer: spring-collapsed rows, a counting subtotal, cancel-first checkout
- One price routine everywhere: badge, struck-through old price, Free and Owned
- A page that scrolls as one sheet with sticky filter chips and per-pixel edge fades
- Cards drawn layer by layer (covers, shapes, each font) to keep the draw calls low

Source: [`src/concepts/store.cpp`](../src/concepts/store.cpp) &middot; tests: [`tests/unit/store_test.cpp`](../tests/unit/store_test.cpp) &middot; sound set: `glass`

<table>
<tr>
<td width="50%" valign="top"><img src="media/designs/store.jpg" alt="Storefront: store"></td>
<td width="50%" valign="top"><img src="media/designs/store-cart.jpg" alt="Storefront: store-cart"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/store-confirm.jpg" alt="Storefront: store-confirm"></td>
<td width="50%" valign="top"><img src="media/designs/store-flight.jpg" alt="Storefront: store-flight"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/store-product.jpg" alt="Storefront: store-product"></td>
<td width="50%" valign="top"><img src="media/designs/store-purchased.jpg" alt="Storefront: store-purchased"></td>
</tr>
</table>

<a id="trophies"></a>

## 14 &middot; Trophy Room

An achievements cabinet: metal medals, counting numbers, an unlock with ceremony.

<img src="media/designs/trophies.webp" width="640" alt="Trophy Room in motion">

**What it demonstrates**

- Medals built from gradients: rim, reversed groove, face, embossed emblem, light streaks
- A specular streak that moves with focus and time, so the metal seems to catch the light
- A turning medal: an ellipse from polygon bands, its edges redrawn as anti-aliased lines
- An opaque recess behind the list, so its edge fades match whatever the backdrop does
- Rows keyed by achievement: position springs make a new sort travel and a filter re-flow
- Unlock ceremony: glass toast with a clipped shine sweep, sparks with gravity, confetti
- Tabular numerals drawn in fixed cells so counting numbers never jitter
- Fanfare pitch climbs with completion; hidden names uncover with their own cue

Source: [`src/concepts/trophies.cpp`](../src/concepts/trophies.cpp) &middot; tests: [`tests/unit/trophies_test.cpp`](../tests/unit/trophies_test.cpp) &middot; sound set: `paper`

<table>
<tr>
<td width="50%" valign="top"><img src="media/designs/trophies.jpg" alt="Trophy Room: trophies"></td>
<td width="50%" valign="top"><img src="media/designs/trophies-cabinet.jpg" alt="Trophy Room: trophies-cabinet"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/trophies-detail.jpg" alt="Trophy Room: trophies-detail"></td>
<td width="50%" valign="top"><img src="media/designs/trophies-earned.jpg" alt="Trophy Room: trophies-earned"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/trophies-rarest.jpg" alt="Trophy Room: trophies-rarest"></td>
<td width="50%" valign="top"><img src="media/designs/trophies-unlock.jpg" alt="Trophy Room: trophies-unlock"></td>
</tr>
</table>

<a id="files"></a>

## 15 &middot; File Browser

A file manager: aligned columns, folders that keep your place, visible results.

<img src="media/designs/files.webp" width="640" alt="File Browser in motion">

**What it demonstrates**

- Mono digits on shared right edges: sizes and dates read as columns
- Per-row slot springs: sorting and deleting move rows instead of redrawing them
- Folder transitions slide with the direction of travel; every folder keeps its focus
- Frosted context popover pinned to the focused row, kept on screen at the edges
- Operations with visible results: measured progress, collapsing rows, a live gauge
- File-kind icons assembled from kit shapes, tinted by kind
- Cues that carry state: row pitch, a climbing mark, a drive that sounds as full as it is

Source: [`src/concepts/files.cpp`](../src/concepts/files.cpp) &middot; tests: [`tests/unit/files_test.cpp`](../tests/unit/files_test.cpp) &middot; sound set: `paper`

<table>
<tr>
<td width="50%" valign="top"><img src="media/designs/files.jpg" alt="File Browser: files"></td>
<td width="50%" valign="top"><img src="media/designs/files-copying.jpg" alt="File Browser: files-copying"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/files-delete.jpg" alt="File Browser: files-delete"></td>
<td width="50%" valign="top"><img src="media/designs/files-details.jpg" alt="File Browser: files-details"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/files-menu.jpg" alt="File Browser: files-menu"></td>
<td width="50%" valign="top"><img src="media/designs/files-picker.jpg" alt="File Browser: files-picker"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/files-rename.jpg" alt="File Browser: files-rename"></td>
<td width="50%" valign="top"><img src="media/designs/files-selection.jpg" alt="File Browser: files-selection"></td>
</tr>
</table>

<a id="inventory"></a>

## 16 &middot; Satchel

An inventory you handle: lift, carry, swap, stack and equip, with a comparing tooltip.

<img src="media/designs/inventory.webp" width="640" alt="Satchel in motion">

**What it demonstrates**

- Items own a spring position: swap, sort, filter and equip are one travel animation
- Pick up and place: lift, shadow, speed tilt, a trailing follow and pulsing valid targets
- Comparing tooltip on frosted glass: arrows and differences against the worn item
- Icons composed from kit shapes and tinted per item; rarity as border and inner glow
- Paper sounds with meaning: merge pitched by the stack, connect by the loadout's power
- Totals that count, a carry bar that warms, refusals explained in a status line

Source: [`src/concepts/inventory.cpp`](../src/concepts/inventory.cpp) &middot; tests: [`tests/unit/inventory_test.cpp`](../tests/unit/inventory_test.cpp) &middot; sound set: `paper`

<table>
<tr>
<td width="50%" valign="top"><img src="media/designs/inventory.jpg" alt="Satchel: inventory"></td>
<td width="50%" valign="top"><img src="media/designs/inventory-carry.jpg" alt="Satchel: inventory-carry"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/inventory-equip.jpg" alt="Satchel: inventory-equip"></td>
<td width="50%" valign="top"><img src="media/designs/inventory-sort.jpg" alt="Satchel: inventory-sort"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/inventory-sorted.jpg" alt="Satchel: inventory-sorted"></td>
<td width="50%" valign="top"><img src="media/designs/inventory-weapons.jpg" alt="Satchel: inventory-weapons"></td>
</tr>
</table>

<a id="boot"></a>

## 17 &middot; Launch Sequence

Before the menu: studio splash, title, profiles and an honest loading screen.

<img src="media/designs/boot.webp" width="640" alt="Launch Sequence in motion">

**What it demonstrates**

- Logo assembly: staggered tween::Bounce strokes, a ring drawn with arc(), settling tracking
- Light sweep: the logo redrawn in white inside a moving, slanted stack of clip strips
- Gradient lettering from twelve clipped bands, over a halo of offset copies
- An honest loader: timed stages of uneven speed, a spring-smoothed bar, a spinner for stalls
- Tips that cross-fade on a timer and step by hand; key art cropped and drifted through uv
- Five designed screen changes (through black, slide, push, iris) that reverse in flight

Source: [`src/concepts/boot.cpp`](../src/concepts/boot.cpp) &middot; tests: [`tests/unit/boot_test.cpp`](../tests/unit/boot_test.cpp) &middot; sound set: `glass`

<table>
<tr>
<td width="50%" valign="top"><img src="media/designs/boot.jpg" alt="Launch Sequence: boot"></td>
<td width="50%" valign="top"><img src="media/designs/boot-ingame.jpg" alt="Launch Sequence: boot-ingame"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/boot-loading.jpg" alt="Launch Sequence: boot-loading"></td>
<td width="50%" valign="top"><img src="media/designs/boot-profiles.jpg" alt="Launch Sequence: boot-profiles"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/boot-ready.jpg" alt="Launch Sequence: boot-ready"></td>
<td width="50%" valign="top"><img src="media/designs/boot-title.jpg" alt="Launch Sequence: boot-title"></td>
</tr>
</table>

<a id="settings"></a>

## 18 &middot; Control Room

A settings screen with real controls: sliders, switches, steppers, a dialog.

<img src="media/designs/settings.webp" width="640" alt="Control Room in motion">

**What it demonstrates**

- Live settings: every control edits app::Context::settings and applies at once
- One spring focus rectangle that travels between the rail and the rows
- Slider, stepper, toggle and action row built from DrawList shapes and springs
- Value-pitched slider cues, soft refusals at the ends of every range
- Staggered cross-fade of rows, description and button hints on every change
- Frosted confirm dialog that focuses Cancel, and one Saved toast per burst

Source: [`src/concepts/settings.cpp`](../src/concepts/settings.cpp) &middot; tests: [`tests/unit/settings_test.cpp`](../tests/unit/settings_test.cpp) &middot; sound set: `glass`

<table>
<tr>
<td width="50%" valign="top"><img src="media/designs/settings.jpg" alt="Control Room: settings"></td>
<td width="50%" valign="top"><img src="media/designs/settings-about.jpg" alt="Control Room: settings-about"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/settings-audio.jpg" alt="Control Room: settings-audio"></td>
<td width="50%" valign="top"><img src="media/designs/settings-controller.jpg" alt="Control Room: settings-controller"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/settings-display.jpg" alt="Control Room: settings-display"></td>
<td width="50%" valign="top"><img src="media/designs/settings-reset.jpg" alt="Control Room: settings-reset"></td>
</tr>
</table>

<a id="themes"></a>

## 19 &middot; Theme Lab

One screen in thirty design languages: L2 and R2 restyle every widget.

**What it demonstrates**

- ui::Theme: thirty design languages as plain data (colour, shape, depth, type, motion, sound)
- ui::Painter: one widget set drawn in any theme; surfaces built eleven different ways
- Looks modelled on well-known web frameworks, measured from their own component pages
- Motion and sound belong to the theme: speed, bounce and sound set change with it
- Row and column focus navigation that keeps your column across rows of different widgets
- A bitmap face drawn from rectangles and hand-drawn strokes, for looks a font cannot give

Source: [`src/concepts/themes.cpp`](../src/concepts/themes.cpp) &middot; tests: [`tests/unit/concepts_test.cpp`](../tests/unit/concepts_test.cpp) &middot; sound set: `glass`

<table>
<tr>
<td width="50%" valign="top"><img src="media/designs/themes.jpg" alt="Theme Lab: themes"></td>
<td width="50%" valign="top"><img src="media/designs/themes-dialog.jpg" alt="Theme Lab: themes-dialog"></td>
</tr>
</table>

<a id="components"></a>

## 20 &middot; Component Library

Reusable lists, grids, dialogs, forms and indicators, restyled by thirty themes.

<img src="media/designs/components.webp" width="640" alt="Component Library in motion">

**What it demonstrates**

- Components own their focus, motion and sound: a screen places them and reads events
- One style struct per component: a ui::Theme plus the component's own knobs
- Slots (std::function) replace parts of a component's drawing with your own
- Every component drawn in all thirty themes, restyled live without losing state
- Page changes slide in the direction of travel; theme changes hide behind a veil

Source: [`src/concepts/components.cpp`](../src/concepts/components.cpp) &middot; tests: [`tests/unit/concepts_test.cpp`](../tests/unit/concepts_test.cpp) &middot; sound set: `glass`

<table>
<tr>
<td width="50%" valign="top"><img src="media/designs/components.jpg" alt="Component Library: components"></td>
<td width="50%" valign="top"><img src="media/designs/components-actions-accent.jpg" alt="Component Library: components-actions-accent"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-actions-compact.jpg" alt="Component Library: components-actions-compact"></td>
<td width="50%" valign="top"><img src="media/designs/components-actions-menu.jpg" alt="Component Library: components-actions-menu"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-actions-tour.jpg" alt="Component Library: components-actions-tour"></td>
<td width="50%" valign="top"><img src="media/designs/components-actions.jpg" alt="Component Library: components-actions"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-brutal-actions.jpg" alt="Component Library: components-brutal-actions"></td>
<td width="50%" valign="top"><img src="media/designs/components-brutal-collections.jpg" alt="Component Library: components-brutal-collections"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-brutal-data.jpg" alt="Component Library: components-brutal-data"></td>
<td width="50%" valign="top"><img src="media/designs/components-brutal-entry.jpg" alt="Component Library: components-brutal-entry"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-brutal-forms.jpg" alt="Component Library: components-brutal-forms"></td>
<td width="50%" valign="top"><img src="media/designs/components-brutal-game.jpg" alt="Component Library: components-brutal-game"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-brutal-indicators.jpg" alt="Component Library: components-brutal-indicators"></td>
<td width="50%" valign="top"><img src="media/designs/components-brutal-layout.jpg" alt="Component Library: components-brutal-layout"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-brutal-lists.jpg" alt="Component Library: components-brutal-lists"></td>
<td width="50%" valign="top"><img src="media/designs/components-brutal-media.jpg" alt="Component Library: components-brutal-media"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-brutal-navigation.jpg" alt="Component Library: components-brutal-navigation"></td>
<td width="50%" valign="top"><img src="media/designs/components-brutal-notifications.jpg" alt="Component Library: components-brutal-notifications"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-brutal-overlays.jpg" alt="Component Library: components-brutal-overlays"></td>
<td width="50%" valign="top"><img src="media/designs/components-brutal-pickers.jpg" alt="Component Library: components-brutal-pickers"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-brutal-structure.jpg" alt="Component Library: components-brutal-structure"></td>
<td width="50%" valign="top"><img src="media/designs/components-classic.jpg" alt="Component Library: components-classic"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-clay.jpg" alt="Component Library: components-clay"></td>
<td width="50%" valign="top"><img src="media/designs/components-collections-hero.jpg" alt="Component Library: components-collections-hero"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-collections-paged.jpg" alt="Component Library: components-collections-paged"></td>
<td width="50%" valign="top"><img src="media/designs/components-collections-posters.jpg" alt="Component Library: components-collections-posters"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-collections-wheel.jpg" alt="Component Library: components-collections-wheel"></td>
<td width="50%" valign="top"><img src="media/designs/components-collections.jpg" alt="Component Library: components-collections"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-data-charts.jpg" alt="Component Library: components-data-charts"></td>
<td width="50%" valign="top"><img src="media/designs/components-data-plain.jpg" alt="Component Library: components-data-plain"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-data-range.jpg" alt="Component Library: components-data-range"></td>
<td width="50%" valign="top"><img src="media/designs/components-data.jpg" alt="Component Library: components-data"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-entry-flat.jpg" alt="Component Library: components-entry-flat"></td>
<td width="50%" valign="top"><img src="media/designs/components-entry-numeric.jpg" alt="Component Library: components-entry-numeric"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-entry-pin.jpg" alt="Component Library: components-entry-pin"></td>
<td width="50%" valign="top"><img src="media/designs/components-entry-prompt.jpg" alt="Component Library: components-entry-prompt"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-entry.jpg" alt="Component Library: components-entry"></td>
<td width="50%" valign="top"><img src="media/designs/components-forms-compact.jpg" alt="Component Library: components-forms-compact"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-forms-help.jpg" alt="Component Library: components-forms-help"></td>
<td width="50%" valign="top"><img src="media/designs/components-forms-panel.jpg" alt="Component Library: components-forms-panel"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-forms-wide.jpg" alt="Component Library: components-forms-wide"></td>
<td width="50%" valign="top"><img src="media/designs/components-forms.jpg" alt="Component Library: components-forms"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-game-inventory.jpg" alt="Component Library: components-game-inventory"></td>
<td width="50%" valign="top"><img src="media/designs/components-game-nodes.jpg" alt="Component Library: components-game-nodes"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-game-pause.jpg" alt="Component Library: components-game-pause"></td>
<td width="50%" valign="top"><img src="media/designs/components-game-profiles.jpg" alt="Component Library: components-game-profiles"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-game.jpg" alt="Component Library: components-game"></td>
<td width="50%" valign="top"><img src="media/designs/components-hazard.jpg" alt="Component Library: components-hazard"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-indicators-chunky.jpg" alt="Component Library: components-indicators-chunky"></td>
<td width="50%" valign="top"><img src="media/designs/components-indicators-loaded.jpg" alt="Component Library: components-indicators-loaded"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-indicators-slim.jpg" alt="Component Library: components-indicators-slim"></td>
<td width="50%" valign="top"><img src="media/designs/components-indicators-status.jpg" alt="Component Library: components-indicators-status"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-indicators.jpg" alt="Component Library: components-indicators"></td>
<td width="50%" valign="top"><img src="media/designs/components-layout-guides.jpg" alt="Component Library: components-layout-guides"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-layout-modal.jpg" alt="Component Library: components-layout-modal"></td>
<td width="50%" valign="top"><img src="media/designs/components-layout-panes.jpg" alt="Component Library: components-layout-panes"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-layout-stacked.jpg" alt="Component Library: components-layout-stacked"></td>
<td width="50%" valign="top"><img src="media/designs/components-layout.jpg" alt="Component Library: components-layout"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-lists-bar.jpg" alt="Component Library: components-lists-bar"></td>
<td width="50%" valign="top"><img src="media/designs/components-lists-fill.jpg" alt="Component Library: components-lists-fill"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-lists-glow.jpg" alt="Component Library: components-lists-glow"></td>
<td width="50%" valign="top"><img src="media/designs/components-lists.jpg" alt="Component Library: components-lists"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-media-compact.jpg" alt="Component Library: components-media-compact"></td>
<td width="50%" valign="top"><img src="media/designs/components-media-loading.jpg" alt="Component Library: components-media-loading"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-media-reader.jpg" alt="Component Library: components-media-reader"></td>
<td width="50%" valign="top"><img src="media/designs/components-media-zoom.jpg" alt="Component Library: components-media-zoom"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-media.jpg" alt="Component Library: components-media"></td>
<td width="50%" valign="top"><img src="media/designs/components-navigation-boxed.jpg" alt="Component Library: components-navigation-boxed"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-navigation-menu.jpg" alt="Component Library: components-navigation-menu"></td>
<td width="50%" valign="top"><img src="media/designs/components-navigation-segmented.jpg" alt="Component Library: components-navigation-segmented"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-navigation-underline.jpg" alt="Component Library: components-navigation-underline"></td>
<td width="50%" valign="top"><img src="media/designs/components-navigation.jpg" alt="Component Library: components-navigation"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-notifications-accent.jpg" alt="Component Library: components-notifications-accent"></td>
<td width="50%" valign="top"><img src="media/designs/components-notifications-center.jpg" alt="Component Library: components-notifications-center"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-notifications-compact.jpg" alt="Component Library: components-notifications-compact"></td>
<td width="50%" valign="top"><img src="media/designs/components-notifications-progress.jpg" alt="Component Library: components-notifications-progress"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-notifications-queue.jpg" alt="Component Library: components-notifications-queue"></td>
<td width="50%" valign="top"><img src="media/designs/components-notifications.jpg" alt="Component Library: components-notifications"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-overlays-bottom.jpg" alt="Component Library: components-overlays-bottom"></td>
<td width="50%" valign="top"><img src="media/designs/components-overlays-compact.jpg" alt="Component Library: components-overlays-compact"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-overlays-sheet.jpg" alt="Component Library: components-overlays-sheet"></td>
<td width="50%" valign="top"><img src="media/designs/components-overlays-toasts.jpg" alt="Component Library: components-overlays-toasts"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-overlays.jpg" alt="Component Library: components-overlays"></td>
<td width="50%" valign="top"><img src="media/designs/components-paper.jpg" alt="Component Library: components-paper"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-pickers-compact.jpg" alt="Component Library: components-pickers-compact"></td>
<td width="50%" valign="top"><img src="media/designs/components-pickers-hsv.jpg" alt="Component Library: components-pickers-hsv"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-pickers-select.jpg" alt="Component Library: components-pickers-select"></td>
<td width="50%" valign="top"><img src="media/designs/components-pickers.jpg" alt="Component Library: components-pickers"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-pico.jpg" alt="Component Library: components-pico"></td>
<td width="50%" valign="top"><img src="media/designs/components-pixel-actions.jpg" alt="Component Library: components-pixel-actions"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-pixel-collections.jpg" alt="Component Library: components-pixel-collections"></td>
<td width="50%" valign="top"><img src="media/designs/components-pixel-data.jpg" alt="Component Library: components-pixel-data"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-pixel-entry.jpg" alt="Component Library: components-pixel-entry"></td>
<td width="50%" valign="top"><img src="media/designs/components-pixel-forms.jpg" alt="Component Library: components-pixel-forms"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-pixel-game.jpg" alt="Component Library: components-pixel-game"></td>
<td width="50%" valign="top"><img src="media/designs/components-pixel-indicators.jpg" alt="Component Library: components-pixel-indicators"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-pixel-layout.jpg" alt="Component Library: components-pixel-layout"></td>
<td width="50%" valign="top"><img src="media/designs/components-pixel-lists.jpg" alt="Component Library: components-pixel-lists"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-pixel-media.jpg" alt="Component Library: components-pixel-media"></td>
<td width="50%" valign="top"><img src="media/designs/components-pixel-navigation.jpg" alt="Component Library: components-pixel-navigation"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-pixel-notifications.jpg" alt="Component Library: components-pixel-notifications"></td>
<td width="50%" valign="top"><img src="media/designs/components-pixel-overlays.jpg" alt="Component Library: components-pixel-overlays"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-pixel-pickers.jpg" alt="Component Library: components-pixel-pickers"></td>
<td width="50%" valign="top"><img src="media/designs/components-pixel-structure.jpg" alt="Component Library: components-pixel-structure"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-sketch-actions.jpg" alt="Component Library: components-sketch-actions"></td>
<td width="50%" valign="top"><img src="media/designs/components-sketch-collections.jpg" alt="Component Library: components-sketch-collections"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-sketch-data.jpg" alt="Component Library: components-sketch-data"></td>
<td width="50%" valign="top"><img src="media/designs/components-sketch-entry.jpg" alt="Component Library: components-sketch-entry"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-sketch-forms.jpg" alt="Component Library: components-sketch-forms"></td>
<td width="50%" valign="top"><img src="media/designs/components-sketch-game.jpg" alt="Component Library: components-sketch-game"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-sketch-indicators.jpg" alt="Component Library: components-sketch-indicators"></td>
<td width="50%" valign="top"><img src="media/designs/components-sketch-layout.jpg" alt="Component Library: components-sketch-layout"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-sketch-lists.jpg" alt="Component Library: components-sketch-lists"></td>
<td width="50%" valign="top"><img src="media/designs/components-sketch-media.jpg" alt="Component Library: components-sketch-media"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-sketch-navigation.jpg" alt="Component Library: components-sketch-navigation"></td>
<td width="50%" valign="top"><img src="media/designs/components-sketch-notifications.jpg" alt="Component Library: components-sketch-notifications"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-sketch-overlays.jpg" alt="Component Library: components-sketch-overlays"></td>
<td width="50%" valign="top"><img src="media/designs/components-sketch-pickers.jpg" alt="Component Library: components-sketch-pickers"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-sketch-structure.jpg" alt="Component Library: components-sketch-structure"></td>
<td width="50%" valign="top"><img src="media/designs/components-structure-compact.jpg" alt="Component Library: components-structure-compact"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-structure-quick.jpg" alt="Component Library: components-structure-quick"></td>
<td width="50%" valign="top"><img src="media/designs/components-structure-radial.jpg" alt="Component Library: components-structure-radial"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/components-structure-vertical.jpg" alt="Component Library: components-structure-vertical"></td>
<td width="50%" valign="top"><img src="media/designs/components-structure.jpg" alt="Component Library: components-structure"></td>
</tr>
</table>

<a id="toolbox"></a>

## 21 &middot; Toolbox

The kit on one screen: shapes, type, motion, glyphs and every sound.

<img src="media/designs/toolbox.webp" width="640" alt="Toolbox in motion">

**What it demonstrates**

- Every DrawList shape: rounded and rotated rectangles, gradients, arcs, stars, shadows, glows
- Four baked distance-field fonts, sharp at any size, with letter tracking
- Easing curves and the two spring types, plotted and running
- Controller glyphs drawn from shapes, lit by the buttons you hold
- A sound board: every cue of both sound sets, panned by where it sits

Source: [`src/concepts/toolbox.cpp`](../src/concepts/toolbox.cpp) &middot; tests: [`tests/unit/concepts_test.cpp`](../tests/unit/concepts_test.cpp) &middot; sound set: `glass`

<table>
<tr>
<td width="50%" valign="top"><img src="media/designs/toolbox.jpg" alt="Toolbox: toolbox"></td>
<td width="50%" valign="top"><img src="media/designs/toolbox-board.jpg" alt="Toolbox: toolbox-board"></td>
</tr>
<tr>
<td width="50%" valign="top"><img src="media/designs/toolbox-glass-set.jpg" alt="Toolbox: toolbox-glass-set"></td>
</tr>
</table>
<!-- END:designs -->
