# The craft: what makes a console UI feel finished

A screen can be correct and still feel like a tool. The difference between
that and something that feels like a console's own software is not one big
feature. It is a few dozen small decisions, made the same way everywhere.
This page lists them. Each one names where the kit does it for you and which
design in the app shows it.

Read this before you design a screen. Use the checklist at the end before you
call one done.

## 1. The ten-foot rules

The player sits three metres away and holds a controller.

- **Lay out in the 1920 x 1080 virtual canvas.** The viewport scales it to the
  surface (1080p, 1440p or 4K), so one layout serves every TV. Never use real
  pixels.
- **Keep content inside the safe area.** 96 px from the left and right edges,
  about 60 px from the top and bottom. Backdrops and scrims may bleed to the
  edge; text and focusable things may not.
- **Text sizes.** Body text is 24 to 28. Nothing the player must read is under
  20. Small tracked labels may be 15 to 18 if they are short and in capitals.
  Hero titles are 60 to 96.
- **One focus, always visible.** There is no pointer. At every moment exactly
  one thing is focused and the player can find it in under a second: it is
  larger, brighter, ringed or lit. Colour alone is not enough.
- **Every screen answers "what do the buttons do?"** A hint row with the
  controller glyphs (`ui::draw_hints`) sits in a corner. It changes when the
  context changes (a dialog shows its own hints).

## 2. Motion: nothing teleports

Movement tells the player what changed and where it went.

- **Positions and sizes chase targets with springs.** Set `target`, call
  `update(dt)`, draw `value` (`tween::Spring`, `ui::SpringRect`). A spring
  handles interruption for free: when the player changes direction mid-move,
  the motion bends instead of restarting. This is why held D-pad navigation
  feels fluid.
- **The focus highlight is its own object.** Do not draw a highlight on the
  focused item; draw one highlight that glides between items
  (`ui::SpringRect`). The eye follows it.
- **One-shot changes use easing curves.** Arrivals use `cubic_out` or
  `quint_out` (fast start, soft stop). Departures use `cubic_in` and are
  shorter than arrivals. Overshoot (`back_out`) is for small things that pop:
  a star, a badge, a toggle thumb. `elastic_out` is a celebration, not a
  default.
- **Durations.** Focus moves settle in 120 to 200 ms (spring omega 18 to 22).
  Panels and sheets take 250 to 350 ms (omega 12 to 14). Screen changes take
  400 to 500 ms. Anything over 600 ms needs a reason.
- **Stagger entrances.** When a screen opens, its parts arrive 30 to 90 ms
  apart, top to bottom or by importance (`tween::stagger`). The player reads
  the screen in the order it assembles.
- **Cross-fade content, do not swap it.** When the focused item changes, the
  old title fades and slides out quickly and the new one arrives a beat later
  (see `Aurora::draw_hero`).
- **Idle life, in small doses.** A slow breathing glow on the focus
  (`ui::breathe`), artwork that floats a few pixels, a backdrop that drifts.
  Amplitudes stay small: the screen is alive, not busy.
- **Respect "Reduce motion".** `context.settings.reduced_motion` shortens
  transitions to plain fades and stops decorative movement. Check it wherever
  you slide, zoom or bob.
- **Animation time is frame-start to frame-start** and clamped (`main.cpp`).
  All motion takes `dt`; nothing counts frames.

## 3. Sound: every action answers

Silence reads as "nothing happened".

- **Ask for cues, not files.** A screen calls `feedback.play(audio::Cue::focus)`.
  The active design's sound set decides how that sounds. One vocabulary, many
  voices (see [SOUND.md](SOUND.md)).
- **The four sounds every screen has:** `focus` (the highlight moved), `select`
  (confirmed), `back` (cancelled), `error` (refused).
- **Place sounds in space.** Pan a focus tick by where the focused thing is
  (`ui::pan_for_x`). The effect is subtle and the player never notices it; the
  screen just feels physical.
- **Pitch carries meaning.** Lower shelves sound lower. A slider rises in
  pitch as its value rises. A streak climbs. Use the `pitch` argument.
- **Variations and jitter are automatic.** The sound bank rotates through a
  cue's recordings and detunes each play by up to 3 %, so held navigation
  never sounds like a machine gun.
- **Levels.** Ticks are quiet (about -33 dBFS RMS), interface sounds a step
  louder (-27), chimes and fanfares louder still (-23). Navigation must be
  comfortable at the volume the player uses for games.
- **Big moments duck the music.** `complete` and `welcome` lower the music
  for a moment (`MusicPlayer::duck`).
- **Rumble is punctuation.** A short, light pulse for a refusal
  (`feedback.rumble(0.25f, 0.05f)`), a stronger one for a launch. Never on
  ordinary navigation.

## 4. Edges and refusals

What happens at the end of a list says more about polish than the list does.

- **The end of a list answers.** Pressing right on the last card plays the
  `error` cue at low gain, gives a light rumble and nudges the highlight
  (`ui::Pulse` + `ui::shake`). On a held (repeating) direction it stays silent
  so the player is not punished for holding the D-pad.
- **Disabled is visible and explained.** A disabled action is dimmed and, when
  focused, says why.
- **Destructive actions confirm.** The dialog focuses the safe choice.
- **Back always works** and always goes exactly one step up.
- **Losing the controller is not an input.** `InputFrame::focus_lost` releases
  everything; the system overlay taking the pad must not "press" anything.

## 5. Depth: layers, light and glass

Flat screens look like documents. Console screens have a near and a far.

- **Three layers.** A backdrop that moves slowly (a procedural shader, see
  [BACKDROPS.md](BACKDROPS.md)), the content, and overlays (dialogs, sheets,
  toasts).
- **Shadows say "this floats".** A soft shadow under the focused card, offset
  down 10 to 26 px, softness 30 to 46 (`list.shadow`). Resting cards have none
  or a faint one.
- **Glow says "this is lit".** The focused thing spills its own accent colour
  (`list.glow`). Use the item's colour, not one global highlight colour.
- **Frosted glass for anything modal.** Set `frame.glass = true` and draw the
  panel in `frame.overlay` with `overlay.glass(frame.glass_texture, ...)`: the
  screen behind is blurred into it. Finish the panel with a tint and a
  one-pixel light border. Dim and slightly shrink the scene behind it.
- **Colour follows content.** Let the backdrop and the accents take the
  palette of the focused item, eased (`ui::SpringColor`). The whole screen
  responds to the selection.

## 6. Typography

- **Two weights do most of the work.** Regular for reading, SemiBold for
  titles, labels and buttons. A display face for hero titles. A monospaced
  face for numbers that change (timers, counters), so they do not jitter.
- **Hierarchy by size and opacity, not by colour.** Primary text is opaque;
  secondary text is the same colour at 55 to 80 % alpha. Reserve the accent
  colour for one thing per region.
- **Small labels: capitals with tracking.** `ui::upper(label)`, size 15 to 20,
  tracking 3 to 4. They organise a screen without shouting.
- **Never let text overflow.** `Font::fit` adds an ellipsis; `ui::paragraph`
  wraps and limits the number of lines.
- **Baselines, not boxes.** Text is positioned by its baseline. To centre a
  line of size `s` vertically on `cy`, use a baseline of about `cy + 0.35 * s`.

## 7. Responsiveness and honesty

- **Input acts on the frame it arrives.** Never wait for an animation to end
  before accepting the next press; springs make that safe.
- **Taps are not lost.** The input tracker accumulates press edges across
  every pad sample of a frame.
- **Never block the frame.** Scan folders, decode images and load saves on a
  worker thread; show a placeholder that fades to the content.
- **Sixty frames per second, always.** A menu that drops frames feels broken
  however good it looks. See [PERFORMANCE.md](PERFORMANCE.md) for what the
  console tolerates.
- **Show real state.** Progress bars show measured progress. A "Saved" toast
  appears after the write succeeded.

## 8. Consistency

- **One design language per screen family.** Corner radii, spacing steps,
  shadow strength and timing are constants at the top of the file, not numbers
  scattered through the drawing code.
- **Spacing on a grid.** Multiples of 4 or 8. Gaps between related things are
  smaller than gaps between groups.
- **Same action, same button, same sound, everywhere.** Cross confirms, Circle
  goes back (or the reverse when the player swapped them in settings: use
  `Action::confirm` and `Action::back`, never the physical buttons).

## The checklist

Before a screen is done:

- [ ] Everything readable sits inside the safe area; body text is at least 24.
- [ ] Exactly one focused element, findable at a glance, with a gliding
      highlight.
- [ ] The screen assembles with a staggered entrance; `enter()` restarts it.
- [ ] Focus moves, opens, closes and refusals each have a cue; focus sounds
      are panned.
- [ ] List ends answer with a soft refusal; held directions stay quiet.
- [ ] Changing content cross-fades; nothing pops in or out.
- [ ] Overlays use glass, dim the scene behind and have their own hints.
- [ ] `reduced_motion` is honoured.
- [ ] Text cannot overflow its space.
- [ ] A hint row shows what every used button does.
- [ ] `draw()` changes no state; all motion is driven by `dt` in `update()`.
- [ ] The design's `tour()` shows its best states, and the snapshots look
      right at 1920 x 1080.
- [ ] It holds 60 FPS on the console (see the tour report).
