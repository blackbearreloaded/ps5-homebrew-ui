// ps5-homebrew-ui - Design "Control Room": a settings screen whose controls are real.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A rail of categories on the left, the rows of the active one in a panel on
// the right. Every control edits the live settings of the app, so volumes,
// the button swap, the hint size and "Reduce motion" answer on the frame of
// the press. It is the kit's widget reference: slider, stepper, toggle, action
// row, confirm dialog and toast. What makes it feel finished:
//
//   - one focus rectangle for the whole screen: a spring that travels between
//     the rail and the rows and changes size on the way, so the eye never has
//     to search for where the focus went;
//   - controls never jump: a slider's fill and a switch's thumb chase the
//     value with springs, a stepper's text leaves and arrives in the direction
//     of travel, and all of that also plays when "Reset" changes ten values
//     at once;
//   - sound carries the value: a slider rises in pitch as it fills, a switch
//     sounds higher on than off, and the ends of a range refuse softly;
//   - the rows of a category arrive staggered and the old ones fade first,
//     the description under them cross-fades, and so do the button hints,
//     which show Circle for "confirm" the moment the buttons are swapped;
//   - each category ends in a small live preview of what its rows change;
//   - the destructive action asks first, in a frosted dialog that focuses
//     "Cancel", and a "Saved" toast appears once per burst of changes, after
//     the app has written the file.

#include "concepts/concepts.hpp"

#include "core/tween.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace hui::concepts
{

namespace
{

using gfx::Color;
using gfx::Rect;

// ---- the design language -------------------------------------------------

const Color kInk = Color::rgb(0xe9f3f1);
const Color kAccent = Color::rgb(0x5ee6c8);
const Color kDanger = Color::rgb(0xff8f8f);
const Color kDeep = Color::rgb(0x0a171d);    // text on bright surfaces, scrims
const Color kSurface = Color::rgb(0x13242c); // the panel
const Color kClear = Color::rgb(0x000000, 0.0f);

constexpr float kPi = 3.14159265f;
constexpr float kMargin = 96.0f;
constexpr float kRadius = 18.0f;      // rows, rail items, cards, the focus
constexpr float kPanelRadius = 28.0f; // the panel and the dialog

constexpr float kRailW = 372.0f;
constexpr float kRailY = 236.0f;
constexpr float kRailH = 72.0f;
constexpr float kRailPitch = 84.0f;

constexpr Rect kPanel{504.0f, 236.0f, 1320.0f, 692.0f};
constexpr float kPanelPad = 24.0f; // panel edge to a row
constexpr float kRowPad = 28.0f;   // row edge to its label and control
constexpr float kRowsY = kPanel.y + 104.0f;
constexpr float kRowH = 72.0f;
constexpr float kRowPitch = 80.0f;
constexpr float kFooterY = kPanel.y + kPanel.h - 84.0f; // hairline above the description
constexpr float kTileH = 172.0f;
constexpr float kTileGap = 16.0f;

constexpr float kSliderW = 440.0f;
constexpr float kToggleW = 84.0f;
constexpr float kToggleH = 44.0f;
constexpr float kStepperW = 300.0f; // chevron to chevron

constexpr Rect kDialog{560.0f, 348.0f, 800.0f, 384.0f};

// The shell writes the settings file 0.75 s after the last change. The toast
// waits a little longer, so "Saved" is said after the write, not before.
constexpr float kSaveDelay = 0.8f;
constexpr float kToastSeconds = 2.0f;

constexpr const char *kTechniques[] = {
    "Live settings: every control edits app::Context::settings and applies at once",
    "One spring focus rectangle that travels between the rail and the rows",
    "Slider, stepper, toggle and action row built from DrawList shapes and springs",
    "Value-pitched slider cues, soft refusals at the ends of every range",
    "Staggered cross-fade of rows, description and button hints on every change",
    "Frosted confirm dialog that focuses Cancel, and one Saved toast per burst",
};

// The tour changes values to show the controls working, then puts each one
// back: on the console it runs against the player's real settings.
constexpr app::TourStep kTour[] = {
    {0.5f, 0, Direction::right},
    {0.4f, 0, Direction::right},
    {0.2f, 0, Direction::right},
    {1.3f, 0, Direction::left, "audio"},
    {0.2f, 0, Direction::left},
    {0.4f, action_bit(Action::jump_next)},
    {0.3f, 0, Direction::down},
    {0.25f, 0, Direction::down},
    {0.3f, 0, Direction::right},
    {0.9f, 0, Direction::left, "display"},
    {0.4f, action_bit(Action::jump_next)},
    {0.5f, action_bit(Action::confirm)},
    {0.9f, action_bit(Action::confirm), Direction::none, "controller"},
    {0.5f, action_bit(Action::jump_next)},
    {0.5f, action_bit(Action::jump_next)},
    {2.6f, action_bit(Action::confirm), Direction::none, "about"},
    {0.9f, action_bit(Action::back), Direction::none, "reset"},
    {0.6f, action_bit(Action::back)},
};

// ---- what the screen edits -------------------------------------------------

// One editable value. Two rows may show the same field (hint size appears
// under Display and under Accessibility), so the animation state of a control
// belongs to the field, not to the row.
enum class Field : std::uint8_t
{
    music,
    effects,
    menu,
    resolution,
    overlay,
    hint_size,
    swap,
    vibration,
    light_bar,
    reduce_motion,
    reset,
    count,
};
constexpr std::size_t kFieldCount = static_cast<std::size_t>(Field::count);

enum class Kind : std::uint8_t
{
    slider,  // 0..10, left and right
    stepper, // a few named choices, left and right
    toggle,  // on or off, confirm
    action,  // confirm opens the dialog
};

struct Row
{
    const char *label;
    const char *description; // shown under the rows while this one is focused
    Kind kind;
    Field field;
};

struct Category
{
    const char *name;
    const char *summary; // shown under the rows while the rail is focused
    const Row *rows;
    int count;
};

constexpr const char *kHintSizeHelp = "How large the button hints at the bottom of the screen are.";

constexpr Row kAudioRows[] = {
    {"Music volume", "How loud the background music plays.", Kind::slider, Field::music},
    {"Effects volume", "Sounds with weight: launches, fanfares and everything inside a game.",
     Kind::slider, Field::effects},
    {"Menu sounds volume", "Focus ticks, confirmations and the other sounds of the interface.",
     Kind::slider, Field::menu},
};
constexpr Row kDisplayRows[] = {
    {"Resolution", "The picture sent to the TV. Applies the next time the app starts.",
     Kind::stepper, Field::resolution},
    {"Show performance overlay",
     "Frame rate, frame time and draw counts in the bottom-left corner.", Kind::toggle,
     Field::overlay},
    {"Hint size", kHintSizeHelp, Kind::stepper, Field::hint_size},
};
constexpr Row kControllerRows[] = {
    {"Swap Cross and Circle", "Circle confirms and Cross goes back, everywhere in the app.",
     Kind::toggle, Field::swap},
    {"Vibration", "A short rumble when something is refused and on big moments.", Kind::toggle,
     Field::vibration},
    {"Light bar follows the design",
     "The controller's light takes the accent colour of the design.", Kind::toggle,
     Field::light_bar},
};
constexpr Row kAccessRows[] = {
    {"Reduce motion", "Replaces slides, zooms and overshoot with short fades.", Kind::toggle,
     Field::reduce_motion},
    {"Hint size", kHintSizeHelp, Kind::stepper, Field::hint_size},
};
constexpr Row kAboutRows[] = {
    {"Reset all settings", "Returns every option on these pages to its default value.",
     Kind::action, Field::reset},
};

constexpr int kAbout = 4;
constexpr Category kCategories[] = {
    {"Audio", "Balance music, effects and menu sounds.", kAudioRows, 3},
    {"Display", "Resolution, the performance overlay and the size of the hints.", kDisplayRows, 3},
    {"Controller", "Button layout, vibration and the light bar.", kControllerRows, 3},
    {"Accessibility", "Make the interface calmer and easier to read.", kAccessRows, 2},
    {"About", "What this app is, and the way back to its defaults.", kAboutRows, 1},
};
constexpr int kCategoryCount = 5;

constexpr const char *kHintSizes[] = {"Small", "Medium", "Large"};
constexpr const char *kMotionNotes[] = {
    "Screens, panels and rows fade instead of sliding",
    "The backdrop and the idle glows hold still",
    "Switches and dialogs settle without overshoot",
    "Sounds and the focus highlight work exactly as before",
};

// What the hint row offers; one mode per focus context so it can cross-fade.
enum class HintMode : std::uint8_t
{
    rail,
    slider,
    stepper,
    toggle_off,
    toggle_on,
    action,
    dialog,
};

// The animation state of one field's control.
struct Control
{
    int seen = 0;           // the value the animations last reacted to
    int previous = 0;       // ... and the one before, for the stepper's cross-fade
    float direction = 1.0f; // +1 when the value last rose, -1 when it fell
    tween::Spring level;    // slider fill, toggle track colour
    tween::Bounce thumb;    // toggle thumb: overshoots a little
    tween::Timer change;    // stepper text leaving and arriving
    ui::Pulse press;        // the nudge of a chevron or thumb on the press
};

float as_float(int value)
{
    return static_cast<float>(value);
}

Rect rail_rect(int category)
{
    return {kMargin, kRailY + as_float(category) * kRailPitch, kRailW, kRailH};
}

Rect row_rect(int category, int row)
{
    float y = kRowsY + as_float(row) * kRowPitch;
    if (category == kAbout)
        y += 2.0f * (kTileH + kTileGap) + 8.0f; // the action sits under the fact tiles
    return {kPanel.x + kPanelPad, y, kPanel.w - 2.0f * kPanelPad, kRowH};
}

// Baseline that centres a line of the given size on cy.
float centred(float cy, float size)
{
    return cy + 0.35f * size;
}

class SettingsDesign final : public app::Concept
{
  public:
    explicit SettingsDesign(app::Context &context)
        : context_(context), start_resolution_(context.settings.resolution)
    {
        for (std::size_t i = 0; i < kFieldCount; ++i)
        {
            Control &control = controls_[i];
            control.seen = control.previous = value(static_cast<Field>(i));
            control.level.snap(as_float(control.seen));
            control.thumb.snap(as_float(control.seen));
        }
        focus_.snap(rail_rect(0));
        marker_.snap(rail_rect(0).y);
        hint_scale_.snap(as_float(context.settings.hint_size));
        footer_shown_ = footer_previous_ = footer_text();
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "settings",
            "Control Room",
            "A settings screen with real controls: sliders, switches, steppers, a dialog",
            "src/concepts/settings.cpp",
            audio::SoundSet::glass,
            kAccent,
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        age_ = 0.0f;
        content_age_ = -0.2f; // the rows start after the panel has arrived
        leave_.running = false;
        dialog_open_ = false;
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        const bool calm = context_.settings.reduced_motion;
        age_ += dt;
        clock_ += dt;
        content_age_ += dt;
        if (!calm)
            drift_ += dt; // the backdrop and the idle glow hold still under "Reduce motion"

        if (dialog_open_)
            update_dialog(input, feedback);
        else
            update_screen(input, feedback);
        update_toast(dt, feedback);

        // ---- animation state ----
        animate_controls(dt, calm);
        focus_.target(column_ == Column::rail ? rail_rect(category_)
                                              : row_rect(category_, row_[category_]));
        focus_.update(dt, calm ? 36.0f : 20.0f);
        marker_.target = rail_rect(category_).y;
        marker_.update(dt, calm ? 36.0f : 16.0f);
        refuse_.update(dt, 9.0f);
        leave_.update(dt);
        hint_scale_.target = as_float(context_.settings.hint_size);
        hint_scale_.update(dt, calm ? 36.0f : 14.0f);
        dialog_.target = dialog_open_ ? 1.0f : 0.0f;
        dialog_.update(dt, calm ? 40.0f : 14.0f);
        choice_position_.target = as_float(choice_);
        choice_position_.update(dt, calm ? 36.0f : 22.0f);
        toast_.target = toast_left_ > 0.0f ? 1.0f : 0.0f;
        toast_.update(dt, calm ? 40.0f : 13.0f);
        rumble_show_.update(dt, 2.5f);

        // The description and the hints follow the focus with a cross-fade.
        if (const char *wanted = footer_text(); wanted != footer_shown_)
        {
            footer_previous_ = footer_shown_;
            footer_shown_ = wanted;
            footer_.start(calm ? 0.12f : 0.24f);
        }
        footer_.update(dt);
        if (const HintMode wanted = hint_mode(); wanted != hints_shown_)
        {
            hints_previous_ = hints_shown_;
            hints_shown_ = wanted;
            hints_.start(calm ? 0.12f : 0.22f);
        }
        hints_.update(dt);
    }

    void draw(app::Frame &frame) const override
    {
        frame.backdrop.mode = gfx::BackdropMode::dots;
        frame.backdrop.colors[0] = Color::rgb(0x081319);
        frame.backdrop.colors[1] = Color::rgb(0x0e2a30);
        frame.backdrop.colors[2] = Color::rgb(0x2f8f86);
        frame.backdrop.time = drift_;

        gfx::DrawList &list = frame.scene;
        // The dialog pushes the screen back: it shrinks a little and dims.
        const float back = dialog_.value;
        list.push_transform(1.0f - 0.03f * back * travel(), 960, 540, 0, 0);
        draw_header(list);
        draw_rail(list);
        draw_panel(list);
        draw_focus(list);
        draw_rail_labels(list);
        if (leave_.running)
        {
            // The old rows leave quickly; the new ones stagger in behind them.
            const float t = leave_.progress();
            draw_category(list, previous_, 1.0f - tween::smoothstep(t * 1.25f),
                          -change_direction_ * 18.0f * tween::cubic_in(t) * travel(), 10.0f);
        }
        draw_category(list, category_, 1.0f, 0.0f, content_age_);
        draw_footer(list);
        list.pop_transform();
        if (back > 0.01f)
            list.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                              kDeep.with_alpha(0.6f * back));

        if (back > 0.01f)
        {
            frame.glass = true;
            draw_dialog(frame.overlay, frame.glass_texture);
        }
        draw_hints(frame);
        draw_toast(frame.overlay);
    }

    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    enum class Column : std::uint8_t
    {
        rail,
        rows,
    };

    // ---- the settings, by field ----

    int value(Field field) const
    {
        const Settings &s = context_.settings;
        switch (field)
        {
        case Field::music:
            return s.music_volume;
        case Field::effects:
            return s.sfx_volume;
        case Field::menu:
            return s.ui_volume;
        case Field::resolution:
            return s.resolution;
        case Field::overlay:
            return s.show_fps ? 1 : 0;
        case Field::hint_size:
            return s.hint_size;
        case Field::swap:
            return s.swap_confirm ? 1 : 0;
        case Field::vibration:
            return s.haptics ? 1 : 0;
        case Field::light_bar:
            return s.light_bar ? 1 : 0;
        case Field::reduce_motion:
            return s.reduced_motion ? 1 : 0;
        default:
            return 0;
        }
    }

    static int limit(Field field)
    {
        switch (field)
        {
        case Field::music:
        case Field::effects:
        case Field::menu:
            return 10;
        case Field::resolution:
            return Settings::kResolutionCount - 1;
        case Field::hint_size:
            return 2;
        default:
            return 1;
        }
    }

    // Writes a field and tells the app, which applies and saves it.
    void store(Field field, int next)
    {
        Settings &s = context_.settings;
        switch (field)
        {
        case Field::music:
            s.music_volume = next;
            break;
        case Field::effects:
            s.sfx_volume = next;
            break;
        case Field::menu:
            s.ui_volume = next;
            break;
        case Field::resolution:
            s.resolution = next;
            break;
        case Field::overlay:
            s.show_fps = next != 0;
            break;
        case Field::hint_size:
            s.hint_size = next;
            break;
        case Field::swap:
            s.swap_confirm = next != 0;
            break;
        case Field::vibration:
            s.haptics = next != 0;
            break;
        case Field::light_bar:
            s.light_bar = next != 0;
            break;
        case Field::reduce_motion:
            s.reduced_motion = next != 0;
            break;
        default:
            return;
        }
        mark_changed("Saved");
    }

    void mark_changed(const char *message)
    {
        context_.settings_changed = true;
        // Restarting the wait on every change is what makes a held slider
        // produce one toast when it comes to rest, not one per step.
        save_wait_ = kSaveDelay;
        pending_message_ = message;
    }

    static const char *choice_text(Field field, int index)
    {
        if (field == Field::resolution)
            return Settings::kResolutions[std::clamp(index, 0, Settings::kResolutionCount - 1)]
                .label;
        return kHintSizes[std::clamp(index, 0, 2)];
    }

    const Control &control(Field field) const
    {
        return controls_[static_cast<std::size_t>(field)];
    }
    Control &control(Field field)
    {
        return controls_[static_cast<std::size_t>(field)];
    }

    const Row &focused_row() const
    {
        return kCategories[category_].rows[row_[category_]];
    }

    // 1 when things may slide and overshoot, 0 under "Reduce motion".
    float travel() const
    {
        return context_.settings.reduced_motion ? 0.0f : 1.0f;
    }

    // Cross, unless the player swapped the buttons a moment ago on this screen.
    ui::Button confirm_button() const
    {
        return context_.settings.swap_confirm ? ui::Button::circle : ui::Button::cross;
    }
    ui::Button back_button() const
    {
        return context_.settings.swap_confirm ? ui::Button::cross : ui::Button::circle;
    }

    const char *footer_text() const
    {
        return column_ == Column::rail ? kCategories[category_].summary : focused_row().description;
    }

    HintMode hint_mode() const
    {
        if (dialog_open_)
            return HintMode::dialog;
        if (column_ == Column::rail)
            return HintMode::rail;
        const Row &row = focused_row();
        switch (row.kind)
        {
        case Kind::slider:
            return HintMode::slider;
        case Kind::stepper:
            return HintMode::stepper;
        case Kind::toggle:
            return value(row.field) != 0 ? HintMode::toggle_on : HintMode::toggle_off;
        default:
            return HintMode::action;
        }
    }

    // ---- input ----

    void refuse(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav_repeat)
            return; // holding a direction against an edge is not a mistake
        feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        feedback.rumble(0.25f, 0.05f);
        refuse_.trigger();
    }

    void show_category(int next, app::Feedback &feedback)
    {
        previous_ = category_;
        change_direction_ = next > category_ ? 1.0f : -1.0f;
        category_ = next;
        // The new page starts as the old one is fading, so the panel is
        // never empty and two titles are never readable in the same place.
        leave_.start(context_.settings.reduced_motion ? 0.1f : 0.14f);
        content_age_ = -0.06f;
        // Categories sound like steps of a scale, lower down the rail.
        feedback.play(audio::Cue::tab, 1.1f - 0.045f * as_float(next),
                      ui::pan_for_x(rail_rect(next).cx()));
    }

    void step_category(int delta, const InputFrame &input, app::Feedback &feedback)
    {
        const int next = category_ + delta;
        if (next < 0 || next >= kCategoryCount)
            refuse(input, feedback);
        else
            show_category(next, feedback);
    }

    void update_screen(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.is_pressed(Action::jump_prev))
            step_category(-1, input, feedback);
        else if (input.is_pressed(Action::jump_next))
            step_category(1, input, feedback);
        else if (column_ == Column::rail)
            update_rail(input, feedback);
        else
            update_rows(input, feedback);
    }

    void update_rail(const InputFrame &input, app::Feedback &feedback)
    {
        switch (input.nav)
        {
        case Direction::up:
            step_category(-1, input, feedback);
            break;
        case Direction::down:
            step_category(1, input, feedback);
            break;
        case Direction::right:
            enter_rows(feedback, audio::Cue::focus);
            break;
        case Direction::left:
            refuse(input, feedback);
            break;
        case Direction::none:
            break;
        }
        if (input.is_pressed(Action::confirm))
            enter_rows(feedback, audio::Cue::select);
    }

    void enter_rows(app::Feedback &feedback, audio::Cue cue)
    {
        if (column_ == Column::rows)
            return;
        column_ = Column::rows;
        feedback.play(cue, 1.0f, ui::pan_for_x(kPanel.cx()));
    }

    void leave_rows(app::Feedback &feedback, audio::Cue cue)
    {
        column_ = Column::rail;
        feedback.play(cue, 1.0f, ui::pan_for_x(rail_rect(category_).cx()));
    }

    void update_rows(const InputFrame &input, app::Feedback &feedback)
    {
        const Category &category = kCategories[category_];
        int &row = row_[category_];
        const Row &current = category.rows[row];
        const bool adjustable = current.kind == Kind::slider || current.kind == Kind::stepper;
        switch (input.nav)
        {
        case Direction::up:
        case Direction::down:
        {
            const int next = row + (input.nav == Direction::down ? 1 : -1);
            if (next < 0 || next >= category.count)
            {
                refuse(input, feedback);
                break;
            }
            row = next;
            feedback.play(audio::Cue::focus, 1.04f - 0.03f * as_float(next),
                          ui::pan_for_x(kPanel.cx()));
            break;
        }
        case Direction::left:
            // Left adjusts where there is something to adjust; on a switch or
            // an action it is free, so it leads back to the rail.
            if (adjustable)
                adjust(current, -1, input, feedback);
            else
                leave_rows(feedback, audio::Cue::focus);
            break;
        case Direction::right:
            if (adjustable)
                adjust(current, 1, input, feedback);
            else
                refuse(input, feedback);
            break;
        case Direction::none:
            break;
        }

        if (input.is_pressed(Action::confirm))
            activate(category.rows[row], feedback);
        if (input.is_pressed(Action::back))
            leave_rows(feedback, audio::Cue::back);
    }

    // Left or right on a slider or a stepper: one step, or a soft refusal.
    void adjust(const Row &row, int delta, const InputFrame &input, app::Feedback &feedback)
    {
        const int next = value(row.field) + delta;
        Control &state = control(row.field);
        state.direction = as_float(delta); // also aims the nudge of a refusal
        if (next < 0 || next > limit(row.field))
        {
            refuse(input, feedback);
            return;
        }
        store(row.field, next);
        state.press.trigger();
        const float pan = ui::pan_for_x(control_x(row, next));
        if (row.kind == Kind::slider)
            feedback.play(audio::Cue::slider, 0.85f + 0.04f * as_float(next), pan);
        else
            feedback.play(audio::Cue::slider, 0.94f + 0.06f * as_float(next), pan);
    }

    void activate(const Row &row, app::Feedback &feedback)
    {
        switch (row.kind)
        {
        case Kind::toggle:
        {
            const bool on = value(row.field) == 0;
            store(row.field, on ? 1 : 0);
            control(row.field).press.trigger();
            feedback.play(audio::Cue::toggle, on ? 1.06f : 0.94f,
                          ui::pan_for_x(control_x(row, on ? 1 : 0)));
            if (row.field == Field::vibration && on)
            {
                // The setting demonstrates itself: this is what was turned on.
                feedback.rumble(0.55f, 0.14f);
                rumble_show_.trigger();
            }
            break;
        }
        case Kind::stepper:
        {
            // Confirm walks the choices forward and wraps: a one-button way
            // through a short list.
            const int next = (value(row.field) + 1) % (limit(row.field) + 1);
            store(row.field, next);
            control(row.field).press.trigger();
            control(row.field).direction = 1.0f;
            feedback.play(audio::Cue::slider, 0.94f + 0.06f * as_float(next),
                          ui::pan_for_x(control_x(row, next)));
            break;
        }
        case Kind::action:
            dialog_open_ = true;
            choice_ = 0; // the safe choice
            choice_position_.snap(0.0f);
            feedback.play(audio::Cue::modal_open);
            break;
        case Kind::slider:
            break; // a slider has nothing to confirm; the hints do not offer it
        }
    }

    void update_dialog(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav == Direction::left || input.nav == Direction::right)
        {
            const int next = input.nav == Direction::right ? 1 : 0;
            if (next != choice_)
            {
                choice_ = next;
                feedback.play(audio::Cue::focus, 1.0f, next == 1 ? 0.2f : -0.2f);
            }
            else
            {
                refuse(input, feedback);
            }
        }
        if (input.is_pressed(Action::confirm))
        {
            dialog_open_ = false;
            if (choice_ == 1)
            {
                reset_settings();
                feedback.play(audio::Cue::select);
                feedback.rumble(0.4f, 0.1f);
            }
            else
            {
                feedback.play(audio::Cue::modal_close);
            }
        }
        else if (input.is_pressed(Action::back))
        {
            dialog_open_ = false;
            feedback.play(audio::Cue::back);
        }
    }

    void reset_settings()
    {
        // Which design the app opens with is the shell's business, not a
        // preference shown on this screen: it survives the reset.
        const int design = context_.settings.concept_index;
        context_.settings = Settings{};
        context_.settings.concept_index = design;
        mark_changed("Defaults restored");
    }

    void update_toast(float dt, app::Feedback &feedback)
    {
        toast_left_ = std::max(0.0f, toast_left_ - dt);
        toast_age_ += dt;
        if (save_wait_ <= 0.0f)
            return;
        save_wait_ -= dt;
        if (save_wait_ > 0.0f)
            return;
        // A toast that is still up is only kept up: one chime per burst.
        if (toast_left_ <= 0.0f || toast_message_ != pending_message_)
        {
            feedback.play(audio::Cue::saved, 1.0f, 0.4f);
            toast_age_ = 0.0f;
        }
        toast_message_ = pending_message_;
        toast_left_ = kToastSeconds;
    }

    void animate_controls(float dt, bool calm)
    {
        for (std::size_t i = 0; i < kFieldCount; ++i)
        {
            Control &state = controls_[i];
            // Reacting to the value, not to the button, means a reset (or any
            // change from outside) animates exactly like a press.
            const int now = value(static_cast<Field>(i));
            if (now != state.seen)
            {
                if (state.press.value <= 0.0f)
                    state.direction = now > state.seen ? 1.0f : -1.0f;
                state.previous = state.seen;
                state.seen = now;
                state.change.start(calm ? 0.12f : 0.26f);
            }
            state.level.target = as_float(now);
            state.level.update(dt, calm ? 40.0f : 18.0f);
            state.thumb.target = as_float(now);
            state.thumb.update(dt, 22.0f, calm ? 1.0f : 0.6f);
            state.change.update(dt);
            state.press.update(dt, 10.0f);
        }
    }

    // ---- geometry of the controls (shared by drawing and sound panning) ----

    static float control_right(const Rect &row)
    {
        return row.x + row.w - kRowPad;
    }

    // Where the moving part of a row's control is, for stereo placement.
    float control_x(const Row &row, int at) const
    {
        const float right = control_right(row_rect(category_, row_[category_]));
        if (row.kind == Kind::slider)
            return right - 76.0f - kSliderW + kSliderW * as_float(at) / 10.0f;
        if (row.kind == Kind::stepper)
            return right - kStepperW * 0.5f;
        return right - kToggleW * 0.5f;
    }

    // ---- drawing ----

    void draw_header(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = tween::stagger(age_, 0, 0.06f, 0.5f);
        const float lift = 14.0f * (1.0f - in) * travel();
        list.push_opacity(in);
        ui::text(list, fonts.semibold, "CONTROL ROOM", kMargin + 2, 118 - lift, 18, kAccent,
                 gfx::Align::left, 4.0f);
        ui::text(list, fonts.display, "Settings", kMargin - 3, 186 - lift, 64, kInk);
        list.pop_opacity();
    }

    // The marker of the active category: a quiet plate that glides along the
    // rail. It stays when the focus rectangle moves into the rows, so the
    // rail always says which page the panel shows.
    void draw_rail(gfx::DrawList &list) const
    {
        const float in = tween::stagger(age_, 1, 0.05f, 0.45f);
        const Rect plate{kMargin, marker_.value, kRailW, kRailH};
        list.rounded_rect(plate, kRadius, kInk.with_alpha(0.06f * in));

        const float note_in = tween::stagger(age_, 8, 0.07f, 0.5f);
        list.push_opacity(note_in);
        list.circle(kMargin + 6, 866, 4, kAccent);
        ui::paragraph(list, context_.fonts.regular, "Changes apply at once and are saved for you.",
                      kMargin + 24, 874, 22, kRailW - 40, 32, kInk.with_alpha(0.55f), 2);
        list.pop_opacity();
    }

    // Icons and names are drawn after the focus rectangle so they sit on it.
    void draw_rail_labels(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        for (int i = 0; i < kCategoryCount; ++i)
        {
            const float in = tween::stagger(age_, 1 + i, 0.05f, 0.45f);
            const Rect r = rail_rect(i);
            const bool active = i == category_;
            const float x = r.x - 28.0f * (1.0f - in) * travel();
            list.push_opacity(in);
            draw_icon(list, i, x + 46, r.cy(), active ? kAccent : kInk.with_alpha(0.6f));
            ui::text(list, active ? fonts.semibold : fonts.regular, kCategories[i].name, x + 88,
                     centred(r.cy(), 28), 28, kInk.with_alpha(active ? 1.0f : 0.68f));
            list.pop_opacity();
        }
        // The marker's accent bar rides on the plate's left edge.
        list.rounded_rect({kMargin + 8, marker_.value + 20, 4, kRailH - 40}, 2,
                          kAccent.with_alpha(tween::stagger(age_, 1, 0.05f, 0.45f)));
    }

    static void draw_icon(gfx::DrawList &list, int category, float cx, float cy, Color ink)
    {
        switch (category)
        {
        case 0: // speaker: body, cone and two waves
        {
            list.rounded_rect({cx - 15, cy - 5, 8, 10}, 2, ink);
            const float cone[] = {cx - 9, cy - 5, cx - 1, cy - 12, cx - 1, cy + 12, cx - 9, cy + 5};
            list.polygon(cone, 4, ink);
            list.arc(cx - 1, cy, 9, 2.5f, kPi * 0.22f, kPi * 0.56f, ink);
            list.arc(cx - 1, cy, 16, 2.5f, kPi * 0.22f, kPi * 0.56f, ink);
            break;
        }
        case 1: // screen on a stand
            list.bordered_rect({cx - 16, cy - 13, 32, 21}, 4, kClear, 2.5f, ink);
            list.line(cx - 7, cy + 13, cx + 7, cy + 13, 2.5f, ink);
            break;
        case 2: // controller: body, d-pad, one button
            list.bordered_rect({cx - 17, cy - 10, 34, 20}, 10, kClear, 2.5f, ink);
            list.line(cx - 10, cy, cx - 4, cy, 2.0f, ink);
            list.line(cx - 7, cy - 3, cx - 7, cy + 3, 2.0f, ink);
            list.circle(cx + 7, cy, 2.5f, ink);
            break;
        case 3: // a figure in a circle
            list.ring(cx, cy, 16, 2.5f, ink);
            list.circle(cx, cy - 8, 2.5f, ink);
            list.line(cx - 7, cy - 3, cx + 7, cy - 3, 2.2f, ink);
            list.line(cx, cy - 3, cx, cy + 3, 2.2f, ink);
            list.line(cx, cy + 3, cx - 4, cy + 9, 2.2f, ink);
            list.line(cx, cy + 3, cx + 4, cy + 9, 2.2f, ink);
            break;
        default: // "i" in a circle
            list.ring(cx, cy, 16, 2.5f, ink);
            list.circle(cx, cy - 7, 2.2f, ink);
            list.line(cx, cy - 1, cx, cy + 8, 3.0f, ink);
            break;
        }
    }

    void draw_panel(gfx::DrawList &list) const
    {
        const float in = tween::stagger(age_, 3, 0.06f, 0.55f);
        list.push_opacity(in);
        list.shadow({kPanel.x, kPanel.y + 18, kPanel.w, kPanel.h}, kPanelRadius, 44,
                    Color::rgb(0x000000, 0.4f));
        list.gradient_rect(kPanel, kPanelRadius, kSurface.with_alpha(0.94f),
                           gfx::mix(kSurface, kDeep, 0.35f).with_alpha(0.94f));
        list.bordered_rect(kPanel, kPanelRadius, kClear, 1.5f, kInk.with_alpha(0.09f));
        list.rounded_rect({kPanel.x + kPanelPad, kFooterY, kPanel.w - 2 * kPanelPad, 1.5f}, 0,
                          kInk.with_alpha(0.1f));
        list.pop_opacity();
    }

    // The one focus of the screen. It is lit (glow), lifted (shadow), filled
    // and ringed, so it reads without relying on colour.
    void draw_focus(gfx::DrawList &list) const
    {
        Rect r = focus_.value();
        r.x += ui::shake(refuse_.value, clock_, 9.0f, 9.0f);
        const float breath = context_.settings.reduced_motion ? 0.5f : ui::breathe(drift_);
        list.push_opacity(tween::stagger(age_, 4, 0.06f, 0.4f));
        list.shadow({r.x, r.y + 10, r.w, r.h}, kRadius, 24, Color::rgb(0x000000, 0.4f));
        list.glow(r, kRadius, 18, kAccent.with_alpha(0.14f + 0.1f * breath));
        list.rounded_rect(r, kRadius, gfx::mix(kSurface, kAccent, 0.17f));
        list.bordered_rect(r, kRadius, kClear, 2.0f, kAccent);
        list.pop_opacity();
    }

    // One category's page: title, rows, preview. Drawn at an opacity and an
    // offset so the leaving and the arriving page can overlap. `clock` is the
    // page's own entrance time; a large value means "fully arrived".
    void draw_category(gfx::DrawList &list, int index, float alpha, float slide, float clock) const
    {
        if (alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const Category &category = kCategories[index];
        const bool current = index == category_ && alpha >= 1.0f;
        const float rise = 18.0f * travel();
        list.push_opacity(alpha);

        const float title_in = tween::stagger(clock, 0, 0.05f, 0.3f);
        list.push_opacity(title_in);
        ui::text(list, fonts.semibold, category.name, kPanel.x + kPanelPad + kRowPad,
                 kPanel.y + 66 + slide + rise * (1.0f - title_in), 36, kInk);
        list.pop_opacity();

        // On the About page the facts come first, then the action.
        const int first = index == kAbout ? 3 : 1;
        for (int i = 0; i < category.count; ++i)
        {
            const float in = tween::stagger(clock, first + i, 0.05f, 0.32f);
            Rect r = row_rect(index, i);
            r.y += slide + rise * (1.0f - in);
            list.push_opacity(in);
            draw_row(list, category.rows[i], r,
                     current && column_ == Column::rows && i == row_[index]);
            list.pop_opacity();
        }

        const float extra_in =
            tween::stagger(clock, index == kAbout ? 1 : category.count + 1, 0.05f, 0.36f);
        const float y = slide + rise * (1.0f - extra_in);
        list.push_opacity(extra_in);
        switch (index)
        {
        case 0:
            draw_levels(list, y);
            break;
        case 1:
            draw_display_preview(list, y);
            break;
        case 2:
            draw_pad_preview(list, y);
            break;
        case 3:
            draw_motion_card(list, y);
            break;
        default:
            draw_facts(list, y);
            break;
        }
        list.pop_opacity();
        list.pop_opacity();
    }

    void draw_row(gfx::DrawList &list, const Row &row, const Rect &r, bool focused) const
    {
        const ui::Fonts &fonts = context_.fonts;
        if (!focused)
            list.rounded_rect(r, kRadius, kInk.with_alpha(0.035f));
        ui::text(list, focused ? fonts.semibold : fonts.regular, row.label, r.x + kRowPad,
                 centred(r.cy(), 28), 28, kInk.with_alpha(focused ? 1.0f : 0.84f));
        switch (row.kind)
        {
        case Kind::slider:
            draw_slider(list, row.field, r, focused);
            break;
        case Kind::stepper:
            draw_stepper(list, row.field, r, focused);
            break;
        case Kind::toggle:
            draw_toggle(list, row.field, r, focused);
            break;
        case Kind::action:
            draw_action(list, r, focused);
            break;
        }
    }

    // Slider: track, fill, thumb, and the number in the monospaced face so
    // its digits do not shift the layout when they change.
    void draw_slider(gfx::DrawList &list, Field field, const Rect &r, bool focused) const
    {
        const Control &state = control(field);
        const float right = control_right(r);
        const float cy = r.cy();
        const Rect track{right - 76.0f - kSliderW, cy - 4, kSliderW, 8};
        const float level = tween::clamp01(state.level.value / 10.0f);
        const float x = track.x + track.w * level;

        list.rounded_rect(track, 4, kInk.with_alpha(0.16f));
        if (level > 0.004f)
            list.gradient_rect_h({track.x, track.y, std::max(8.0f, track.w * level), track.h}, 4,
                                 gfx::mix(kAccent, kSurface, 0.45f), kAccent);
        // Eleven detents under the track: the slider moves in whole steps.
        for (int i = 0; i <= 10; ++i)
        {
            const float tx = track.x + track.w * as_float(i) / 10.0f;
            const bool lit = as_float(i) <= state.level.value + 0.05f;
            list.circle(tx, cy + 17, 1.6f, lit ? kAccent.with_alpha(0.7f) : kInk.with_alpha(0.22f));
        }
        if (focused)
        {
            const float size = 14.0f + 3.0f * state.press.value;
            list.glow({x - size, cy - size, size * 2, size * 2}, size, 16,
                      kAccent.with_alpha(0.55f));
            list.circle(x, cy, size, kInk);
            list.circle(x, cy, 5, kAccent);
        }
        else
        {
            list.circle(x, cy, 10, kInk.with_alpha(0.9f));
        }
        char text[8];
        std::snprintf(text, sizeof(text), "%d", state.seen);
        ui::text(list, context_.fonts.mono, text, right, centred(cy, 30), 30,
                 focused ? kInk : kInk.with_alpha(0.8f), gfx::Align::right);
    }

    // Toggle: a pill whose thumb crosses on an underdamped spring and whose
    // track eases from grey to the accent. The state is also written out, so
    // it never depends on colour alone.
    void draw_toggle(gfx::DrawList &list, Field field, const Rect &r, bool focused) const
    {
        const Control &state = control(field);
        const float right = control_right(r);
        const Rect pill{right - kToggleW, r.cy() - kToggleH * 0.5f, kToggleW, kToggleH};
        const float on = tween::clamp01(state.level.value);
        const float stretch = 7.0f * state.press.value; // the thumb squashes on the press
        const float x = pill.x + 22.0f + (kToggleW - 44.0f) * state.thumb.value;

        if (focused)
            list.glow(pill, kToggleH * 0.5f, 12, kAccent.with_alpha(0.18f + 0.3f * on));
        // The rim keeps the unlit track visible on the focus rectangle's fill.
        list.bordered_rect(pill, kToggleH * 0.5f, gfx::mix(Color::rgb(0x34484f), kAccent, on), 1.5f,
                           gfx::mix(kInk.with_alpha(0.3f), kAccent, on));
        // The thumb turns from light to dark as the track turns bright; its
        // shadow keeps an edge on it in the moment the two tones meet.
        const Rect thumb{x - 17 - stretch * 0.5f, pill.cy() - 17, 34 + stretch, 34};
        list.shadow({thumb.x, thumb.y + 2, thumb.w, thumb.h}, 17, 8, Color::rgb(0x000000, 0.45f));
        list.rounded_rect(thumb, 17, gfx::mix(kInk, kDeep, on));

        // "Off" has faded before "On" appears: the two words never overlap.
        const ui::Fonts &fonts = context_.fonts;
        const float alpha = focused ? 0.95f : 0.66f;
        const float baseline = centred(r.cy(), 24);
        ui::text(list, fonts.regular, "On", pill.x - 20, baseline, 24,
                 kInk.with_alpha(alpha * tween::clamp01(on * 2.0f - 1.0f)), gfx::Align::right);
        ui::text(list, fonts.regular, "Off", pill.x - 20, baseline, 24,
                 kInk.with_alpha(alpha * tween::clamp01(1.0f - on * 2.0f)), gfx::Align::right);
    }

    // A value that just changed: the old text leaves against the direction
    // of travel while the new one arrives from where the press pointed.
    void draw_changing(gfx::DrawList &list, const ui::FontRef &font, const Control &state,
                       const char *before, const char *now, float x, float baseline, float size,
                       Color color, gfx::Align align) const
    {
        if (!state.change.running)
        {
            ui::text(list, font, now, x, baseline, size, color, align);
            return;
        }
        const float t = state.change.progress();
        const float reach = 30.0f * state.direction * travel();
        ui::text(list, font, before, x - reach * tween::cubic_in(tween::clamp01(t * 2.2f)),
                 baseline, size, color.with_alpha(1.0f - tween::smoothstep(t * 2.2f)), align);
        const float arrive = tween::clamp01((t - 0.2f) / 0.8f);
        ui::text(list, font, now, x + reach * (1.0f - tween::quint_out(arrive)), baseline, size,
                 color.with_alpha(tween::smoothstep(arrive)), align);
    }

    // Stepper: the value between two chevrons. A chevron dims when there is
    // nothing further that way and nudges outward on the press.
    void draw_stepper(gfx::DrawList &list, Field field, const Rect &r, bool focused) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const Control &state = control(field);
        const float right = control_right(r);
        const float cx = right - kStepperW * 0.5f;
        const float cy = r.cy();
        if (focused)
            list.rounded_rect({cx - kStepperW * 0.5f + 34, cy - 24, kStepperW - 68, 48}, 14,
                              kDeep.with_alpha(0.35f));
        draw_changing(list, fonts.semibold, state, choice_text(field, state.previous),
                      choice_text(field, state.seen), cx, centred(cy, 28), 28,
                      focused ? kInk : kInk.with_alpha(0.84f), gfx::Align::center);

        const float nudge = 7.0f * state.press.value * travel();
        const float rest = focused ? 1.0f : 0.5f;
        const float left_alpha = state.seen > 0 ? rest : 0.16f;
        const float right_alpha = state.seen < limit(field) ? rest : 0.16f;
        const Color tint = focused ? kAccent : kInk;
        // The triangles are glyphs of the monospaced face: sharp at any size.
        ui::text(list, fonts.mono, "\xE2\x97\x80",
                 cx - kStepperW * 0.5f + 12 - (state.direction < 0.0f ? nudge : 0.0f),
                 centred(cy, 32) - 1, 32, tint.with_alpha(left_alpha), gfx::Align::center);
        ui::text(list, fonts.mono, "\xE2\x96\xB6",
                 cx + kStepperW * 0.5f - 12 + (state.direction > 0.0f ? nudge : 0.0f),
                 centred(cy, 32) - 1, 32, tint.with_alpha(right_alpha), gfx::Align::center);
    }

    void draw_action(gfx::DrawList &list, const Rect &r, bool focused) const
    {
        const float right = control_right(r);
        const float cy = r.cy();
        const Color ink = focused ? kDanger : kInk.with_alpha(0.6f);
        ui::text(list, context_.fonts.regular, "Asks before it does anything", right - 34,
                 centred(cy, 24), 24, kInk.with_alpha(focused ? 0.8f : 0.55f), gfx::Align::right);
        list.line(right - 12, cy - 9, right - 3, cy, 3.0f, ink);
        list.line(right - 12, cy + 9, right - 3, cy, 3.0f, ink);
    }

    // The frame every preview sits in; returns the rectangle of the card.
    Rect draw_card(gfx::DrawList &list, int index, float dy, const char *title) const
    {
        const Rect last = row_rect(index, kCategories[index].count - 1);
        const float top = last.y + last.h + 24.0f;
        const Rect card{last.x, top + dy, last.w, kFooterY - 24.0f - top};
        list.bordered_rect(card, kRadius, kDeep.with_alpha(0.32f), 1.5f, kInk.with_alpha(0.06f));
        ui::text(list, context_.fonts.semibold, title, card.x + kRowPad, card.y + 42, 16,
                 kInk.with_alpha(0.5f), gfx::Align::left, 3.5f);
        return card;
    }

    // Audio: what each bus really plays at. The mixer's curve is perceptual
    // (Settings::gain), so the meters are in decibels, not in slider steps.
    void draw_levels(gfx::DrawList &list, float dy) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const Rect card = draw_card(list, 0, dy, "OUTPUT LEVEL");
        const char *names[] = {"Music", "Effects", "Menu"};
        constexpr int kSegments = 40;
        const float x0 = card.x + 200;
        const float x1 = card.x + card.w - 180;
        const float pitch = (x1 - x0) / as_float(kSegments);
        for (int i = 0; i < 3; ++i)
        {
            const Control &state = controls_[static_cast<std::size_t>(i)];
            const float cy = card.y + 86 + as_float(i) * 46;
            ui::text(list, fonts.regular, names[i], card.x + kRowPad, centred(cy, 22), 22,
                     kInk.with_alpha(0.72f));
            // 0 dB at ten, -48 dB at the left end of the meter.
            const float step = std::max(state.level.value, 0.0f) / 10.0f;
            const float db = step > 0.001f ? 40.0f * std::log10(step) : -96.0f;
            const float lit = tween::clamp01(1.0f + db / 48.0f) * as_float(kSegments);
            for (int s = 0; s < kSegments; ++s)
            {
                const float amount = tween::clamp01(lit - as_float(s));
                const Color on = gfx::mix(gfx::mix(kAccent, kSurface, 0.4f), kAccent,
                                          as_float(s) / as_float(kSegments));
                list.rounded_rect({x0 + as_float(s) * pitch, cy - 8, pitch - 4, 16}, 3,
                                  gfx::mix(kInk.with_alpha(0.1f), on, amount));
            }
            // Digits in the monospaced face, the unit in the reading face.
            const float right = card.x + card.w - kRowPad;
            if (state.seen == 0)
            {
                ui::text(list, fonts.regular, "Muted", right, centred(cy, 22), 22,
                         kInk.with_alpha(0.6f), gfx::Align::right);
                continue;
            }
            char text[16];
            std::snprintf(text, sizeof(text), "%.0f",
                          static_cast<double>(40.0f * std::log10(as_float(state.seen) / 10.0f)));
            ui::text(list, fonts.regular, "dB", right, centred(cy, 22), 22, kInk.with_alpha(0.6f),
                     gfx::Align::right);
            ui::text(list, fonts.mono, text, right - 36, centred(cy, 22), 22, kInk.with_alpha(0.9f),
                     gfx::Align::right);
        }
    }

    // Display: the chosen picture size in numbers, and the hint size shown on
    // a sample hint row that grows and shrinks with the setting.
    void draw_display_preview(gfx::DrawList &list, float dy) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const Rect card = draw_card(list, 1, dy, "OUTPUT");
        const Control &state = control(Field::resolution);
        const auto size_text = [](int index, char *out, std::size_t size)
        {
            const Settings::Resolution &mode =
                Settings::kResolutions[std::clamp(index, 0, Settings::kResolutionCount - 1)];
            std::snprintf(out, size, "%d \xC3\x97 %d", mode.width, mode.height);
        };
        char before[24];
        char now[24];
        size_text(state.previous, before, sizeof(before));
        size_text(state.seen, now, sizeof(now));
        draw_changing(list, fonts.mono, state, before, now, card.x + kRowPad, card.y + 118, 40,
                      kInk, gfx::Align::left);
        // Honest about when it applies: the running picture is still the one
        // the app started with.
        const bool pending = state.seen != start_resolution_;
        list.circle(card.x + kRowPad + 6, card.y + 164, 5,
                    pending ? kAccent : kInk.with_alpha(0.3f));
        ui::text(list, fonts.regular,
                 pending ? "Restart the app to switch to this resolution"
                         : "This is the resolution in use now",
                 card.x + kRowPad + 24, card.y + 172, 22, kInk.with_alpha(pending ? 0.9f : 0.6f));

        const float x = card.x + card.w * 0.5f + 40;
        list.rounded_rect({x - 40, card.y + 28, 1.5f, card.h - 56}, 0, kInk.with_alpha(0.08f));
        ui::text(list, fonts.semibold, "HINT PREVIEW", x, card.y + 42, 16, kInk.with_alpha(0.5f),
                 gfx::Align::left, 3.5f);
        const ui::Hint hints[] = {{confirm_button(), "Select"}, {back_button(), "Back"}};
        ui::HintLayout layout = hint_layout();
        layout.cy = card.y + 132;
        ui::draw_hints(list, fonts, ui::GlyphStyle::dark(), hints, 2, x, false, layout);
    }

    // Controller: a pad drawn from shapes that reacts to all three switches.
    void draw_pad_preview(gfx::DrawList &list, float dy) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const Rect card = draw_card(list, 2, dy, "YOUR CONTROLLER");
        const float cx = card.x + 300;
        const float cy = card.cy() + 6;
        const float swap = tween::clamp01(control(Field::swap).level.value);
        const float light = tween::clamp01(control(Field::light_bar).level.value);
        const float rumble = tween::clamp01(control(Field::vibration).level.value);
        const Color body = Color::rgb(0x2a444e);
        const Color dark = Color::rgb(0x12232a);

        // Vibration: waves beside the grips, which swell when it is switched on.
        if (rumble > 0.01f)
        {
            const float swell = 0.45f + 0.55f * rumble_show_.value;
            for (int i = 0; i < 2; ++i)
            {
                const float radius = 74.0f + as_float(i) * 14.0f + 6.0f * rumble_show_.value;
                const Color wave = kAccent.with_alpha(rumble * swell * (i == 0 ? 0.9f : 0.5f));
                list.arc(cx - 96, cy + 22, radius, 3, kPi * 1.3f, kPi * 0.4f, wave);
                list.arc(cx + 96, cy + 22, radius, 3, kPi * 0.3f, kPi * 0.4f, wave);
            }
        }
        // Grips, then the body across them.
        list.rounded_rect({cx - 136, cy - 52, 72, 126}, 34, body);
        list.rounded_rect({cx + 64, cy - 52, 72, 126}, 34, body);
        list.rounded_rect({cx - 136, cy - 52, 272, 84}, 34, body);
        // Touchpad with the light bar along its sides.
        const Rect pad{cx - 48, cy - 52, 96, 50};
        const Color bar = gfx::mix(kInk.with_alpha(0.14f), kAccent, light);
        list.glow({pad.x - 4, pad.y + 6, pad.w + 8, pad.h - 10}, 10, 14,
                  kAccent.with_alpha(0.5f * light));
        list.rounded_rect({pad.x - 4, pad.y, pad.w + 8, pad.h + 2}, 12, bar);
        list.rounded_rect(pad, 10, dark);
        // D-pad and sticks.
        list.rounded_rect({cx - 98, cy - 28, 10, 34}, 3, dark);
        list.rounded_rect({cx - 110, cy - 16, 34, 10}, 3, dark);
        list.circle(cx - 44, cy + 26, 17, dark);
        list.circle(cx + 44, cy + 26, 17, dark);
        list.ring(cx - 44, cy + 26, 17, 2, kInk.with_alpha(0.12f));
        list.ring(cx + 44, cy + 26, 17, 2, kInk.with_alpha(0.12f));
        // Face buttons: Cross below, Circle right. The one that confirms is lit.
        const float fx = cx + 93;
        const float fy = cy - 11;
        list.circle(fx, fy - 17, 7.5f, dark);
        list.circle(fx - 17, fy, 7.5f, dark);
        list.circle(fx, fy + 17, 7.5f, gfx::mix(kAccent, dark, swap));
        list.circle(fx + 17, fy, 7.5f, gfx::mix(dark, kAccent, swap));

        // The legend: the two glyphs trade places when the buttons are swapped.
        const float x = card.x + 620;
        const float top = card.y + 96;
        const float bottom = card.y + 154;
        const ui::GlyphStyle style = ui::GlyphStyle::dark();
        ui::text(list, fonts.regular, "Confirm", x + 60, centred(top, 26), 26, kInk);
        ui::text(list, fonts.regular, "Back", x + 60, centred(bottom, 26), 26, kInk);
        // They pass on an arc, so they go around each other instead of through.
        const float arc = 16.0f * std::sin(swap * kPi) * travel();
        ui::draw_button(list, fonts, style, ui::Button::cross, x + arc,
                        tween::lerp(top, bottom, swap), 40);
        ui::draw_button(list, fonts, style, ui::Button::circle, x - arc,
                        tween::lerp(bottom, top, swap), 40);

        const float sx = card.x + 900;
        draw_status(list, sx, top, "Vibration", rumble);
        draw_status(list, sx, bottom, "Light bar", light);
    }

    // A lamp and a name: lit when the feature is on.
    void draw_status(gfx::DrawList &list, float x, float cy, const char *name, float on) const
    {
        list.glow({x - 7, cy - 7, 14, 14}, 7, 10, kAccent.with_alpha(0.5f * on));
        list.circle(x, cy, 7, gfx::mix(Color::rgb(0x3a4f58), kAccent, on));
        ui::text(list, context_.fonts.regular, name, x + 26, centred(cy, 26), 26,
                 kInk.with_alpha(0.6f + 0.4f * on));
    }

    // Accessibility: what "Reduce motion" does, said plainly.
    void draw_motion_card(gfx::DrawList &list, float dy) const
    {
        const Rect card = draw_card(list, 3, dy, "WITH REDUCE MOTION ON");
        const float on = tween::clamp01(control(Field::reduce_motion).level.value);
        for (int i = 0; i < 4; ++i)
        {
            const float y = card.y + 106 + as_float(i) * 50;
            list.circle(card.x + kRowPad + 6, y - 9, 5,
                        gfx::mix(kInk.with_alpha(0.3f), kAccent, on));
            ui::text(list, context_.fonts.regular, kMotionNotes[i], card.x + kRowPad + 30, y, 25,
                     kInk.with_alpha(0.68f + 0.26f * on));
        }
    }

    // About: four facts as tiles. The frame rate is the live number.
    void draw_facts(gfx::DrawList &list, float dy) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const app::Telemetry &telemetry = context_.telemetry;
        const bool measured = telemetry.fps > 0.0f;
        char designs[32];
        char rate[24];
        char frame[32];
        std::snprintf(designs, sizeof(designs), "%zu in this build",
                      app::concept_registry().size());
        std::snprintf(rate, sizeof(rate), "%.1f fps", static_cast<double>(telemetry.fps));
        std::snprintf(frame, sizeof(frame), "%.2f ms a frame",
                      static_cast<double>(telemetry.average_ms));
        const char *labels[] = {"APPLICATION", "DESIGNS", "RENDERER", "FRAME RATE"};
        const char *values[] = {"ps5-homebrew-ui", designs, "Rendered with OpenGL 4.6",
                                measured ? rate : "Measuring"};
        const char *notes[] = {"A reference for console-grade interfaces",
                               "Switch between them with L1 and R1",
                               "Every shape on screen is drawn by one shader",
                               measured ? frame : "The first second is still being counted"};
        const Rect area = row_rect(0, 0);
        const float w = (area.w - kTileGap) * 0.5f;
        for (int i = 0; i < 4; ++i)
        {
            const Rect tile{area.x + as_float(i % 2) * (w + kTileGap),
                            kRowsY + as_float(i / 2) * (kTileH + kTileGap) + dy, w, kTileH};
            // Numbers that change every second sit in the monospaced face.
            const bool live = i == 3 && measured;
            list.bordered_rect(tile, kRadius, kDeep.with_alpha(0.32f), 1.5f,
                               kInk.with_alpha(0.06f));
            ui::text(list, fonts.semibold, labels[i], tile.x + kRowPad, tile.y + 46, 16,
                     kInk.with_alpha(0.5f), gfx::Align::left, 3.5f);
            ui::text(list, live ? fonts.mono : fonts.semibold, values[i], tile.x + kRowPad,
                     tile.y + 98, 34, kInk);
            ui::text(list, live ? fonts.mono : fonts.regular, notes[i], tile.x + kRowPad,
                     tile.y + 138, 22, kInk.with_alpha(0.6f));
        }
    }

    // The description of whatever is focused, cross-fading as the focus moves.
    void draw_footer(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = tween::stagger(age_, 7, 0.07f, 0.5f);
        const float x = kPanel.x + kPanelPad + kRowPad;
        const float cy = kFooterY + 42;
        const float width = kPanel.w - 2 * (kPanelPad + kRowPad) - 44;
        list.push_opacity(in);
        list.ring(x + 13, cy, 13, 2, kAccent.with_alpha(0.8f));
        list.circle(x + 13, cy - 5.5f, 1.8f, kAccent);
        list.line(x + 13, cy - 0.5f, x + 13, cy + 6, 2.4f, kAccent);
        const float t = footer_.running ? footer_.progress() : 1.0f;
        if (t < 1.0f)
            ui::text(list, fonts.regular, fonts.regular.font->fit(footer_previous_, 24, width),
                     x + 44, centred(cy, 24), 24,
                     kInk.with_alpha(0.74f * (1.0f - tween::smoothstep(t * 2.0f))));
        const float arrive = tween::smoothstep((t - 0.45f) / 0.55f);
        ui::text(list, fonts.regular, fonts.regular.font->fit(footer_shown_, 24, width),
                 x + 44 + 12.0f * (1.0f - arrive) * travel(), centred(cy, 24), 24,
                 kInk.with_alpha(0.74f * arrive));
        list.pop_opacity();
    }

    // Glyph and text sizes of the hint row follow the "Hint size" setting,
    // eased, so the row grows instead of snapping.
    ui::HintLayout hint_layout() const
    {
        ui::HintLayout layout;
        const float scale = hint_scale_.value;
        layout.size = 32.0f + 8.0f * scale;
        layout.text_size = 22.0f + 4.0f * scale;
        layout.item_gap = 36.0f + 8.0f * scale;
        return layout;
    }

    // The dialog's hints live with the dialog, above the dimmed screen.
    static gfx::DrawList &hint_list(app::Frame &frame, HintMode mode)
    {
        return mode == HintMode::dialog ? frame.overlay : frame.scene;
    }

    // The hints of one focus context, context-specific ones first so that
    // "Back" and "Category" keep their place at the right end of the row.
    int build_hints(HintMode mode, ui::Hint *hints) const
    {
        const ui::Hint category{ui::Button::l2, "Category", ui::Button::r2};
        const ui::Hint back{back_button(), "Back"};
        int count = 0;
        switch (mode)
        {
        case HintMode::rail:
            hints[count++] = {ui::Button::dpad, "Choose"};
            hints[count++] = {confirm_button(), "Open"};
            break;
        case HintMode::slider:
            hints[count++] = {ui::Button::dpad, "Adjust"};
            hints[count++] = back;
            break;
        case HintMode::stepper:
            hints[count++] = {ui::Button::dpad, "Change"};
            hints[count++] = back;
            break;
        case HintMode::toggle_off:
            hints[count++] = {confirm_button(), "Turn on"};
            hints[count++] = back;
            break;
        case HintMode::toggle_on:
            hints[count++] = {confirm_button(), "Turn off"};
            hints[count++] = back;
            break;
        case HintMode::action:
            hints[count++] = {confirm_button(), "Reset"};
            hints[count++] = back;
            break;
        case HintMode::dialog:
            hints[count++] = {confirm_button(), "Choose"};
            hints[count++] = {back_button(), "Cancel"};
            break;
        }
        if (mode != HintMode::dialog)
            hints[count++] = category;
        return count;
    }

    // The hint row. When the context changes, the hints both rows share stay
    // where they are; only the ones that differ fade out and then in. The row
    // is right-aligned and changes width with its labels, so two overlapping
    // rows would be unreadable and a full fade would blink on every switch.
    void draw_hints(app::Frame &frame) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const ui::GlyphStyle style = ui::GlyphStyle::dark();
        const ui::HintLayout layout = hint_layout();
        const float in = tween::stagger(age_, 9, 0.07f, 0.5f);
        gfx::DrawList &list = hint_list(frame, hints_shown_);
        gfx::DrawList &old_list = hint_list(frame, hints_previous_);
        ui::Hint now[4];
        ui::Hint before[4];
        const int now_count = build_hints(hints_shown_, now);
        const int before_count = hints_.running ? build_hints(hints_previous_, before) : 0;

        int shared = 0;
        if (hints_.running && &list == &old_list)
        {
            while (shared < std::min(now_count, before_count))
            {
                const ui::Hint &a = now[now_count - 1 - shared];
                const ui::Hint &b = before[before_count - 1 - shared];
                if (a.button != b.button || a.second != b.second || std::strcmp(a.label, b.label))
                    break;
                ++shared;
            }
        }
        const int kept = hints_.running ? shared : now_count;
        float x = gfx::kVirtualWidth - kMargin;
        list.push_opacity(in);
        const float kept_width =
            ui::draw_hints(list, fonts, style, now + (now_count - kept), kept, x, true, layout);
        list.pop_opacity();
        if (!hints_.running)
            return;
        if (kept > 0)
            x -= kept_width + layout.item_gap;
        const float t = hints_.progress();
        old_list.push_opacity(in * (1.0f - tween::smoothstep(t * 2.0f)));
        ui::draw_hints(old_list, fonts, style, before, before_count - shared, x, true, layout);
        old_list.pop_opacity();
        list.push_opacity(in * tween::smoothstep(t * 2.0f - 1.0f));
        ui::draw_hints(list, fonts, style, now, now_count - shared, x, true, layout);
        list.pop_opacity();
    }

    static Rect dialog_button(float position)
    {
        constexpr float kPad = 48.0f;
        constexpr float kGap = 24.0f;
        const float w = (kDialog.w - 2 * kPad - kGap) * 0.5f;
        return {kDialog.x + kPad + position * (w + kGap), kDialog.y + kDialog.h - kPad - 68, w, 68};
    }

    void draw_dialog(gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float t = dialog_.value;
        list.push_opacity(tween::clamp01(t * 1.5f));
        list.push_transform(1.0f - 0.06f * (1.0f - t) * travel(), 960, 540, 0,
                            26.0f * (1.0f - t) * travel());
        list.shadow({kDialog.x, kDialog.y + 24, kDialog.w, kDialog.h}, kPanelRadius, 60,
                    Color::rgb(0x000000, 0.55f));
        // Frosted panel: the blurred screen, a tint, then a hairline of light.
        list.glass(glass, kDialog, kPanelRadius, Color::rgb(0xffffff));
        list.gradient_rect(kDialog, kPanelRadius,
                           gfx::mix(kSurface, kAccent, 0.08f).with_alpha(0.5f),
                           kDeep.with_alpha(0.62f));
        list.bordered_rect(kDialog, kPanelRadius, kClear, 1.5f, kInk.with_alpha(0.24f));

        const float x = kDialog.x + 48;
        list.ring(x + 24, kDialog.y + 78, 24, 3, kDanger);
        list.line(x + 24, kDialog.y + 67, x + 24, kDialog.y + 81, 3.5f, kDanger);
        list.circle(x + 24, kDialog.y + 90, 2.4f, kDanger);
        ui::text(list, fonts.semibold, "Reset all settings?", x + 68, kDialog.y + 92, 40, kInk);
        ui::paragraph(list, fonts.regular,
                      "Volumes, display and controller options go back to how the app first "
                      "started. This cannot be undone.",
                      x, kDialog.y + 164, 26, kDialog.w - 96, 38, kInk.with_alpha(0.86f), 2);

        // Two buttons, one highlight that springs between them and turns
        // from white to the warning colour as it reaches "Reset".
        const float position = choice_position_.value;
        Rect lit = dialog_button(position);
        lit.x += ui::shake(refuse_.value, clock_, 8.0f, 9.0f);
        const Color fill = gfx::mix(kInk, kDanger, tween::clamp01(position));
        list.glow(lit, 34, 16, fill.with_alpha(0.3f));
        list.rounded_rect(lit, 34, fill);
        const char *names[] = {"Cancel", "Reset"};
        for (int i = 0; i < 2; ++i)
        {
            const Rect r = dialog_button(as_float(i));
            const bool focused = i == choice_;
            if (!focused)
                list.bordered_rect(r, 34, kInk.with_alpha(0.06f), 1.5f, kInk.with_alpha(0.2f));
            ui::text(list, fonts.semibold, names[i], r.cx(), centred(r.cy(), 28), 28,
                     focused ? kDeep : (i == 1 ? kDanger : kInk.with_alpha(0.9f)),
                     gfx::Align::center);
        }
        list.pop_transform();
        list.pop_opacity();
    }

    // The toast: a pill that drops in under the top edge, with a check mark
    // that draws itself. It lives in the overlay so it also shows over the
    // dialog.
    void draw_toast(gfx::DrawList &list) const
    {
        const float t = toast_.value;
        if (t <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const float w = 84.0f + fonts.semibold.measure(toast_message_, 24);
        const Rect pill{gfx::kVirtualWidth - kMargin - w, 92.0f - 70.0f * (1.0f - t) * travel(), w,
                        60};
        list.push_opacity(tween::clamp01(t * 1.3f));
        list.shadow({pill.x, pill.y + 12, pill.w, pill.h}, 30, 30, Color::rgb(0x000000, 0.5f));
        list.bordered_rect(pill, 30, gfx::mix(kSurface, kAccent, 0.1f).with_alpha(0.97f), 1.5f,
                           kAccent.with_alpha(0.55f));
        const float cx = pill.x + 32;
        const float cy = pill.cy();
        list.circle(cx, cy, 17, kAccent);
        // Two strokes, drawn one after the other.
        const float stroke = tween::clamp01(toast_age_ / 0.28f);
        const float first = tween::clamp01(stroke / 0.4f);
        const float second = tween::clamp01((stroke - 0.4f) / 0.6f);
        list.line(cx - 7.5f, cy + 0.5f, cx - 7.5f + 5.0f * first, cy + 0.5f + 5.0f * first, 3.2f,
                  kDeep);
        if (second > 0.0f)
            list.line(cx - 2.5f, cy + 5.5f, cx - 2.5f + 10.0f * second, cy + 5.5f - 11.0f * second,
                      3.2f, kDeep);
        ui::text(list, fonts.semibold, toast_message_, pill.x + 62, centred(cy, 24), 24, kInk);
        list.pop_opacity();
    }

    app::Context &context_;
    int start_resolution_; // the resolution the app is running at
    float age_ = 0.0f;     // seconds since enter(): drives the entrance
    float clock_ = 0.0f;   // free-running: refusal shakes
    float drift_ = 0.0f;   // backdrop and idle glow; stops under "Reduce motion"

    Column column_ = Column::rail;
    int category_ = 0;
    std::array<int, kCategoryCount> row_{}; // the last focused row of each category
    ui::SpringRect focus_;                  // the one highlight of the screen
    tween::Spring marker_;                  // y of the active category's plate
    ui::Pulse refuse_;

    int previous_ = 0;              // the category whose rows are fading out
    float change_direction_ = 1.0f; // +1 when the new category is further down
    tween::Timer leave_;
    float content_age_ = 0.0f; // entrance clock of the rows shown now

    std::array<Control, kFieldCount> controls_;
    tween::Spring hint_scale_; // 0 small .. 2 large, eased
    ui::Pulse rumble_show_;    // the pad preview's waves when vibration is turned on

    const char *footer_shown_ = "";
    const char *footer_previous_ = "";
    tween::Timer footer_;
    HintMode hints_shown_ = HintMode::rail;
    HintMode hints_previous_ = HintMode::rail;
    tween::Timer hints_;

    bool dialog_open_ = false;
    tween::Spring dialog_;
    int choice_ = 0; // 0 Cancel, 1 Reset
    tween::Spring choice_position_;

    float save_wait_ = 0.0f;  // counts down from the last change to the toast
    float toast_left_ = 0.0f; // seconds the toast stays up
    float toast_age_ = 10.0f; // seconds since it appeared: draws the check mark
    const char *pending_message_ = "Saved";
    const char *toast_message_ = "Saved";
    tween::Spring toast_;
};

} // namespace

std::unique_ptr<app::Concept> make_settings(app::Context &context)
{
    return std::make_unique<SettingsDesign>(context);
}

} // namespace hui::concepts
