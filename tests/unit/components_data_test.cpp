// ps5-homebrew-ui - Tests: the data components (table, details, timeline, charts, calendar,
// status). Copyright (C) 2026 BlackBearReloaded SPDX-License-Identifier: GPL-3.0-or-later

#include "component_fixture.hpp"
#include "ui/components/calendar.hpp"
#include "ui/components/chart.hpp"
#include "ui/components/detail_list.hpp"
#include "ui/components/status.hpp"
#include "ui/components/table.hpp"
#include "ui/components/timeline.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;
using hui::ui::CalendarDate;
using hui::ui::Event;

class ComponentsData : public hui::testing::ComponentFixture
{
  protected:
    ComponentsData()
    {
        // A small leaderboard: a name, a score and a column that cannot sort.
        std::vector<hui::ui::TableColumn> columns(3);
        columns[0].title = "Player";
        columns[0].strong = true;
        columns[1].title = "Score";
        columns[1].width = 90.0f;
        columns[1].numeric = true;
        columns[1].align = hui::gfx::Align::right;
        columns[2].title = "Form";
        columns[2].width = 60.0f;
        columns[2].sortable = false;
        table_.set_columns(columns);
        const char *names[] = {"Mara", "tobi", "Lin", "Ada", "Rui", "Noor"};
        const int scores[] = {30, 50, 10, 60, 20, 40};
        std::vector<hui::ui::TableRow> rows;
        for (int i = 0; i < 6; ++i)
        {
            hui::ui::TableRow row;
            row.id = 100 + i;
            row.cells = {{names[i], 0.0},
                         {std::to_string(scores[i]), static_cast<double>(scores[i])},
                         {"", 0.0}};
            rows.push_back(row);
        }
        rows[4].disabled = true;
        table_.set_rows(rows);
        table_.set_bounds({100.0f, 100.0f, 520.0f, 240.0f});

        bars_.set_categories({"Mon", "Tue", "Wed"});
        bars_.set_series({{"Solo", {2.0f, 4.0f, 3.0f}}, {"Co-op", {1.0f, 0.0f, 2.0f}}}, true);
        bars_.set_bounds({100.0f, 100.0f, 400.0f, 280.0f});
        line_.set_labels({"0", "1", "2", "3"});
        line_.set_series({{"ms", {16.0f, 18.0f, 15.0f, 21.0f}}}, true);
        line_.set_bounds({100.0f, 100.0f, 400.0f, 280.0f});
        donut_.set_slices({{"Games", 60.0f}, {"Media", 30.0f}, {"Saves", 10.0f}}, true);
        donut_.set_bounds({100.0f, 100.0f, 400.0f, 280.0f});

        calendar_.set_today({2026, 10, 2}); // a Friday
        calendar_.set_bounds({100.0f, 100.0f, 430.0f, 330.0f});
    }

    template <typename Component> Event send(Component &component, const hui::InputFrame &input)
    {
        feedback_.clear();
        const Event event = component.handle(input, feedback_);
        for (int i = 0; i < 30; ++i)
            component.update(kFrame);
        return event;
    }

    // The name in the first cell of the focused row.
    const std::string &focused_name() const
    {
        return table_.rows()[static_cast<std::size_t>(table_.focus())].cells[0].text;
    }

    hui::ui::Table table_;
    hui::ui::BarChart bars_;
    hui::ui::LineChart line_;
    hui::ui::DonutChart donut_;
    hui::ui::Calendar calendar_;
};

TEST_F(ComponentsData, TableSortMovesRowsAndKeepsTheFocusOnItsRow)
{
    table_.set_focus(2); // Lin, the lowest score
    table_.sort_by(1, hui::ui::SortOrder::descending);
    EXPECT_EQ(focused_name(), "Lin");
    EXPECT_EQ(table_.focus_place(), 5);
    EXPECT_EQ(table_.place_of(3), 0); // Ada, 60

    // The dedicated action: a figure column starts with the largest, then
    // turns over, then the next sortable column takes its turn.
    table_.sort_by(-1, hui::ui::SortOrder::none);
    feedback_.clear();
    EXPECT_EQ(table_.cycle_sort(feedback_), Event::changed);
    EXPECT_TRUE(asked(Cue::toggle));
    EXPECT_EQ(table_.sort_column(), 0);
    EXPECT_EQ(table_.sort_order(), hui::ui::SortOrder::ascending);
    EXPECT_EQ(table_.place_of(3), 0); // Ada comes first, and case does not matter:
    EXPECT_EQ(table_.place_of(1), 5); // "tobi" is last, not before "Ada"
    table_.cycle_sort(feedback_);
    EXPECT_EQ(table_.sort_order(), hui::ui::SortOrder::descending);
    table_.cycle_sort(feedback_);
    EXPECT_EQ(table_.sort_column(), 1);
    EXPECT_EQ(table_.sort_order(), hui::ui::SortOrder::descending);
    EXPECT_EQ(focused_name(), "Lin");

    // Sorting animates: the rows are on their way, then arrive.
    table_.set_focus(3, true);
    const float before = table_.row_rect(3).y;
    table_.sort_by(1, hui::ui::SortOrder::ascending);
    table_.update(kFrame);
    EXPECT_NE(table_.row_rect(3).y, before);
}

TEST_F(ComponentsData, TableHeaderZoneColumnCursorAndEdges)
{
    EXPECT_EQ(send(table_, nav(Direction::up)), Event::moved);
    EXPECT_TRUE(table_.in_header());
    EXPECT_EQ(table_.column_cursor(), 0);
    EXPECT_EQ(send(table_, nav(Direction::right)), Event::moved);
    EXPECT_EQ(table_.column_cursor(), 1);
    // The third column cannot sort: the cursor does not go there.
    EXPECT_EQ(send(table_, nav(Direction::right)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_EQ(send(table_, press(Action::confirm)), Event::changed);
    EXPECT_EQ(table_.sort_column(), 1);
    EXPECT_EQ(send(table_, nav(Direction::up)), Event::refused);

    table_.style.exits.up = true;
    EXPECT_EQ(send(table_, nav(Direction::up)), Event::none);
    EXPECT_EQ(table_.exit(), Direction::up);
    EXPECT_TRUE(feedback_.cues.empty());

    EXPECT_EQ(send(table_, nav(Direction::down)), Event::moved);
    EXPECT_FALSE(table_.in_header());
    EXPECT_EQ(table_.exit(), Direction::none);
    // Among the rows, left and right leave the table or refuse.
    EXPECT_EQ(send(table_, nav(Direction::right)), Event::refused);
    table_.set_focus(3); // Ada: first, now that the highest score leads
    EXPECT_EQ(table_.focus_place(), 0);
    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(send(table_, nav(Direction::down)), Event::moved);
    hui::InputFrame held = nav(Direction::down);
    held.nav_repeat = true;
    EXPECT_EQ(send(table_, held), Event::refused);
    EXPECT_TRUE(feedback_.cues.empty());
    // The focused row stays inside the view while it scrolls.
    const hui::gfx::Rect row = table_.row_rect(table_.focus());
    EXPECT_GE(row.y, 100.0f);
    EXPECT_LE(row.y + row.h, 340.0f + 0.5f);
}

TEST_F(ComponentsData, TableActivatesOrMarksAndKeepsMarksAcrossNewRows)
{
    EXPECT_EQ(send(table_, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(asked(Cue::select));
    table_.set_focus(4);
    EXPECT_EQ(send(table_, press(Action::confirm)), Event::refused); // disabled

    table_.style.multi_select = true;
    table_.set_focus(1);
    EXPECT_EQ(send(table_, press(Action::confirm)), Event::changed);
    EXPECT_TRUE(table_.selected(1));
    EXPECT_EQ(table_.selected_count(), 1);

    // New data: rows are matched by id, so the mark and the focus follow
    // their row to wherever it now is.
    std::vector<hui::ui::TableRow> rows = table_.rows();
    std::swap(rows[1], rows[5]);
    table_.set_rows(rows);
    EXPECT_TRUE(table_.selected(5));
    EXPECT_FALSE(table_.selected(1));
    EXPECT_EQ(focused_name(), "tobi");
    EXPECT_EQ(send(table_, press(Action::confirm)), Event::changed);
    EXPECT_EQ(table_.selected_count(), 0);
    EXPECT_EQ(send(table_, press(Action::back)), Event::cancelled);
    EXPECT_TRUE(asked(Cue::back));
}

TEST_F(ComponentsData, DetailListFocusesOnlyWhatCanBeActivated)
{
    hui::ui::DetailList details;
    std::vector<hui::ui::DetailItem> items(6);
    items[0].label = "System";
    items[0].header = true;
    items[1].label = "Version";
    items[1].value = "1.4.2";
    items[1].focusable = true;
    items[2].label = "Renderer";
    items[2].value = "A value long enough to need a second line in any face";
    items[3].label = "Build";
    items[3].value = "8841";
    items[3].focusable = true;
    items[4].label = "Region";
    items[4].value = "Europe";
    items[5].label = "Folder";
    items[5].value = "/data/saves";
    items[5].focusable = true;
    details.set_items(items);
    details.set_bounds({100.0f, 100.0f, 520.0f, 200.0f});
    details.measure(fonts_);

    EXPECT_EQ(details.focus(), 1);
    EXPECT_EQ(send(details, nav(Direction::down)), Event::moved);
    EXPECT_EQ(details.focus(), 3);
    EXPECT_EQ(send(details, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(asked(Cue::select));
    EXPECT_EQ(send(details, nav(Direction::down)), Event::moved);
    EXPECT_EQ(details.focus(), 5);
    EXPECT_EQ(send(details, nav(Direction::down)), Event::refused);
    details.style.exits.left = true;
    EXPECT_EQ(send(details, nav(Direction::left)), Event::none);
    EXPECT_EQ(details.exit(), Direction::left);
    // The focused pair was scrolled into view.
    const hui::gfx::Rect row = details.item_rect(5);
    EXPECT_LE(row.y + row.h, 300.0f + 0.5f);

    // A wrapped value makes its pair taller than a one-line one.
    EXPECT_GT(details.item_rect(2).h, details.item_rect(4).h);

    // In a grid, pairs sit side by side and left and right move between them.
    details.style.layout = hui::ui::DetailLayout::grid;
    details.style.columns = 3;
    details.set_items(items);
    details.set_focus(1);
    EXPECT_EQ(send(details, nav(Direction::right)), Event::moved);
    EXPECT_EQ(details.focus(), 3);
    EXPECT_FLOAT_EQ(details.item_rect(1).y, details.item_rect(3).y);

    // Nothing focusable: up and down read the sheet by scrolling it.
    for (hui::ui::DetailItem &item : items)
        item.focusable = false;
    details.style.layout = hui::ui::DetailLayout::rows;
    details.set_items(items);
    EXPECT_EQ(details.focus(), -1);
    const float top = details.item_rect(1).y;
    EXPECT_EQ(send(details, nav(Direction::down)), Event::moved);
    EXPECT_LT(details.item_rect(1).y, top);
    EXPECT_EQ(send(details, press(Action::confirm)), Event::none);
}

TEST_F(ComponentsData, TimelineSkipsGroupTitlesAndLeavesThroughItsEdges)
{
    hui::ui::Timeline feed;
    std::vector<hui::ui::TimelineEntry> entries(5);
    entries[0].title = "Today";
    entries[0].group = true;
    entries[1].title = "Trophy earned";
    entries[1].body = "A body long enough to wrap to a second line in a narrow feed.";
    entries[2].title = "Save uploaded";
    entries[3].title = "Earlier";
    entries[3].group = true;
    entries[4].title = "Update installed";
    entries[4].card_height = 40.0f;
    feed.set_entries(entries);
    feed.set_bounds({100.0f, 100.0f, 420.0f, 220.0f});
    feed.measure(fonts_);

    EXPECT_EQ(feed.focus(), 1);
    EXPECT_EQ(send(feed, nav(Direction::up)), Event::refused);
    EXPECT_EQ(send(feed, nav(Direction::down)), Event::moved);
    EXPECT_TRUE(asked(Cue::focus));
    EXPECT_EQ(send(feed, nav(Direction::down)), Event::moved);
    EXPECT_EQ(feed.focus(), 4); // over the group title
    EXPECT_EQ(send(feed, press(Action::confirm)), Event::activated);
    const hui::gfx::Rect entry = feed.entry_rect(4);
    EXPECT_LE(entry.y + entry.h, 320.0f + 0.5f);
    // The body wrapped, and the card's room was reserved.
    EXPECT_GT(feed.entry_rect(1).h, feed.entry_rect(2).h);
    EXPECT_GT(feed.entry_rect(4).h, feed.entry_rect(2).h + 39.0f);

    feed.style.exits.down = true;
    feed.style.exits.right = true;
    EXPECT_EQ(send(feed, nav(Direction::down)), Event::none);
    EXPECT_EQ(feed.exit(), Direction::down);
    EXPECT_EQ(send(feed, nav(Direction::right)), Event::none);
    EXPECT_EQ(feed.exit(), Direction::right);
    EXPECT_EQ(send(feed, nav(Direction::left)), Event::refused);
}

TEST_F(ComponentsData, ChartScalesEndOnNiceTicks)
{
    EXPECT_FLOAT_EQ(hui::ui::nice_step(100.0f, 4), 25.0f);
    EXPECT_FLOAT_EQ(hui::ui::nice_step(7.0f, 4), 2.0f);
    EXPECT_FLOAT_EQ(hui::ui::nice_step(0.9f, 4), 0.25f);
    EXPECT_FLOAT_EQ(hui::ui::nice_step(42000.0f, 4), 20000.0f);
    EXPECT_FLOAT_EQ(hui::ui::nice_step(0.0f, 4), 1.0f);

    const hui::ui::ChartScale hours = hui::ui::nice_scale(0.0f, 5.6f, 4);
    EXPECT_FLOAT_EQ(hours.low, 0.0f);
    EXPECT_FLOAT_EQ(hours.step, 2.0f);
    EXPECT_FLOAT_EQ(hours.high, 6.0f);
    const hui::ui::ChartScale both = hui::ui::nice_scale(-12.0f, 31.0f, 4);
    EXPECT_LE(both.low, -12.0f);
    EXPECT_GE(both.high, 31.0f);
    EXPECT_NEAR(std::fmod(both.high - both.low, both.step), 0.0f, 1e-3f);
    const hui::ui::ChartScale exact = hui::ui::nice_scale(0.0f, 30.0f, 3);
    EXPECT_FLOAT_EQ(exact.high, 30.0f); // already on a tick: no extra interval
    const hui::ui::ChartScale flat = hui::ui::nice_scale(5.0f, 5.0f, 4);
    EXPECT_GT(flat.high, flat.low);

    EXPECT_EQ(hui::ui::format_value(950.0), "950");
    EXPECT_EQ(hui::ui::format_value(1500.0), "1.5k");
    EXPECT_EQ(hui::ui::format_value(2000000.0), "2M");
    EXPECT_EQ(hui::ui::format_value(2.5), "2.5");
}

TEST_F(ComponentsData, BarChartWalksItsBarsAndReportsTheirValues)
{
    EXPECT_EQ(bars_.focus(), 0);
    EXPECT_FLOAT_EQ(bars_.focused_value(), 2.0f);
    // Grouped: every bar takes the focus, series by series.
    EXPECT_EQ(send(bars_, nav(Direction::right)), Event::moved);
    EXPECT_EQ(bars_.focus(), 0);
    EXPECT_EQ(bars_.focus_series(), 1);
    EXPECT_FLOAT_EQ(bars_.focused_value(), 1.0f);
    EXPECT_EQ(send(bars_, nav(Direction::right)), Event::moved);
    EXPECT_EQ(bars_.focus(), 1);
    EXPECT_EQ(bars_.focus_series(), 0);
    EXPECT_EQ(send(bars_, nav(Direction::up)), Event::refused);
    EXPECT_EQ(send(bars_, press(Action::confirm)), Event::activated);

    // Stacked: a category is one stop and its value is the total.
    bars_.style.layout = hui::ui::BarLayout::stacked;
    bars_.set_focus(2);
    EXPECT_FLOAT_EQ(bars_.focused_value(), 5.0f);
    EXPECT_EQ(send(bars_, nav(Direction::right)), Event::refused);
    bars_.style.exits.right = true;
    EXPECT_EQ(send(bars_, nav(Direction::right)), Event::none);
    EXPECT_EQ(bars_.exit(), Direction::right);
    EXPECT_EQ(send(bars_, nav(Direction::left)), Event::moved);
    EXPECT_EQ(bars_.focus(), 1);

    // Across: the categories run down, so up and down walk them.
    bars_.style.horizontal = true;
    EXPECT_EQ(send(bars_, nav(Direction::down)), Event::moved);
    EXPECT_EQ(bars_.focus(), 2);
    EXPECT_EQ(send(bars_, nav(Direction::left)), Event::refused);

    // New data keeps the focus and changes what it reports.
    bars_.set_series({{"Solo", {1.0f, 1.0f, 8.0f}}, {"Co-op", {0.0f, 0.0f, 1.0f}}});
    EXPECT_EQ(bars_.focus(), 2);
    EXPECT_FLOAT_EQ(bars_.focused_value(), 9.0f);
}

TEST_F(ComponentsData, LineCursorAndDonutSlices)
{
    EXPECT_EQ(send(line_, nav(Direction::left)), Event::refused);
    EXPECT_EQ(send(line_, nav(Direction::right)), Event::moved);
    EXPECT_TRUE(asked(Cue::focus));
    EXPECT_EQ(line_.focus(), 1);
    line_.set_focus(3);
    line_.style.exits.right = true;
    EXPECT_EQ(send(line_, nav(Direction::right)), Event::none);
    EXPECT_EQ(line_.exit(), Direction::right);
    EXPECT_EQ(send(line_, nav(Direction::down)), Event::refused);
    // A shorter series keeps the cursor on a step that exists.
    line_.set_series({{"ms", {16.0f, 17.0f}}});
    EXPECT_EQ(line_.focus(), 1);

    EXPECT_FLOAT_EQ(donut_.total(), 100.0f);
    EXPECT_EQ(send(donut_, nav(Direction::left)), Event::moved); // wraps
    EXPECT_EQ(donut_.focus(), 2);
    EXPECT_EQ(send(donut_, nav(Direction::right)), Event::moved);
    EXPECT_EQ(donut_.focus(), 0);
    donut_.style.exits.left = true; // an exit beats the wrap
    EXPECT_EQ(send(donut_, nav(Direction::left)), Event::none);
    EXPECT_EQ(donut_.exit(), Direction::left);
    donut_.style.wrap = false;
    donut_.set_focus(2);
    EXPECT_EQ(send(donut_, nav(Direction::right)), Event::refused);
    EXPECT_EQ(send(donut_, press(Action::confirm)), Event::activated);
    donut_.set_slices({{"Games", 10.0f}});
    EXPECT_EQ(donut_.focus(), 0);
    EXPECT_FLOAT_EQ(donut_.total(), 10.0f);
}

TEST_F(ComponentsData, CalendarKnowsTheCalendar)
{
    using hui::ui::add_days;
    using hui::ui::days_between;
    using hui::ui::days_in_month;
    using hui::ui::weekday_of;
    EXPECT_TRUE(hui::ui::is_leap_year(2024));
    EXPECT_TRUE(hui::ui::is_leap_year(2000));
    EXPECT_FALSE(hui::ui::is_leap_year(1900));
    EXPECT_FALSE(hui::ui::is_leap_year(2026));
    EXPECT_EQ(days_in_month(2024, 2), 29);
    EXPECT_EQ(days_in_month(2026, 2), 28);
    EXPECT_EQ(days_in_month(2026, 4), 30);
    EXPECT_EQ(days_in_month(2026, 12), 31);
    EXPECT_EQ(weekday_of({1970, 1, 1}), 4);  // Thursday
    EXPECT_EQ(weekday_of({2000, 2, 29}), 2); // Tuesday
    EXPECT_EQ(weekday_of({2026, 10, 2}), 5); // Friday
    EXPECT_EQ(weekday_of({2024, 12, 29}), 0);
    EXPECT_EQ(add_days({2026, 12, 31}, 1), (CalendarDate{2027, 1, 1}));
    EXPECT_EQ(add_days({2024, 3, 1}, -1), (CalendarDate{2024, 2, 29}));
    EXPECT_EQ(add_days({2026, 3, 1}, -1), (CalendarDate{2026, 2, 28}));
    EXPECT_EQ(days_between({2026, 1, 1}, {2027, 1, 1}), 365);
    EXPECT_EQ(days_between({2024, 1, 1}, {2025, 1, 1}), 366);
    EXPECT_EQ(days_between({2026, 10, 9}, {2026, 10, 2}), -7);

    // The first-weekday knob moves a day to another column, not another row.
    const hui::gfx::Rect monday_first = calendar_.day_rect({2026, 10, 2});
    calendar_.style.week_start = hui::ui::WeekStart::sunday;
    const hui::gfx::Rect sunday_first = calendar_.day_rect({2026, 10, 2});
    EXPECT_GT(sunday_first.x, monday_first.x);
    EXPECT_FLOAT_EQ(sunday_first.y, monday_first.y);
}

TEST_F(ComponentsData, CalendarMovesByDaysAndChangesMonthAtItsEdges)
{
    EXPECT_EQ(send(calendar_, nav(Direction::right)), Event::moved);
    EXPECT_EQ(calendar_.focus(), (CalendarDate{2026, 10, 3}));
    EXPECT_TRUE(asked(Cue::focus));
    EXPECT_EQ(send(calendar_, nav(Direction::down)), Event::moved);
    EXPECT_EQ(calendar_.focus(), (CalendarDate{2026, 10, 10}));
    // Up from the first week leaves the month: the page cue, not the tick.
    calendar_.set_focus({2026, 10, 2});
    EXPECT_EQ(send(calendar_, nav(Direction::up)), Event::moved);
    EXPECT_EQ(calendar_.focus(), (CalendarDate{2026, 9, 25}));
    EXPECT_EQ(calendar_.month(), 9);
    EXPECT_TRUE(asked(Cue::tab));
    calendar_.set_focus({2026, 12, 31});
    EXPECT_EQ(send(calendar_, nav(Direction::right)), Event::moved);
    EXPECT_EQ(calendar_.focus(), (CalendarDate{2027, 1, 1}));
    EXPECT_EQ(calendar_.year(), 2027);

    // An edge that is an exit hands the focus back instead.
    calendar_.set_focus({2026, 10, 4}); // Sunday: the last column
    calendar_.style.exits.right = true;
    calendar_.style.exits.up = true;
    EXPECT_EQ(send(calendar_, nav(Direction::right)), Event::none);
    EXPECT_EQ(calendar_.exit(), Direction::right);
    EXPECT_EQ(send(calendar_, nav(Direction::up)), Event::none);
    EXPECT_EQ(calendar_.exit(), Direction::up);
    EXPECT_EQ(calendar_.focus(), (CalendarDate{2026, 10, 4}));
    EXPECT_EQ(send(calendar_, nav(Direction::left)), Event::moved);
    EXPECT_EQ(calendar_.exit(), Direction::none);

    calendar_.set_limits({2026, 10, 1}, {2026, 10, 31});
    calendar_.set_focus({2026, 10, 31});
    EXPECT_EQ(send(calendar_, nav(Direction::right)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_EQ(calendar_.focus(), (CalendarDate{2026, 10, 31}));
}

TEST_F(ComponentsData, CalendarSelectsADayOrARange)
{
    EXPECT_FALSE(calendar_.has_selection());
    EXPECT_EQ(send(calendar_, press(Action::confirm)), Event::changed);
    EXPECT_TRUE(asked(Cue::toggle));
    EXPECT_EQ(calendar_.selected(), (CalendarDate{2026, 10, 2}));

    calendar_.disabled = [](const CalendarDate &date) { return date.day == 13; };
    calendar_.set_focus({2026, 10, 13});
    EXPECT_EQ(send(calendar_, press(Action::confirm)), Event::refused);
    EXPECT_EQ(calendar_.selected(), (CalendarDate{2026, 10, 2}));

    // A range: confirm the start, then the end; picked backwards they swap.
    calendar_.style.select = hui::ui::CalendarSelect::range;
    calendar_.clear_selection();
    calendar_.set_focus({2026, 10, 20});
    EXPECT_EQ(send(calendar_, press(Action::confirm)), Event::changed);
    EXPECT_TRUE(calendar_.has_selection());
    EXPECT_FALSE(calendar_.has_range());
    calendar_.set_focus({2026, 10, 9});
    EXPECT_EQ(send(calendar_, press(Action::confirm)), Event::changed);
    EXPECT_TRUE(calendar_.has_range());
    EXPECT_EQ(calendar_.range_start(), (CalendarDate{2026, 10, 9}));
    EXPECT_EQ(calendar_.range_end(), (CalendarDate{2026, 10, 20}));
    // A third confirm starts a new range.
    EXPECT_EQ(send(calendar_, press(Action::confirm)), Event::changed);
    EXPECT_FALSE(calendar_.has_range());

    calendar_.style.select = hui::ui::CalendarSelect::none;
    EXPECT_EQ(send(calendar_, press(Action::confirm)), Event::activated);
    EXPECT_EQ(send(calendar_, press(Action::back)), Event::cancelled);
}

TEST_F(ComponentsData, CountdownWarnsTicksAndFinishes)
{
    hui::ui::Countdown timer;
    EXPECT_FALSE(timer.finished());
    timer.style.warning_at = 30.0f;
    timer.style.danger_at = 10.0f;
    timer.style.pulse_at = 3.0f;
    timer.start(65.0f);
    EXPECT_EQ(timer.text(), "1:05");
    EXPECT_EQ(timer.zone(), hui::ui::Status::neutral);
    timer.set_remaining(29.0f);
    EXPECT_EQ(timer.zone(), hui::ui::Status::warning);
    timer.set_remaining(4.2f);
    EXPECT_EQ(timer.zone(), hui::ui::Status::danger);
    EXPECT_EQ(timer.text(), "0:05"); // rounded up: zero only when it is over

    // No tick above the pulse threshold, one per second below it.
    feedback_.clear();
    for (int i = 0; i < 30; ++i)
        timer.update(kFrame, feedback_); // 4.2 -> 3.7: crosses 4.0, still above 3
    EXPECT_FALSE(asked(Cue::slider));
    for (int i = 0; i < 60; ++i)
        timer.update(kFrame, feedback_); // crosses 3.0
    EXPECT_TRUE(asked(Cue::slider));
    EXPECT_FALSE(asked(Cue::notify));
    timer.pause();
    const float held = timer.remaining();
    timer.update(1.0f, feedback_);
    EXPECT_FLOAT_EQ(timer.remaining(), held);
    timer.resume();
    for (int i = 0; i < 240 && !timer.finished(); ++i)
        timer.update(kFrame, feedback_);
    EXPECT_TRUE(timer.finished());
    EXPECT_FALSE(timer.running());
    EXPECT_TRUE(asked(Cue::notify));
    EXPECT_EQ(timer.text(), "0:00");

    timer.start(3725.0f);
    EXPECT_EQ(timer.text(), "1:02:05");
    EXPECT_FALSE(timer.finished());
}

TEST_F(ComponentsData, StorageBarAndStatusBarKeepTheirFigures)
{
    hui::ui::StorageBar storage;
    storage.set_capacity(800.0f);
    storage.set_categories({{"Games", 400.0f}, {"Media", 100.0f}, {"Saves", 20.0f}}, true);
    storage.set_bounds({100.0f, 100.0f, 700.0f, 84.0f});
    EXPECT_FLOAT_EQ(storage.used(), 520.0f);
    EXPECT_FLOAT_EQ(storage.free(), 280.0f);
    storage.set_active(true);
    EXPECT_EQ(send(storage, nav(Direction::right)), Event::moved);
    EXPECT_EQ(storage.focus(), 1);
    EXPECT_EQ(send(storage, nav(Direction::right)), Event::moved);
    EXPECT_EQ(send(storage, nav(Direction::right)), Event::refused);
    storage.style.exits.down = true;
    EXPECT_EQ(send(storage, nav(Direction::down)), Event::none);
    EXPECT_EQ(storage.exit(), Direction::down);
    // More than the drive holds leaves no free space, not a negative one.
    storage.set_categories({{"Games", 900.0f}});
    EXPECT_FLOAT_EQ(storage.free(), 0.0f);
    EXPECT_EQ(storage.focus(), 0);

    hui::ui::StatusBar bar;
    bar.set_battery(1.6f, true);
    EXPECT_FLOAT_EQ(bar.battery(), 1.0f);
    EXPECT_TRUE(bar.charging());
    bar.set_signal(9);
    EXPECT_EQ(bar.signal(), bar.style.signal_bars);
    bar.set_signal(-2);
    EXPECT_EQ(bar.signal(), 0);
    bar.set_player(2, true);
    bar.set_player(7, true); // no such slot: ignored
    EXPECT_TRUE(bar.player(2).connected);
    EXPECT_FALSE(bar.player(3).connected);
}

TEST_F(ComponentsData, DrawInEveryThemeAndVariantAndKeepTheirState)
{
    hui::ui::DetailList details;
    details.set_items({{"System", "", true},
                       {"Version", "1.4.2", false, true},
                       {"Renderer", "OpenGL 4.6 core, instanced signed-distance shapes"},
                       {"Latency", "16.6 ms", false, false, true}});
    details.set_bounds({100.0f, 100.0f, 520.0f, 260.0f});
    hui::ui::Timeline feed;
    std::vector<hui::ui::TimelineEntry> entries(3);
    entries[0].title = "Today";
    entries[0].group = true;
    entries[1].time = "21:14";
    entries[1].title = "Trophy earned";
    entries[1].body = "First light: finish a chapter without a lantern.";
    entries[1].status = hui::ui::Status::success;
    entries[2].time = "Mon";
    entries[2].title = "Update installed";
    entries[2].card_height = 40.0f;
    feed.set_entries(entries);
    feed.set_bounds({100.0f, 100.0f, 520.0f, 260.0f});
    hui::ui::Countdown timer;
    timer.label = "Next match";
    timer.start(8.0f);
    hui::ui::StorageBar storage;
    storage.title = "Console storage";
    storage.set_capacity(825.0f);
    storage.set_categories({{"Games", 412.0f}, {"Media", 96.0f}, {"Saves", 12.0f}});
    storage.set_bounds({100.0f, 100.0f, 700.0f, 84.0f});
    hui::ui::StatusBar bar;
    bar.title = "Lantern Pass";
    bar.clock = "21:47";
    bar.set_battery(0.15f, true);
    bar.set_signal(0);
    bar.set_player(0, true);
    bar.style.battery_percent = true;
    hui::ui::Legend legend;
    legend.set_items({{"Solo", hui::gfx::Color::rgb(0x3e9eff), "12"}, {"Co-op", {}, ""}});

    table_.style.pinned_id = 103;
    table_.sort_by(1, hui::ui::SortOrder::descending);
    table_.set_focus(2);
    calendar_.set_marks({{{2026, 10, 9}}, {{2026, 10, 9}, hui::ui::Status::danger}});
    calendar_.select_range({2026, 10, 5}, {2026, 10, 14});
    bars_.set_focus(1, 1);
    line_.set_focus(2);
    donut_.set_focus(1);
    using hui::ui::HighlightKind;
    constexpr HighlightKind kKinds[] = {HighlightKind::tint, HighlightKind::fill,
                                        HighlightKind::ring, HighlightKind::glow};

    for (const hui::ui::Theme &theme : hui::ui::themes())
    {
        for (int variant = 0; variant < 4; ++variant)
        {
            const bool odd = variant % 2 == 1;
            const HighlightKind kind = kKinds[variant];
            for (hui::ui::ComponentStyle *style :
                 {static_cast<hui::ui::ComponentStyle *>(&table_.style),
                  static_cast<hui::ui::ComponentStyle *>(&details.style),
                  static_cast<hui::ui::ComponentStyle *>(&feed.style),
                  static_cast<hui::ui::ComponentStyle *>(&bars_.style),
                  static_cast<hui::ui::ComponentStyle *>(&line_.style),
                  static_cast<hui::ui::ComponentStyle *>(&donut_.style),
                  static_cast<hui::ui::ComponentStyle *>(&calendar_.style),
                  static_cast<hui::ui::ComponentStyle *>(&timer.style),
                  static_cast<hui::ui::ComponentStyle *>(&storage.style),
                  static_cast<hui::ui::ComponentStyle *>(&bar.style),
                  static_cast<hui::ui::ComponentStyle *>(&legend.style)})
            {
                style->theme = theme;
                style->reduced_motion = variant == 3;
            }
            table_.style.highlight.kind = kind;
            table_.style.lines = static_cast<hui::ui::TableLines>(variant % 3);
            table_.style.multi_select = odd;
            table_.style.rank = !odd;
            table_.style.panel = variant != 2;
            details.style.highlight.kind = kind;
            details.style.layout = static_cast<hui::ui::DetailLayout>(variant % 3);
            details.style.panel = !odd;
            feed.style.highlight.kind = kind;
            feed.style.time = static_cast<hui::ui::TimelineTime>(variant % 3);
            feed.style.node = odd ? hui::ui::TimelineNode::hollow : hui::ui::TimelineNode::filled;
            feed.style.panel = !odd;
            bars_.style.horizontal = variant >= 2;
            bars_.style.layout = odd ? hui::ui::BarLayout::stacked : hui::ui::BarLayout::grouped;
            bars_.style.value_labels = odd;
            bars_.style.panel = variant != 2;
            bars_.style.legend = static_cast<hui::ui::LegendPlacement>(variant);
            line_.style.shape = static_cast<hui::ui::LineShape>(variant % 3);
            line_.style.area = !odd;
            line_.style.points = odd;
            line_.style.panel = variant != 2;
            donut_.style.legend = static_cast<hui::ui::LegendPlacement>(variant);
            donut_.style.percent = odd;
            donut_.style.center = variant != 3;
            calendar_.style.highlight.kind = kind;
            calendar_.style.select = static_cast<hui::ui::CalendarSelect>(variant % 3);
            calendar_.style.week_start =
                odd ? hui::ui::WeekStart::sunday : hui::ui::WeekStart::monday;
            calendar_.style.outside_days = !odd;
            timer.style.shape = odd ? hui::ui::CountdownShape::ring : hui::ui::CountdownShape::text;
            timer.set_bounds(variant == 3 ? hui::gfx::Rect{100.0f, 100.0f, 120.0f, 150.0f}
                                          : hui::gfx::Rect{100.0f, 100.0f, 240.0f, 56.0f});
            storage.style.header = !odd;
            storage.style.on_panel = odd;
            bar.style.panel = !odd;
            legend.style.vertical = odd;

            // Halfway through an animation is where a bad number would hide.
            for (int frame = 0; frame < 3; ++frame)
            {
                table_.update(kFrame);
                details.update(kFrame);
                feed.update(kFrame);
                bars_.update(kFrame);
                line_.update(kFrame);
                donut_.update(kFrame);
                calendar_.update(kFrame);
                timer.update(kFrame);
                storage.update(kFrame);
                bar.update(kFrame);
                legend.update(kFrame);
            }
            hui::ui::Canvas target = canvas();
            table_.draw(target);
            details.draw(target);
            feed.draw(target);
            bars_.draw(target);
            line_.draw(target);
            donut_.draw(target);
            calendar_.draw(target);
            timer.draw(target);
            storage.draw(target);
            bar.draw(target);
            legend.draw(target);
            expect_drawn(theme.id);
        }
        // Restyling is not a reset.
        EXPECT_EQ(table_.focus(), 2) << theme.id;
        EXPECT_EQ(table_.sort_column(), 1) << theme.id;
        EXPECT_EQ(bars_.focus(), 1) << theme.id;
        EXPECT_EQ(line_.focus(), 2) << theme.id;
        EXPECT_EQ(donut_.focus(), 1) << theme.id;
        EXPECT_EQ(calendar_.focus(), (CalendarDate{2026, 10, 2})) << theme.id;
        EXPECT_TRUE(calendar_.has_range()) << theme.id;
        EXPECT_EQ(details.focus(), 1) << theme.id;
        EXPECT_EQ(feed.focus(), 1) << theme.id;
    }

    // An empty component draws its panel and nothing else, without a fault.
    hui::ui::Table empty_table;
    hui::ui::BarChart empty_bars;
    hui::ui::LineChart empty_line;
    hui::ui::DonutChart empty_donut;
    hui::ui::Timeline empty_feed;
    hui::ui::DetailList empty_details;
    hui::ui::Canvas target = canvas();
    empty_table.draw(target);
    empty_bars.draw(target);
    empty_line.draw(target);
    empty_donut.draw(target);
    empty_feed.draw(target);
    empty_details.draw(target);
    EXPECT_EQ(empty_table.handle(nav(Direction::down), feedback_), Event::none);
    EXPECT_EQ(empty_bars.handle(nav(Direction::right), feedback_), Event::none);
    EXPECT_EQ(empty_donut.handle(nav(Direction::right), feedback_), Event::none);
    expect_drawn("empty");
}

} // namespace
