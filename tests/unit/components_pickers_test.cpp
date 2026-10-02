// ps5-homebrew-ui - Tests: the picker components.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "component_fixture.hpp"
#include "ui/components/choice_group.hpp"
#include "ui/components/color_picker.hpp"
#include "ui/components/range_slider.hpp"
#include "ui/components/select.hpp"
#include "ui/components/tag_select.hpp"
#include "ui/components/wheel_picker.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

namespace
{

using hui::Action;
using hui::Direction;
using hui::InputFrame;
using hui::audio::Cue;
using hui::gfx::Color;
using hui::gfx::Rect;
using hui::ui::CheckGroup;
using hui::ui::ColorPicker;
using hui::ui::DatePicker;
using hui::ui::Event;
using hui::ui::RadioGroup;
using hui::ui::RangeSlider;
using hui::ui::Select;
using hui::ui::Slider;
using hui::ui::TagSelect;
using hui::ui::TimePicker;
using hui::ui::WheelPicker;

class ComponentsPickers : public hui::testing::ComponentFixture
{
  protected:
    // One input to a component, with the feedback cleared first.
    template <typename Component> Event send(Component &component, const InputFrame &input)
    {
        feedback_.clear();
        const Event event = component.handle(input, feedback_);
        for (int i = 0; i < 20; ++i)
            component.update(kFrame);
        return event;
    }

    static InputFrame held(Direction direction)
    {
        InputFrame input = nav(direction);
        input.nav_repeat = true;
        return input;
    }

    static Select make_select()
    {
        Select select;
        std::vector<hui::ui::SelectOption> options;
        for (const char *name : {"North", "East", "South", "West", "Up", "Down", "In", "Out"})
        {
            hui::ui::SelectOption option;
            option.label = name;
            options.push_back(option);
        }
        options[3].disabled = true;
        options[4].description = "Has a second line";
        select.set_label("Direction");
        select.set_options(options);
        select.set_index(1);
        select.set_bounds({100.0f, 100.0f, 420.0f, select.preferred_height()});
        return select;
    }

    static TagSelect make_tags()
    {
        TagSelect tags;
        std::vector<hui::ui::TagOption> options;
        for (const char *name : {"Action", "Puzzle", "Racing", "Horror", "Sports", "Arcade",
                                 "Rhythm", "Stealth", "Tactics", "Sandbox", "Platform", "Trivia"})
            options.push_back({name});
        tags.set_title("Genres");
        tags.set_options(options);
        tags.set_bounds({100.0f, 100.0f, 420.0f, 300.0f});
        return tags;
    }
};

TEST_F(ComponentsPickers, SelectOpensPicksAndCloses)
{
    Select select = make_select();
    EXPECT_EQ(send(select, nav(Direction::down)), Event::none); // the screen's to use
    EXPECT_EQ(send(select, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(select.is_open());
    EXPECT_TRUE(asked(Cue::modal_open));
    EXPECT_EQ(select.focus(), 1); // the list starts on the current option

    EXPECT_EQ(send(select, nav(Direction::down)), Event::moved);
    EXPECT_EQ(send(select, press(Action::confirm)), Event::changed);
    EXPECT_TRUE(asked(Cue::select));
    EXPECT_FALSE(select.is_open());
    EXPECT_EQ(select.index(), 2);
    EXPECT_EQ(select.value(), "South");

    // A disabled option refuses and the list stays; back changes nothing.
    send(select, press(Action::confirm));
    send(select, nav(Direction::down));
    EXPECT_EQ(send(select, press(Action::confirm)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_TRUE(select.is_open());
    EXPECT_EQ(send(select, press(Action::back)), Event::cancelled);
    EXPECT_TRUE(asked(Cue::modal_close));
    EXPECT_FALSE(select.is_open());
    EXPECT_EQ(select.index(), 2);

    // Confirm on the current option closes without a change.
    send(select, press(Action::confirm));
    EXPECT_EQ(send(select, press(Action::confirm)), Event::cancelled);
    EXPECT_EQ(select.index(), 2);

    // step_closed: left and right pick without the list, past disabled options.
    select.style.step_closed = true;
    EXPECT_EQ(send(select, nav(Direction::right)), Event::changed);
    EXPECT_EQ(select.index(), 4);
    EXPECT_FALSE(select.is_open());

    // The list opens below, flips above when there is more room there, and
    // stays inside its limits.
    select = make_select();
    const Rect limits{0.0f, 0.0f, 1920.0f, 1080.0f};
    select.set_limits(limits);
    EXPECT_FALSE(select.opens_above());
    Rect list = select.popover_rect();
    EXPECT_GE(list.y, select.field_rect().y + select.field_rect().h);

    select.set_bounds({100.0f, 940.0f, 420.0f, select.preferred_height()});
    EXPECT_TRUE(select.opens_above());
    list = select.popover_rect();
    EXPECT_LE(list.y + list.h, select.field_rect().y);
    EXPECT_GE(list.y, limits.y);

    // Squeezed from both sides it shows fewer rows rather than leave the limits.
    select.set_limits({0.0f, 700.0f, 1920.0f, 380.0f});
    list = select.popover_rect();
    EXPECT_GE(list.y, 700.0f);
    EXPECT_LE(list.y + list.h, 1080.0f);
}

TEST_F(ComponentsPickers, CheckGroupTogglesAndSelectsAll)
{
    CheckGroup group;
    group.style.select_all = true;
    group.set_items({{"One"}, {"Two"}, {"Three", "", true}, {"Four"}});
    group.set_bounds({100.0f, 100.0f, 400.0f, 300.0f});
    EXPECT_EQ(group.focus(), 0);

    EXPECT_EQ(send(group, press(Action::confirm)), Event::changed);
    EXPECT_TRUE(group.checked(0));
    EXPECT_TRUE(asked(Cue::toggle));
    EXPECT_GT(feedback_.cues.back().pitch, 1.0f); // on is the higher note
    EXPECT_EQ(send(group, press(Action::confirm)), Event::changed);
    EXPECT_FALSE(group.checked(0));
    EXPECT_LT(feedback_.cues.back().pitch, 1.0f);

    // The header: everything that is not disabled, then nothing.
    EXPECT_EQ(send(group, nav(Direction::up)), Event::moved);
    EXPECT_EQ(group.focus(), -1);
    EXPECT_EQ(send(group, press(Action::confirm)), Event::changed);
    EXPECT_EQ(group.changed_index(), -1);
    EXPECT_EQ(group.checked_count(), 3);
    EXPECT_FALSE(group.checked(2));
    EXPECT_EQ(send(group, press(Action::confirm)), Event::changed);
    EXPECT_EQ(group.checked_count(), 0);

    // A disabled item takes the focus and refuses; the end refuses, quietly
    // on a held direction, or hands the focus on through an exit.
    send(group, nav(Direction::down));
    send(group, nav(Direction::down));
    send(group, nav(Direction::down));
    EXPECT_EQ(group.focus(), 2);
    EXPECT_EQ(send(group, press(Action::confirm)), Event::refused);
    send(group, nav(Direction::down));
    EXPECT_EQ(send(group, nav(Direction::down)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_EQ(send(group, held(Direction::down)), Event::refused);
    EXPECT_TRUE(feedback_.cues.empty());
    group.style.exits.down = true;
    EXPECT_EQ(send(group, nav(Direction::down)), Event::none);
    EXPECT_EQ(group.exit(), Direction::down);
    EXPECT_EQ(group.focus(), 3);

    // A grid moves in two directions.
    group.style.layout = hui::ui::ChoiceLayout::grid;
    group.style.columns = 2;
    group.set_focus(0);
    EXPECT_EQ(send(group, nav(Direction::right)), Event::moved);
    EXPECT_EQ(group.focus(), 1);
    EXPECT_EQ(send(group, nav(Direction::down)), Event::moved);
    EXPECT_EQ(group.focus(), 3);
}

TEST_F(ComponentsPickers, RadioGroupKeepsOneSelected)
{
    RadioGroup group;
    group.set_items({{"Solo"}, {"Co-op"}, {"Versus"}});
    group.set_bounds({100.0f, 100.0f, 480.0f, 200.0f});
    group.set_selected(0);

    send(group, nav(Direction::down));
    EXPECT_EQ(group.selected(), 0); // moving is not selecting
    EXPECT_EQ(send(group, press(Action::confirm)), Event::changed);
    EXPECT_EQ(group.selected(), 1);
    EXPECT_EQ(send(group, press(Action::confirm)), Event::none);
    EXPECT_EQ(group.selected(), 1);
    group.style.allow_none = true;
    EXPECT_EQ(send(group, press(Action::confirm)), Event::changed);
    EXPECT_EQ(group.selected(), -1);

    group.style.select_on_move = true;
    EXPECT_EQ(send(group, nav(Direction::down)), Event::changed);
    EXPECT_EQ(group.selected(), 2);
    EXPECT_TRUE(asked(Cue::toggle));
    EXPECT_FALSE(asked(Cue::focus));

    // In a row, left and right move and up is an edge.
    group.style.layout = hui::ui::ChoiceLayout::horizontal;
    EXPECT_EQ(send(group, nav(Direction::left)), Event::changed);
    EXPECT_EQ(group.selected(), 1);
    EXPECT_EQ(send(group, nav(Direction::up)), Event::refused);
    group.style.wrap = true;
    send(group, nav(Direction::right));
    EXPECT_EQ(send(group, nav(Direction::right)), Event::changed);
    EXPECT_EQ(group.selected(), 0);
}

TEST_F(ComponentsPickers, TagSelectMovesByPositionAndHoldsItsColumn)
{
    TagSelect tags = make_tags();
    tags.layout(fonts_);
    ASSERT_GE(tags.rows(), 3);
    EXPECT_EQ(tags.row_of(0), 0);

    EXPECT_EQ(send(tags, nav(Direction::right)), Event::moved);
    EXPECT_EQ(tags.focus(), 1);
    const float column = tags.chip_rect(1).cx();
    EXPECT_EQ(send(tags, nav(Direction::down)), Event::moved);
    const int below = tags.focus();
    EXPECT_EQ(tags.row_of(below), 1);
    // The chip under the starting point, or the nearest one.
    const Rect under = tags.chip_rect(below);
    for (int i = 0; i < static_cast<int>(tags.options().size()); ++i)
    {
        if (tags.row_of(i) != 1)
            continue;
        const Rect other = tags.chip_rect(i);
        const auto distance = [column](const Rect &r)
        { return column < r.x ? r.x - column : (column > r.x + r.w ? column - r.x - r.w : 0.0f); };
        EXPECT_LE(distance(under), distance(other));
    }
    // Down and back up returns to where it started, whatever the rows between.
    send(tags, nav(Direction::down));
    EXPECT_EQ(tags.row_of(tags.focus()), 2);
    send(tags, nav(Direction::up));
    EXPECT_EQ(tags.focus(), below);
    send(tags, nav(Direction::up));
    EXPECT_EQ(tags.focus(), 1);

    EXPECT_EQ(send(tags, nav(Direction::up)), Event::refused);
    tags.style.exits.up = true;
    EXPECT_EQ(send(tags, nav(Direction::up)), Event::none);
    EXPECT_EQ(tags.exit(), Direction::up);

    // Without flow the end of a row is an edge; with it the next row follows.
    int last = 0;
    while (last + 1 < static_cast<int>(tags.options().size()) && tags.row_of(last + 1) == 0)
        ++last;
    tags.set_focus(last);
    EXPECT_EQ(send(tags, nav(Direction::right)), Event::refused);
    tags.style.flow = true;
    EXPECT_EQ(send(tags, nav(Direction::right)), Event::moved);
    EXPECT_EQ(tags.focus(), last + 1);
}

TEST_F(ComponentsPickers, TagSelectLimitRefusesSoftly)
{
    TagSelect tags = make_tags();
    tags.style.max_selected = 2;
    tags.layout(fonts_);
    EXPECT_EQ(send(tags, press(Action::confirm)), Event::changed);
    EXPECT_TRUE(asked(Cue::toggle));
    send(tags, nav(Direction::right));
    EXPECT_EQ(send(tags, press(Action::confirm)), Event::changed);
    send(tags, nav(Direction::right));
    EXPECT_EQ(send(tags, press(Action::confirm)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_EQ(tags.selected_count(), 2);
    EXPECT_EQ(tags.selection(), (std::vector<int>{0, 1}));

    // Switching one off makes room again.
    send(tags, nav(Direction::left));
    EXPECT_EQ(send(tags, press(Action::confirm)), Event::changed);
    EXPECT_LT(feedback_.cues.back().pitch, 1.0f);
    EXPECT_EQ(tags.selected_count(), 1);

    tags.style.single = true;
    send(tags, nav(Direction::right));
    EXPECT_EQ(send(tags, press(Action::confirm)), Event::changed);
    EXPECT_EQ(tags.selection(), (std::vector<int>{2}));
}

TEST_F(ComponentsPickers, ColorPickerSwatches)
{
    ColorPicker picker;
    picker.set_bounds({100.0f, 100.0f, 560.0f, 220.0f});
    picker.set_index(0);
    EXPECT_EQ(picker.focus(), 0);
    EXPECT_EQ(ColorPicker::hex_of(Color::rgb(0x3e9eff)), "#3E9EFF");

    EXPECT_EQ(send(picker, nav(Direction::right)), Event::moved);
    EXPECT_EQ(picker.index(), 0); // the ring moved, the colour did not
    EXPECT_EQ(send(picker, press(Action::confirm)), Event::changed);
    EXPECT_EQ(picker.index(), 1);
    EXPECT_EQ(picker.hex(), ColorPicker::hex_of(picker.palette()[1]));
    EXPECT_EQ(send(picker, press(Action::confirm)), Event::none);

    EXPECT_EQ(send(picker, nav(Direction::down)), Event::moved);
    EXPECT_EQ(picker.focus(), 1 + picker.style.columns);
    EXPECT_EQ(send(picker, nav(Direction::left)), Event::moved);
    EXPECT_EQ(send(picker, nav(Direction::left)), Event::refused);
    picker.style.exits.left = true;
    EXPECT_EQ(send(picker, nav(Direction::left)), Event::none);
    EXPECT_EQ(picker.exit(), Direction::left);

    picker.style.select_on_move = true;
    EXPECT_EQ(send(picker, nav(Direction::right)), Event::changed);
    EXPECT_EQ(picker.index(), picker.focus());
}

TEST_F(ComponentsPickers, ColorPickerHueSaturationValue)
{
    float h = 0.0f, s = 0.0f, v = 0.0f;
    ColorPicker::to_hsv(ColorPicker::from_hsv(200.0f, 0.4f, 0.8f), &h, &s, &v);
    EXPECT_NEAR(h, 200.0f, 0.01f);
    EXPECT_NEAR(s, 0.4f, 0.001f);
    EXPECT_NEAR(v, 0.8f, 0.001f);

    ColorPicker picker;
    picker.style.kind = hui::ui::ColorPickerKind::hsv;
    picker.set_bounds({100.0f, 100.0f, 560.0f, 220.0f});
    picker.set_color(ColorPicker::from_hsv(200.0f, 0.5f, 0.5f));
    EXPECT_EQ(picker.index(), -1); // not one of the presets

    EXPECT_EQ(send(picker, nav(Direction::right)), Event::changed);
    EXPECT_NEAR(picker.saturation(), 0.55f, 0.001f);
    EXPECT_TRUE(asked(Cue::slider));
    EXPECT_EQ(send(picker, nav(Direction::up)), Event::changed);
    EXPECT_NEAR(picker.value(), 0.55f, 0.001f);

    // Confirm switches to the hue strip, where the hue goes round.
    EXPECT_EQ(send(picker, press(Action::confirm)), Event::moved);
    EXPECT_EQ(picker.zone(), 1);
    EXPECT_EQ(send(picker, nav(Direction::right)), Event::changed);
    EXPECT_NEAR(picker.hue(), 210.0f, 0.01f);
    picker.set_color(ColorPicker::from_hsv(355.0f, 0.5f, 0.5f));
    EXPECT_EQ(send(picker, nav(Direction::right)), Event::changed);
    EXPECT_NEAR(picker.hue(), 5.0f, 0.01f);
    EXPECT_EQ(send(picker, nav(Direction::up)), Event::moved);
    EXPECT_EQ(picker.zone(), 0);

    // A limit of the field refuses.
    picker.set_color(ColorPicker::from_hsv(120.0f, 1.0f, 1.0f));
    EXPECT_EQ(send(picker, nav(Direction::right)), Event::refused);
    EXPECT_EQ(send(picker, nav(Direction::up)), Event::refused);

    // The stick moves the marker smoothly: the change is reported on the
    // frame after, and the stick's own direction steps are ignored.
    InputFrame tilt = idle();
    tilt.stick_x = -1.0f;
    tilt.nav = Direction::left;
    tilt.nav_from_stick = true;
    const float before = picker.saturation();
    feedback_.clear();
    EXPECT_EQ(picker.handle(tilt, feedback_), Event::none);
    picker.update(0.1f);
    EXPECT_LT(picker.saturation(), before);
    EXPECT_GT(picker.saturation(), before - 0.2f);
    EXPECT_EQ(picker.handle(idle(), feedback_), Event::changed);
    const float after = picker.saturation();
    picker.update(0.1f); // no stick this frame: nothing moves
    picker.update(0.1f);
    EXPECT_EQ(picker.saturation(), after);
}

TEST_F(ComponentsPickers, WheelPickerTurnsWrapsAndSpeedsUp)
{
    WheelPicker wheel;
    hui::ui::WheelColumn letters;
    letters.values = {"A", "B", "C", "D", "E"};
    hui::ui::WheelColumn numbers;
    for (int i = 0; i < 40; ++i)
        numbers.values.push_back(std::to_string(i));
    wheel.set_columns({letters, numbers});
    wheel.set_bounds({100.0f, 100.0f, 360.0f, wheel.preferred_height()});

    EXPECT_EQ(send(wheel, nav(Direction::up)), Event::refused); // no wrap: the first is an end
    EXPECT_EQ(send(wheel, nav(Direction::down)), Event::changed);
    EXPECT_EQ(wheel.index(0), 1);
    EXPECT_EQ(wheel.value(0), "B");
    EXPECT_TRUE(asked(Cue::tick));
    const float down_pitch = feedback_.cues.back().pitch;
    EXPECT_EQ(send(wheel, nav(Direction::up)), Event::changed);
    EXPECT_LT(feedback_.cues.back().pitch, down_pitch);

    wheel.style.wrap = true;
    EXPECT_EQ(send(wheel, nav(Direction::up)), Event::changed);
    EXPECT_EQ(wheel.index(0), 4);
    EXPECT_EQ(send(wheel, nav(Direction::down)), Event::changed);
    EXPECT_EQ(wheel.index(0), 0);

    // Columns: left and right, with edges that refuse or exit.
    EXPECT_EQ(send(wheel, nav(Direction::left)), Event::refused);
    EXPECT_EQ(send(wheel, nav(Direction::right)), Event::moved);
    EXPECT_EQ(wheel.column(), 1);
    wheel.style.exits.right = true;
    EXPECT_EQ(send(wheel, nav(Direction::right)), Event::none);
    EXPECT_EQ(wheel.exit(), Direction::right);

    // A held direction takes longer strides after a moment.
    wheel.style.wrap = false;
    send(wheel, nav(Direction::down));
    const int steps = wheel.style.fast_after + 4;
    for (int i = 0; i < steps; ++i)
        send(wheel, held(Direction::down));
    EXPECT_EQ(wheel.changed_column(), 1);
    EXPECT_GT(wheel.index(1), steps + 1);
    // ... and lands on the end instead of stopping short of it.
    for (int i = 0; i < 40; ++i)
        send(wheel, held(Direction::down));
    EXPECT_EQ(wheel.index(1), 39);
    EXPECT_EQ(send(wheel, held(Direction::down)), Event::refused);
    EXPECT_TRUE(feedback_.cues.empty());
}

TEST_F(ComponentsPickers, DatePickerKnowsTheMonths)
{
    EXPECT_TRUE(DatePicker::is_leap_year(2024));
    EXPECT_TRUE(DatePicker::is_leap_year(2000));
    EXPECT_FALSE(DatePicker::is_leap_year(1900));
    EXPECT_FALSE(DatePicker::is_leap_year(2023));
    EXPECT_EQ(DatePicker::days_in_month(2024, 2), 29);
    EXPECT_EQ(DatePicker::days_in_month(2023, 2), 28);
    EXPECT_EQ(DatePicker::days_in_month(2023, 4), 30);
    EXPECT_EQ(DatePicker::days_in_month(2023, 12), 31);

    DatePicker date;
    date.set_years(1990, 2030);
    date.set_date(2024, 1, 31);
    date.set_bounds({100.0f, 100.0f, 360.0f, date.preferred_height()});
    EXPECT_EQ(date.text(), "31 Jan 2024");
    EXPECT_EQ(date.wheel().column_at(0).values.size(), 31u);

    // January 31 turned to February is the 29th in a leap year ...
    EXPECT_EQ(send(date, nav(Direction::right)), Event::moved);
    EXPECT_EQ(send(date, nav(Direction::down)), Event::changed);
    EXPECT_EQ(date.month(), 2);
    EXPECT_EQ(date.day(), 29);
    EXPECT_EQ(date.wheel().column_at(0).values.size(), 29u);
    // ... and the 28th when the year turns to an ordinary one.
    send(date, nav(Direction::right));
    EXPECT_EQ(send(date, nav(Direction::up)), Event::changed);
    EXPECT_EQ(date.year(), 2023);
    EXPECT_EQ(date.day(), 28);
    EXPECT_EQ(date.text(), "28 Feb 2023");

    // Another order keeps the date and the active part (the year).
    date.style.order = hui::ui::DateOrder::year_month_day;
    date.update(kFrame);
    EXPECT_EQ(date.text(), "2023-02-28");
    EXPECT_EQ(date.wheel().column(), 0);
    EXPECT_EQ(send(date, nav(Direction::down)), Event::changed);
    EXPECT_EQ(date.year(), 2024);
}

TEST_F(ComponentsPickers, TimePickerTwelveAndTwentyFourHours)
{
    TimePicker time;
    time.set_time(19, 30);
    time.set_bounds({100.0f, 100.0f, 260.0f, time.preferred_height()});
    EXPECT_EQ(time.text(), "19:30");
    EXPECT_EQ(time.wheel().column_count(), 2);
    EXPECT_EQ(send(time, nav(Direction::down)), Event::changed);
    EXPECT_EQ(time.hour(), 20);

    time.style.twelve_hour = true;
    time.update(kFrame);
    EXPECT_EQ(time.wheel().column_count(), 3);
    EXPECT_EQ(time.text(), "8:30 PM");
    send(time, nav(Direction::right));
    send(time, nav(Direction::right));
    EXPECT_EQ(send(time, nav(Direction::up)), Event::changed);
    EXPECT_EQ(time.hour(), 8);
    EXPECT_EQ(time.text(), "8:30 AM");
    EXPECT_EQ(send(time, nav(Direction::up)), Event::refused); // two values never wrap

    time.set_time(0, 5);
    EXPECT_EQ(time.text(), "12:05 AM");
    time.style.minute_step = 15;
    time.update(kFrame);
    EXPECT_EQ(time.wheel().column_at(1).values.size(), 4u);
    send(time, nav(Direction::left));
    EXPECT_EQ(time.wheel().column(), 1);
    EXPECT_EQ(send(time, nav(Direction::down)), Event::changed);
    EXPECT_EQ(time.minute(), 15);
}

TEST_F(ComponentsPickers, SliderStepsRefusesAndSpeedsUp)
{
    Slider slider;
    slider.set_range(0.0f, 100.0f, 10.0f);
    slider.set_unit(" %");
    slider.set_value(80.0f);
    slider.set_bounds({100.0f, 100.0f, 480.0f, slider.preferred_height()});
    EXPECT_EQ(slider.text(), "80 %");

    EXPECT_EQ(send(slider, nav(Direction::right)), Event::changed);
    EXPECT_FLOAT_EQ(slider.value(), 90.0f);
    EXPECT_TRUE(asked(Cue::slider));
    const float pitch = feedback_.cues.back().pitch;
    EXPECT_EQ(send(slider, nav(Direction::right)), Event::changed);
    EXPECT_GT(feedback_.cues.back().pitch, pitch); // the cue rises with the value
    EXPECT_EQ(send(slider, nav(Direction::right)), Event::refused);
    EXPECT_FLOAT_EQ(slider.value(), 100.0f);
    EXPECT_EQ(send(slider, nav(Direction::up)), Event::none); // the screen's to use

    slider.set_range(0.0f, 100.0f, 1.0f);
    slider.set_value(100.0f);
    send(slider, nav(Direction::left));
    for (int i = 0; i < slider.style.fast_after + 2; ++i)
        send(slider, held(Direction::left));
    EXPECT_LT(slider.value(), 99.0f - static_cast<float>(slider.style.fast_after + 2));

    slider.format = [](float value) { return value >= 50.0f ? "High" : "Low"; };
    EXPECT_EQ(slider.text(), "High");
}

TEST_F(ComponentsPickers, RangeSliderThumbsDoNotCross)
{
    RangeSlider range;
    range.set_range(0.0f, 10.0f, 1.0f);
    range.set_min_gap(2.0f);
    range.set_values(9.0f, 2.0f); // given the wrong way round
    EXPECT_FLOAT_EQ(range.low(), 2.0f);
    EXPECT_FLOAT_EQ(range.high(), 9.0f);
    range.set_values(3.0f, 6.0f);
    range.set_bounds({100.0f, 100.0f, 480.0f, range.preferred_height()});
    EXPECT_EQ(range.thumb(), 0);
    EXPECT_EQ(range.text(), "3 - 6");

    EXPECT_EQ(send(range, nav(Direction::right)), Event::changed);
    EXPECT_FLOAT_EQ(range.low(), 4.0f);
    EXPECT_EQ(send(range, nav(Direction::right)), Event::refused); // the gap holds it back
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_FLOAT_EQ(range.low(), 4.0f);

    EXPECT_EQ(send(range, press(Action::confirm)), Event::moved);
    EXPECT_EQ(range.thumb(), 1);
    EXPECT_EQ(send(range, nav(Direction::left)), Event::refused);
    EXPECT_FLOAT_EQ(range.high(), 6.0f);
    EXPECT_EQ(send(range, nav(Direction::right)), Event::changed);
    EXPECT_FLOAT_EQ(range.high(), 7.0f);

    EXPECT_EQ(send(range, nav(Direction::up)), Event::none);
    range.style.vertical_switches = true;
    EXPECT_EQ(send(range, nav(Direction::up)), Event::moved);
    EXPECT_EQ(range.thumb(), 0);
}

TEST_F(ComponentsPickers, RestylingKeepsEveryValue)
{
    Select select = make_select();
    CheckGroup checks;
    checks.set_items({{"One"}, {"Two"}});
    checks.set_checked(1, true);
    TagSelect tags = make_tags();
    tags.set_selected(3, true);
    tags.set_focus(5);
    ColorPicker picker;
    picker.set_index(4);
    DatePicker date;
    date.set_date(2012, 12, 21);
    TimePicker time;
    time.set_time(7, 45);
    RangeSlider range;
    range.set_values(20.0f, 60.0f);
    for (const hui::ui::Theme &theme : hui::ui::themes())
    {
        select.style.theme = theme;
        checks.style.theme = theme;
        tags.style.theme = theme;
        picker.style.theme = theme;
        picker.style.kind = hui::ui::ColorPickerKind::hsv;
        date.style.theme = theme;
        time.style.theme = theme;
        range.style.theme = theme;
        select.update(kFrame);
        checks.update(kFrame);
        tags.update(kFrame);
        picker.update(kFrame);
        date.update(kFrame);
        time.update(kFrame);
        range.update(kFrame);
        EXPECT_EQ(select.index(), 1) << theme.id;
        EXPECT_TRUE(checks.checked(1)) << theme.id;
        EXPECT_TRUE(tags.selected(3)) << theme.id;
        EXPECT_EQ(tags.focus(), 5) << theme.id;
        EXPECT_EQ(picker.index(), 4) << theme.id;
        EXPECT_EQ(date.text(), "21 Dec 2012") << theme.id;
        EXPECT_EQ(time.text(), "07:45") << theme.id;
        EXPECT_FLOAT_EQ(range.low(), 20.0f) << theme.id;
    }
}

TEST_F(ComponentsPickers, DrawsInEveryThemeAndVariant)
{
    using hui::ui::HighlightKind;
    Select select = make_select();
    CheckGroup checks;
    checks.set_title("Checks");
    checks.set_items({{"One", "With a description"}, {"Two"}, {"Three", "", true}, {"Four"}});
    checks.set_checked(0, true);
    RadioGroup radios;
    radios.set_items({{"Solo"}, {"Co-op"}, {"Versus"}});
    radios.set_selected(1);
    TagSelect tags = make_tags();
    tags.set_selected(2, true);
    ColorPicker picker;
    picker.set_title("Colour");
    WheelPicker wheel;
    hui::ui::WheelColumn sizes;
    sizes.values = {"XS", "S", "M", "L", "XL"};
    wheel.set_columns({sizes, sizes});
    DatePicker date;
    date.set_title("Date");
    TimePicker time;
    Slider slider;
    slider.set_label("Volume");
    RangeSlider range;
    range.set_label("Range");

    select.set_bounds({100.0f, 100.0f, 420.0f, 90.0f});
    checks.set_bounds({100.0f, 220.0f, 420.0f, 260.0f});
    radios.set_bounds({100.0f, 500.0f, 420.0f, 160.0f});
    tags.set_bounds({600.0f, 100.0f, 500.0f, 200.0f});
    picker.set_bounds({600.0f, 320.0f, 560.0f, 220.0f});
    wheel.set_bounds({600.0f, 560.0f, 240.0f, 220.0f});
    date.set_bounds({1200.0f, 100.0f, 320.0f, 220.0f});
    time.set_bounds({1200.0f, 340.0f, 240.0f, 220.0f});
    slider.set_bounds({1200.0f, 580.0f, 420.0f, 120.0f});
    range.set_bounds({1200.0f, 720.0f, 420.0f, 120.0f});

    for (const hui::ui::Theme &theme : hui::ui::themes())
    {
        for (int variant = 0; variant < 3; ++variant)
        {
            const HighlightKind kind = variant == 0   ? HighlightKind::tint
                                       : variant == 1 ? HighlightKind::fill
                                                      : HighlightKind::ring;
            select.style.theme = theme;
            select.style.label =
                variant == 1 ? hui::ui::SelectLabel::inside : hui::ui::SelectLabel::above;
            select.style.highlight.kind = kind;
            select.style.on_page = variant == 2;
            checks.style.theme = theme;
            checks.style.highlight.kind = kind;
            checks.style.select_all = variant != 1;
            checks.style.layout =
                variant == 2 ? hui::ui::ChoiceLayout::grid : hui::ui::ChoiceLayout::vertical;
            checks.style.on_page = variant == 2;
            radios.style.theme = theme;
            radios.style.highlight.kind = kind;
            radios.style.layout =
                variant == 1 ? hui::ui::ChoiceLayout::horizontal : hui::ui::ChoiceLayout::vertical;
            tags.style.theme = theme;
            tags.style.shape = variant == 0   ? hui::ui::TagShape::theme
                               : variant == 1 ? hui::ui::TagShape::pill
                                              : hui::ui::TagShape::square;
            tags.style.check = variant != 2;
            tags.style.align = variant == 1 ? hui::gfx::Align::center : hui::gfx::Align::left;
            picker.style.theme = theme;
            picker.style.kind =
                variant == 1 ? hui::ui::ColorPickerKind::hsv : hui::ui::ColorPickerKind::swatches;
            picker.style.preview = variant != 2;
            wheel.style.theme = theme;
            wheel.style.highlight.kind = kind;
            wheel.style.boxed = variant != 2;
            wheel.style.on_page = variant == 2;
            wheel.style.wrap = variant == 1;
            date.style.theme = theme;
            date.style.order = variant == 1 ? hui::ui::DateOrder::month_day_year
                                            : hui::ui::DateOrder::day_month_year;
            date.style.months =
                variant == 2 ? hui::ui::MonthStyle::full_name : hui::ui::MonthStyle::short_name;
            time.style.theme = theme;
            time.style.twelve_hour = variant == 1;
            for (hui::ui::SliderStyle *style : {&slider.style, &range.style})
            {
                style->theme = theme;
                style->bubble = variant == 0   ? hui::ui::BubbleMode::always
                                : variant == 1 ? hui::ui::BubbleMode::focused
                                               : hui::ui::BubbleMode::never;
                style->ticks = variant == 1 ? 6 : 0;
                style->end_labels = variant == 2;
            }

            const bool focused = variant != 2;
            select.set_active(focused);
            checks.set_active(focused);
            radios.set_active(focused);
            tags.set_active(focused);
            picker.set_active(focused);
            picker.set_engaged(variant == 1);
            wheel.set_active(focused);
            wheel.set_engaged(variant == 0);
            date.set_active(focused);
            time.set_active(focused);
            slider.set_active(focused);
            range.set_active(focused);
            if (variant == 1)
                select.open(feedback_);
            else
                select.dismiss();
            for (int frame = 0; frame < 4; ++frame)
            {
                select.update(kFrame);
                checks.update(kFrame);
                radios.update(kFrame);
                tags.update(kFrame);
                picker.update(kFrame);
                wheel.update(kFrame);
                date.update(kFrame);
                time.update(kFrame);
                slider.update(kFrame);
                range.update(kFrame);
            }

            hui::ui::Canvas target = canvas();
            select.draw(target);
            checks.draw(target);
            radios.draw(target);
            tags.draw(target);
            picker.draw(target);
            wheel.draw(target);
            date.draw(target);
            time.draw(target);
            slider.draw(target);
            range.draw(target);
            select.draw_popover(target);
            expect_drawn(theme.id);
        }
    }
}

} // namespace
