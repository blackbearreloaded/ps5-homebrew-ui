// ps5-homebrew-ui - Tests: Form, Stepper, ChoicePicker and TextField behaviour.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "component_fixture.hpp"
#include "ui/components/choice.hpp"
#include "ui/components/form.hpp"
#include "ui/components/stepper.hpp"
#include "ui/components/text_field.hpp"

#include <gtest/gtest.h>

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;
using hui::ui::ChoicePicker;
using hui::ui::Event;
using hui::ui::Form;
using hui::ui::Stepper;
using hui::ui::TextField;

enum Id
{
    kSwitch = 1,
    kChoice,
    kSlider,
    kStepper,
    kValue,
    kAction,
    kLocked,
    kFine,
};
constexpr int kValueRow = 6; // the index of the read-only row

class ComponentsForms : public hui::testing::ComponentFixture
{
  protected:
    ComponentsForms()
    {
        form_.add_header("Section");
        form_.add_toggle(kSwitch, "Switch", false).description = "What the switch does";
        form_.add_choice(kChoice, "Choice", {"One", "Two", "Three"}, 0);
        form_.add_slider(kSlider, "Slider", 50.0f, 0.0f, 100.0f, 10.0f).unit = " %";
        form_.add_stepper(kStepper, "Stepper", 4, 0, 10, 2);
        form_.add_header("More");
        form_.add_value(kValue, "Value", "Read only");
        form_.add_action(kAction, "Action");
        form_.add_toggle(kLocked, "Locked", true).disabled = true;
        form_.add_slider(kFine, "Fine", 0.0f, 0.0f, 100.0f, 1.0f);
        form_.set_bounds({100.0f, 100.0f, 800.0f, 260.0f});
    }

    // One input to a component, then half a second of animation.
    template <typename Component> Event send(Component &component, const hui::InputFrame &input)
    {
        feedback_.clear();
        const Event event = component.handle(input, feedback_);
        for (int i = 0; i < 30; ++i)
            component.update(kFrame);
        return event;
    }
    Event send(const hui::InputFrame &input)
    {
        return send(form_, input);
    }
    static hui::InputFrame repeat(Direction direction)
    {
        hui::InputFrame input = nav(direction);
        input.nav_repeat = true;
        return input;
    }
    // The pitch the last input asked this cue to play at; 0 when it did not.
    float pitch(Cue cue) const
    {
        for (const hui::audio::CueEvent &event : feedback_.cues)
        {
            if (event.cue == cue)
                return event.pitch;
        }
        return 0.0f;
    }

    Form form_;
};

TEST_F(ComponentsForms, FocusSkipsHeadersAndReadOnlyRows)
{
    EXPECT_EQ(form_.focus_id(), kSwitch);
    EXPECT_EQ(send(nav(Direction::up)), Event::refused);
    for (int i = 0; i < 3; ++i)
        EXPECT_EQ(send(nav(Direction::down)), Event::moved);
    EXPECT_TRUE(asked(Cue::focus));
    EXPECT_EQ(form_.focus_id(), kStepper);
    EXPECT_EQ(send(nav(Direction::down)), Event::moved);
    EXPECT_EQ(form_.focus_id(), kAction);

    // The knob that lets read-only rows take the focus: they refuse confirm.
    form_.style.focus_values = true;
    EXPECT_EQ(send(nav(Direction::up)), Event::moved);
    EXPECT_EQ(form_.focus(), kValueRow);
    EXPECT_EQ(send(press(Action::confirm)), Event::refused);
}

TEST_F(ComponentsForms, ToggleFlipsWithConfirmAndDirections)
{
    EXPECT_EQ(send(press(Action::confirm)), Event::changed);
    EXPECT_TRUE(form_.toggle_value(kSwitch));
    EXPECT_EQ(form_.changed_id(), kSwitch);
    EXPECT_GT(pitch(Cue::toggle), 1.0f); // up for on

    EXPECT_EQ(send(nav(Direction::right)), Event::refused); // it is on already
    EXPECT_EQ(send(nav(Direction::left)), Event::changed);
    EXPECT_FALSE(form_.toggle_value(kSwitch));
    EXPECT_GT(pitch(Cue::toggle), 0.0f);
    EXPECT_LT(pitch(Cue::toggle), 1.0f); // down for off
}

TEST_F(ComponentsForms, ChoiceCyclesAndWrapIsAKnob)
{
    form_.focus_row(kChoice);
    EXPECT_TRUE(form_.uses_horizontal());
    EXPECT_EQ(send(nav(Direction::left)), Event::changed); // wraps by default
    EXPECT_EQ(form_.choice_index(kChoice), 2);
    EXPECT_EQ(form_.choice_text(kChoice), "Three");
    EXPECT_EQ(form_.changed_id(), kChoice);
    EXPECT_TRUE(asked(Cue::toggle));

    form_.style.wrap_choices = false;
    EXPECT_EQ(send(nav(Direction::right)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_EQ(form_.choice_index(kChoice), 2);
    // Confirm always finds a next option.
    EXPECT_EQ(send(press(Action::confirm)), Event::changed);
    EXPECT_EQ(form_.choice_index(kChoice), 0);
}

TEST_F(ComponentsForms, SliderStepsClampsAndStaysQuietOnRepeat)
{
    form_.focus_row(kSlider);
    EXPECT_EQ(send(nav(Direction::right)), Event::changed);
    EXPECT_FLOAT_EQ(form_.slider_value(kSlider), 60.0f);
    EXPECT_EQ(form_.changed_id(), kSlider);
    const float low = pitch(Cue::slider);
    EXPECT_GT(low, 0.0f);
    EXPECT_EQ(form_.slider_text(*form_.row(kSlider)), "60 %");

    form_.set_slider(kSlider, 90.0f);
    EXPECT_EQ(send(nav(Direction::right)), Event::changed);
    EXPECT_GT(pitch(Cue::slider), low); // the pitch follows the value
    EXPECT_EQ(send(nav(Direction::right)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_EQ(send(repeat(Direction::right)), Event::refused);
    EXPECT_TRUE(feedback_.cues.empty());
    EXPECT_FLOAT_EQ(form_.slider_value(kSlider), 100.0f);
}

TEST_F(ComponentsForms, HeldSliderAccelerates)
{
    form_.focus_row(kFine);
    EXPECT_EQ(send(nav(Direction::right)), Event::changed);
    for (int i = 0; i < 5; ++i)
        send(repeat(Direction::right));
    EXPECT_FLOAT_EQ(form_.slider_value(kFine), 6.0f);
    // The seventh step of a hold is fast_factor steps long.
    send(repeat(Direction::right));
    EXPECT_FLOAT_EQ(form_.slider_value(kFine), 10.0f);
    // A fresh press is a single step again.
    send(nav(Direction::left));
    EXPECT_FLOAT_EQ(form_.slider_value(kFine), 9.0f);
}

TEST_F(ComponentsForms, StepperRowHonoursItsRange)
{
    form_.focus_row(kStepper);
    for (int i = 0; i < 3; ++i)
        EXPECT_EQ(send(nav(Direction::right)), Event::changed);
    EXPECT_EQ(form_.stepper_value(kStepper), 10);
    EXPECT_EQ(form_.changed_id(), kStepper);
    EXPECT_TRUE(asked(Cue::slider));
    EXPECT_EQ(send(nav(Direction::right)), Event::refused);
    EXPECT_EQ(send(nav(Direction::left)), Event::changed);
    EXPECT_EQ(form_.stepper_value(kStepper), 8);
    EXPECT_EQ(send(press(Action::confirm)), Event::none); // nothing to confirm
}

TEST_F(ComponentsForms, ActionActivatesAndDisabledRowsRefuse)
{
    form_.focus_row(kAction);
    EXPECT_FALSE(form_.uses_horizontal());
    // Left and right are the screen's to use on a row that has no use for them.
    EXPECT_EQ(send(nav(Direction::right)), Event::none);
    EXPECT_TRUE(feedback_.cues.empty());
    EXPECT_EQ(send(press(Action::confirm)), Event::activated);
    EXPECT_EQ(form_.changed_id(), kAction);
    EXPECT_TRUE(asked(Cue::select));

    form_.focus_row(kLocked);
    EXPECT_EQ(send(press(Action::confirm)), Event::refused);
    EXPECT_EQ(send(nav(Direction::left)), Event::refused);
    EXPECT_TRUE(form_.toggle_value(kLocked));
    EXPECT_EQ(send(press(Action::back)), Event::cancelled);
    EXPECT_TRUE(asked(Cue::back));
}

TEST_F(ComponentsForms, SettersAreSilentAndStateSurvivesRestyling)
{
    form_.set_toggle(kSwitch, true);
    form_.set_choice(kChoice, 1);
    form_.set_slider(kSlider, 250.0f); // clamped
    form_.set_stepper(kStepper, 6);
    form_.set_value_text(kValue, "Changed");
    form_.focus_row(kSlider);

    form_.style.theme = hui::ui::themes()[5];
    form_.style.compact = true;
    form_.style.values_right = false;
    form_.style.panel = true;
    for (int i = 0; i < 30; ++i)
        form_.update(kFrame);

    EXPECT_TRUE(feedback_.cues.empty());
    EXPECT_TRUE(form_.toggle_value(kSwitch));
    EXPECT_EQ(form_.choice_index(kChoice), 1);
    EXPECT_FLOAT_EQ(form_.slider_value(kSlider), 100.0f);
    EXPECT_EQ(form_.stepper_value(kStepper), 6);
    EXPECT_EQ(form_.value_text(kValue), "Changed");
    EXPECT_EQ(form_.focus_id(), kSlider);
    // An id the form does not have reads as nothing and writes nowhere.
    form_.set_toggle(99, true);
    EXPECT_FALSE(form_.toggle_value(99));
    EXPECT_EQ(form_.row(99), nullptr);
}

TEST_F(ComponentsForms, ScrollKeepsTheFocusedRowInsideAndHelpFollowsIt)
{
    EXPECT_EQ(form_.help_text(), "What the switch does");
    for (int i = 0; i < 8; ++i)
        send(nav(Direction::down));
    EXPECT_EQ(form_.focus_id(), kFine);
    EXPECT_TRUE(form_.help_text().empty());
    EXPECT_EQ(send(nav(Direction::down)), Event::refused);
    const hui::gfx::Rect row = form_.row_rect(form_.focus());
    EXPECT_GE(row.y, 100.0f - 0.5f);
    EXPECT_LE(row.y + row.h, 360.0f + 0.5f);

    form_.style.wrap = true;
    EXPECT_EQ(send(nav(Direction::down)), Event::moved);
    EXPECT_EQ(form_.focus_id(), kSwitch);
    // The focused row is taller by its description line.
    EXPECT_GT(form_.row_rect(form_.focus()).h, form_.row_rect(2).h);
}

TEST_F(ComponentsForms, StandaloneStepper)
{
    Stepper stepper;
    stepper.set_range(0, 30, 4);
    stepper.set_value(28);
    stepper.set_suffix(" px");
    EXPECT_EQ(stepper.text(), "28 px");
    EXPECT_EQ(send(stepper, nav(Direction::right)), Event::changed);
    EXPECT_EQ(stepper.value(), 30); // lands on the limit, off the step grid
    EXPECT_TRUE(asked(Cue::slider));
    EXPECT_EQ(send(stepper, nav(Direction::right)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_EQ(send(stepper, nav(Direction::left)), Event::changed);
    EXPECT_EQ(stepper.value(), 28); // ... and back onto it

    stepper.style.wrap = true;
    stepper.set_value(30);
    EXPECT_EQ(send(stepper, nav(Direction::right)), Event::changed);
    EXPECT_EQ(stepper.value(), 0);
    EXPECT_FLOAT_EQ(stepper.fraction(), 0.0f);

    stepper.format = [](int value) { return value == 0 ? std::string("Off") : std::string("On"); };
    EXPECT_EQ(stepper.text(), "Off");
    EXPECT_EQ(send(stepper, press(Action::back)), Event::cancelled);
}

TEST_F(ComponentsForms, StandaloneChoicePicker)
{
    ChoicePicker picker;
    picker.set_options({"Low", "Medium", "High"});
    picker.set_index(2);
    EXPECT_EQ(picker.value(), "High");
    EXPECT_EQ(send(picker, nav(Direction::right)), Event::changed); // wraps
    EXPECT_EQ(picker.index(), 0);
    EXPECT_GT(pitch(Cue::toggle), 1.0f);

    picker.style.wrap = false;
    EXPECT_EQ(send(picker, nav(Direction::left)), Event::refused);
    EXPECT_EQ(picker.index(), 0);
    EXPECT_EQ(send(picker, press(Action::confirm)), Event::changed);
    EXPECT_EQ(picker.index(), 1);

    picker.style.confirm_cycles = false;
    EXPECT_EQ(send(picker, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(asked(Cue::select));
    EXPECT_EQ(picker.index(), 1);

    ChoicePicker empty;
    EXPECT_TRUE(empty.value().empty());
    EXPECT_EQ(send(empty, nav(Direction::right)), Event::refused);
}

TEST_F(ComponentsForms, TextFieldEditsAndReportsConfirm)
{
    TextField field;
    field.style.max_length = 5;
    EXPECT_TRUE(field.insert("h\xC3\xA9llo world")); // cut to five characters, not bytes
    EXPECT_EQ(field.text(), "h\xC3\xA9llo");
    EXPECT_EQ(field.length(), 5);
    EXPECT_TRUE(field.full());
    EXPECT_FALSE(field.insert('x'));

    feedback_.clear();
    EXPECT_EQ(field.insert('x', feedback_), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    feedback_.clear();
    EXPECT_EQ(field.backspace(feedback_), Event::changed);
    EXPECT_TRUE(asked(Cue::erase));
    EXPECT_EQ(field.text(), "h\xC3\xA9ll");
    feedback_.clear();
    EXPECT_EQ(field.insert('!', feedback_), Event::changed);
    EXPECT_TRUE(asked(Cue::type));
    EXPECT_FALSE(field.insert('\n')); // one line only

    for (int i = 0; i < 4; ++i)
        EXPECT_TRUE(field.backspace());
    EXPECT_EQ(field.text(), "h");
    EXPECT_TRUE(field.backspace());
    EXPECT_FALSE(field.backspace());
    field.set_text("abc");
    field.clear();
    EXPECT_TRUE(field.text().empty());

    EXPECT_EQ(send(field, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(asked(Cue::select));
    field.set_disabled(true);
    EXPECT_EQ(send(field, press(Action::confirm)), Event::refused);

    // The helper line, and the error that replaces it, need room.
    const float bare = field.preferred_height();
    field.set_error("Too short");
    EXPECT_GT(field.preferred_height(), bare);
    EXPECT_EQ(field.error(), "Too short");
}

TEST_F(ComponentsForms, DrawsInEveryThemeAndVariant)
{
    using hui::ui::HighlightKind;
    Stepper stepper;
    stepper.set_range(0, 10);
    stepper.set_bounds({100.0f, 400.0f, 240.0f, 56.0f});
    stepper.set_active(true);
    ChoicePicker picker;
    picker.set_options({"One", "Two", "Three"});
    picker.set_bounds({400.0f, 400.0f, 360.0f, 56.0f});
    picker.set_active(true);
    TextField field;
    field.set_label("Name");
    field.set_placeholder("Nobody yet");
    field.set_helper("Shown to other players");
    field.set_bounds({100.0f, 500.0f, 420.0f, 130.0f});
    field.set_active(true);
    // Draw them mid-animation: a change, a refusal and a focus fading in.
    send(stepper, nav(Direction::right));
    send(picker, nav(Direction::right));
    stepper.update(kFrame);
    picker.update(kFrame);
    field.update(kFrame);
    form_.focus_row(kAction);
    form_.row(kAction)->danger = true;

    int variant = 0;
    for (const hui::ui::Theme &theme : hui::ui::themes())
    {
        for (HighlightKind kind :
             {HighlightKind::ring, HighlightKind::fill, HighlightKind::tint, HighlightKind::bar,
              HighlightKind::underline, HighlightKind::glow, HighlightKind::none})
        {
            ++variant;
            form_.style.theme = theme;
            form_.style.highlight.kind = kind;
            form_.style.compact = variant % 2 == 0;
            form_.style.panel = variant % 3 == 0;
            form_.style.dividers = variant % 3 == 1;
            form_.style.values_right = variant % 4 != 0;
            form_.style.description_inline = variant % 5 != 0;
            form_.style.stepper_buttons = variant % 2 == 0 ? hui::ui::StepperButtons::plain
                                                           : hui::ui::StepperButtons::surface;
            form_.update(kFrame);
            stepper.style.theme = theme;
            stepper.style.buttons = form_.style.stepper_buttons;
            stepper.style.boxed = variant % 3 == 0;
            picker.style.theme = theme;
            picker.style.boxed = variant % 2 == 0;
            picker.style.dots = variant % 3 != 0;
            field.style.theme = theme;
            field.style.password = variant % 2 == 0;
            field.style.max_length = variant % 3 == 0 ? 8 : 0;
            field.set_text(variant % 4 == 0 ? "" : "A rather long value for a field");
            field.set_error(variant % 5 == 0 ? "Not like this" : "");

            {
                hui::ui::Canvas target = canvas();
                form_.draw(target);
                expect_drawn(theme.id);
            }
            hui::ui::Canvas target = canvas();
            stepper.draw(target);
            picker.draw(target);
            field.draw(target);
            expect_drawn(theme.id);
        }
    }
}

} // namespace
