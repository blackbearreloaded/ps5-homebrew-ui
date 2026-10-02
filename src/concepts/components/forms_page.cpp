// ps5-homebrew-ui - Component Library page: Form, Stepper, ChoicePicker and TextField.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A settings screen in two columns. On the left a ui::Form with every row
// kind, long enough to scroll. On the right a panel that shows what the form's
// values mean (a small screen that dims with Brightness, volume meters, the
// current choices) and the three parts of the form that also work on their
// own: a Stepper, a ChoicePicker and a TextField.
//
// The one design problem of such a screen is left and right: most rows use
// them to edit, so they cannot also mean "go to the other column". The rule
// here: Triangle always changes column, and left / right change column only
// where the focused thing has no use for them (Form::uses_horizontal()).

#include "concepts/components/page.hpp"

#include "ui/components/choice.hpp"
#include "ui/components/form.hpp"
#include "ui/components/stepper.hpp"
#include "ui/components/text_field.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace hui::concepts::gallery
{

namespace
{

using gfx::Color;
using gfx::Rect;

// ---- layout -----------------------------------------------------------------

constexpr Rect kFormBounds{96.0f, 290.0f, 900.0f, 590.0f};
constexpr Rect kFormBoundsWithHelp{96.0f, 290.0f, 900.0f, 506.0f};
constexpr Rect kHelp{96.0f, 816.0f, 900.0f, 64.0f};
constexpr Rect kPanel{1044.0f, 290.0f, 780.0f, 590.0f};
constexpr float kInset = 30.0f;
constexpr float kLeft = kPanel.x + kInset;
constexpr float kWidth = kPanel.w - 2.0f * kInset;
constexpr Rect kScreen{kLeft + 6.0f, 352.0f, 300.0f, 169.0f};
constexpr float kMeters = kLeft + 348.0f;
constexpr float kRule = 548.0f;
constexpr float kControl = kLeft + 250.0f; // where the standalone controls start
constexpr Rect kStepper{kControl, 566.0f, 300.0f, 56.0f};
constexpr Rect kPicker{kControl, 636.0f, kWidth - 250.0f, 56.0f};
constexpr float kField = 708.0f;
constexpr float kCaption = 268.0f;
constexpr float kStatus = 930.0f;

// ---- content ----------------------------------------------------------------

enum Id
{
    kHdr = 1,
    kResolution,
    kRefresh,
    kBrightness,
    kScreenSize,
    kMaster,
    kMusic,
    kEffects,
    kOutput,
    kNight,
    kVibration,
    kDeadZone,
    kInvert,
    kMotion,
    kProfile,
    kStorage,
    kRestore,
    kSignOut,
    kErase,
};

enum Part
{
    kPartStepper,
    kPartPicker,
    kPartField,
    kParts,
};

constexpr const char *kPartNames[kParts] = {"Players", "Difficulty", ""};

// Typed one letter at a time when the text field is confirmed. The last one
// is longer than the field allows, to show the refusal and the error line.
constexpr const char *kNames[] = {"Marlowe", "Quillon", "Nightjar of the North"};
constexpr int kNameCount = static_cast<int>(std::size(kNames));
constexpr int kNameLimit = 12;
constexpr float kTypeEvery = 0.085f;

constexpr const char *kVariants[] = {
    "Comfortable, tint highlight",
    "Compact, dividers, in a panel",
    "Wide labels, bar, values at the left",
    "Theme ring, help line, masked field",
};
constexpr int kVariantCount = static_cast<int>(std::size(kVariants));

constexpr ui::Hint kHintsEdit[] = {
    {ui::Button::dpad, "Move, edit"},
    {ui::Button::cross, "Choose"},
    {ui::Button::triangle, "Other column"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kHintsCross[] = {
    {ui::Button::dpad, "Move, other column"},
    {ui::Button::cross, "Choose"},
    {ui::Button::triangle, "Other column"},
    {ui::Button::square, "Variant"},
};

constexpr app::TourStep kTour[] = {
    {0.6f, 0, Direction::left},
    {0.3f, 0, Direction::down},
    {0.3f, 0, Direction::left},
    {0.3f, 0, Direction::down},
    {0.25f, 0, Direction::down},
    {0.25f, 0, Direction::right},
    {0.2f, 0, Direction::right},
    {0.2f, 0, Direction::right},
    {0.7f, action_bit(Action::north), Direction::none, "forms"},
    {0.3f, 0, Direction::right},
    {0.3f, 0, Direction::down},
    {0.3f, 0, Direction::right},
    {0.3f, 0, Direction::down},
    {0.3f, action_bit(Action::confirm)},
    {1.3f, action_bit(Action::north), Direction::none, "forms-panel"},
    {0.3f, 0, Direction::down},
    {0.25f, 0, Direction::down},
    {0.25f, 0, Direction::down},
    {0.3f, action_bit(Action::west)},
    {0.8f, action_bit(Action::west), Direction::none, "forms-compact"},
    {0.8f, action_bit(Action::west), Direction::none, "forms-wide"},
    {0.8f, action_bit(Action::west), Direction::none, "forms-help"},
};

std::string percent(float value)
{
    char text[16];
    std::snprintf(text, sizeof(text), "%d %%", static_cast<int>(std::lround(value * 100.0f)));
    return text;
}

class FormsPage final : public Page
{
  public:
    explicit FormsPage(app::Context &context) : context_(context)
    {
        build_form();

        players_.set_range(1, 4);
        players_.set_value(2);
        players_.format = [](int value)
        { return std::string(value == 1 ? "1 player" : std::to_string(value) + " players"); };
        players_.set_bounds(kStepper);

        difficulty_.set_options({"Story", "Balanced", "Veteran", "Merciless"});
        difficulty_.set_index(1);
        difficulty_.set_bounds(kPicker);

        name_.set_label("Profile name");
        name_.set_placeholder("Confirm to type a name");
        name_.set_helper("A screen opens its keyboard here; this page types for you");

        restyle(ui::default_theme(), false);
        snap_preview();
    }

    const char *title() const override
    {
        return "Forms";
    }
    const char *summary() const override
    {
        return "ui::Form, and its parts on their own: Stepper, ChoicePicker, TextField";
    }
    const char *variant() const override
    {
        return kVariants[variant_];
    }

    void restyle(const ui::Theme &theme, bool reduced_motion) override
    {
        theme_ = theme;
        reduced_ = reduced_motion;
        apply_variant();
    }

    void enter() override
    {
        form_.enter();
        status_.clear();
    }

    void update(const InputFrame &input, float dt, ui::Feedback &feedback) override
    {
        clock_ += dt;
        help_age_ += dt;
        if (input.is_pressed(Action::west))
        {
            variant_ = (variant_ + 1) % kVariantCount;
            apply_variant();
            ui::play_cue(feedback, form_.style, form_.style.sounds.change);
        }
        else if (input.is_pressed(Action::north))
        {
            set_column(1 - column_, feedback);
        }
        else if (column_ == 0)
        {
            update_form(input, feedback);
        }
        else
        {
            update_panel(input, feedback);
        }
        type_name(dt, feedback);

        form_.set_active(column_ == 0);
        players_.set_active(column_ == 1 && part_ == kPartStepper);
        difficulty_.set_active(column_ == 1 && part_ == kPartPicker);
        name_.set_active(column_ == 1 && part_ == kPartField);
        form_.update(dt);
        players_.update(dt);
        difficulty_.update(dt);
        name_.update(dt);
        edge_.update(dt, 9.0f);
        update_preview(dt);
    }

    void draw(ui::Canvas &canvas) const override
    {
        ui::Painter paint(canvas.list, canvas.fonts, theme_, canvas.glass);
        const float nudge = reduced_ ? 0.0f : ui::shake(edge_.value, clock_, 10.0f);
        paint.label("A settings form: every row kind, sections, descriptions",
                    kFormBounds.x + (column_ == 0 ? nudge : 0.0f), kCaption, 20.0f,
                    column_ == 0 ? paint.page_text() : paint.page_text_muted());
        paint.label("What the values mean, and the form's parts on their own",
                    kPanel.x + (column_ == 1 ? nudge : 0.0f), kCaption, 20.0f,
                    column_ == 1 ? paint.page_text() : paint.page_text_muted());

        form_.draw(canvas);
        if (variant_ == 3)
            draw_help(canvas, paint);

        paint.panel(kPanel);
        draw_preview(canvas, paint);
        canvas.list.rounded_rect({kLeft, kRule, kWidth, 1.5f}, 0.0f,
                                 theme_.text_muted.with_alpha(0.22f));
        draw_parts(canvas, paint);

        if (!status_.empty())
            paint.body(ui::fit_body(paint, status_, 22.0f, kFormBounds.w), kPageArea.x, kStatus,
                       22.0f, paint.page_text_muted());
        paint.body(ui::fit_body(paint,
                                "Left / right change column where the row has no use for them",
                                20.0f, kPanel.w),
                   kPanel.x, kStatus, 20.0f, paint.page_text_muted());
    }

    std::span<const ui::Hint> hints() const override
    {
        const bool edits = column_ == 0 ? form_.uses_horizontal() : part_ != kPartField;
        return edits ? std::span<const ui::Hint>(kHintsEdit)
                     : std::span<const ui::Hint>(kHintsCross);
    }
    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    void build_form()
    {
        form_.add_header("Display");
        form_.add_toggle(kHdr, "HDR", true).description =
            "A wider range of light and colour, on televisions that support it";
        form_.add_choice(kResolution, "Resolution", {"1080p", "1440p", "2160p"}, 2).description =
            "The picture is scaled to what the television accepts";
        form_.add_choice(kRefresh, "Refresh rate", {"60 Hz", "120 Hz"}, 0);
        {
            ui::FormRow &row =
                form_.add_slider(kBrightness, "Brightness", 70.0f, 0.0f, 100.0f, 5.0f);
            row.unit = " %";
            row.description = "Raise it until the darkest part of the picture is just visible";
        }
        {
            ui::FormRow &row = form_.add_stepper(kScreenSize, "Screen size", 94, 80, 100, 2);
            row.stepper.set_suffix(" %");
            row.description = "Shrink the picture if the television cuts its edges";
        }

        form_.add_header("Sound");
        {
            ui::FormRow &row =
                form_.add_slider(kMaster, "Master volume", 80.0f, 0.0f, 100.0f, 1.0f);
            row.unit = " %";
            row.description = "Hold a direction: after a moment it moves four steps at a time";
        }
        form_.add_slider(kMusic, "Music", 6.0f, 0.0f, 10.0f, 1.0f);
        {
            // No step: a continuous value, shown through a format of its own.
            ui::FormRow &row = form_.add_slider(kEffects, "Effects", 0.76f, 0.0f, 1.0f);
            row.format = percent;
        }
        form_.add_choice(kOutput, "Output", {"Stereo", "Surround", "Headphones"}, 1);
        form_.add_toggle(kNight, "Night mode", false).description =
            "Softens loud sounds and lifts quiet ones";

        form_.add_header("Controller");
        form_.add_choice(kVibration, "Vibration", {"Off", "Light", "Strong"}, 2);
        form_.add_stepper(kDeadZone, "Stick dead zone", 12, 0, 30, 2).stepper.set_suffix(" %");
        form_.add_toggle(kInvert, "Invert vertical look", false);
        {
            ui::FormRow &row = form_.add_toggle(kMotion, "Motion aiming", false);
            row.disabled = true;
            row.description = "Connect a controller with a motion sensor to use this";
        }

        form_.add_header("Account");
        form_.add_value(kProfile, "Signed in as", "Guest");
        form_.add_value(kStorage, "Saved data", "18.4 GB of 64 GB");
        form_.add_action(kRestore, "Restore defaults").description =
            "Puts every setting on this page back as it was";
        form_.add_action(kSignOut, "Sign out").chevron = true;
        {
            ui::FormRow &row = form_.add_action(kErase, "Erase saved data");
            row.danger = true;
            row.description = "This cannot be undone";
        }
    }

    void restore_defaults()
    {
        // The setters are silent and the form animates to the new values.
        form_.set_toggle(kHdr, true);
        form_.set_choice(kResolution, 2);
        form_.set_choice(kRefresh, 0);
        form_.set_slider(kBrightness, 70.0f);
        form_.set_stepper(kScreenSize, 94);
        form_.set_slider(kMaster, 80.0f);
        form_.set_slider(kMusic, 6.0f);
        form_.set_slider(kEffects, 0.76f);
        form_.set_choice(kOutput, 1);
        form_.set_toggle(kNight, false);
        form_.set_choice(kVibration, 2);
        form_.set_stepper(kDeadZone, 12);
        form_.set_toggle(kInvert, false);
    }

    // Square: the same components with other knobs. Every style starts from
    // its defaults again, so a variant only names what it changes. Values,
    // focus and scroll are not part of a style and survive.
    void apply_variant()
    {
        ui::FormStyle form;
        ui::StepperStyle stepper;
        ui::ChoiceStyle picker;
        ui::TextFieldStyle field;
        field.max_length = kNameLimit;
        Rect bounds = kFormBounds;
        switch (variant_)
        {
        case 1:
            form.compact = true;
            form.dividers = true;
            form.panel = true;
            form.stepper_buttons = ui::StepperButtons::plain;
            form.focus_shift = 0.0f;
            stepper.buttons = ui::StepperButtons::plain;
            stepper.boxed = true;
            picker.dots = true;
            field.counter = false;
            break;
        case 2:
            form.values_right = false;
            form.label_ratio = 0.5f;
            form.control_width = 300.0f;
            form.highlight.kind = ui::HighlightKind::bar;
            form.header_rule = false;
            stepper.boxed = true;
            picker.boxed = false;
            picker.dots = true;
            picker.wrap = false;
            break;
        case 3:
            form.highlight.kind = ui::HighlightKind::ring;
            form.description_inline = false;
            form.focus_values = true;
            form.toggle_text = false;
            form.row_height = 66.0f;
            picker.confirm_cycles = false;
            field.password = true;
            bounds = kFormBoundsWithHelp;
            break;
        default:
            break;
        }
        const auto themed = [this](ui::ComponentStyle &style)
        {
            style.theme = theme_;
            style.reduced_motion = reduced_;
        };
        themed(form);
        themed(stepper);
        themed(picker);
        themed(field);
        form_.style = form;
        players_.style = stepper;
        difficulty_.style = picker;
        name_.style = field;
        form_.set_bounds(bounds);
        name_.set_bounds({kLeft, kField, kWidth, name_.preferred_height()});
    }

    void set_column(int column, ui::Feedback &feedback)
    {
        column_ = column;
        ui::play_cue(feedback, form_.style, form_.style.sounds.move,
                     column_ == 0 ? kFormBounds.cx() : kPanel.cx());
    }

    void update_form(const InputFrame &input, ui::Feedback &feedback)
    {
        const bool sideways = input.nav == Direction::left || input.nav == Direction::right;
        if (sideways && !form_.uses_horizontal())
        {
            if (input.nav == Direction::right)
                set_column(1, feedback);
            else
                ui::refuse(feedback, form_.style, input, edge_, kFormBounds.cx());
            return;
        }
        const ui::Event event = form_.handle(input, feedback);
        if (event == ui::Event::moved)
            help_age_ = 0.0f;
        if (event == ui::Event::none)
            return;
        const ui::FormRow &row = form_.row_at(form_.focus());
        switch (event)
        {
        case ui::Event::changed:
            status_ = "Event::changed  \xC2\xB7  " + row.label + " = " + value_of(row);
            break;
        case ui::Event::activated:
            status_ = "Event::activated  \xC2\xB7  " + row.label;
            if (form_.changed_id() == kRestore)
                restore_defaults();
            break;
        case ui::Event::refused:
            status_ = "Event::refused  \xC2\xB7  " + row.label;
            break;
        case ui::Event::cancelled:
            status_ = "Event::cancelled";
            break;
        default:
            status_ = "Event::moved  \xC2\xB7  " + row.label;
            break;
        }
    }

    std::string value_of(const ui::FormRow &row) const
    {
        switch (row.kind)
        {
        case ui::FormRowKind::toggle:
            return row.on ? "on" : "off";
        case ui::FormRowKind::choice:
            return row.choice.value();
        case ui::FormRowKind::slider:
            return form_.slider_text(row);
        case ui::FormRowKind::stepper:
            return row.stepper.text();
        default:
            return row.text;
        }
    }

    void update_panel(const InputFrame &input, ui::Feedback &feedback)
    {
        const Rect &at = part_ == kPartStepper  ? kStepper
                         : part_ == kPartPicker ? kPicker
                                                : name_.bounds();
        if (input.nav == Direction::up || input.nav == Direction::down)
        {
            const int next = part_ + (input.nav == Direction::down ? 1 : -1);
            if (next < 0 || next >= kParts)
            {
                ui::refuse(feedback, form_.style, input, edge_, at.cx());
                return;
            }
            part_ = next;
            ui::play_cue(feedback, form_.style, form_.style.sounds.move, at.cx());
            return;
        }
        ui::Event event = ui::Event::none;
        if (part_ == kPartStepper)
        {
            event = players_.handle(input, feedback);
            if (event == ui::Event::changed)
                status_ = "Stepper  \xC2\xB7  Event::changed  \xC2\xB7  " + players_.text();
        }
        else if (part_ == kPartPicker)
        {
            event = difficulty_.handle(input, feedback);
            if (event == ui::Event::changed)
                status_ =
                    "ChoicePicker  \xC2\xB7  Event::changed  \xC2\xB7  " + difficulty_.value();
            else if (event == ui::Event::activated)
                status_ = "ChoicePicker  \xC2\xB7  Event::activated (confirm_cycles is off)";
        }
        else
        {
            // The text field has no use for left and right: they change column.
            if (input.nav == Direction::left)
            {
                set_column(0, feedback);
                return;
            }
            if (input.nav == Direction::right)
            {
                ui::refuse(feedback, form_.style, input, edge_, at.cx());
                return;
            }
            event = name_.handle(input, feedback);
            if (event == ui::Event::activated)
            {
                // Where a real screen opens its keyboard, this one starts typing.
                name_.clear();
                name_.set_error("");
                typing_ = kNames[next_name_];
                next_name_ = (next_name_ + 1) % kNameCount;
                type_timer_ = 0.0f;
                status_ = "TextField  \xC2\xB7  Event::activated  \xC2\xB7  typing with insert()";
            }
        }
        if (event == ui::Event::refused)
            status_ = "Event::refused";
        else if (event == ui::Event::cancelled)
            status_ = "Event::cancelled";
    }

    // The stand-in for a keyboard: one insert() per tick, with the field's
    // own type cue. The first character that does not fit ends it.
    void type_name(float dt, ui::Feedback &feedback)
    {
        if (typing_ == nullptr)
            return;
        type_timer_ += dt;
        while (typing_ != nullptr && type_timer_ >= kTypeEvery)
        {
            type_timer_ -= kTypeEvery;
            if (*typing_ == '\0')
            {
                typing_ = nullptr;
            }
            else if (name_.insert(*typing_, feedback) == ui::Event::refused)
            {
                char text[48];
                std::snprintf(text, sizeof(text), "%d characters at most", kNameLimit);
                name_.set_error(text);
                typing_ = nullptr;
            }
            else
            {
                ++typing_;
            }
        }
        form_.set_value_text(kProfile, name_.text().empty() ? "Guest" : name_.text());
    }

    // ---- the live preview ----

    void preview_targets()
    {
        brightness_.target = form_.slider_value(kBrightness) / 100.0f;
        hdr_.target = form_.toggle_value(kHdr) ? 1.0f : 0.0f;
        size_.target = static_cast<float>(form_.stepper_value(kScreenSize)) / 100.0f;
        const float master = form_.slider_value(kMaster) / 100.0f;
        meter_[0].target = master;
        meter_[1].target = master * form_.slider_value(kMusic) / 10.0f;
        meter_[2].target = master * form_.slider_value(kEffects);
    }

    void snap_preview()
    {
        preview_targets();
        brightness_.snap(brightness_.target);
        hdr_.snap(hdr_.target);
        size_.snap(size_.target);
        for (tween::Spring &meter : meter_)
            meter.snap(meter.target);
    }

    void update_preview(float dt)
    {
        preview_targets();
        const float omega = reduced_ ? 60.0f : 14.0f;
        brightness_.update(dt, omega);
        hdr_.update(dt, omega);
        size_.update(dt, omega);
        for (tween::Spring &meter : meter_)
            meter.update(dt, omega);
    }

    void draw_preview(ui::Canvas &canvas, ui::Painter &paint) const
    {
        gfx::DrawList &list = canvas.list;
        paint.label("LIVE PREVIEW", kLeft, 330.0f, 18.0f, theme_.text_muted);

        // A small television: the picture dims with Brightness, gains a bright
        // highlight with HDR and shrinks with Screen size.
        const Rect bezel = kScreen.inset(-6.0f);
        list.rounded_rect(bezel, 10.0f, Color::rgb(0x08090c));
        const float size = size_.value;
        const Rect picture{kScreen.cx() - kScreen.w * size * 0.5f,
                           kScreen.cy() - kScreen.h * size * 0.5f, kScreen.w * size,
                           kScreen.h * size};
        const demo::Item &title = context_.catalog[0];
        if (title.cover != 0)
            // The middle band of the square cover fills the wide picture.
            list.image(title.cover, picture, {0.0f, 0.78f, 1.0f, -0.5625f}, Color::rgb(0xffffff),
                       5.0f);
        else
            list.gradient_rect(picture, 5.0f, title.mid, title.dark);
        const float glare = hdr_.value;
        if (glare > 0.01f)
            list.glow({picture.x + picture.w * 0.7f, picture.y + picture.h * 0.24f, 2.0f, 2.0f},
                      1.0f, 46.0f * size, title.accent.with_alpha(0.75f * glare));
        list.rounded_rect(picture, 5.0f, Color::rgb(0x000000, 0.82f * (1.0f - brightness_.value)));
        if (glare > 0.01f)
        {
            const Rect badge{picture.x + picture.w - 62.0f, picture.y + 10.0f, 52.0f, 26.0f};
            list.rounded_rect(badge, 6.0f, Color::rgb(0xffffff, 0.92f * glare));
            ui::text(list, canvas.fonts.semibold, "HDR", badge.cx(), badge.cy() + 6.5f, 18.0f,
                     Color::rgb(0x101216, glare), gfx::Align::center);
        }
        const std::string mode =
            form_.choice_text(kResolution) + "  \xC2\xB7  " + form_.choice_text(kRefresh);
        ui::text(list, canvas.fonts.semibold, mode, picture.x + 12.0f,
                 picture.y + picture.h - 12.0f, 20.0f, Color::rgb(0xffffff, 0.92f));

        // Volume meters: each bus is the master times its own level.
        constexpr const char *kBus[] = {"Master", "Music", "Effects"};
        const float right = kLeft + kWidth;
        for (int i = 0; i < 3; ++i)
        {
            const float y = 360.0f + 40.0f * static_cast<float>(i);
            paint.label(kBus[i], kMeters, y + 7.0f, 20.0f, theme_.text_muted);
            paint.progress({kMeters + 116.0f, y - 8.0f, right - kMeters - 116.0f, 16.0f},
                           meter_[static_cast<std::size_t>(i)].value);
        }
        const std::string sound =
            form_.choice_text(kOutput) + (form_.toggle_value(kNight) ? ", night mode" : "");
        paint.body(ui::fit_body(paint, sound, 21.0f, right - kMeters), kMeters, 488.0f, 21.0f,
                   theme_.text);
        char pad[96];
        std::snprintf(pad, sizeof(pad), "Vibration %s, dead zone %d %%%s",
                      form_.choice_text(kVibration).c_str(), form_.stepper_value(kDeadZone),
                      form_.toggle_value(kInvert) ? ", inverted" : "");
        paint.body(ui::fit_body(paint, pad, 21.0f, right - kMeters), kMeters, 518.0f, 21.0f,
                   theme_.text_muted);
    }

    void draw_parts(ui::Canvas &canvas, ui::Painter &paint) const
    {
        for (int i = 0; i < kPartField; ++i)
        {
            const Rect &at = i == kPartStepper ? kStepper : kPicker;
            const bool focused = column_ == 1 && part_ == i;
            paint.label(kPartNames[i], kLeft, at.cy() + 8.0f, 24.0f,
                        focused ? theme_.text : theme_.text_muted);
        }
        players_.draw(canvas);
        difficulty_.draw(canvas);
        name_.draw(canvas);
    }

    // Variant 3 turns the inline description off and shows Form::help_text()
    // in a place of the screen's own.
    void draw_help(ui::Canvas &canvas, ui::Painter &paint) const
    {
        const std::string &help = form_.help_text();
        paint.well(kHelp, paint.control_radius(kHelp), theme_.surface_high);
        const float shown = reduced_ ? 1.0f : tween::cubic_out(help_age_ / 0.25f);
        canvas.list.push_opacity(shown);
        paint.body(ui::fit_body(paint, help.empty() ? "This row needs no explanation" : help, 22.0f,
                                kHelp.w - 48.0f),
                   kHelp.x + 24.0f, kHelp.cy() + 8.0f, 22.0f,
                   help.empty() ? theme_.text_muted : theme_.text);
        canvas.list.pop_opacity();
    }

    app::Context &context_;
    ui::Theme theme_ = ui::default_theme();
    bool reduced_ = false;
    ui::Form form_;
    ui::Stepper players_;
    ui::ChoicePicker difficulty_;
    ui::TextField name_;
    int column_ = 0;
    int part_ = kPartStepper;
    int variant_ = 0;
    int next_name_ = 0;
    const char *typing_ = nullptr; // the rest of the name being typed
    float type_timer_ = 0.0f;
    float clock_ = 0.0f;
    float help_age_ = 10.0f;
    ui::Pulse edge_;
    std::string status_;
    tween::Spring brightness_, hdr_, size_;
    tween::Spring meter_[3];
};

} // namespace

std::unique_ptr<Page> make_forms_page(app::Context &context)
{
    return std::make_unique<FormsPage>(context);
}

} // namespace hui::concepts::gallery
