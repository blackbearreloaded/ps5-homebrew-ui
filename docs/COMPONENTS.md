# The component library

The kit has three levels, and you can enter at any of them:

| Level | What you get | Where |
| --- | --- | --- |
| Shapes | Rectangles, arcs, text, images, glass: you draw everything | [KIT.md](KIT.md) |
| Widgets | `ui::Painter`: stateless themed controls (a button, a switch); you keep their values and animate them | [THEMES.md](THEMES.md) |
| **Components** | Whole pieces of interface that own their focus, values, motion and sound: a list, a grid, a dialog, a form | this page |

A component is what you reach for when you want a working settings screen or
a library grid this afternoon. You place it, forward input, and react to what
it reports. Everything about how it looks and feels is a field you can change.

In the app, open **Component Library** (L1 / R1). **L2 / R2** turn its pages,
**Square** cycles each page's variants, **Options** restyles everything in the
next theme.

<img src="media/designs/components.webp" width="640" alt="The Component Library design in motion">

## The five rules

Every component follows them, so knowing one is knowing all of them
([`component.hpp`](../src/ui/components/component.hpp)):

1. **`style`** is a public struct of plain values. It holds a `ui::Theme`
   (palette, how surfaces are built, corners, type, speed, bounce, sound set)
   and the component's own knobs. Change any of it at any time; state is kept.
2. **`set_bounds(rect)`** says where it is, in the 1920 x 1080 virtual canvas.
3. **`handle(input, feedback)`** takes one frame of input while the component
   has the focus and returns a `ui::Event` (`moved`, `changed`, `activated`,
   `cancelled`, `refused` or `none`). It plays its own cues and rumble.
4. **`update(dt)`** advances its animation. Call it every frame, focused or not.
5. **`draw(canvas)`** is `const`. It may run more than once per frame.

```cpp
#include "ui/components.hpp"   // everything, or one header per component

class Library final : public app::Concept
{
    ui::GridView grid_;
    ui::Dialog confirm_;

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        clock_ += dt;
        if (confirm_.is_open())
        {
            if (confirm_.handle(input, feedback) == ui::Event::activated && confirm_.choice() == 1)
                remove(grid_.focus());
        }
        else if (grid_.handle(input, feedback) == ui::Event::activated)
        {
            confirm_.open();
        }
        grid_.update(dt);
        confirm_.update(dt);
    }

    void draw(app::Frame &frame) const override
    {
        ui::Canvas scene{frame.scene, context_.fonts, frame.glass_texture, clock_};
        grid_.draw(scene);
        frame.glass = confirm_.visible();              // blur the screen behind the dialog
        ui::Canvas overlay{frame.overlay, context_.fonts, frame.glass_texture, clock_};
        confirm_.draw(overlay);
    }
};
```

Only one component should receive `handle()` in a frame: the one that has the
focus. Moving the focus between components is the screen's job (see how
[`lists_page.cpp`](../src/concepts/components/lists_page.cpp) does it with
left and right, and `set_active()`).

## Customising, from broad to fine

1. **Pick a theme.** `component.style.theme = ui::themes()[n];` restyles the
   whole component: surfaces are built the theme's way (frosted glass, hard
   offset shadows, bevels, pen strokes), corners are round, cut or pixel
   notched, text uses the theme's faces, motion its speed and bounce. All
   thirty built-in themes work with every component. A theme is plain data,
   so making your own is filling in a struct ([THEMES.md](THEMES.md)).
2. **Edit the theme's tokens for this component only.** The style holds its
   own copy: `list.style.theme.radius = 0; list.style.theme.accent = lime;`
3. **Turn the component's knobs.** Sizes, gaps, text sizes, variants,
   behaviour: `grid.style.columns = 6; tabs.style.kind = ui::TabKind::underline;`
   Each component's page below lists every knob with its default.
4. **Fill a slot.** Slots are `std::function` members that replace part of
   the drawing with your own code: a list row's leading image, a grid cell, a
   sheet's content, a ring's centre.
5. **Change its voice.** `style.sounds` maps each thing a component does to a
   cue. Assign another cue, name a sound set, scale gain and rumble, or set a
   cue to `audio::Cue::count` to silence it.
6. **Respect the player.** Mirror the reduced-motion setting into
   `style.reduced_motion`: travel and bounce become plain fades.

## The catalogue

<!-- BEGIN:components -->
<!-- END:components -->

## Shared pieces

- **`ui::Highlight`**: the gliding focus highlight every list-like component
  uses, in seven kinds (ring, fill, tint, bar, underline, glow, none). Use it
  in your own components for the same feel.
- **`ui::play_cue`** and **`ui::refuse`**: cues placed in the stereo field by
  screen position, and the standard soft refusal (quiet error, short rumble,
  shake, silent on a held direction).
- **`ui::fit_label`** / **`ui::fit_body`**: text cut to a width in the
  theme's faces.
- **`ui::Feedback`**: what a component asks the platform to play; the same
  struct a design receives in `update()`.

## Writing a component

Start from [`list.hpp`](../src/ui/components/list.hpp) and
[`list.cpp`](../src/ui/components/list.cpp), the reference.

- [ ] A style struct deriving from `ui::ComponentStyle`; every number a user
      might want to change is a commented field with a sensible default.
- [ ] Surfaces, wells, focus rings and text go through `ui::Painter` with
      `style.theme`. No colour literals.
- [ ] Motion uses `style.omega()` and `style.damping()`; `reduced_motion`
      removes travel.
- [ ] Sounds go through `ui::play_cue(feedback, style, style.sounds.x, x)`.
- [ ] Edges refuse softly and stay silent on `input.nav_repeat`.
- [ ] Changing `style` never resets state.
- [ ] A page in the gallery with at least three variants, tests that include
      "draws in every theme", and a page under `docs/components/`.
- [ ] You looked at it in at least six themes, light and dark, round and
      square.

The kit's components do not include anything from `app/`, `concepts/` or
`demo/`: copy `src/ui`, `src/gfx`, `src/core` and `src/audio` into another
project and they come along ([ADOPTING.md](ADOPTING.md)).
