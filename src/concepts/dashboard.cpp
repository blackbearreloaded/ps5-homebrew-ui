// ps5-homebrew-ui - Design "Pulse Dashboard": a bento grid of live data tiles.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A dark, technical dashboard: eight tiles of different sizes on a 12-column
// grid, some charting the app's own telemetry (frame times, draw calls,
// voices, uptime) and some charting the sample library. What makes it feel
// finished:
//
//   - focus moves spatially between tiles of different sizes: one small
//     function scores the candidates in the pressed direction (spatial_next),
//     and the same function drives the selection inside the detail views;
//   - one focus ring glides between the tiles and morphs to each tile's size
//     and accent colour, while the focused tile lifts, glows and casts a shadow;
//   - Cross expands the focused tile into a detail view as a shared element:
//     its rectangle and its label travel to their new places, the other tiles
//     fade and fall back, then the larger chart fades in. Circle retraces it;
//   - data arrives instead of appearing: numbers count up, bars, rings and
//     storage segments grow from zero with a stagger, the graph rises from its
//     baseline, and all of it replays when the design is entered;
//   - every tile has an honest empty state, because the real telemetry may be
//     all zeros: a baseline, a dash, a dim row of pips;
//   - focus ticks are panned and pitched by where the tile sits, grid edges
//     refuse softly, and Square freezes the live data with a visible chip.

#include "concepts/concepts.hpp"

#include "core/tween.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <span>
#include <string>

namespace hui::concepts
{

namespace
{

using gfx::Color;
using gfx::Rect;

// ---- the design language ---------------------------------------------------

const Color kInk = Color::rgb(0xe9eefc);
const Color kClear = Color::rgb(0x000000, 0.0f);
const Color kBlack = Color::rgb(0x000000);
const Color kPanelTop = Color::rgb(0x171f33);
const Color kPanelBottom = Color::rgb(0x0e1424);
const Color kNight = Color::rgb(0x0a0e1a); // text on a bright accent
// One accent per tile family.
const Color kCyan = Color::rgb(0x4fe0ff);  // the app's own telemetry
const Color kLime = Color::rgb(0xb8f26b);  // play activity
const Color kAmber = Color::rgb(0xffc24a); // the library
const Color kRose = Color::rgb(0xff6f9c);  // storage

constexpr float kMuted = 0.62f; // alpha of secondary text
constexpr float kFaint = 0.38f; // alpha of tertiary text and empty states

// The grid: twelve columns and four rows inside the safe area.
constexpr float kGridX = 96.0f;
constexpr float kGridY = 164.0f;
constexpr float kGridW = 1728.0f;
constexpr float kGridH = 800.0f;
constexpr float kGap = 20.0f;
constexpr float kRadius = 28.0f;
constexpr float kPad = 28.0f;    // inner padding of a tile
constexpr float kLabelY = 46.0f; // baseline of a tile's label
constexpr float kLabelSize = 17.0f;
constexpr float kLift = 0.02f; // extra scale of the focused tile

// The detail view the focused tile expands into.
constexpr Rect kDetail{96.0f, 150.0f, 1728.0f, 818.0f};
constexpr float kDetailRadius = 40.0f;
constexpr float kDetailPad = 56.0f;
constexpr float kLeft = kDetail.x + kDetailPad; // content edges
constexpr float kRight = kDetail.x + kDetail.w - kDetailPad;
constexpr float kWide = kRight - kLeft;
constexpr float kFlight = 0.001f; // below this a tile counts as back in the grid

constexpr float kBudgetMs = 16.67f; // one frame at 60 Hz
constexpr float kTau = 6.2831853f;
constexpr const char *kDash = "\xE2\x80\x94";

constexpr const char *kTechniques[] = {
    "Spatial focus navigation across tiles of mixed sizes, scored by one small function",
    "Shared-element expand and collapse: the tile's rectangle and label travel to the detail view",
    "One focus ring that glides and morphs to each tile's size and accent (ui::SpringRect)",
    "Charts that arrive: counting numbers, staggered bars, growing rings and segments",
    "Live telemetry with honest empty states, and a pause switch shown as a chip",
    "Focus sounds panned and pitched by the tile's position; soft refusals at the grid's edges",
};

enum Tile : int
{
    kFrame,
    kPlay,
    kRenderer,
    kCompletion,
    kStorage,
    kSession,
    kTrophies,
    kRecent,
    kTileCount,
};

struct TileSpec
{
    const char *label;    // tracked capitals on the tile; travels into the detail view
    const char *headline; // the detail view's title
    const char *pick;     // what the D-pad selects in the detail view (null: nothing)
    int column, row, columns, rows;
    std::uint32_t accent;
};

// In reading order: ties in the navigation go to the earlier tile.
constexpr TileSpec kTiles[kTileCount] = {
    {"FRAME TIME", "Frame pacing", "Window", 0, 0, 6, 2, 0x4fe0ff},
    {"PLAY TIME", "This week, day by day", "Day", 6, 0, 3, 2, 0xb8f26b},
    {"RENDERER", "What one frame costs", nullptr, 9, 0, 3, 2, 0x4fe0ff},
    {"LIBRARY", "Library completion", "Title", 0, 2, 3, 2, 0xffc24a},
    {"STORAGE", "Console storage", "Category", 3, 2, 6, 1, 0xff6f9c},
    {"SESSION", "This session", nullptr, 3, 3, 3, 1, 0x4fe0ff},
    {"ACHIEVEMENTS", "Achievements", "Title", 6, 3, 3, 1, 0xffc24a},
    {"RECENTLY PLAYED", "Jump back in", "Title", 9, 2, 3, 2, 0xb8f26b},
};

Color accent_of(int tile)
{
    return Color::rgb(kTiles[tile].accent);
}

Rect tile_rect(int tile)
{
    const TileSpec &spec = kTiles[tile];
    const float column = (kGridW - 11.0f * kGap) / 12.0f;
    const float row = (kGridH - 3.0f * kGap) / 4.0f;
    return {kGridX + static_cast<float>(spec.column) * (column + kGap),
            kGridY + static_cast<float>(spec.row) * (row + kGap),
            static_cast<float>(spec.columns) * (column + kGap) - kGap,
            static_cast<float>(spec.rows) * (row + kGap) - kGap};
}

// ---- invented data -----------------------------------------------------------

constexpr int kDays = 7;
constexpr const char *kDayShort[kDays] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};
constexpr const char *kDayLong[kDays] = {"Monday", "Tuesday",  "Wednesday", "Thursday",
                                         "Friday", "Saturday", "Sunday"};

struct Segment
{
    const char *name;
    int gigabytes;
    const char *lines[3];
};
constexpr int kSegments = 4;
constexpr int kDriveGb = 825;
constexpr Segment kStorageParts[kSegments] = {
    {"Games", 468, {"24 titles installed", "The largest takes 61 GB", "3 not played this year"}},
    {"Captures", 96, {"212 screenshots", "38 video clips", "The oldest is from March"}},
    {"Saves", 38, {"23 titles have saves", "All of them backed up", "Last backup: today"}},
    {"Free", 223, {"Room for about 9 titles", "Captures could free 96 GB", "No clean-up needed"}},
};

Color segment_color(int index)
{
    switch (index)
    {
    case 0:
        return kRose;
    case 1:
        return Color::rgb(0xffa9c4);
    case 2:
        return Color::rgb(0xb48cff);
    default:
        return kInk.with_alpha(0.16f);
    }
}

// The "free" part is a faint track in the bar; its legend dot needs more presence.
Color legend_color(int index)
{
    return index == kSegments - 1 ? kInk.with_alpha(0.4f) : segment_color(index);
}

constexpr const char *kWhen[] = {"Today",      "Yesterday",  "2 days ago",
                                 "3 days ago", "5 days ago", "Last week"};

constexpr int kRanked = 12; // titles listed in the library and achievement details
constexpr int kRecentCount = 6;
constexpr int kStarsPerTitle = 5;
constexpr int kPips = 12;   // voices shown on the tile
constexpr int kVoices = 32; // voices the mixer has
constexpr int kSpark = 120; // history of the renderer sparklines
constexpr int kLongWindow = 720;
constexpr int kWindows[3] = {240, 480, 720};

// ---- geometry helpers ----------------------------------------------------------

Rect lerp_rect(const Rect &a, const Rect &b, float t)
{
    return {tween::lerp(a.x, b.x, t), tween::lerp(a.y, b.y, t), tween::lerp(a.w, b.w, t),
            tween::lerp(a.h, b.h, t)};
}

// r scaled about its own centre.
Rect scaled(const Rect &r, float scale)
{
    return {r.cx() - r.w * scale * 0.5f, r.cy() - r.h * scale * 0.5f, r.w * scale, r.h * scale};
}

// Spatial navigation: which rectangle does a direction press move to?
//
// A candidate qualifies when its centre lies beyond the leading edge of the
// rectangle the focus is on (to move right, its centre must be right of the
// current right edge). Each candidate is then scored, lowest wins:
//
//     score = along + kMissPenalty * miss + kCentrePull * offset
//
//   along   the empty distance between the two facing edges: how far the
//           focus has to travel in the pressed direction;
//   miss    how far the two rectangles miss each other sideways, zero when
//           their spans overlap. A tile that sits squarely in the path beats
//           a nearer one that is off to the side, which is what makes a wide
//           tile reachable from the narrow ones stacked around it;
//   offset  the sideways distance between the centres, weighted lightly: among
//           tiles that are all in the path it prefers the best aligned one.
//
// Ties go to the earlier rectangle, so list them in reading order. Returns -1
// when nothing lies that way: the caller answers with a soft refusal.
constexpr float kMissPenalty = 2.0f;
constexpr float kCentrePull = 0.25f;

int spatial_next(std::span<const Rect> rects, int from, Direction direction)
{
    if (direction == Direction::none || from < 0 || from >= static_cast<int>(rects.size()))
        return -1;
    const bool horizontal = direction == Direction::left || direction == Direction::right;
    const float sign = direction == Direction::right || direction == Direction::down ? 1.0f : -1.0f;
    // Work on one axis pair: "main" runs along the direction, "cross" across.
    const auto main_low = [horizontal](const Rect &r) { return horizontal ? r.x : r.y; };
    const auto main_size = [horizontal](const Rect &r) { return horizontal ? r.w : r.h; };
    const auto cross_low = [horizontal](const Rect &r) { return horizontal ? r.y : r.x; };
    const auto cross_size = [horizontal](const Rect &r) { return horizontal ? r.h : r.w; };

    const Rect &origin = rects[static_cast<std::size_t>(from)];
    const float leading = sign > 0.0f ? main_low(origin) + main_size(origin) : main_low(origin);
    int best = -1;
    float best_score = 0.0f;
    for (int i = 0; i < static_cast<int>(rects.size()); ++i)
    {
        if (i == from)
            continue;
        const Rect &candidate = rects[static_cast<std::size_t>(i)];
        const float centre = main_low(candidate) + main_size(candidate) * 0.5f;
        if ((centre - leading) * sign <= 0.0f)
            continue;
        const float facing =
            sign > 0.0f ? main_low(candidate) : main_low(candidate) + main_size(candidate);
        const float along = std::max(0.0f, (facing - leading) * sign);
        const float miss =
            std::max({0.0f, cross_low(candidate) - (cross_low(origin) + cross_size(origin)),
                      cross_low(origin) - (cross_low(candidate) + cross_size(candidate))});
        const float offset = std::fabs((cross_low(candidate) + cross_size(candidate) * 0.5f) -
                                       (cross_low(origin) + cross_size(origin) * 0.5f));
        const float score = along + kMissPenalty * miss + kCentrePull * offset;
        // The margin keeps an exact tie from being decided by rounding.
        if (best < 0 || score < best_score - 0.01f)
        {
            best = i;
            best_score = score;
        }
    }
    return best;
}

Direction opposite(Direction direction)
{
    switch (direction)
    {
    case Direction::left:
        return Direction::right;
    case Direction::right:
        return Direction::left;
    case Direction::up:
        return Direction::down;
    case Direction::down:
        return Direction::up;
    case Direction::none:
        break;
    }
    return Direction::none;
}

// ---- the detail views' selectable items ------------------------------------------

// Geometry shared by update() (where the cursor may go) and draw().
constexpr Rect kBarsArea{kLeft, 320.0f, 1080.0f, 520.0f}; // the week chart in its detail view
constexpr float kBarsHeadroom = 150.0f;                   // above the tallest bar: the tooltip
constexpr float kListX = 640.0f;                          // title lists on the right
constexpr float kListY = 296.0f;

int detail_count(int tile)
{
    switch (tile)
    {
    case kFrame:
        return 3;
    case kPlay:
        return kDays;
    case kCompletion:
    case kTrophies:
        return kRanked;
    case kStorage:
        return kSegments;
    case kRecent:
        return kRecentCount;
    default:
        return 0;
    }
}

Rect detail_item_rect(int tile, int index)
{
    const float i = static_cast<float>(index);
    switch (tile)
    {
    case kFrame: // three window chips, top right
        return {kRight - (3.0f - i) * 176.0f + 12.0f, 176.0f, 164.0f, 52.0f};
    case kPlay: // one column per day
        return {kBarsArea.x + i * kBarsArea.w / kDays, 310.0f, kBarsArea.w / kDays, 590.0f};
    case kCompletion: // two columns of six rows
        return {kListX + static_cast<float>(index / 6) * 576.0f,
                kListY + static_cast<float>(index % 6) * 104.0f, 552.0f, 92.0f};
    case kStorage: // four cards in a row
        return {kLeft + i * 410.0f, 420.0f, 386.0f, 480.0f};
    case kTrophies: // three columns of four cards
        return {kListX + static_cast<float>(index % 3) * 384.0f,
                kListY + static_cast<float>(index / 3) * 156.0f, 360.0f, 144.0f};
    case kRecent: // six cards in a row
    {
        const float width = (kWide - 5.0f * 24.0f) / 6.0f;
        return {kLeft + i * (width + 24.0f), 296.0f, width, width + 96.0f};
    }
    default:
        return {};
    }
}

// One heartbeat as a polyline: x in 0..1 of the beat, y in -1..1 (up positive).
constexpr float kBeat[][2] = {{0.0f, 0.0f},  {0.28f, 0.0f},   {0.34f, 0.16f}, {0.40f, 0.0f},
                              {0.45f, 0.0f}, {0.48f, -0.14f}, {0.53f, 1.0f},  {0.58f, -0.38f},
                              {0.62f, 0.0f}, {0.70f, 0.0f},   {0.78f, 0.26f}, {0.86f, 0.0f},
                              {1.0f, 0.0f}};
constexpr int kBeatPoints = static_cast<int>(sizeof(kBeat) / sizeof(kBeat[0]));

float beat_y(float u)
{
    for (int i = 1; i < kBeatPoints; ++i)
    {
        if (u <= kBeat[i][0])
            return tween::lerp(kBeat[i - 1][1], kBeat[i][1],
                               tween::inverse_lerp(kBeat[i - 1][0], kBeat[i][0], u));
    }
    return 0.0f;
}

// Entrance progress of one part of a chart, `time` seconds into its animation.
float rise(float time, int index = 0, float step = 0.05f, float duration = 0.7f)
{
    return tween::stagger(time, index, step, duration);
}

constexpr app::TourStep kTour[] = {
    {0.5f, 0, Direction::right},
    {0.3f, 0, Direction::down},
    {0.9f, 0, Direction::up, "focus"}, // the ring on the wide storage tile
    {0.5f, action_bit(Action::confirm)},
    {0.08f, 0, Direction::none, "expanding"}, // the shared element in flight
    {0.6f, 0, Direction::left},
    {0.3f, 0, Direction::left},
    {0.9f, action_bit(Action::back), Direction::none, "bars"},
    {0.5f, 0, Direction::left},
    {0.3f, 0, Direction::down},
    {0.4f, action_bit(Action::confirm)},
    {0.6f, 0, Direction::down},
    {0.3f, 0, Direction::right},
    {0.9f, action_bit(Action::back), Direction::none, "library"},
    {0.5f, 0, Direction::up},
    {0.4f, action_bit(Action::confirm)},
    {0.5f, 0, Direction::right},
    {0.4f, action_bit(Action::west)},
    {0.9f, action_bit(Action::west), Direction::none, "frames"}, // paused; then live again
    {0.4f, action_bit(Action::back)},
};

struct Stats
{
    int count = 0;
    float min = 0.0f, avg = 0.0f, max = 0.0f;
    float p50 = 0.0f, p95 = 0.0f, p99 = 0.0f;
    int late = 0; // frames that took more than a budget and a half: a missed refresh
};

class Dashboard final : public app::Concept
{
  public:
    explicit Dashboard(app::Context &context) : context_(context)
    {
        const auto &items = context.catalog.items();
        const int count = static_cast<int>(items.size());
        designs_ = static_cast<int>(app::concept_registry().size());

        // The week: every title's hours are dealt onto a day, so the chart is
        // invented but stays tied to the catalogue the other designs show.
        int best_hours[kDays] = {};
        for (int i = 0; i < count; ++i)
        {
            const demo::Item &it = items[static_cast<std::size_t>(i)];
            const int day = (i + 5) % kDays;
            day_hours_[day] += static_cast<float>(it.hours) * 0.03f;
            if (it.hours >= best_hours[day])
            {
                best_hours[day] = it.hours;
                day_top_[day] = i;
            }
            average_ += it.progress / static_cast<float>(std::max(1, count));
            finished_ += it.progress >= 1.0f ? 1 : 0;
            started_ += it.progress > 0.0f && it.progress < 1.0f ? 1 : 0;
            earned_ += stars_of(it);
            perfect_ += stars_of(it) == kStarsPerTitle ? 1 : 0;
        }
        titles_ = std::max(1, count);
        for (int day = 0; day < kDays; ++day)
        {
            week_total_ += day_hours_[day];
            day_peak_ = std::max(day_peak_, day_hours_[day]);
            if (day_hours_[day] > day_hours_[busiest_])
                busiest_ = day;
        }
        day_peak_ = std::max(day_peak_, 0.1f);

        // The lists: the twelve titles furthest along, and six in progress.
        std::array<int, 64> order{};
        const int listed = std::min(count, static_cast<int>(order.size()));
        for (int i = 0; i < listed; ++i)
            order[static_cast<std::size_t>(i)] = i;
        std::stable_sort(order.begin(), order.begin() + listed,
                         [&items](int a, int b)
                         {
                             return items[static_cast<std::size_t>(a)].progress >
                                    items[static_cast<std::size_t>(b)].progress;
                         });
        for (int i = 0; i < kRanked; ++i)
            ranked_[static_cast<std::size_t>(i)] = order[static_cast<std::size_t>(i % titles_)];
        int found = 0;
        for (int i = 0; i < count && found < kRecentCount; ++i)
        {
            const float progress = items[static_cast<std::size_t>(i)].progress;
            if (progress > 0.0f && progress < 1.0f)
                recent_[static_cast<std::size_t>(found++)] = i;
        }
        for (; found < kRecentCount; ++found)
            recent_[static_cast<std::size_t>(found)] = found;

        // Frames presented before this design was created are history too.
        const app::Telemetry &telemetry = context.telemetry;
        last_head_ = telemetry.head;
        for (std::size_t age = app::Telemetry::kHistory; age-- > 0;)
        {
            if (telemetry.sample(age) > 0.0f)
                push_frame(telemetry.sample(age));
        }
        read_telemetry();
        tile_stats_ = measure(kWindows[0]);

        cursor_[kPlay] = kDays - 1; // today
        ring_.snap(ring_rect());
        ring_colour_.snap(accent_of(focus_));
        lift_[focus_].snap(1.0f);
        live_blend_.snap(1.0f);
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "dashboard",
            "Pulse Dashboard",
            "A bento grid of live data tiles that expand into detail views",
            "src/concepts/dashboard.cpp",
            audio::SoundSet::glass,
            kCyan,
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        age_ = 0.0f;
        expanded_ = false;
        for (tween::Spring &spring : expand_)
            spring.snap(0.0f);
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        age_ += dt;
        clock_ += dt;
        detail_age_ += dt;

        if (input.is_pressed(Action::west))
        {
            live_ = !live_;
            feedback.play(audio::Cue::toggle, live_ ? 1.06f : 0.94f, 0.5f);
        }
        if (expanded_)
            update_detail(input, feedback);
        else
            update_grid(input, feedback);
        ingest();

        // ---- animation state ----
        const bool reduced = calm();
        for (int i = 0; i < kTileCount; ++i)
        {
            lift_[i].target = i == focus_ ? 1.0f : 0.0f;
            lift_[i].update(dt, 16.0f);
            expand_[i].target = expanded_ && i == detail_ ? 1.0f : 0.0f;
            expand_[i].update(dt, reduced ? 40.0f : 11.0f);
        }
        ring_.target(ring_rect());
        ring_.update(dt, 20.0f);
        ring_colour_.target(accent_of(focus_));
        ring_colour_.update(dt, 12.0f);
        refusal_.update(dt, 9.0f);
        live_blend_.target = live_ ? 1.0f : 0.0f;
        live_blend_.update(dt, 14.0f);
        if (detail_count(detail_) > 0)
            cursor_ring_.target(detail_item_rect(detail_, cursor_[detail_]));
        cursor_ring_.update(dt, 20.0f);
        leader_y_.target = leader_target();
        leader_y_.update(dt, 16.0f);
        inner_ring_.target = item(ranked_[static_cast<std::size_t>(cursor_[kCompletion])]).progress;
        inner_ring_.update(dt, 9.0f);
        swap_.update(dt);
    }

    void draw(app::Frame &frame) const override
    {
        frame.backdrop.mode = gfx::BackdropMode::dots;
        frame.backdrop.colors[0] = Color::rgb(0x05070f);
        frame.backdrop.colors[1] = Color::rgb(0x0b1122);
        frame.backdrop.colors[2] = Color::rgb(0x2b3d66);
        frame.backdrop.time = context_.settings.reduced_motion ? 0.0f : clock_;

        const bool reduced = calm();
        float open = 0.0f; // how far the most expanded tile is
        for (const tween::Spring &spring : expand_)
            open = std::max(open, spring.value);

        // The grid falls back and fades while a tile is expanded. A tile in
        // flight is not drawn here: it lives in the overlay until it lands.
        gfx::DrawList &scene = frame.scene;
        scene.push_transform(reduced ? 1.0f : 1.0f - 0.05f * open, 960, 564, 0, 0);
        scene.push_opacity(1.0f - 0.7f * open);
        for (int i = 0; i < kTileCount; ++i)
        {
            if (i != focus_ && (reduced || expand_[i].value <= kFlight))
                draw_resting(scene, i);
        }
        if (reduced || expand_[focus_].value <= kFlight)
            draw_resting(scene, focus_); // last: its glow and shadow sit on its neighbours
        draw_ring(scene, open);
        scene.pop_opacity();
        scene.pop_transform();
        if (open > kFlight)
            scene.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                               Color::rgb(0x04060c, 0.5f * open));
        draw_header(scene);

        if (open > kFlight)
        {
            frame.glass = true;
            // Tiles still collapsing first, the one that is opening on top.
            for (int i = 0; i < kTileCount; ++i)
            {
                if (i != detail_ && expand_[i].value > kFlight)
                    draw_flight(frame.overlay, frame.glass_texture, i);
            }
            if (expand_[detail_].value > kFlight)
                draw_flight(frame.overlay, frame.glass_texture, detail_);
        }
        draw_hints(frame, open);
    }

    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    // ---- data ----------------------------------------------------------------

    const demo::Item &item(int index) const
    {
        return context_.catalog[static_cast<std::size_t>(index)];
    }

    static int stars_of(const demo::Item &it)
    {
        return static_cast<int>(std::lround(it.progress * static_cast<float>(kStarsPerTitle)));
    }

    bool calm() const
    {
        return context_.settings.reduced_motion;
    }

    float frame_at(int age) const
    {
        return frames_[static_cast<std::size_t>((frames_head_ + kLongWindow - 1 - age) %
                                                kLongWindow)];
    }

    void push_frame(float ms)
    {
        frames_[static_cast<std::size_t>(frames_head_)] = ms;
        frames_head_ = (frames_head_ + 1) % kLongWindow;
        frames_count_ = std::min(frames_count_ + 1, kLongWindow);
    }

    void read_telemetry()
    {
        const app::Telemetry &telemetry = context_.telemetry;
        shown_fps_ = telemetry.fps;
        shown_calls_ = static_cast<int>(telemetry.draw_calls);
        shown_shapes_ = static_cast<int>(telemetry.instances);
        shown_voices_ = telemetry.voices;
    }

    // Copies what the telemetry gained since the last update into this
    // design's own, longer history. While paused the new frames are skipped,
    // so the graph and the numbers stand still.
    void ingest()
    {
        const app::Telemetry &telemetry = context_.telemetry;
        const std::size_t history = app::Telemetry::kHistory;
        const std::size_t fresh = (telemetry.head + history - last_head_) % history;
        last_head_ = telemetry.head;
        if (!live_)
            return;
        for (std::size_t age = fresh; age-- > 0;)
            push_frame(telemetry.sample(age));
        read_telemetry();
        calls_history_[static_cast<std::size_t>(spark_head_)] = static_cast<float>(shown_calls_);
        shapes_history_[static_cast<std::size_t>(spark_head_)] = static_cast<float>(shown_shapes_);
        spark_head_ = (spark_head_ + 1) % kSpark;
        spark_count_ = std::min(spark_count_ + 1, kSpark);
        if (fresh > 0)
        {
            tile_stats_ = measure(kWindows[0]);
            detail_stats_ = measure(kWindows[cursor_[kFrame]]);
        }
    }

    // Minimum, mean, maximum and percentiles of the newest `window` frames.
    Stats measure(int window) const
    {
        Stats stats;
        stats.count = std::min(frames_count_, window);
        if (stats.count == 0)
            return stats;
        std::array<float, kLongWindow> sorted;
        float sum = 0.0f;
        for (int age = 0; age < stats.count; ++age)
        {
            sorted[static_cast<std::size_t>(age)] = frame_at(age);
            sum += frame_at(age);
            stats.late += frame_at(age) > kBudgetMs * 1.5f ? 1 : 0;
        }
        std::sort(sorted.begin(), sorted.begin() + stats.count);
        const auto at = [&sorted, &stats](float fraction)
        {
            const int index =
                static_cast<int>(fraction * static_cast<float>(stats.count - 1) + 0.5f);
            return sorted[static_cast<std::size_t>(std::clamp(index, 0, stats.count - 1))];
        };
        stats.min = sorted[0];
        stats.max = at(1.0f);
        stats.avg = sum / static_cast<float>(stats.count);
        stats.p50 = at(0.5f);
        stats.p95 = at(0.95f);
        stats.p99 = at(0.99f);
        return stats;
    }

    // ---- input -----------------------------------------------------------------

    void refuse(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav_repeat)
            return; // a held direction is not punished
        feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        feedback.rumble(0.25f, 0.05f);
        refusal_.trigger();
        refusal_x_ = input.nav == Direction::right  ? 1.0f
                     : input.nav == Direction::left ? -1.0f
                                                    : 0.0f;
        refusal_y_ = input.nav == Direction::down ? 1.0f
                     : input.nav == Direction::up ? -1.0f
                                                  : 0.0f;
    }

    void update_grid(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav != Direction::none)
        {
            std::array<Rect, kTileCount> rects;
            for (int i = 0; i < kTileCount; ++i)
                rects[static_cast<std::size_t>(i)] = tile_rect(i);
            // Pressing the opposite direction right after a move returns to
            // where the focus came from, so a detour never loses the way back.
            int next = input.nav == opposite(came_by_) ? came_from_
                                                       : spatial_next(rects, focus_, input.nav);
            if (next < 0)
                next = spatial_next(rects, focus_, input.nav);
            if (next >= 0)
            {
                came_from_ = focus_;
                came_by_ = input.nav;
                focus_ = next;
                // The tick is placed where the tile is: panned by its column
                // and a little lower in pitch for the lower rows.
                const Rect r = tile_rect(focus_);
                feedback.play(audio::Cue::focus, 1.08f - 0.16f * (r.cy() - kGridY) / kGridH,
                              ui::pan_for_x(r.cx()));
            }
            else
            {
                refuse(input, feedback);
            }
        }
        if (input.is_pressed(Action::confirm))
        {
            expanded_ = true;
            detail_ = focus_;
            detail_age_ = 0.0f;
            came_from_ = -1;
            if (detail_count(detail_) > 0)
                cursor_ring_.snap(detail_item_rect(detail_, cursor_[detail_]));
            leader_y_.snap(leader_target());
            swap_.running = false;
            detail_stats_ = measure(kWindows[cursor_[kFrame]]);
            feedback.play(audio::Cue::open, 1.0f, ui::pan_for_x(tile_rect(focus_).cx()) * 0.5f);
        }
    }

    void update_detail(const InputFrame &input, app::Feedback &feedback)
    {
        const int count = detail_count(detail_);
        if (input.nav != Direction::none)
        {
            std::array<Rect, kRanked> rects;
            for (int i = 0; i < count; ++i)
                rects[static_cast<std::size_t>(i)] = detail_item_rect(detail_, i);
            const int next =
                spatial_next(std::span<const Rect>(rects.data(), static_cast<std::size_t>(count)),
                             cursor_[detail_], input.nav);
            if (next >= 0)
            {
                previous_ = cursor_[detail_];
                cursor_[detail_] = next;
                swap_.start(calm() ? 0.12f : 0.32f);
                const float pan = ui::pan_for_x(detail_item_rect(detail_, next).cx());
                if (detail_ == kFrame || detail_ == kPlay || detail_ == kStorage)
                {
                    // An ordered row is a stepper: its pitch climbs with the index.
                    feedback.play(audio::Cue::slider, 0.9f + 0.04f * static_cast<float>(next), pan);
                }
                else
                {
                    feedback.play(audio::Cue::focus, 1.0f, pan);
                }
                if (detail_ == kFrame)
                    detail_stats_ = measure(kWindows[next]);
            }
            else
            {
                refuse(input, feedback);
            }
        }
        if (input.is_pressed(Action::back))
        {
            expanded_ = false;
            feedback.play(audio::Cue::back, 1.0f, ui::pan_for_x(tile_rect(detail_).cx()) * 0.5f);
        }
    }

    // ---- layout that follows the state ------------------------------------------

    Rect ring_rect() const
    {
        return scaled(tile_rect(focus_), 1.0f + kLift).inset(-5.0f);
    }

    // Where the tooltip's leader ends in the week chart: just above the
    // selected bar's value.
    float leader_target() const
    {
        const float tallest = kBarsArea.h - kBarsHeadroom;
        return kBarsArea.y + kBarsArea.h - day_hours_[cursor_[kPlay]] / day_peak_ * tallest - 44.0f;
    }

    // 1 when the detail cursor rests on `target`, falling to 0 as it glides
    // away: lets colours follow the highlight instead of switching.
    float nearness(const Rect &target) const
    {
        const Rect ring = cursor_ring_.value();
        const float dx = std::fabs(ring.cx() - target.cx()) / std::max(1.0f, target.w);
        const float dy = std::fabs(ring.cy() - target.cy()) / std::max(1.0f, target.h);
        return 1.0f - tween::clamp01(std::max(dx, dy));
    }

    float tile_in(int tile) const
    {
        return calm() ? tween::cubic_out(age_ / 0.3f) : tween::stagger(age_, tile, 0.06f, 0.5f);
    }

    // Seconds into a tile's chart animation, which starts as the tile lands.
    float chart_time(int tile) const
    {
        return calm() ? 99.0f : age_ - 0.25f - 0.06f * static_cast<float>(tile);
    }

    // ---- small drawing helpers -----------------------------------------------------

    float caps(gfx::DrawList &list, std::string_view value, float x, float baseline, float size,
               Color color, gfx::Align align = gfx::Align::left) const
    {
        return ui::text(list, context_.fonts.semibold, value, x, baseline, size, color, align,
                        3.0f);
    }

    // A number that is either a value or, with no data behind it, a dash.
    void number(gfx::DrawList &list, bool known, const char *format, double value, float x,
                float baseline, float size, Color color, gfx::Align align = gfx::Align::left) const
    {
        char text[32];
        if (known)
            std::snprintf(text, sizeof(text), format, value);
        else
            std::snprintf(text, sizeof(text), "%s", kDash);
        ui::text(list, context_.fonts.mono, text, x, baseline, size,
                 known ? color : color.with_alpha(kFaint), align);
    }

    void dashed(gfx::DrawList &list, float x1, float x2, float y, Color color) const
    {
        const int dashes = static_cast<int>(std::ceil((x2 - x1) / 18.0f));
        for (int i = 0; i < dashes; ++i)
        {
            const float x = x1 + static_cast<float>(i) * 18.0f;
            list.line(x, y, std::min(x + 8.0f, x2), y, 1.5f, color);
        }
    }

    void star_row(gfx::DrawList &list, float x, float cy, float radius, float pitch, float filled,
                  float time) const
    {
        for (int k = 0; k < kStarsPerTitle; ++k)
        {
            const float cx = x + radius + static_cast<float>(k) * pitch;
            const float amount = tween::clamp01(filled - static_cast<float>(k));
            list.star(cx, cy, radius, kInk.with_alpha(0.14f));
            if (amount <= 0.0f)
                continue;
            // Each star pops in turn; a partly earned one is cut by a clip.
            const float pop =
                tween::back_out(tween::clamp01((time - 0.07f * static_cast<float>(k)) / 0.4f));
            if (amount < 1.0f)
                list.push_clip({cx - radius, cy - radius, radius * 2.0f * amount, radius * 2.0f});
            list.star(cx, cy, radius * pop, kAmber);
            if (amount < 1.0f)
                list.pop_clip();
        }
    }

    // ---- frame graph (tile and detail) ------------------------------------------------

    void frame_graph(gfx::DrawList &list, const Rect &g, int window, const Stats &stats, float time,
                     bool detailed) const
    {
        const ui::Fonts &fonts = context_.fonts;
        char text[32];
        // The scale always shows two frame budgets, more if a spike needs it.
        const float top = std::max(kBudgetMs * 2.0f, stats.max * 1.15f);
        const float base = g.y + g.h;
        const float rules[] = {kBudgetMs * 0.5f, kBudgetMs, kBudgetMs * 2.0f};
        for (int k = 0; k < 3; ++k)
        {
            const float y = base - rules[k] / top * g.h;
            if (k == 1)
                dashed(list, g.x, g.x + g.w, y, kInk.with_alpha(0.34f));
            else
                list.rounded_rect({g.x, y, g.w, 1.0f}, 0, kInk.with_alpha(0.07f));
        }
        // A tick every 60 frames, so the time axis reads even with no data.
        for (int frame = 0; frame <= window; frame += 60)
        {
            const float x =
                g.x + g.w - g.w * static_cast<float>(frame) / static_cast<float>(window);
            list.rounded_rect({x - 1.0f, base - 9.0f, 2.0f, 9.0f}, 1, kInk.with_alpha(0.2f));
        }
        list.rounded_rect({g.x, base - 1.0f, g.w, 2.0f}, 1, kInk.with_alpha(0.22f));
        if (stats.count < 2)
        {
            // Nothing measured yet: the baseline alone, and why.
            list.rounded_rect({g.x, base - 1.5f, g.w, 3.0f}, 1.5f, kCyan.with_alpha(0.45f));
            const float size = detailed ? 26.0f : 22.0f;
            ui::text(list, fonts.regular, "Waiting for the first frames", g.cx(),
                     base - g.h * 0.375f + size * 0.35f, size, kInk.with_alpha(kFaint),
                     gfx::Align::center);
        }

        const float grow = rise(time, 0, 0.0f, 0.9f); // the graph rises from the baseline
        const float step = g.w / static_cast<float>(window - 1);
        const float thickness = detailed ? 3.0f : 2.5f;
        float last_x = 0.0f;
        float last_y = 0.0f;
        for (int age = 0; stats.count >= 2 && age < stats.count; ++age)
        {
            const float x = g.x + g.w - static_cast<float>(age) * step;
            const float y = base - tween::clamp01(frame_at(age) / top) * g.h * grow;
            // A soft fill under the line, one thin gradient per sample. Its
            // top alpha grows with the column's height, so every column has
            // the same tint at the same level and a spike leaves no stripe.
            list.gradient_rect({x - step * 0.5f, y, step, base - y}, 0,
                               kCyan.with_alpha(0.5f * (base - y) / g.h), kCyan.with_alpha(0.0f));
            if (age > 0)
                list.line(last_x, last_y, x, y, thickness, kCyan);
            last_x = x;
            last_y = y;
        }
        if (stats.count >= 2)
        {
            // The newest sample: a lit dot while live, a hollow one while paused.
            const float newest = base - tween::clamp01(frame_at(0) / top) * g.h * grow;
            const float blink = calm() ? 0.5f : ui::breathe(clock_, 1.2f);
            list.glow({g.x + g.w - 6, newest - 6, 12, 12}, 6, 14,
                      kCyan.with_alpha(0.7f * blink * live_blend_.value));
            list.circle(g.x + g.w, newest, 6, kCyan);
            list.circle(g.x + g.w, newest, 3, gfx::mix(kPanelBottom, kInk, live_blend_.value));
        }

        // The scale's labels go on last, each on a small plate, so the line
        // can pass behind them without making them unreadable.
        for (int k = detailed ? 0 : 1; k < (detailed ? 3 : 2); ++k)
        {
            const float y = base - rules[k] / top * g.h;
            const float size = 18.0f;
            const Color ink = kInk.with_alpha(k == 1 ? 0.75f : 0.5f);
            std::snprintf(text, sizeof(text), "%.2f", static_cast<double>(rules[k]));
            const float width = fonts.mono.measure(text, size) + fonts.regular.measure("ms", size);
            list.rounded_rect({g.x + 8.0f, y - size * 0.5f - 5.0f, width + 26.0f, size + 10.0f},
                              (size + 10.0f) * 0.5f, kPanelBottom.with_alpha(0.86f));
            const float used =
                ui::text(list, fonts.mono, text, g.x + 18.0f, y + size * 0.35f, size, ink);
            ui::text(list, fonts.regular, "ms", g.x + 24.0f + used, y + size * 0.35f, size, ink);
        }
    }

    void sparkline(gfx::DrawList &list, const Rect &g, const std::array<float, kSpark> &values,
                   float time) const
    {
        list.rounded_rect({g.x, g.y + g.h - 1.0f, g.w, 2.0f}, 1, kInk.with_alpha(0.18f));
        float peak = 1.0f;
        for (int age = 0; age < spark_count_; ++age)
            peak = std::max(
                peak, values[static_cast<std::size_t>((spark_head_ + kSpark - 1 - age) % kSpark)]);
        const float grow = rise(time, 0, 0.0f, 0.8f);
        const float step = g.w / static_cast<float>(kSpark - 1);
        float last_x = 0.0f;
        float last_y = 0.0f;
        for (int age = 0; age < spark_count_; ++age)
        {
            const float value =
                values[static_cast<std::size_t>((spark_head_ + kSpark - 1 - age) % kSpark)];
            const float x = g.x + g.w - static_cast<float>(age) * step;
            const float y = g.y + g.h - value / (peak * 1.15f) * g.h * grow;
            if (age > 0)
                list.line(last_x, last_y, x, y, 2.5f, kCyan);
            last_x = x;
            last_y = y;
        }
    }

    // A heartbeat trace: `beats` copies of kBeat with a bright head sweeping
    // through them and a tail that fades behind it.
    void heartbeat(gfx::DrawList &list, const Rect &r, int beats, float thickness) const
    {
        const float span = r.w / static_cast<float>(beats);
        const float period = 1.1f * static_cast<float>(beats);
        const float head = calm() ? 1.0f : std::fmod(clock_, period) / period;
        const float mid = r.cy();
        const float amplitude = r.h * 0.5f;
        for (int b = 0; b < beats; ++b)
        {
            for (int k = 1; k < kBeatPoints; ++k)
            {
                const float x1 = r.x + (static_cast<float>(b) + kBeat[k - 1][0]) * span;
                const float x2 = r.x + (static_cast<float>(b) + kBeat[k][0]) * span;
                // How long ago the head passed this segment, as a fraction of a sweep.
                float behind = head - ((x1 + x2) * 0.5f - r.x) / r.w;
                behind -= std::floor(behind);
                const float glow = calm() ? 0.75f : 0.22f + 0.78f * std::pow(1.0f - behind, 3.0f);
                list.line(x1, mid - kBeat[k - 1][1] * amplitude, x2, mid - kBeat[k][1] * amplitude,
                          thickness, kCyan.with_alpha(glow));
            }
        }
        if (calm())
            return;
        const float u = head * static_cast<float>(beats);
        const float hx = r.x + head * r.w;
        const float hy = mid - beat_y(u - std::floor(u)) * amplitude;
        list.glow({hx - 4, hy - 4, 8, 8}, 4, 14, kCyan.with_alpha(0.8f));
        list.circle(hx, hy, thickness * 1.5f, kInk);
    }

    // ---- week chart (tile and detail) -----------------------------------------------

    void week_bars(gfx::DrawList &list, const Rect &area, float bar, float headroom,
                   float value_size, float time, bool detailed) const
    {
        const ui::Fonts &fonts = context_.fonts;
        char text[16];
        const float pitch = area.w / kDays;
        const float tallest = area.h - headroom;
        const float base = area.y + area.h;
        for (int day = 0; day < kDays; ++day)
        {
            const float grow = rise(time, day, 0.06f, 0.6f);
            const float selected = detailed ? nearness(detail_item_rect(kPlay, day))
                                            : (day == cursor_[kPlay] ? 1.0f : 0.0f);
            const float h = std::max(bar * 0.5f, day_hours_[day] / day_peak_ * tallest * grow);
            const float x = area.x + pitch * (static_cast<float>(day) + 0.5f) - bar * 0.5f;
            const float radius = std::min(bar * 0.3f, 14.0f);
            list.rounded_rect({x, base - tallest, bar, tallest}, radius, kInk.with_alpha(0.04f));
            if (selected > 0.01f)
                list.glow({x, base - h, bar, h}, radius, 22, kLime.with_alpha(0.3f * selected));
            list.gradient_rect({x, base - h, bar, h}, radius,
                               kLime.with_alpha(tween::lerp(0.5f, 1.0f, selected)),
                               kLime.with_alpha(tween::lerp(0.22f, 0.6f, selected)));
            std::snprintf(text, sizeof(text), "%.1f", static_cast<double>(day_hours_[day] * grow));
            ui::text(list, fonts.mono, text, x + bar * 0.5f, base - h - value_size * 0.55f,
                     value_size, kInk.with_alpha(tween::lerp(kMuted, 1.0f, selected)),
                     gfx::Align::center);
            // Seven labels share the tile's width, so they are tracked less there.
            ui::text(list, fonts.semibold, kDayShort[day], x + bar * 0.5f + 1.0f,
                     base + (detailed ? 38.0f : 28.0f), detailed ? 18.0f : 15.0f,
                     kInk.with_alpha(tween::lerp(kMuted, 1.0f, selected)), gfx::Align::center,
                     detailed ? 3.0f : 1.5f);
        }
    }

    // ---- storage bar (tile and detail) ------------------------------------------------

    void storage_bar(gfx::DrawList &list, const Rect &bar, float time, bool detailed) const
    {
        const float radius = std::min(10.0f, bar.h * 0.5f);
        const float gap = 4.0f;
        list.rounded_rect(bar, radius, kInk.with_alpha(0.05f));
        float x = bar.x;
        for (int k = 0; k < kSegments; ++k)
        {
            // The segments fill in sequence, each starting as the one before
            // it is nearly done.
            const float grow = rise(time, k, 0.16f, 0.5f);
            const float full = bar.w * static_cast<float>(kStorageParts[k].gigabytes) / kDriveGb;
            const float w = std::max(0.0f, full * grow - gap);
            const float selected = detailed ? nearness(detail_item_rect(kStorage, k)) : 0.0f;
            if (w > 1.0f)
            {
                const Rect part{x, bar.y, w, bar.h};
                if (selected > 0.01f)
                    list.glow(part, radius, 20, segment_color(k).with_alpha(0.45f * selected));
                list.rounded_rect(part, radius,
                                  detailed ? gfx::mix(segment_color(k).with_alpha(0.55f),
                                                      segment_color(k), selected)
                                           : segment_color(k));
            }
            x += full * grow;
        }
    }

    // ---- tile contents -------------------------------------------------------------
    // Each takes the tile's rectangle and the seconds since its chart started
    // to animate, and draws everything but the label.

    void tile_frame(gfx::DrawList &list, const Rect &r, float time) const
    {
        const ui::Fonts &fonts = context_.fonts;
        char text[48];
        const bool known = tile_stats_.count > 0;
        const float up = rise(time, 0, 0.0f, 0.9f);
        caps(list, "FPS", r.x + r.w - kPad, r.y + 84, 17, kInk.with_alpha(kMuted),
             gfx::Align::right);
        number(list, known, "%.0f", static_cast<double>(shown_fps_ * up), r.x + r.w - kPad - 56,
               r.y + 84, 64, kInk, gfx::Align::right);
        if (!known)
            std::snprintf(text, sizeof(text), "No frames presented yet");
        else if (tile_stats_.late == 0)
            std::snprintf(text, sizeof(text), "No late frames in the last %d", tile_stats_.count);
        else
            std::snprintf(text, sizeof(text), "%d late of the last %d frames", tile_stats_.late,
                          tile_stats_.count);
        ui::text(list, fonts.regular, text, r.x + kPad, r.y + 84, 22, kInk.with_alpha(kMuted));

        frame_graph(list, {r.x + kPad, r.y + 116, r.w - 2 * kPad, 186}, kWindows[0], tile_stats_,
                    time, false);

        const char *names[] = {"MIN", "AVG", "MAX"};
        const float values[] = {tile_stats_.min, tile_stats_.avg, tile_stats_.max};
        for (int k = 0; k < 3; ++k)
        {
            const float x = r.x + kPad + static_cast<float>(k) * 190.0f;
            caps(list, names[k], x, r.y + 352, 15, kInk.with_alpha(kMuted));
            number(list, known, "%.2f", static_cast<double>(values[k] * up), x + 56, r.y + 354, 26,
                   kInk);
        }
        ui::text(list, fonts.regular, "in milliseconds", r.x + r.w - kPad, r.y + 353, 20,
                 kInk.with_alpha(kFaint), gfx::Align::right);
    }

    void tile_play(gfx::DrawList &list, const Rect &r, float time) const
    {
        const ui::Fonts &fonts = context_.fonts;
        char text[16];
        std::snprintf(text, sizeof(text), "%.1f",
                      static_cast<double>(week_total_ * rise(time, 0, 0.0f, 0.9f)));
        const float w = ui::text(list, fonts.mono, text, r.x + kPad, r.y + 102, 46, kInk);
        ui::text(list, fonts.regular, "hours this week", r.x + kPad + w + 12, r.y + 102, 22,
                 kInk.with_alpha(kMuted));
        week_bars(list, {r.x + kPad, r.y + 132, r.w - 2 * kPad, 200}, 32, 30, 18, time, false);
    }

    void tile_renderer(gfx::DrawList &list, const Rect &r, float time) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float up = rise(time, 0, 0.0f, 0.9f);
        char text[16];
        std::snprintf(text, sizeof(text), "%d",
                      static_cast<int>(static_cast<float>(shown_calls_) * up + 0.5f));
        ui::text(list, fonts.mono, text, r.x + kPad, r.y + 118, 60, kInk);
        ui::text(list, fonts.regular, "draw calls this frame", r.x + kPad, r.y + 150, 20,
                 kInk.with_alpha(kMuted));
        std::snprintf(text, sizeof(text), "%d",
                      static_cast<int>(static_cast<float>(shown_shapes_) * up + 0.5f));
        ui::text(list, fonts.mono, text, r.x + kPad, r.y + 226, 60, kInk);
        ui::text(list, fonts.regular, "shapes this frame", r.x + kPad, r.y + 258, 20,
                 kInk.with_alpha(kMuted));

        list.rounded_rect({r.x + kPad, r.y + 284, r.w - 2 * kPad, 1}, 0, kInk.with_alpha(0.1f));
        caps(list, "SOUNDS PLAYING", r.x + kPad, r.y + 320, 15, kInk.with_alpha(kMuted));
        std::snprintf(text, sizeof(text), "%d", shown_voices_);
        ui::text(list, fonts.mono, text, r.x + r.w - kPad, r.y + 322, 24, kInk, gfx::Align::right);
        const float pitch = (r.w - 2 * kPad - 16.0f) / static_cast<float>(kPips - 1);
        for (int k = 0; k < kPips; ++k)
        {
            const float cx = r.x + kPad + 8.0f + static_cast<float>(k) * pitch;
            const float in = rise(time, k, 0.03f, 0.3f);
            if (k < shown_voices_)
            {
                list.glow({cx - 6, r.y + 346, 12, 12}, 6, 10, kCyan.with_alpha(0.5f));
                list.circle(cx, r.y + 352, 8, kCyan);
            }
            else
            {
                list.circle(cx, r.y + 352, 8 * in, kInk.with_alpha(0.12f));
            }
        }
    }

    void tile_completion(gfx::DrawList &list, const Rect &r, float time) const
    {
        const ui::Fonts &fonts = context_.fonts;
        char text[16];
        const float up = rise(time, 0, 0.0f, 1.0f);
        const float cx = r.cx();
        const float cy = r.y + 178;
        list.arc(cx, cy, 98, 16, 0.0f, kTau, kInk.with_alpha(0.08f));
        list.arc(cx, cy, 98, 16, 0.0f, kTau * average_ * up, kAmber);
        std::snprintf(text, sizeof(text), "%d%%", static_cast<int>(average_ * 100.0f * up + 0.5f));
        ui::text(list, fonts.mono, text, cx, cy + 12, 50, kInk, gfx::Align::center);
        ui::text(list, fonts.regular, "complete", cx, cy + 42, 20, kInk.with_alpha(kMuted),
                 gfx::Align::center);

        const int counts[] = {finished_, started_};
        const char *names[] = {"finished", "in progress"};
        for (int k = 0; k < 2; ++k)
        {
            const float x = r.x + kPad + static_cast<float>(k) * (r.w - 2 * kPad) * 0.48f;
            const float y = r.y + 332;
            const float grow = rise(time, 2 + k, 0.12f, 0.8f);
            list.arc(x + 24, y, 24, 6, 0.0f, kTau, kInk.with_alpha(0.08f));
            list.arc(x + 24, y, 24, 6, 0.0f,
                     kTau * static_cast<float>(counts[k]) / static_cast<float>(titles_) * grow,
                     k == 0 ? kAmber : gfx::mix(kAmber, kInk, 0.55f));
            std::snprintf(text, sizeof(text), "%d",
                          static_cast<int>(static_cast<float>(counts[k]) * grow + 0.5f));
            ui::text(list, fonts.mono, text, x + 62, y - 2, 26, kInk);
            ui::text(list, fonts.regular, names[k], x + 62, y + 22, 20, kInk.with_alpha(kMuted));
        }
    }

    void tile_storage(gfx::DrawList &list, const Rect &r, float time) const
    {
        const ui::Fonts &fonts = context_.fonts;
        char text[24];
        const int used = kDriveGb - kStorageParts[kSegments - 1].gigabytes;
        std::snprintf(text, sizeof(text), "of %d GB used", kDriveGb);
        const float w = fonts.regular.measure(text, 22);
        ui::text(list, fonts.regular, text, r.x + r.w - kPad, r.y + 50, 22, kInk.with_alpha(kMuted),
                 gfx::Align::right);
        std::snprintf(
            text, sizeof(text), "%d",
            static_cast<int>(static_cast<float>(used) * rise(time, 0, 0.0f, 0.9f) + 0.5f));
        ui::text(list, fonts.mono, text, r.x + r.w - kPad - w - 12, r.y + 50, 34, kInk,
                 gfx::Align::right);

        storage_bar(list, {r.x + kPad, r.y + 76, r.w - 2 * kPad, 28}, time, false);

        float x = r.x + kPad;
        for (int k = 0; k < kSegments; ++k)
        {
            list.push_opacity(rise(time, k, 0.16f, 0.5f));
            list.circle(x + 7, r.y + 143, 7, legend_color(k));
            x += 24;
            x += ui::text(list, fonts.regular, kStorageParts[k].name, x, r.y + 150, 20,
                          kInk.with_alpha(0.85f));
            std::snprintf(text, sizeof(text), "%d", kStorageParts[k].gigabytes);
            x += 10 +
                 ui::text(list, fonts.mono, text, x + 10, r.y + 150, 20, kInk.with_alpha(kMuted));
            x += 6 +
                 ui::text(list, fonts.regular, "GB", x + 6, r.y + 150, 20, kInk.with_alpha(kMuted));
            x += 40;
            list.pop_opacity();
        }
    }

    void tile_session(gfx::DrawList &list, const Rect &r, float time) const
    {
        const ui::Fonts &fonts = context_.fonts;
        char text[48];
        const int seconds = static_cast<int>(context_.telemetry.uptime);
        std::snprintf(text, sizeof(text), "%02d:%02d:%02d", seconds / 3600 % 100, seconds / 60 % 60,
                      seconds % 60);
        const float w = ui::text(list, fonts.mono, text, r.x + kPad, r.y + 104, 40, kInk);
        list.push_opacity(rise(time, 1, 0.1f, 0.5f));
        heartbeat(list, {r.x + kPad + w + 24, r.y + 62, r.w - 2 * kPad - w - 24, 56}, 2, 2.5f);
        list.pop_opacity();
        std::snprintf(
            text, sizeof(text), "%d designs loaded",
            static_cast<int>(static_cast<float>(designs_) * rise(time, 0, 0.0f, 0.9f) + 0.5f));
        ui::text(list, fonts.regular, text, r.x + kPad, r.y + 150, 20, kInk.with_alpha(kMuted));
    }

    void tile_trophies(gfx::DrawList &list, const Rect &r, float time) const
    {
        const ui::Fonts &fonts = context_.fonts;
        char text[24];
        const float up = rise(time, 0, 0.0f, 0.9f);
        const int total = titles_ * kStarsPerTitle;
        const float share = static_cast<float>(earned_) / static_cast<float>(total);
        std::snprintf(text, sizeof(text), "%d",
                      static_cast<int>(static_cast<float>(earned_) * up + 0.5f));
        const float w = ui::text(list, fonts.mono, text, r.x + kPad, r.y + 104, 40, kInk);
        std::snprintf(text, sizeof(text), "of %d", total);
        ui::text(list, fonts.regular, text, r.x + kPad + w + 12, r.y + 104, 22,
                 kInk.with_alpha(kMuted));
        star_row(list, r.x + r.w - kPad - 4 * 34.0f - 28.0f, r.y + 90, 14, 34,
                 share * kStarsPerTitle, time - 0.2f);

        const Rect bar{r.x + kPad, r.y + 126, r.w - 2 * kPad, 10};
        list.rounded_rect(bar, 5, kInk.with_alpha(0.1f));
        list.rounded_rect({bar.x, bar.y, std::max(10.0f, bar.w * share * up), bar.h}, 5, kAmber);
        std::snprintf(text, sizeof(text), "%d%% unlocked",
                      static_cast<int>(share * 100.0f * up + 0.5f));
        ui::text(list, fonts.regular, text, r.x + kPad, r.y + 164, 20, kInk.with_alpha(kMuted));
    }

    void tile_recent(gfx::DrawList &list, const Rect &r, float time) const
    {
        const ui::Fonts &fonts = context_.fonts;
        char text[32];
        for (int k = 0; k < 3; ++k)
        {
            const demo::Item &it = item(recent_[static_cast<std::size_t>(k)]);
            const float in = rise(time, k, 0.09f, 0.5f);
            const float y = r.y + 74 + static_cast<float>(k) * 100.0f + 12.0f * (1.0f - in);
            list.push_opacity(in);
            list.image(it.cover, {r.x + kPad, y, 84, 84}, gfx::kCanvasUv, Color::rgb(0xffffff), 16);
            const float x = r.x + kPad + 104;
            const float width = r.w - 2 * kPad - 104;
            ui::text(list, fonts.semibold, fonts.semibold.font->fit(it.title, 24, width), x, y + 30,
                     24, kInk);
            std::snprintf(text, sizeof(text), "%d h played", it.hours);
            ui::text(list, fonts.regular, text, x, y + 58, 20, kInk.with_alpha(kMuted));
            list.rounded_rect({x, y + 72, width, 6}, 3, kInk.with_alpha(0.1f));
            list.rounded_rect({x, y + 72, std::max(6.0f, width * it.progress * in), 6}, 3, kLime);
            list.pop_opacity();
        }
    }

    void draw_tile_content(gfx::DrawList &list, int tile, const Rect &r, float time) const
    {
        switch (tile)
        {
        case kFrame:
            tile_frame(list, r, time);
            break;
        case kPlay:
            tile_play(list, r, time);
            break;
        case kRenderer:
            tile_renderer(list, r, time);
            break;
        case kCompletion:
            tile_completion(list, r, time);
            break;
        case kStorage:
            tile_storage(list, r, time);
            break;
        case kSession:
            tile_session(list, r, time);
            break;
        case kTrophies:
            tile_trophies(list, r, time);
            break;
        default:
            tile_recent(list, r, time);
            break;
        }
    }

    // ---- detail contents -------------------------------------------------------------
    // Laid out in the final rectangle (kDetail); `time` is the seconds since
    // the detail's charts started to animate.

    // A quiet inner card: stat blocks, list rows.
    void block(gfx::DrawList &list, const Rect &r, float radius = 20.0f) const
    {
        list.bordered_rect(r, radius, kInk.with_alpha(0.045f), 1.0f, kInk.with_alpha(0.06f));
    }

    // The glide highlight of a detail view, with the refusal nudge.
    Rect cursor_rect() const
    {
        Rect ring = cursor_ring_.value();
        const float nudge = ui::shake(refusal_.value, clock_, 10.0f, 9.0f);
        ring.x += nudge * refusal_x_;
        ring.y += nudge * refusal_y_;
        return ring;
    }

    void detail_frame(gfx::DrawList &list, float time) const
    {
        const ui::Fonts &fonts = context_.fonts;
        char text[32];
        // The window stepper: one bright pill glides under the three labels.
        const Rect pill = cursor_rect();
        for (int k = 0; k < 3; ++k)
            block(list, detail_item_rect(kFrame, k), 26);
        list.glow(pill, 26, 16, kCyan.with_alpha(0.35f));
        list.rounded_rect(pill, 26, kCyan);
        for (int k = 0; k < 3; ++k)
        {
            const Rect chip = detail_item_rect(kFrame, k);
            std::snprintf(text, sizeof(text), "%d frames", kWindows[k]);
            ui::text(list, fonts.semibold, text, chip.cx(), chip.cy() + 8, 22,
                     gfx::mix(kInk.with_alpha(0.8f), kNight, nearness(chip)), gfx::Align::center);
        }

        const int window = kWindows[cursor_[kFrame]];
        const Rect graph{kLeft, 300, kWide, 372};
        frame_graph(list, graph, window, detail_stats_, time, true);
        std::snprintf(text, sizeof(text), "%d frames ago", window);
        ui::text(list, fonts.regular, text, graph.x, graph.y + graph.h + 30, 20,
                 kInk.with_alpha(kFaint));
        ui::text(list, fonts.regular, live_ ? "now" : "paused", graph.x + graph.w,
                 graph.y + graph.h + 30, 20, kInk.with_alpha(kFaint), gfx::Align::right);

        const bool known = detail_stats_.count > 0;
        const char *names[] = {"FPS", "FASTEST", "AVERAGE", "SLOWEST", "MEDIAN", "95TH", "99TH"};
        const char *units[] = {"per second",    "ms",           "ms", "ms", "ms",
                               "ms, 5% slower", "ms, 1% slower"};
        const float values[] = {shown_fps_,        detail_stats_.min, detail_stats_.avg,
                                detail_stats_.max, detail_stats_.p50, detail_stats_.p95,
                                detail_stats_.p99};
        const float width = (kWide - 6.0f * 16.0f) / 7.0f;
        for (int k = 0; k < 7; ++k)
        {
            const float in = rise(time, k, 0.05f, 0.5f);
            const Rect card{kLeft + static_cast<float>(k) * (width + 16.0f),
                            734 + 16.0f * (1.0f - in), width, 166};
            list.push_opacity(in);
            block(list, card);
            caps(list, names[k], card.x + 22, card.y + 42, 15,
                 k == 0 ? kCyan : kInk.with_alpha(kMuted));
            number(list, known, k == 0 ? "%.0f" : "%.2f", static_cast<double>(values[k] * in),
                   card.x + 22, card.y + 100, 42, kInk);
            ui::text(list, fonts.regular, units[k], card.x + 22, card.y + 136, 20,
                     kInk.with_alpha(kMuted));
            list.pop_opacity();
        }
    }

    void detail_renderer(gfx::DrawList &list, float time) const
    {
        const ui::Fonts &fonts = context_.fonts;
        char text[64];
        const char *names[] = {"DRAW CALLS", "SHAPES", "SOUNDS PLAYING"};
        const char *notes[] = {"batches sent to the GPU this frame", "instanced quads this frame",
                               "of 32 mixer voices"};
        const int values[] = {shown_calls_, shown_shapes_, shown_voices_};
        const float width = (kWide - 2.0f * 24.0f) / 3.0f;
        for (int k = 0; k < 3; ++k)
        {
            const float in = rise(time, k, 0.08f, 0.5f);
            const Rect card{kLeft + static_cast<float>(k) * (width + 24.0f),
                            300 + 18.0f * (1.0f - in), width, 380};
            list.push_opacity(in);
            block(list, card, 24);
            caps(list, names[k], card.x + 28, card.y + 50, 16, kCyan);
            std::snprintf(text, sizeof(text), "%d",
                          static_cast<int>(static_cast<float>(values[k]) * in + 0.5f));
            ui::text(list, fonts.mono, text, card.x + 28, card.y + 158, 88, kInk);
            ui::text(list, fonts.regular, notes[k], card.x + 28, card.y + 200, 22,
                     kInk.with_alpha(kMuted));
            const Rect chart{card.x + 28, card.y + 238, card.w - 56, 108};
            if (k == 0)
                sparkline(list, chart, calls_history_, time);
            else if (k == 1)
                sparkline(list, chart, shapes_history_, time);
            else
            {
                // One pip per mixer voice, two rows of sixteen.
                const float pitch = chart.w / 16.0f;
                for (int v = 0; v < kVoices; ++v)
                {
                    const float cx = chart.x + pitch * (static_cast<float>(v % 16) + 0.5f);
                    const float cy = chart.y + 30 + static_cast<float>(v / 16) * 44.0f;
                    if (v < shown_voices_)
                        list.glow({cx - 7, cy - 7, 14, 14}, 7, 10, kCyan.with_alpha(0.5f));
                    list.circle(cx, cy, 10, v < shown_voices_ ? kCyan : kInk.with_alpha(0.12f));
                }
            }
            list.pop_opacity();
        }
        list.push_opacity(rise(time, 4, 0.08f, 0.5f));
        if (shown_calls_ > 0)
            std::snprintf(text, sizeof(text), "%.1f shapes per draw call",
                          static_cast<double>(shown_shapes_) / static_cast<double>(shown_calls_));
        else
            std::snprintf(text, sizeof(text), "No frame measured yet");
        ui::text(list, fonts.semibold, text, kLeft, 756, 30, kInk);
        ui::paragraph(list, fonts.regular,
                      "Every shape is one instanced quad. A draw call ends when the texture or "
                      "the clip rectangle changes, so a screen that shares its textures and "
                      "clips whole regions stays cheap however much it draws.",
                      kLeft, 806, 24, 1240, 36, kInk.with_alpha(kMuted), 3);
        list.pop_opacity();
    }

    // The tooltip's text for one day, so two of them can cross-fade.
    void bubble_text(gfx::DrawList &list, const Rect &bubble, int day, float alpha) const
    {
        if (alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        char text[96];
        list.push_opacity(alpha);
        ui::text(list, fonts.semibold, kDayLong[day], bubble.x + 24, bubble.y + 38, 24, kInk);
        ui::text(list, fonts.regular, "h", bubble.x + bubble.w - 24, bubble.y + 38, 24, kLime,
                 gfx::Align::right);
        std::snprintf(text, sizeof(text), "%.1f", static_cast<double>(day_hours_[day]));
        ui::text(list, fonts.mono, text, bubble.x + bubble.w - 46, bubble.y + 38, 24, kLime,
                 gfx::Align::right);
        std::snprintf(text, sizeof(text), "Mostly %s", item(day_top_[day]).title);
        ui::text(list, fonts.regular, fonts.regular.font->fit(text, 20, bubble.w - 48),
                 bubble.x + 24, bubble.y + 70, 20, kInk.with_alpha(kMuted));
        list.pop_opacity();
    }

    void detail_play(gfx::DrawList &list, float time) const
    {
        const ui::Fonts &fonts = context_.fonts;
        char text[64];
        const Rect column = cursor_rect();
        list.rounded_rect({column.x + 10, column.y, column.w - 20, column.h}, 22,
                          kInk.with_alpha(0.05f));

        const float tallest = kBarsArea.h - kBarsHeadroom;
        const float base = kBarsArea.y + kBarsArea.h;
        const float mean = week_total_ / kDays;
        const float mean_y = base - mean / day_peak_ * tallest;
        list.push_opacity(rise(time, 6, 0.06f, 0.5f));
        dashed(list, kBarsArea.x, kBarsArea.x + kBarsArea.w, mean_y, kInk.with_alpha(0.3f));
        std::snprintf(text, sizeof(text), "average %.1f h", static_cast<double>(mean));
        ui::text(list, fonts.regular, text, kBarsArea.x + 4, mean_y - 10, 20,
                 kInk.with_alpha(kMuted));
        list.pop_opacity();
        week_bars(list, kBarsArea, 84, kBarsHeadroom, 24, time, true);

        // The tooltip rides on the cursor's spring, so it glides along the
        // top of the chart with the selection; a dotted leader drops to the
        // selected bar and the text cross-fades.
        const float cx = column.cx();
        const Rect bubble{std::clamp(cx - 170.0f, kLeft, kBarsArea.x + kBarsArea.w - 340.0f),
                          kBarsArea.y + 2.0f, 340.0f, 88.0f};
        const float tip = bubble.y + bubble.h + 12.0f;
        list.push_opacity(rise(time, 7, 0.06f, 0.4f));
        const int dots = static_cast<int>(std::ceil((leader_y_.value - tip - 10.0f) / 12.0f));
        for (int i = 0; i < dots; ++i)
            list.circle(cx, tip + 10.0f + static_cast<float>(i) * 12.0f, 2.0f,
                        kLime.with_alpha(0.55f));
        list.shadow({bubble.x, bubble.y + 10, bubble.w, bubble.h}, 20, 26, kBlack.with_alpha(0.5f));
        list.rotated_rect({cx - 10, tip - 22, 20, 20}, 4, kTau / 8.0f, Color::rgb(0x222c48));
        list.bordered_rect(bubble, 20, Color::rgb(0x222c48), 1.5f, kLime.with_alpha(0.45f));
        if (swap_.running)
        {
            const float t = swap_.progress();
            bubble_text(list, bubble, previous_, 1.0f - tween::smoothstep(t * 2.2f));
            bubble_text(list, bubble, cursor_[kPlay], tween::smoothstep((t - 0.25f) / 0.75f));
        }
        else
        {
            bubble_text(list, bubble, cursor_[kPlay], 1.0f);
        }
        list.pop_opacity();

        // The week in words.
        const float x = 1316.0f;
        const float up = rise(time, 0, 0.0f, 0.9f);
        std::snprintf(text, sizeof(text), "%.1f", static_cast<double>(week_total_ * up));
        const float w = ui::text(list, fonts.mono, text, x, 400, 84, kInk);
        ui::text(list, fonts.regular, "hours", x + w + 16, 400, 28, kInk.with_alpha(kMuted));
        ui::text(list, fonts.regular, "played this week", x, 444, 24, kInk.with_alpha(kMuted));
        const char *names[] = {"Daily average", "Busiest day", "Most played", "Titles played"};
        for (int k = 0; k < 4; ++k)
        {
            const float in = rise(time, 2 + k, 0.07f, 0.5f);
            const float y = 520 + static_cast<float>(k) * 92.0f + 14.0f * (1.0f - in);
            list.push_opacity(in);
            ui::text(list, fonts.regular, names[k], x, y, 22, kInk.with_alpha(kMuted));
            if (k == 0)
                std::snprintf(text, sizeof(text), "%.1f hours", static_cast<double>(mean));
            else if (k == 1)
                std::snprintf(text, sizeof(text), "%s", kDayLong[busiest_]);
            else if (k == 2)
                std::snprintf(text, sizeof(text), "%s", item(day_top_[busiest_]).title);
            else
                std::snprintf(text, sizeof(text), "%d of %d", started_ + finished_, titles_);
            ui::text(list, fonts.semibold, fonts.semibold.font->fit(text, 30, kRight - x), x,
                     y + 38, 30, kInk);
            list.pop_opacity();
        }
    }

    void detail_completion(gfx::DrawList &list, float time) const
    {
        const ui::Fonts &fonts = context_.fonts;
        char text[96];
        const float up = rise(time, 0, 0.0f, 1.0f);
        const demo::Item &chosen = item(ranked_[static_cast<std::size_t>(cursor_[kCompletion])]);
        // Outer ring: the library's average. Inner ring: the selected title,
        // in its own colour, on a spring so it sweeps between values.
        const float cx = 392.0f;
        const float cy = 548.0f;
        list.arc(cx, cy, 178, 26, 0.0f, kTau, kInk.with_alpha(0.08f));
        list.arc(cx, cy, 178, 26, 0.0f, kTau * average_ * up, kAmber);
        list.arc(cx, cy, 136, 12, 0.0f, kTau, kInk.with_alpha(0.06f));
        list.arc(cx, cy, 136, 12, 0.0f, kTau * inner_ring_.value * up, chosen.accent);
        std::snprintf(text, sizeof(text), "%d%%", static_cast<int>(average_ * 100.0f * up + 0.5f));
        ui::text(list, fonts.mono, text, cx, cy + 16, 76, kInk, gfx::Align::center);
        ui::text(list, fonts.regular, "library average", cx, cy + 54, 22, kInk.with_alpha(kMuted),
                 gfx::Align::center);

        std::snprintf(text, sizeof(text), "%s  %d%%", chosen.title,
                      static_cast<int>(inner_ring_.value * 100.0f + 0.5f));
        const std::string legend = fonts.regular.font->fit(text, 22, 400);
        const float half = fonts.regular.measure(legend, 22) * 0.5f;
        list.circle(cx - half - 16, 781, 7, chosen.accent);
        ui::text(list, fonts.regular, legend, cx + 6, 788, 22, kInk.with_alpha(0.85f),
                 gfx::Align::center);

        const int counts[] = {finished_, started_, titles_ - finished_ - started_};
        const char *names[] = {"finished", "in progress", "not started"};
        for (int k = 0; k < 3; ++k)
        {
            const float x = kLeft + 20 + static_cast<float>(k) * 160.0f;
            std::snprintf(
                text, sizeof(text), "%d",
                static_cast<int>(static_cast<float>(counts[k]) * rise(time, 2 + k, 0.1f, 0.7f) +
                                 0.5f));
            ui::text(list, fonts.mono, text, x, 856, 36, kInk);
            ui::text(list, fonts.regular, names[k], x, 886, 20, kInk.with_alpha(kMuted));
        }

        const Rect ring = cursor_rect();
        list.glow(ring, 22, 18, kAmber.with_alpha(0.16f));
        list.bordered_rect(ring, 22, kInk.with_alpha(0.07f), 2.0f, kAmber);
        for (int k = 0; k < kRanked; ++k)
        {
            const demo::Item &it = item(ranked_[static_cast<std::size_t>(k)]);
            const Rect row = detail_item_rect(kCompletion, k);
            const float in = rise(time, k, 0.035f, 0.45f);
            list.push_opacity(in);
            list.image(it.cover, {row.x + 16, row.y + 16, 60, 60}, gfx::kCanvasUv,
                       Color::rgb(0xffffff), 12);
            const float x = row.x + 96;
            const float width = row.w - 96 - 104;
            ui::text(list, fonts.semibold, fonts.semibold.font->fit(it.title, 24, width), x,
                     row.y + 42, 24, kInk);
            list.rounded_rect({x, row.y + 60, width, 10}, 5, kInk.with_alpha(0.1f));
            list.rounded_rect({x, row.y + 60, std::max(10.0f, width * it.progress * in), 10}, 5,
                              kAmber);
            std::snprintf(text, sizeof(text), "%d%%",
                          static_cast<int>(it.progress * 100.0f * in + 0.5f));
            ui::text(list, fonts.mono, text, row.x + row.w - 22, row.y + 56, 24, kInk,
                     gfx::Align::right);
            list.pop_opacity();
        }
    }

    void detail_storage(gfx::DrawList &list, float time) const
    {
        const ui::Fonts &fonts = context_.fonts;
        char text[48];
        const int used = kDriveGb - kStorageParts[kSegments - 1].gigabytes;
        std::snprintf(text, sizeof(text), "of %d GB used", kDriveGb);
        const float w = fonts.regular.measure(text, 26);
        ui::text(list, fonts.regular, text, kRight, 258, 26, kInk.with_alpha(kMuted),
                 gfx::Align::right);
        std::snprintf(
            text, sizeof(text), "%d",
            static_cast<int>(static_cast<float>(used) * rise(time, 0, 0.0f, 0.9f) + 0.5f));
        ui::text(list, fonts.mono, text, kRight - w - 14, 258, 52, kInk, gfx::Align::right);

        const Rect bar{kLeft, 300, kWide, 48};
        storage_bar(list, bar, time, true);

        const Rect ring = cursor_rect();
        list.glow(ring, 24, 20, kRose.with_alpha(0.1f));
        list.bordered_rect(ring, 24, kInk.with_alpha(0.04f), 2.0f, kRose);
        for (int k = 0; k < kSegments; ++k)
        {
            const Segment &part = kStorageParts[k];
            const Rect card = detail_item_rect(kStorage, k);
            const float in = rise(time, k, 0.16f, 0.5f);
            list.push_opacity(in);
            block(list, card, 24);
            list.circle(card.x + 38, card.y + 46, 9, legend_color(k));
            ui::text(list, fonts.semibold, part.name, card.x + 60, card.y + 55, 26, kInk);
            std::snprintf(text, sizeof(text), "%d",
                          static_cast<int>(static_cast<float>(part.gigabytes) * in + 0.5f));
            const float gw = ui::text(list, fonts.mono, text, card.x + 28, card.y + 150, 68, kInk);
            ui::text(list, fonts.regular, "GB", card.x + 28 + gw + 12, card.y + 150, 26,
                     kInk.with_alpha(kMuted));
            std::snprintf(text, sizeof(text), "%.0f%% of the drive",
                          static_cast<double>(part.gigabytes) * 100.0 / kDriveGb);
            ui::text(list, fonts.regular, text, card.x + 28, card.y + 192, 22,
                     kInk.with_alpha(kMuted));
            list.rounded_rect({card.x + 28, card.y + 226, card.w - 56, 1}, 0,
                              kInk.with_alpha(0.1f));
            for (int line = 0; line < 3; ++line)
                ui::text(list, fonts.regular,
                         fonts.regular.font->fit(part.lines[line], 22, card.w - 56), card.x + 28,
                         card.y + 278 + static_cast<float>(line) * 44.0f, 22,
                         kInk.with_alpha(0.85f));
            list.pop_opacity();
        }
    }

    void detail_session(gfx::DrawList &list, float time) const
    {
        const ui::Fonts &fonts = context_.fonts;
        char text[48];
        const int seconds = static_cast<int>(context_.telemetry.uptime);
        std::snprintf(text, sizeof(text), "%02d:%02d:%02d", seconds / 3600 % 100, seconds / 60 % 60,
                      seconds % 60);
        ui::text(list, fonts.mono, text, kLeft - 6, 424, 124, kInk);
        ui::text(list, fonts.regular, "since the app opened", kLeft, 474, 24,
                 kInk.with_alpha(kMuted));
        list.push_opacity(rise(time, 1, 0.1f, 0.6f));
        heartbeat(list, {kLeft, 516, kWide, 150}, 8, 3.0f);
        list.pop_opacity();

        const Settings &settings = context_.settings;
        const char *names[] = {"DESIGNS", "SOUND SET", "OUTPUT", "MOTION"};
        const char *notes[] = {"switch with L1 and R1", "tuned chimes in one key",
                               "laid out on a 1920 x 1080 canvas", "from the settings"};
        const float width = (kWide - 3.0f * 24.0f) / 4.0f;
        for (int k = 0; k < 4; ++k)
        {
            const float in = rise(time, 2 + k, 0.07f, 0.5f);
            const Rect card{kLeft + static_cast<float>(k) * (width + 24.0f),
                            724 + 16.0f * (1.0f - in), width, 176};
            list.push_opacity(in);
            block(list, card);
            caps(list, names[k], card.x + 26, card.y + 44, 15, kCyan);
            if (k == 0)
                std::snprintf(text, sizeof(text), "%d loaded", designs_);
            else if (k == 1)
                std::snprintf(text, sizeof(text), "Glass");
            else if (k == 2)
                std::snprintf(text, sizeof(text), "%s",
                              Settings::kResolutions[std::clamp(settings.resolution, 0,
                                                                Settings::kResolutionCount - 1)]
                                  .label);
            else
                std::snprintf(text, sizeof(text), "%s",
                              settings.reduced_motion ? "Reduced" : "Full");
            ui::text(list, fonts.semibold, text, card.x + 26, card.y + 102, 38, kInk);
            ui::text(list, fonts.regular, fonts.regular.font->fit(notes[k], 20, card.w - 52),
                     card.x + 26, card.y + 142, 20, kInk.with_alpha(kMuted));
            list.pop_opacity();
        }
    }

    void detail_trophies(gfx::DrawList &list, float time) const
    {
        const ui::Fonts &fonts = context_.fonts;
        char text[48];
        const float up = rise(time, 0, 0.0f, 0.9f);
        const int total = titles_ * kStarsPerTitle;
        const float share = static_cast<float>(earned_) / static_cast<float>(total);
        std::snprintf(text, sizeof(text), "%d",
                      static_cast<int>(static_cast<float>(earned_) * up + 0.5f));
        const float w = ui::text(list, fonts.mono, text, kLeft, 400, 96, kInk);
        std::snprintf(text, sizeof(text), "of %d", total);
        ui::text(list, fonts.regular, text, kLeft + w + 18, 400, 30, kInk.with_alpha(kMuted));
        ui::text(list, fonts.regular, "achievements unlocked", kLeft, 446, 24,
                 kInk.with_alpha(kMuted));
        star_row(list, kLeft, 524, 30, 78, share * kStarsPerTitle, time - 0.2f);
        const Rect bar{kLeft, 590, 400, 14};
        list.rounded_rect(bar, 7, kInk.with_alpha(0.1f));
        list.rounded_rect({bar.x, bar.y, std::max(14.0f, bar.w * share * up), bar.h}, 7, kAmber);
        std::snprintf(text, sizeof(text), "%d%% of everything there is",
                      static_cast<int>(share * 100.0f * up + 0.5f));
        ui::text(list, fonts.regular, text, kLeft, 644, 22, kInk.with_alpha(kMuted));
        std::snprintf(text, sizeof(text), "%d", perfect_);
        const float pw = ui::text(list, fonts.mono, text, kLeft, 760, 56, kInk);
        ui::text(list, fonts.regular, "titles with every star", kLeft + pw + 16, 760, 24,
                 kInk.with_alpha(kMuted));
        ui::paragraph(list, fonts.regular, "Five stars per title: one for each fifth of its story.",
                      kLeft, 816, 22, 400, 32, kInk.with_alpha(kFaint), 2);

        const Rect ring = cursor_rect();
        list.glow(ring, 22, 18, kAmber.with_alpha(0.16f));
        list.bordered_rect(ring, 22, kInk.with_alpha(0.07f), 2.0f, kAmber);
        for (int k = 0; k < kRanked; ++k)
        {
            const demo::Item &it = item(ranked_[static_cast<std::size_t>(k)]);
            const Rect card = detail_item_rect(kTrophies, k);
            const float in = rise(time, k, 0.035f, 0.45f);
            list.push_opacity(in);
            list.image(it.cover, {card.x + 18, card.y + 18, 108, 108}, gfx::kCanvasUv,
                       Color::rgb(0xffffff), 16);
            const float x = card.x + 146;
            ui::text(list, fonts.semibold, fonts.semibold.font->fit(it.title, 24, card.w - 164), x,
                     card.y + 48, 24, kInk);
            star_row(list, x, card.y + 78, 12, 30, static_cast<float>(stars_of(it)),
                     time - 0.2f - 0.035f * static_cast<float>(k));
            std::snprintf(text, sizeof(text), "%d of %d", stars_of(it), kStarsPerTitle);
            ui::text(list, fonts.regular, text, x, card.y + 122, 20, kInk.with_alpha(kMuted));
            list.pop_opacity();
        }
    }

    // The selected title in words, drawn at an opacity and an offset so two
    // of them can cross-fade.
    void recent_info(gfx::DrawList &list, int index, float alpha, float slide) const
    {
        if (alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const demo::Item &it = item(recent_[static_cast<std::size_t>(index)]);
        char text[64];
        list.push_opacity(alpha);
        const float x = kLeft + slide;
        caps(list, ui::upper(it.genre), x, 704, 17, kLime);
        ui::text(list, fonts.display, it.title, x - 2, 760, 46, kInk);
        ui::paragraph(list, fonts.regular, it.blurb, x, 808, 26, 1000, 38, kInk.with_alpha(0.82f),
                      2);
        const float sx = 1340.0f + slide;
        std::snprintf(text, sizeof(text), "%d", it.hours);
        float w = ui::text(list, fonts.mono, text, sx, 760, 46, kInk);
        ui::text(list, fonts.regular, "hours", sx + w + 12, 760, 24, kInk.with_alpha(kMuted));
        std::snprintf(text, sizeof(text), "%d%%", static_cast<int>(it.progress * 100.0f + 0.5f));
        w = ui::text(list, fonts.mono, text, sx + 220, 760, 46, kInk);
        ui::text(list, fonts.regular, "done", sx + 220 + w + 12, 760, 24, kInk.with_alpha(kMuted));
        list.star(sx + 12, 812, 12, kAmber);
        std::snprintf(text, sizeof(text), "%.1f  \xC2\xB7  %s", static_cast<double>(it.rating),
                      it.studio);
        ui::text(list, fonts.regular, fonts.regular.font->fit(text, 24, kRight - sx - 36), sx + 36,
                 820, 24, kInk.with_alpha(0.82f));
        list.pop_opacity();
    }

    void detail_recent(gfx::DrawList &list, float time) const
    {
        const ui::Fonts &fonts = context_.fonts;
        char text[32];
        const Rect ring = cursor_rect();
        for (int k = 0; k < kRecentCount; ++k)
        {
            const demo::Item &it = item(recent_[static_cast<std::size_t>(k)]);
            const Rect card = detail_item_rect(kRecent, k);
            const float in = rise(time, k, 0.06f, 0.5f);
            const float chosen = nearness(card);
            list.push_opacity(in);
            const Rect art{card.x, card.y + 18.0f * (1.0f - in), card.w, card.w};
            list.image(it.cover, art, gfx::kCanvasUv,
                       Color::rgb(0xffffff).with_alpha(tween::lerp(0.78f, 1.0f, chosen)), 22);
            ui::text(list, fonts.semibold, fonts.semibold.font->fit(it.title, 24, card.w), card.x,
                     card.y + card.w + 38, 24, kInk.with_alpha(tween::lerp(0.8f, 1.0f, chosen)));
            ui::text(list, fonts.regular, kWhen[k], card.x, card.y + card.w + 66, 20,
                     kInk.with_alpha(kMuted));
            ui::text(list, fonts.regular, "h", card.x + card.w, card.y + card.w + 66, 20,
                     kInk.with_alpha(kMuted), gfx::Align::right);
            std::snprintf(text, sizeof(text), "%d", it.hours);
            ui::text(list, fonts.mono, text, card.x + card.w - 16, card.y + card.w + 66, 20,
                     kInk.with_alpha(kMuted), gfx::Align::right);
            list.rounded_rect({card.x, card.y + card.w + 82, card.w, 6}, 3, kInk.with_alpha(0.1f));
            list.rounded_rect(
                {card.x, card.y + card.w + 82, std::max(6.0f, card.w * it.progress * in), 6}, 3,
                kLime);
            list.pop_opacity();
        }
        // The ring frames the selected cover.
        const Rect frame{ring.x - 6, ring.y - 6, ring.w + 12, ring.w + 12};
        list.glow(frame, 26, 22, kLime.with_alpha(0.25f));
        list.bordered_rect(frame, 26, kClear, 3.0f, kLime);

        list.push_opacity(rise(time, 5, 0.06f, 0.5f));
        if (swap_.running)
        {
            const float t = swap_.progress();
            const float move = calm() ? 0.0f : 1.0f;
            recent_info(list, previous_, 1.0f - tween::smoothstep(t * 2.2f),
                        -28.0f * move * tween::cubic_in(tween::clamp01(t * 2.2f)));
            const float arrive = tween::clamp01((t - 0.25f) / 0.75f);
            recent_info(list, cursor_[kRecent], tween::smoothstep(arrive),
                        36.0f * move * (1.0f - tween::quint_out(arrive)));
        }
        else
        {
            recent_info(list, cursor_[kRecent], 1.0f, 0.0f);
        }
        list.pop_opacity();
    }

    void draw_detail_content(gfx::DrawList &list, int tile, float time) const
    {
        ui::text(list, context_.fonts.display, kTiles[tile].headline, kLeft - 2, 258, 46, kInk);
        switch (tile)
        {
        case kFrame:
            detail_frame(list, time);
            break;
        case kPlay:
            detail_play(list, time);
            break;
        case kRenderer:
            detail_renderer(list, time);
            break;
        case kCompletion:
            detail_completion(list, time);
            break;
        case kStorage:
            detail_storage(list, time);
            break;
        case kSession:
            detail_session(list, time);
            break;
        case kTrophies:
            detail_trophies(list, time);
            break;
        default:
            detail_recent(list, time);
            break;
        }
    }

    // ---- the grid, the ring and the shared-element flight ---------------------------------

    void draw_panel(gfx::DrawList &list, const Rect &r, float radius, float lit, Color accent) const
    {
        list.gradient_rect(r, radius, gfx::mix(kPanelTop, accent, 0.05f * lit).with_alpha(0.9f),
                           gfx::mix(kPanelBottom, accent, 0.02f * lit).with_alpha(0.92f));
        list.bordered_rect(r, radius, kClear, 1.5f,
                           gfx::mix(kInk.with_alpha(0.09f), accent.with_alpha(0.5f), lit));
    }

    // A tile in the grid. The focused one is lifted: a little larger, lit in
    // its accent, with a shadow under it.
    void draw_resting(gfx::DrawList &list, int tile) const
    {
        const float in = tile_in(tile);
        const float lift = lift_[tile].value;
        const Color accent = accent_of(tile);
        Rect r = tile_rect(tile);
        if (!calm())
            r.y += 28.0f * (1.0f - in);
        float dx = 0.0f;
        float dy = 0.0f;
        if (tile == focus_ && !expanded_)
        {
            const float nudge = ui::shake(refusal_.value, clock_, 10.0f, 9.0f);
            dx = nudge * refusal_x_;
            dy = nudge * refusal_y_;
        }
        list.push_opacity(in);
        list.push_transform(1.0f + kLift * lift, r.cx(), r.cy(), dx, dy);
        if (lift > 0.01f)
        {
            list.shadow({r.x, r.y + 16, r.w, r.h}, kRadius, 38, kBlack.with_alpha(0.5f * lift));
            const float breath = calm() ? 0.5f : ui::breathe(clock_);
            list.glow(r, kRadius, 26, accent.with_alpha((0.16f + 0.1f * breath) * lift));
        }
        draw_panel(list, r, kRadius, lift, accent);
        caps(list, kTiles[tile].label, r.x + kPad, r.y + kLabelY, kLabelSize, accent);
        draw_tile_content(list, tile, r, chart_time(tile));
        list.pop_transform();
        list.pop_opacity();
    }

    // The one focus ring: it glides between tiles, takes each tile's size and
    // accent, and steps aside while a tile is expanded.
    void draw_ring(gfx::DrawList &list, float open) const
    {
        Rect ring = ring_.value();
        const float nudge = ui::shake(refusal_.value, clock_, 10.0f, 9.0f);
        if (!expanded_)
        {
            ring.x += nudge * refusal_x_;
            ring.y += nudge * refusal_y_;
        }
        // While its tile is in flight the ring leaves with it, so the two
        // never separate during the last moments of a collapse.
        if (!calm())
            ring = lerp_rect(ring, kDetail.inset(-5.0f), expand_[focus_].value);
        const float alpha = tile_in(focus_) * (1.0f - tween::clamp01(open * 4.0f));
        if (alpha <= 0.01f)
            return;
        list.bordered_rect(ring, kRadius + 5.0f, kClear, 3.0f, ring_colour_.value(alpha));
    }

    // A tile between the grid and the detail view: the shared element. Its
    // rectangle is interpolated by the tile's expand spring, so opening and
    // closing follow the same path and can be interrupted at any point.
    void draw_flight(gfx::DrawList &list, std::uint32_t glass, int tile) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float t = expand_[tile].value;
        const Color accent = accent_of(tile);
        const Rect rest = tile_rect(tile);
        const float scale = 1.0f + kLift * lift_[tile].value;
        const Rect from = scaled(rest, scale);
        // Reduced motion: no travel, the detail view simply fades in over the grid.
        const float travel = calm() ? 1.0f : t;
        const Rect panel = lerp_rect(from, kDetail, travel);
        const float radius = tween::lerp(kRadius, kDetailRadius, travel);

        list.push_opacity(calm() ? t : 1.0f);
        list.shadow({panel.x, panel.y + 20, panel.w, panel.h}, radius, 50,
                    kBlack.with_alpha(0.55f));
        list.glow(panel, radius, 30, accent.with_alpha(0.2f * (1.0f - 0.6f * t)));
        // Frosted: the dimmed grid behind is blurred into the panel.
        list.glass(glass, panel, radius, Color::rgb(0xffffff));
        draw_panel(list, panel, radius, 1.0f, accent);

        // The tile's own content stays pinned to the corner and fades early.
        const float leaving = calm() ? 0.0f : 1.0f - tween::clamp01(t * 2.6f);
        if (leaving > 0.01f)
        {
            list.push_opacity(leaving);
            list.push_transform(tween::lerp(scale, 1.0f, t), panel.x, panel.y, 0, 0);
            draw_tile_content(list, tile, {panel.x, panel.y, rest.w, rest.h}, 99.0f);
            list.pop_transform();
            list.pop_opacity();
        }
        // The label is shared by both states: it travels and grows a little.
        ui::text(list, fonts.semibold, kTiles[tile].label,
                 panel.x + tween::lerp(kPad * scale, kDetailPad, travel),
                 panel.y + tween::lerp(kLabelY * scale, 50.0f, travel),
                 tween::lerp(kLabelSize * scale, 20.0f, travel), accent, gfx::Align::left, 3.0f);

        // The detail content arrives once the panel is most of the way there.
        const float arriving = calm() ? 1.0f : tween::clamp01((t - 0.38f) / 0.42f);
        if (arriving > 0.01f)
        {
            const bool moving = !calm() && t < 0.995f;
            if (moving)
                list.push_clip(panel.inset(2.0f));
            list.push_opacity(arriving);
            list.push_transform(calm() ? 1.0f : tween::lerp(0.97f, 1.0f, arriving), kDetail.cx(),
                                kDetail.cy(), 0, 0);
            draw_detail_content(list, tile, calm() ? 99.0f : detail_age_ - 0.3f);
            list.pop_transform();
            list.pop_opacity();
            if (moving)
                list.pop_clip();
        }
        list.pop_opacity();
    }

    void draw_header(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = tween::stagger(age_, 0, 0.05f, 0.5f);
        list.push_opacity(in);
        const float rise_y = calm() ? 0.0f : 10.0f * (1.0f - in);
        const float w = ui::text(list, fonts.display, "Pulse", kGridX - 3, 124 - rise_y, 56, kInk);
        ui::text(list, fonts.regular, "The console and the library at a glance", kGridX + w + 24,
                 124 - rise_y, 24, kInk.with_alpha(kMuted));

        // The state of Square, always on screen: a chip whose dot and word
        // cross-fade between the two states.
        const float live = live_blend_.value;
        const float chip_w = tween::lerp(172.0f, 122.0f, live); // as wide as its word
        const Rect chip{kGridX + kGridW - chip_w, 82, chip_w, 48};
        const Color dot = gfx::mix(kAmber, kLime, live);
        list.bordered_rect(chip, 24, kPanelTop.with_alpha(0.9f), 1.5f, dot.with_alpha(0.4f));
        const float beat = calm() ? 0.5f : ui::breathe(clock_, 1.2f);
        list.glow({chip.x + 20, chip.cy() - 7, 14, 14}, 7, 12, dot.with_alpha(0.7f * beat * live));
        list.circle(chip.x + 27, chip.cy(), 7, dot);
        caps(list, "LIVE", chip.x + 48, chip.cy() + 6, 17, kInk.with_alpha(live));
        caps(list, "PAUSED", chip.x + 48, chip.cy() + 6, 17, kInk.with_alpha(1.0f - live));
        list.pop_opacity();
    }

    void draw_hints(app::Frame &frame, float open) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const ui::GlyphStyle style = ui::GlyphStyle::dark();
        const char *square = live_ ? "Pause" : "Resume";
        // The two rows cross-fade as the detail view takes over.
        const float grid = tile_in(kTileCount) * (1.0f - tween::clamp01(open * 2.0f));
        if (grid > 0.01f)
        {
            frame.scene.push_opacity(grid);
            const ui::Hint hints[] = {{ui::Button::dpad, "Move"},
                                      {ui::Button::cross, "Expand"},
                                      {ui::Button::square, square}};
            ui::draw_hints(frame.scene, fonts, style, hints, 3, kGridX + kGridW, true);
            frame.scene.pop_opacity();
        }
        const float detail = tween::clamp01(open * 2.0f - 1.0f);
        if (detail > 0.01f)
        {
            frame.overlay.push_opacity(detail);
            ui::Hint hints[3];
            int count = 0;
            if (kTiles[detail_].pick != nullptr)
                hints[count++] = {ui::Button::dpad, kTiles[detail_].pick};
            hints[count++] = {ui::Button::square, square};
            hints[count++] = {ui::Button::circle, "Back"};
            ui::draw_hints(frame.overlay, fonts, style, hints, count, kGridX + kGridW, true);
            frame.overlay.pop_opacity();
        }
    }

    app::Context &context_;

    // Invented data, derived once from the catalogue.
    float day_hours_[kDays] = {};
    int day_top_[kDays] = {}; // the title played most on each day
    float week_total_ = 0.0f;
    float day_peak_ = 0.0f;
    int busiest_ = 0;
    float average_ = 0.0f; // mean story progress of the library
    int finished_ = 0;
    int started_ = 0;
    int titles_ = 1;
    int earned_ = 0;  // stars earned across the library
    int perfect_ = 0; // titles with every star
    int designs_ = 0;
    std::array<int, kRanked> ranked_{};
    std::array<int, kRecentCount> recent_{};

    // Real data: this design's own copy of the telemetry, which Square freezes.
    bool live_ = true;
    std::array<float, kLongWindow> frames_{}; // ring buffer of frame times
    int frames_head_ = 0;
    int frames_count_ = 0;
    std::size_t last_head_ = 0; // telemetry head at the last update
    Stats tile_stats_;
    Stats detail_stats_;
    float shown_fps_ = 0.0f;
    int shown_calls_ = 0;
    int shown_shapes_ = 0;
    int shown_voices_ = 0;
    std::array<float, kSpark> calls_history_{};
    std::array<float, kSpark> shapes_history_{};
    int spark_head_ = 0;
    int spark_count_ = 0;

    float age_ = 0.0f;        // seconds since enter(): drives the entrance
    float clock_ = 0.0f;      // free-running time for idle motion
    float detail_age_ = 0.0f; // seconds since a tile was expanded

    // The grid.
    int focus_ = kFrame;
    int came_from_ = -1; // the tile the focus left, and the direction it took
    Direction came_by_ = Direction::none;
    ui::SpringRect ring_;
    ui::SpringColor ring_colour_;
    tween::Spring lift_[kTileCount];
    ui::Pulse refusal_;
    float refusal_x_ = 0.0f;
    float refusal_y_ = 0.0f;
    tween::Spring live_blend_;

    // The detail view.
    bool expanded_ = false;
    int detail_ = kFrame;              // the tile that is, or was last, expanded
    tween::Spring expand_[kTileCount]; // 0 in the grid, 1 as the detail view
    int cursor_[kTileCount] = {};      // the selection inside each detail view
    int previous_ = 0;                 // ... and the one it is fading out
    ui::SpringRect cursor_ring_;
    tween::Spring leader_y_;
    tween::Spring inner_ring_;
    tween::Timer swap_;
};

} // namespace

std::unique_ptr<app::Concept> make_dashboard(app::Context &context)
{
    return std::make_unique<Dashboard>(context);
}

} // namespace hui::concepts
