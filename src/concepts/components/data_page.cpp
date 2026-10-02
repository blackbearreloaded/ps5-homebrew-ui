// ps5-homebrew-ui - Component Library page: the data components on one dashboard.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A dashboard is the honest test of data components: a leaderboard table,
// three charts, a calendar, an activity feed and a spec sheet share one
// screen under a strip of status widgets, and each must stay readable at
// that size in every theme. The page owns only the focus between them: a
// component handles the D-pad itself and reports the edge the focus left
// through (ui::EdgeExits), and the page hands it to the neighbour on that
// side. Triangle sorts the table or deals the charts new data, which eases
// in; Square swaps a whole set of knobs at once.

#include "concepts/components/page.hpp"

#include "ui/components/calendar.hpp"
#include "ui/components/chart.hpp"
#include "ui/components/detail_list.hpp"
#include "ui/components/status.hpp"
#include "ui/components/table.hpp"
#include "ui/components/timeline.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace hui::concepts::gallery
{

namespace
{

using gfx::Color;
using gfx::Rect;

// ---- layout: a strip of status widgets, then two rows of panels ----
constexpr Rect kStatus{96.0f, 240.0f, 650.0f, 52.0f};
constexpr Rect kTimer{762.0f, 240.0f, 252.0f, 52.0f};
constexpr Rect kStorage{1030.0f, 240.0f, 794.0f, 58.0f};
constexpr float kCaptionA = 326.0f; // caption baselines
constexpr float kCaptionB = 644.0f;
constexpr Rect kTable{96.0f, 338.0f, 600.0f, 276.0f};
constexpr Rect kBar{716.0f, 338.0f, 356.0f, 276.0f};
constexpr Rect kLine{1092.0f, 338.0f, 356.0f, 276.0f};
constexpr Rect kDonut{1468.0f, 338.0f, 356.0f, 276.0f};
constexpr Rect kCalendar{96.0f, 656.0f, 420.0f, 292.0f};
constexpr Rect kTimeline{536.0f, 656.0f, 560.0f, 292.0f};
constexpr Rect kDetails{1116.0f, 656.0f, 708.0f, 292.0f};

// What can take the focus.
enum Item : int
{
    kItemStorage,
    kItemTable,
    kItemBar,
    kItemLine,
    kItemDonut,
    kItemCalendar,
    kItemTimeline,
    kItemDetails,
    kItems,
};

constexpr Rect kItemRects[kItems] = {kStorage, kTable,    kBar,      kLine,
                                     kDonut,   kCalendar, kTimeline, kDetails};
constexpr const char *kCaptions[kItems] = {
    "", "Table", "BarChart", "LineChart", "DonutChart", "Calendar", "Timeline", "DetailList",
};

// Who is beside whom: [item][up, down, left, right], -1 where the page ends.
constexpr int kNeighbours[kItems][4] = {
    {-1, kItemLine, -1, -1},                     // storage
    {kItemStorage, kItemCalendar, -1, kItemBar}, // table
    {kItemStorage, kItemTimeline, kItemTable, kItemLine},
    {kItemStorage, kItemDetails, kItemBar, kItemDonut},
    {kItemStorage, kItemDetails, kItemLine, -1},
    {kItemTable, -1, -1, kItemTimeline},         // calendar
    {kItemBar, -1, kItemCalendar, kItemDetails}, // timeline
    {kItemLine, -1, kItemTimeline, -1},          // details
};

struct Variant
{
    const char *name;
    ui::TableLines lines;
    ui::HighlightKind highlight;
    bool marks; // the table marks rows instead of activating them
    bool horizontal;
    ui::BarLayout bars;
    bool value_labels;
    ui::LineShape line;
    bool area;
    bool points;
    float donut_thickness;
    bool percent;
    ui::CalendarSelect select;
    ui::WeekStart week;
    ui::DetailLayout details;
    ui::TimelineTime time;
    ui::TimelineNode node;
    ui::CountdownShape timer;
    bool chart_panels;
};
constexpr Variant kVariants[] = {
    {"Zebra, grouped bars, one day, rows", ui::TableLines::zebra, ui::HighlightKind::tint, false,
     false, ui::BarLayout::grouped, false, ui::LineShape::theme, true, false, 0.3f, false,
     ui::CalendarSelect::single, ui::WeekStart::monday, ui::DetailLayout::rows,
     ui::TimelineTime::column, ui::TimelineNode::filled, ui::CountdownShape::text, true},
    {"Marks, stacked bars, a range, grid", ui::TableLines::dividers, ui::HighlightKind::bar, true,
     false, ui::BarLayout::stacked, true, ui::LineShape::stepped, false, true, 0.46f, true,
     ui::CalendarSelect::range, ui::WeekStart::sunday, ui::DetailLayout::grid,
     ui::TimelineTime::line, ui::TimelineNode::hollow, ui::CountdownShape::ring, true},
    {"Plain, bars across, no panels, stacked", ui::TableLines::plain, ui::HighlightKind::fill,
     false, true, ui::BarLayout::grouped, true, ui::LineShape::straight, true, true, 0.2f, false,
     ui::CalendarSelect::single, ui::WeekStart::monday, ui::DetailLayout::stacked,
     ui::TimelineTime::none, ui::TimelineNode::filled, ui::CountdownShape::text, false},
};
constexpr int kVariantCount = static_cast<int>(std::size(kVariants));

constexpr ui::Hint kHints[] = {
    {ui::Button::dpad, "Move; past an edge, the next panel"},
    {ui::Button::cross, "Select"},
    {ui::Button::triangle, "Sort / new data"},
    {ui::Button::square, "Variant"},
};

constexpr std::uint32_t kConfirm = action_bit(Action::confirm);
constexpr std::uint32_t kSquare = action_bit(Action::west);
constexpr std::uint32_t kTriangle = action_bit(Action::north);
constexpr app::TourStep kTour[] = {
    {1.2f, 0, Direction::down},
    {0.3f, 0, Direction::down},
    {0.8f, kTriangle, Direction::none, "data"},
    {0.7f, 0, Direction::right},
    {0.3f, 0, Direction::right},
    {0.3f, 0, Direction::right},
    {0.5f, kTriangle},
    {1.0f, 0, Direction::down, "data-charts"},
    {0.4f, 0, Direction::down},
    {0.4f, 0, Direction::left},
    {0.3f, 0, Direction::down},
    {0.3f, kConfirm},
    {0.6f, kSquare},
    {0.4f, kConfirm},
    {0.25f, 0, Direction::down},
    {0.25f, 0, Direction::right},
    {0.3f, kConfirm},
    {0.9f, kSquare, Direction::none, "data-range"},
    {0.5f, 0, Direction::up},
    {0.3f, 0, Direction::up},
    {0.3f, 0, Direction::up},
    {0.9f, kSquare, Direction::none, "data-plain"},
    {0.4f, 0, Direction::up},
};

constexpr const char *kPlayers[] = {
    "Mara Voss",   "Tobi Okafor", "Lin Chen",    "Ada Brandt",  "Rui Sato",
    "Noor Haddad", "Eli Marsh",   "Ines Duarte", "Kofi Mensah",
};
constexpr int kYou = 100; // the id of the pinned row
constexpr const char *kDays[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
constexpr const char *kHours[] = {"00", "03", "06", "09", "12", "15", "18", "21"};

class DataPage final : public Page
{
  public:
    explicit DataPage(app::Context &context) : context_(context)
    {
        fill();
        restyle(ui::default_theme(), false);
    }

    const char *title() const override
    {
        return "Data";
    }
    const char *summary() const override
    {
        return "A table, charts, a calendar, a feed, a spec sheet and status widgets";
    }
    const char *variant() const override
    {
        return kVariants[variant_].name;
    }

    void restyle(const ui::Theme &theme, bool reduced_motion) override
    {
        theme_ = theme;
        const auto base = [&](ui::ComponentStyle &style)
        {
            style.theme = theme;
            style.reduced_motion = reduced_motion;
        };
        base(base_);
        base(status_.style);
        base(timer_.style);
        base(storage_.style);
        base(table_.style);
        base(bars_.style);
        base(line_.style);
        base(donut_.style);
        base(calendar_.style);
        base(feed_.style);
        base(details_.style);
        apply_variant();
    }

    void enter() override
    {
        age_ = 0.0f;
        storage_.enter();
        table_.enter();
        bars_.enter();
        line_.enter();
        donut_.enter();
        calendar_.enter();
        feed_.enter();
        details_.enter();
    }

    void update(const InputFrame &input, float dt, ui::Feedback &feedback) override
    {
        clock_ += dt;
        age_ += dt;
        if (input.is_pressed(Action::west))
        {
            variant_ = (variant_ + 1) % kVariantCount;
            apply_variant();
            ui::play_cue(feedback, base_, base_.sounds.change);
        }
        else if (input.is_pressed(Action::north))
        {
            if (focus_ == kItemTable)
            {
                if (table_.cycle_sort(feedback) == ui::Event::changed)
                    say_sort();
            }
            else
            {
                deal(false);
                feed_.enter();
                ui::play_cue(feedback, base_, base_.sounds.activate, kItemRects[focus_].cx());
                say("New data: the values ease to it");
            }
        }
        else
        {
            route(input, feedback);
        }

        advance(dt);
        status_.update(dt);
        timer_.update(dt, feedback);
        storage_.update(dt);
        table_.update(dt);
        bars_.update(dt);
        line_.update(dt);
        donut_.update(dt);
        calendar_.update(dt);
        feed_.update(dt);
        details_.update(dt);
        edge_.update(dt, 9.0f);
    }

    void draw(ui::Canvas &canvas) const override
    {
        ui::Painter paint(canvas.list, canvas.fonts, theme_, canvas.glass);
        const float shown = tween::cubic_out(age_ / 0.3f);
        const float nudge = ui::shake(edge_.value, clock_, 10.0f);
        canvas.list.push_opacity(shown);
        for (int item = kItemTable; item < kItems; ++item)
        {
            const bool active = item == focus_;
            const Rect &r = kItemRects[item];
            paint.label(ui::fit_label(paint, kCaptions[item], 20.0f, r.w),
                        r.x + (active ? nudge : 0.0f), item < kItemCalendar ? kCaptionA : kCaptionB,
                        20.0f, active ? paint.page_text() : paint.page_text_muted());
        }
        canvas.list.pop_opacity();

        status_.draw(canvas);
        timer_.draw(canvas);
        storage_.draw(canvas);
        table_.draw(canvas);
        calendar_.draw(canvas);
        feed_.draw(canvas);
        details_.draw(canvas);
        donut_.draw(canvas);
        // The charts with a readout go last: the bubble may reach past its
        // panel, and must not slip under a neighbour.
        if (focus_ == kItemBar)
        {
            line_.draw(canvas);
            bars_.draw(canvas);
        }
        else
        {
            bars_.draw(canvas);
            line_.draw(canvas);
        }
    }

    std::span<const ui::Hint> hints() const override
    {
        return kHints;
    }
    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    // ---- set-up ------------------------------------------------------------

    void fill()
    {
        status_.set_bounds(kStatus);
        status_.set_battery(0.34f, false);
        status_.set_signal(4);
        status_.set_player(0, true);
        storage_.set_bounds(kStorage);
        storage_.set_capacity(825.0f);
        timer_.label = "Next match";
        timer_.start(40.0f);

        // The leaderboard. The last column is drawn by the page: the results
        // of a player's last five matches, kept in the row's tag.
        std::vector<ui::TableColumn> columns(4);
        columns[0].title = "Player";
        columns[0].flex = 1.0f;
        columns[0].strong = true;
        columns[1].title = "Score";
        columns[1].width = 96.0f;
        columns[1].align = gfx::Align::right;
        columns[1].numeric = true;
        columns[2].title = "Time";
        columns[2].width = 104.0f;
        columns[2].align = gfx::Align::right;
        columns[2].numeric = true;
        columns[3].title = "Form";
        columns[3].width = 86.0f;
        columns[3].sortable = false;
        columns[3].cell =
            [this](ui::Canvas &canvas, const Rect &cell, const ui::TableRow &row, float)
        {
            for (int i = 0; i < 5; ++i)
            {
                const bool won = ((row.tag >> i) & 1) != 0;
                const Color tone = ui::visible_on(theme_, won ? theme_.success : theme_.danger,
                                                  table_.style.panel);
                ui::draw_marker(canvas, theme_, cell.x + 7.0f + static_cast<float>(i) * 17.0f,
                                cell.cy(), won ? 6.0f : 4.0f, tone.with_alpha(won ? 1.0f : 0.75f));
            }
        };
        table_.set_columns(std::move(columns));
        table_.set_bounds(kTable);
        table_.style.pinned_id = kYou;
        table_.set_rows(leaderboard());
        table_.sort_by(1, ui::SortOrder::descending, false);
        table_.set_focus(0);

        bars_.title = "Hours played";
        bars_.style.axis.unit = "h";
        bars_.set_bounds(kBar);
        bars_.set_categories(std::vector<std::string>(std::begin(kDays), std::end(kDays)));
        line_.title = "Players online";
        line_.set_bounds(kLine);
        line_.set_labels(std::vector<std::string>(std::begin(kHours), std::end(kHours)));
        donut_.title = "Library";
        donut_.style.center_label = "Titles";
        donut_.set_bounds(kDonut);
        deal(true);

        calendar_.set_bounds(kCalendar);
        calendar_.set_today({2026, 10, 2});
        calendar_.set_marks({
            {{2026, 10, 2}},
            {{2026, 10, 9}, ui::Status::success},
            {{2026, 10, 9}, ui::Status::warning},
            {{2026, 10, 14}},
            {{2026, 10, 17}, ui::Status::danger},
            {{2026, 10, 23}, ui::Status::success},
            {{2026, 11, 5}},
            {{2026, 9, 26}, ui::Status::warning},
        });
        // Maintenance windows: nothing can be booked then.
        calendar_.disabled = [](const ui::CalendarDate &date)
        { return date.day == 13 || date.day == 27; };

        std::vector<ui::TimelineEntry> entries(7);
        entries[0].title = "Today";
        entries[0].group = true;
        entries[1].time = "21:14";
        entries[1].title = "Trophy earned";
        entries[1].body = "First light: finish a chapter without a lantern.";
        entries[1].status = ui::Status::success;
        entries[2].time = "20:02";
        entries[2].title = std::string("Played ") + context_.catalog[1].title;
        entries[2].card_height = 52.0f;
        entries[2].status = ui::Status::primary;
        entries[3].time = "18:40";
        entries[3].title = "Download stalled";
        entries[3].body = "The connection dropped at 62%. It resumes by itself.";
        entries[3].status = ui::Status::danger;
        entries[4].title = "Earlier";
        entries[4].group = true;
        entries[5].time = "Tue";
        entries[5].title = "Tobi Okafor joined your party";
        entries[5].status = ui::Status::accent;
        entries[6].time = "Mon";
        entries[6].title = "Update 1.4.2 installed";
        entries[6].body = "Faster loading and two new challenges.";
        entries[6].status = ui::Status::warning;
        feed_.set_entries(std::move(entries));
        feed_.set_bounds(kTimeline);
        feed_.card = [this](ui::Canvas &canvas, const Rect &area, const ui::TimelineEntry &, int,
                            float focus)
        {
            // The card of the "played" entry: a cover and two lines.
            ui::Painter paint(canvas.list, canvas.fonts, theme_, canvas.glass);
            const demo::Item &item = context_.catalog[1];
            const Rect art{area.x, area.y, area.h, area.h};
            const float radius = std::min(theme_.radius, 8.0f);
            if (item.cover != 0)
                canvas.list.image(item.cover, art, gfx::kCanvasUv, Color::rgb(0xffffff), radius);
            else
                canvas.list.gradient_rect(art, radius, item.mid, item.dark);
            const bool on_panel = feed_.style.panel;
            const ui::Inks ink = ui::inks(paint, on_panel);
            const Color strong =
                ui::Highlight::text_color(feed_.style, feed_.style.highlight, focus, ink.text);
            const float x = art.x + art.w + 14.0f;
            const float room = area.x + area.w - x;
            paint.body(
                ui::fit_body(paint, std::string(item.genre) + ", " + item.studio, 19.0f, room), x,
                area.y + 21.0f, 19.0f, gfx::mix(ink.muted, strong, focus * 0.6f));
            char text[48];
            std::snprintf(text, sizeof(text), "%d h played, chapter 4 of 9", item.hours);
            paint.body(ui::fit_body(paint, text, 19.0f, room), x, area.y + 45.0f, 19.0f,
                       gfx::mix(ink.muted, strong, focus * 0.6f));
        };

        std::vector<ui::DetailItem> about(9);
        about[0].label = "Application";
        about[0].header = true;
        about[1].label = "Version";
        about[1].value = "1.4.2";
        about[1].focusable = true;
        about[1].numeric = true;
        about[2].label = "Build";
        about[2].value = "8841";
        about[2].focusable = true;
        about[2].numeric = true;
        about[3].label = "Renderer";
        about[3].value = "OpenGL 4.6 core, instanced signed-distance shapes";
        about[4].label = "Licence";
        about[4].value = "GPL-3.0-or-later";
        about[5].label = "Session";
        about[5].header = true;
        about[6].label = "Signed in as";
        about[6].value = kPlayers[0];
        about[7].label = "Save folder";
        about[7].value = "/data/homebrew/lantern-pass/saves/slot-3";
        about[7].focusable = true;
        about[8].label = "Connection";
        about[8].value = "Wired, 940 Mbps";
        about[8].color = ui::default_theme().success;
        details_.set_items(std::move(about));
        details_.set_bounds(kDetails);
    }

    static std::vector<ui::TableRow> leaderboard()
    {
        constexpr int kScores[] = {48210, 44975, 41300, 39880, 36420, 33150, 30990, 27640, 21875};
        constexpr int kSeconds[] = {6127, 5980, 7214, 6630, 8042, 5411, 7790, 9020, 6808};
        constexpr int kForm[] = {0b11011, 0b10111, 0b01110, 0b11100, 0b00111,
                                 0b10101, 0b01011, 0b00110, 0b10001};
        std::vector<ui::TableRow> rows;
        const auto add = [&rows](int id, const char *name, int score, int seconds, int form)
        {
            ui::TableRow row;
            row.id = id;
            row.tag = form;
            char text[32];
            row.cells.push_back({name, 0.0});
            std::snprintf(text, sizeof(text), "%d", score);
            row.cells.push_back({text, static_cast<double>(score)});
            std::snprintf(text, sizeof(text), "%d:%02d:%02d", seconds / 3600, (seconds / 60) % 60,
                          seconds % 60);
            row.cells.push_back({text, static_cast<double>(seconds)});
            row.cells.push_back({"", 0.0});
            rows.push_back(row);
        };
        for (std::size_t i = 0; i < std::size(kPlayers); ++i)
            add(static_cast<int>(i), kPlayers[i], kScores[i], kSeconds[i], kForm[i]);
        add(kYou, "You", 35010, 6394, 0b11101);
        return rows;
    }

    // The same components, another set of knobs.
    void apply_variant()
    {
        const Variant &v = kVariants[variant_];
        ui::HighlightStyle highlight;
        highlight.kind = v.highlight;

        ui::TableStyle &table = table_.style;
        table.lines = v.lines;
        table.highlight = highlight;
        table.multi_select = v.marks;
        table.rank = !v.marks;
        table.row_height = 40.0f;
        table.header_height = 40.0f;
        table.text_size = 21.0f;
        table.header_size = 16.0f;
        table.exits = {true, true, true, true};
        // Sizes changed: have the table lay its rows out again.
        table_.set_rows(table_.rows());
        table_.sort_by(table_.sort_column(), table_.sort_order(), false);

        for (ui::ChartStyle *chart : {static_cast<ui::ChartStyle *>(&bars_.style),
                                      static_cast<ui::ChartStyle *>(&line_.style),
                                      static_cast<ui::ChartStyle *>(&donut_.style)})
        {
            chart->panel = v.chart_panels;
            chart->exits = {true, true, true, true};
        }
        bars_.style.horizontal = v.horizontal;
        bars_.style.layout = v.bars;
        bars_.style.value_labels = v.value_labels && v.bars == ui::BarLayout::stacked;
        bars_.style.highlight = highlight;
        if (v.highlight == ui::HighlightKind::fill || v.highlight == ui::HighlightKind::bar)
            bars_.style.highlight.kind = ui::HighlightKind::tint; // a plate behind bars stays faint
        line_.style.shape = v.line;
        line_.style.area = v.area;
        line_.style.points = v.points;
        donut_.style.thickness = v.donut_thickness;
        donut_.style.percent = v.percent;
        donut_.style.legend =
            variant_ == 2 ? ui::LegendPlacement::bottom : ui::LegendPlacement::side;

        calendar_.style.select = v.select;
        calendar_.style.week_start = v.week;
        calendar_.style.exits = {true, false, false, true};
        calendar_.style.highlight.kind = v.highlight == ui::HighlightKind::bar
                                             ? ui::HighlightKind::tint
                                             : ui::HighlightKind::ring;
        calendar_.style.title_height = 38.0f;
        calendar_.style.weekday_height = 24.0f;
        calendar_.style.padding = 12.0f;
        // A range and a single day are different answers: changing the
        // question starts again.
        if ((v.select == ui::CalendarSelect::range) != ranged_)
            calendar_.clear_selection();
        ranged_ = v.select == ui::CalendarSelect::range;

        feed_.style.time = v.time;
        feed_.style.node = v.node;
        feed_.style.highlight = highlight;
        feed_.style.panel = v.chart_panels;
        feed_.style.exits = {true, true, true, true};
        feed_.style.title_size = 21.0f;
        feed_.style.padding_y = 8.0f;
        feed_.style.group_height = 34.0f;

        details_.style.layout = v.details;
        details_.style.highlight = highlight;
        details_.style.panel = v.chart_panels;
        details_.style.exits = {true, true, true, true};
        details_.style.columns = 3;
        details_.style.row_height = 40.0f;
        details_.style.header_height = 36.0f;
        details_.style.value_size = 21.0f;
        details_.style.padding_y = 6.0f;
        details_.set_items(details_.items());
        // Both wrap text, and wrapping needs the fonts: lay them out now so
        // the first frame after a restyle is already right.
        feed_.measure(context_.fonts);
        details_.measure(context_.fonts);

        timer_.style.shape = v.timer;
        timer_.style.align = gfx::Align::center;
        timer_.style.warning_at = 30.0f;
        timer_.set_bounds(kTimer);
        storage_.style.header = false;
        storage_.style.spacing = 6.0f;
        storage_.style.exits = {false, true, false, false};
        status_.style.battery_percent = variant_ == 1;
        status_.style.panel = variant_ != 2;
        activate();
    }

    // ---- the pretend world -------------------------------------------------

    float random()
    {
        rng_ = rng_ * 1664525u + 1013904223u;
        return static_cast<float>(rng_ >> 8) / 16777216.0f;
    }

    // New figures for the charts and the drive. Nothing is redrawn: each
    // component is told the new values and eases to them.
    void deal(bool snap)
    {
        ui::ChartSeries solo{"Solo", {}};
        ui::ChartSeries shared{"Co-op", {}};
        for (std::size_t i = 0; i < std::size(kDays); ++i)
        {
            const float weekend = i >= 5 ? 2.0f : 0.0f;
            solo.values.push_back(std::round(1.0f + weekend + 4.0f * random()));
            shared.values.push_back(std::round(weekend + 3.0f * random()));
        }
        bars_.set_series({solo, shared}, snap);

        ui::ChartSeries today{"Today", {}};
        ui::ChartSeries before{"Last week", {}};
        float level = 500.0f + 400.0f * random();
        for (std::size_t i = 0; i < std::size(kHours); ++i)
        {
            // Quiet at night, busy in the evening.
            const float daytime = 0.35f + 0.65f * static_cast<float>(i) / 7.0f;
            level = std::clamp(level + (random() - 0.35f) * 700.0f, 200.0f, 2600.0f);
            today.values.push_back(std::round(level * daytime));
            before.values.push_back(std::round((900.0f + 900.0f * random()) * daytime));
        }
        line_.set_series({today, before}, snap);

        donut_.set_slices({{"Action", std::round(8.0f + 10.0f * random())},
                           {"Racing", std::round(3.0f + 6.0f * random())},
                           {"Puzzle", std::round(4.0f + 8.0f * random())},
                           {"RPG", std::round(2.0f + 5.0f * random())}},
                          snap);

        storage_.set_categories({{"Games", std::round(280.0f + 180.0f * random())},
                                 {"Media", std::round(60.0f + 70.0f * random())},
                                 {"Captures", std::round(30.0f + 50.0f * random())},
                                 {"Saves", std::round(6.0f + 12.0f * random())}},
                                snap);
    }

    void advance(float dt)
    {
        // The controller runs down, is plugged in, fills up, and again.
        if (charging_)
        {
            battery_ += 0.07f * dt;
            charging_ = battery_ < 0.98f;
        }
        else
        {
            battery_ -= 0.018f * dt;
            charging_ = battery_ < 0.12f;
        }
        status_.set_battery(battery_, charging_);
        constexpr int kSignal[] = {4, 3, 4, 2, 3, 0, 3, 4};
        status_.set_signal(kSignal[static_cast<int>(clock_ / 3.5f) % 8]);
        status_.set_player(1, clock_ > 1.5f);
        status_.set_player(2, std::fmod(clock_, 14.0f) > 7.0f);
        const int minutes = 21 * 60 + 47 + static_cast<int>(clock_ / 60.0f);
        char text[16];
        std::snprintf(text, sizeof(text), "%02d:%02d", (minutes / 60) % 24, minutes % 60);
        status_.clock = text;

        // What a component last reported stays in the strip for a moment.
        note_time_ -= dt;
        status_.title = note_time_ > 0.0f ? note_ : std::string("Lantern Pass");

        // The match starts, and the next one is announced.
        if (timer_.finished())
        {
            rest_ += dt;
            if (rest_ > 3.0f)
            {
                rest_ = 0.0f;
                timer_.start(40.0f);
            }
        }
    }

    void say(std::string text)
    {
        note_ = std::move(text);
        note_time_ = 3.0f;
    }

    void say_sort()
    {
        const int column = table_.sort_column();
        if (column < 0)
        {
            say("Table: unsorted");
            return;
        }
        say(std::string("Sorted by ") + table_.columns()[static_cast<std::size_t>(column)].title +
            (table_.sort_order() == ui::SortOrder::ascending ? ", ascending" : ", descending"));
    }

    // ---- focus -------------------------------------------------------------

    void activate()
    {
        storage_.set_active(focus_ == kItemStorage);
        table_.set_active(focus_ == kItemTable);
        bars_.set_active(focus_ == kItemBar);
        line_.set_active(focus_ == kItemLine);
        donut_.set_active(focus_ == kItemDonut);
        calendar_.set_active(focus_ == kItemCalendar);
        feed_.set_active(focus_ == kItemTimeline);
        details_.set_active(focus_ == kItemDetails);
    }

    // The focused component takes the input; when it reports that the focus
    // left through an edge, the neighbour on that side gets it.
    void route(const InputFrame &input, ui::Feedback &feedback)
    {
        ui::Event event = ui::Event::none;
        Direction exit = Direction::none;
        switch (focus_)
        {
        case kItemStorage:
            event = storage_.handle(input, feedback);
            exit = storage_.exit();
            break;
        case kItemTable:
            event = table_.handle(input, feedback);
            exit = table_.exit();
            if (event == ui::Event::changed && !table_.in_header())
            {
                char text[48];
                std::snprintf(text, sizeof(text), "Table: %d marked", table_.selected_count());
                say(text);
            }
            else if (event == ui::Event::changed)
                say_sort();
            else if (event == ui::Event::activated)
                say(std::string("Activated: ") +
                    table_.rows()[static_cast<std::size_t>(table_.focus())].cells[0].text);
            break;
        case kItemBar:
            event = bars_.handle(input, feedback);
            exit = bars_.exit();
            break;
        case kItemLine:
            event = line_.handle(input, feedback);
            exit = line_.exit();
            break;
        case kItemDonut:
            event = donut_.handle(input, feedback);
            exit = donut_.exit();
            break;
        case kItemCalendar:
            event = calendar_.handle(input, feedback);
            exit = calendar_.exit();
            if (event == ui::Event::changed)
                say_selection();
            break;
        case kItemTimeline:
            event = feed_.handle(input, feedback);
            exit = feed_.exit();
            if (event == ui::Event::activated)
                say(std::string("Opened: ") +
                    feed_.entries()[static_cast<std::size_t>(feed_.focus())].title);
            break;
        case kItemDetails:
            event = details_.handle(input, feedback);
            exit = details_.exit();
            if (event == ui::Event::activated)
                say(std::string("Copied: ") +
                    details_.items()[static_cast<std::size_t>(details_.focus())].value);
            break;
        default:
            break;
        }
        if (event != ui::Event::none || exit == Direction::none)
            return;

        const int side = static_cast<int>(exit) - static_cast<int>(Direction::up);
        const int next = kNeighbours[focus_][side];
        if (next < 0)
        {
            ui::refuse(feedback, base_, input, edge_, kItemRects[focus_].cx());
            return;
        }
        focus_ = next;
        arrive(exit);
        activate();
        ui::play_cue(feedback, base_, base_.sounds.move, kItemRects[focus_].cx(),
                     focus_ < kItemCalendar ? 1.04f : 0.96f);
    }

    // A chart entered from one side starts at that side, so crossing the
    // dashboard is one unbroken walk.
    void arrive(Direction travel)
    {
        const bool forward = travel == Direction::right || travel == Direction::down;
        const bool along_bars = bars_.style.horizontal
                                    ? travel == Direction::up || travel == Direction::down
                                    : travel == Direction::left || travel == Direction::right;
        if (focus_ == kItemBar && along_bars)
            bars_.set_focus(forward ? 0 : 99, forward ? 0 : 99);
        else if (focus_ == kItemLine && (travel == Direction::left || travel == Direction::right))
            line_.set_focus(forward ? 0 : 99);
        else if (focus_ == kItemDonut && travel == Direction::right)
            donut_.set_focus(0);
    }

    void say_selection()
    {
        const auto name = [](const ui::CalendarDate &date)
        {
            char text[24];
            std::snprintf(text, sizeof(text), "%d-%02d-%02d", date.year, date.month, date.day);
            return std::string(text);
        };
        if (calendar_.has_range())
            say("Range: " + name(calendar_.range_start()) + " to " + name(calendar_.range_end()));
        else if (calendar_.style.select == ui::CalendarSelect::range)
            say("Range starts " + name(calendar_.selected()) + "; confirm the end");
        else
            say("Selected: " + name(calendar_.selected()));
    }

    app::Context &context_;
    ui::Theme theme_ = ui::default_theme();
    ui::ComponentStyle base_; // the page's own cues
    int variant_ = 0;
    int focus_ = kItemTable;
    float clock_ = 0.0f;
    float age_ = 0.0f;
    ui::Pulse edge_;

    ui::StatusBar status_;
    ui::Countdown timer_;
    ui::StorageBar storage_;
    ui::Table table_;
    ui::BarChart bars_;
    ui::LineChart line_;
    ui::DonutChart donut_;
    ui::Calendar calendar_;
    ui::Timeline feed_;
    ui::DetailList details_;

    // The pretend world's state.
    std::uint32_t rng_ = 0x51d3a7u;
    float battery_ = 0.34f;
    bool charging_ = false;
    float rest_ = 0.0f;
    bool ranged_ = false;
    std::string note_;
    float note_time_ = 0.0f;
};

} // namespace

std::unique_ptr<Page> make_data_page(app::Context &context)
{
    return std::make_unique<DataPage>(context);
}

} // namespace hui::concepts::gallery
