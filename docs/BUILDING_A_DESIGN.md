# Building a design

A *design* is one complete UI: its look, its layout, its motion and its
sounds, in a single `.cpp` file under `src/concepts/`. The app holds many and
switches between them with L1 and R1. This page takes you from an empty file
to a design that renders on a PC, passes its tests and runs on the console.

Read [CRAFT.md](CRAFT.md) first for what to aim at, and keep
[KIT.md](KIT.md) open for what you can call. `src/concepts/aurora.cpp` is the
reference implementation: when in doubt, do what it does.

## The loop

```bash
tools/host-snapshots.sh build/snapshots mydesign   # build for the PC, run the tour, write PNGs
# look at build/snapshots/NN-mydesign*.png, change the code, repeat
make test-unit                                     # every design must pass the shared tests
make                                               # build the PS5 app
```

The snapshot tool renders with the same code as the console, through Mesa's
software renderer, with a fixed 60 Hz clock. A full run takes seconds, so you
can iterate on a design without a console.

## 1. The file

```cpp
// src/concepts/mydesign.cpp
#include "concepts/concepts.hpp"

#include "core/tween.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

namespace hui::concepts
{

namespace
{

using gfx::Color;
using gfx::Rect;

// The design language: every size, radius and colour the screen uses.
const Color kInk = Color::rgb(0xf2f4ff);
const Color kAccent = Color::rgb(0xffb347);
constexpr float kMargin = 96.0f;
constexpr int kItems = 5;

constexpr const char *kTechniques[] = {
    "One line per technique this design demonstrates",
    "They are listed in the info panel (touchpad)",
    "At least three",
};

class MyDesign final : public app::Concept
{
  public:
    explicit MyDesign(app::Context &context) : context_(context)
    {
        highlight_.snap(row_rect(0));
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "mydesign",                   // id: must match the file name
            "My Design",                  // name in the switcher
            "One line: what this design is for",
            "src/concepts/mydesign.cpp",
            audio::SoundSet::glass,       // or paper
            kAccent,                      // controller light bar, shell chrome
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        age_ = 0.0f; // restart the entrance animation
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        age_ += dt;
        clock_ += dt;
        if (input.nav == Direction::up || input.nav == Direction::down)
        {
            const int next = focus_ + (input.nav == Direction::down ? 1 : -1);
            if (next >= 0 && next < kItems)
            {
                focus_ = next;
                feedback.play(audio::Cue::focus);
            }
            else if (!input.nav_repeat)
            {
                feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
                refusal_.trigger();
            }
        }
        if (input.is_pressed(Action::confirm))
            feedback.play(audio::Cue::select);

        highlight_.target(row_rect(focus_));
        highlight_.update(dt, 20.0f);
        refusal_.update(dt, 9.0f);
    }

    void draw(app::Frame &frame) const override
    {
        frame.backdrop.mode = gfx::BackdropMode::gradient;
        frame.backdrop.colors[0] = Color::rgb(0x10131f);
        frame.backdrop.colors[1] = Color::rgb(0x1c1430);
        frame.backdrop.time = clock_;

        gfx::DrawList &list = frame.scene;
        const ui::Fonts &fonts = context_.fonts;
        ui::text(list, fonts.display, "My Design", kMargin, 180, 64, kInk);

        Rect ring = highlight_.value();
        ring.x += ui::shake(refusal_.value, clock_);
        list.rounded_rect(ring, 18, kAccent);
        for (int i = 0; i < kItems; ++i)
        {
            const float in = tween::stagger(age_, i); // rows arrive one by one
            const Rect r = row_rect(i);
            list.push_opacity(in);
            ui::text(list, fonts.semibold, "Row", r.x + 28, r.y + 46 + 20 * (1.0f - in), 28,
                     i == focus_ ? Color::rgb(0x10131f) : kInk);
            list.pop_opacity();
        }

        const ui::Hint hints[] = {{ui::Button::cross, "Select"}};
        ui::draw_hints(list, fonts, ui::GlyphStyle::dark(), hints, 1, 1824, true);
    }

  private:
    static Rect row_rect(int index)
    {
        return {kMargin, 260.0f + static_cast<float>(index) * 84.0f, 520, 72};
    }

    app::Context &context_;
    float age_ = 0.0f;   // seconds since enter(): drives the entrance
    float clock_ = 0.0f; // free-running, for idle motion
    int focus_ = 0;
    ui::SpringRect highlight_;
    ui::Pulse refusal_;
};

} // namespace

std::unique_ptr<app::Concept> make_mydesign(app::Context &context)
{
    return std::make_unique<MyDesign>(context);
}

} // namespace hui::concepts
```

Register it in two places:

```cpp
// src/concepts/concepts.hpp
std::unique_ptr<app::Concept> make_mydesign(app::Context &context);

// src/concepts/registry.cpp, in the order the switcher shows them
concepts::make_mydesign,
```

Nothing else changes: the build scripts pick up every file in `src/`.

## 2. The four functions

| Function | Rule |
| --- | --- |
| `info()` | Returns a static `ConceptInfo`. The `id` equals the file name; a test checks it. |
| `enter()` | The design became visible: reset the entrance clock, close any dialog. Keep the selection: coming back to a design should feel like coming back. |
| `update(input, dt, feedback)` | All logic and all animation state. Read input, move the focus, set spring targets, advance timers with `dt`, ask for sounds through `feedback`. |
| `draw(frame)` | Pure: reads state, writes the frame. It is `const` and may run more than once per frame (during the L1/R1 transition both designs are drawn). Never advance animation here. |

The two clocks in the skeleton are a pattern worth keeping: `age_` restarts in
`enter()` and drives the entrance; `clock_` never restarts and drives idle
motion and the backdrop.

## 3. The frame

`draw()` fills an `app::Frame`:

```
frame.backdrop   procedural shader behind everything      (BackdropSpec)
frame.scene      the screen                               (DrawList)
frame.glass      true: blur backdrop + scene for the overlay
frame.overlay    dialogs, sheets, toasts                  (DrawList)
frame.post       optional shader over everything          (BackdropSpec)
```

Most designs use only `backdrop` and `scene`. Use `overlay` (with `glass`) for
anything modal.

## 4. Input belongs to the design, except three buttons

The shell keeps **L1**, **R1** (switch design) and the **touchpad** (info
panel). Everything else arrives in `InputFrame`. Use L2 and R2 where another
app would use the bumpers for tabs.

Use the logical actions (`Action::confirm`, `Action::back`), not the physical
buttons, so the player's swap setting works.

## 5. The tour

`tour()` returns a script of inputs that shows the design off. It is what
`make host-snapshots` photographs and what the console validation run
replays.

```cpp
constexpr app::TourStep kTour[] = {
    // wait, press, nav, capture name
    {0.4f, 0, Direction::down},
    {0.4f, 0, Direction::down},
    {0.8f, action_bit(Action::confirm), Direction::none, "list"}, // picture, then press
    {0.8f, 0, Direction::none, "dialog"},
    {0.5f, action_bit(Action::back)},
};
...
std::span<const app::TourStep> tour() const override { return kTour; }
```

Each step waits, takes a picture if it names one (before its input acts),
then sends its input for one frame. A step can also hold things during its
wait: `stick_x` / `stick_y` (the left stick), `hold` (action bits) and
`trigger_l` / `trigger_r`, for designs driven by analog input. A first picture is always taken after the
entrance animation. Pictures land in `build/snapshots/NN-id-name.png`.

Leave the design in a neutral state at the end of the tour (dialogs closed).

## 6. Tests

`tests/unit/concepts_test.cpp` runs every registered design through three
checks with no OpenGL: it describes itself, it survives its own tour, and it
survives 600 random inputs while always drawing finite shapes. A new design
is covered the moment it is registered.

Add behaviour tests of your own next to it; `tests/unit/aurora_test.cpp` shows
the pattern (send input, assert on the cues requested).

## 7. Sample content

`context.catalog` holds 24 invented titles with a palette and a rendered
cover each (`demo/catalog.hpp`). Use them wherever a design needs content, so
that designs can be compared on the same material. Do not add real product
names or artwork.

`context.telemetry` has live numbers about the app (frame times, draw calls,
voices): use them where a design wants real, moving data.

`context.settings` is the live settings struct. A design that edits it sets
`context.settings_changed = true`; the app applies and saves the change.

## 8. Before you call it done

Go through the checklist at the end of [CRAFT.md](CRAFT.md), then:

- [ ] `tools/host-snapshots.sh build/snapshots <id>` writes pictures that look
      right; you have looked at each one.
- [ ] `make test-unit` passes.
- [ ] `make` builds the PS5 app without warnings from your file.
- [ ] On the console, the tour report shows the design at 60 FPS (see
      [CONSOLE_VALIDATION.md](CONSOLE_VALIDATION.md)).

## Pitfalls seen while building the designs in this repository

- **PS5 builds have no exceptions and no RTTI.** No `try`, `throw`,
  `dynamic_cast` or `typeid`; `std::function` and the containers are fine.
- **`concept` is a C++20 keyword.** Name variables `design`.
- **Text is placed by baseline**, not by its top edge. A label that looks
  40 px too low was given a top coordinate.
- **Later draws cover earlier ones.** Draw the focused card last so its glow
  and shadow sit on top of its neighbours.
- **`push_opacity` fades a group, including what it overlaps.** Two
  overlapping half-transparent shapes show the overlap; when that matters,
  fade the colours instead.
- **Springs need a `snap` at start-up**, or the first frame animates in from
  zero (a focus ring flying in from the top-left corner).
- **A `static` local in `draw()` is shared by every instance** and by both
  passes of a transition. Keep state in members.
- **Use `snprintf` into a stack buffer for numbers.** Avoid building
  `std::string`s per frame in hot paths; one or two for wrapped text is fine.
