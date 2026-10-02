// ps5-homebrew-ui - Design "Trophy Room": an achievements screen with ceremony.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The screen every game needs, built as a dark display cabinet: a summary of
// one game on the left, its achievements as a scrolling list of medals on the
// right. The subject is reward, so the work went into three things: metal
// that looks like metal, numbers that count, and an unlock that is an event.
// What makes it feel finished:
//
//   - a medal is an object, not an icon: a rim and a face lit from above, a
//     groove lit the other way, an embossed emblem and specular streaks whose
//     length follows the chord of the face, so they can travel across it;
//   - the focused medal grows and its streak moves, as if it were tilted to
//     catch the light; in the detail sheet the medal turns: its width and its
//     streak follow the same angle, and the narrowed disc is a true ellipse
//     built from polygon bands with anti-aliased edges;
//   - one highlight glides between the rows and takes the colour of the
//     focused tier; rows are keyed by achievement, so a new sort makes them
//     travel to their places and a new filter re-flows them with a stagger;
//   - Triangle earns the focused achievement: a toast with a shine sweep, a
//     burst of sparks, the row regains its colour, and the ring, the tier
//     counter, the points and the level bar all count up to the new state;
//   - every number that changes is set in fixed-width cells, so it counts
//     without jittering; the unlock fanfare climbs in pitch as the cabinet
//     fills, and a hidden achievement uncovers its name a beat later.

#include "concepts/concepts.hpp"

#include "core/tween.hpp"
#include "ui/confetti.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <vector>

namespace hui::concepts
{

namespace
{

using gfx::Color;
using gfx::Rect;

// ---- the design language ---------------------------------------------------

const Color kInk = Color::rgb(0xf6eee2);      // warm white: all text
const Color kGold = Color::rgb(0xe6bd6a);     // hairlines, the ring, the one accent
const Color kGoldPale = Color::rgb(0xffe9b8); // highlights on gold
const Color kVoid = Color::rgb(0x080706);     // the cabinet's darkness
const Color kCaseTop = Color::rgb(0x0f0c0a);  // the recess the list sits in
const Color kCaseLow = Color::rgb(0x080706);
const Color kRowTop = Color::rgb(0x221c16); // a row's panel, lit from above
const Color kRowLow = Color::rgb(0x16120e);
const Color kSpot = Color::rgb(0xffc98a); // the spotlights
const Color kClear = Color::rgb(0x000000, 0.0f);
const Color kBlack = Color::rgb(0x000000);
const Color kWhite = Color::rgb(0xffffff);

constexpr float kMargin = 96.0f;
constexpr float kLeftWidth = 588.0f; // the summary column, about 34 %
constexpr float kCaseX = 716.0f;     // the recess the list sits in
constexpr float kCaseWidth = 1108.0f;
constexpr float kCaseRadius = 22.0f;
constexpr float kListX = 732.0f; // the rows inside it
constexpr float kListWidth = 1056.0f;
constexpr float kListTop = 184.0f; // the recess is the list's clipped viewport
constexpr float kListHeight = 784.0f;
constexpr float kListPad = 10.0f; // room for the lifted first row inside the clip
constexpr float kRowHeight = 116.0f;
constexpr float kRowPitch = 128.0f;
constexpr float kRowRadius = 18.0f;
constexpr float kMedal = 36.0f; // a row medal's radius at rest
constexpr float kTabY = 68.0f;
constexpr float kTabHeight = 48.0f;
constexpr float kRingX = kMargin + kLeftWidth * 0.5f;
constexpr float kRingY = 476.0f;
constexpr float kRingRadius = 142.0f;
constexpr float kToastWidth = 540.0f;
constexpr float kToastHeight = 112.0f;
constexpr float kToastHold = 3.4f;    // seconds a toast stays
constexpr float kReflowDelay = 1.6f;  // an earned row leaves the "Locked" list after this
constexpr float kRevealDelay = 0.45f; // a hidden name uncovers a beat after the fanfare
constexpr float kTwoPi = 6.2831853f;

// The specular streak leans like a "/" (clockwise from 12 o'clock) and moves
// along its own normal.
constexpr float kStreakAngle = 0.56f;
const float kStreakNx = std::cos(kStreakAngle);
const float kStreakNy = std::sin(kStreakAngle);

constexpr const char *kTechniques[] = {
    "Medals built from gradients: rim, reversed groove, face, embossed emblem, light streaks",
    "A specular streak that moves with focus and time, so the metal seems to catch the light",
    "A turning medal: an ellipse from polygon bands, its edges redrawn as anti-aliased lines",
    "An opaque recess behind the list, so its edge fades match whatever the backdrop does",
    "Rows keyed by achievement: position springs make a new sort travel and a filter re-flow",
    "Unlock ceremony: glass toast with a clipped shine sweep, sparks with gravity, confetti",
    "Tabular numerals drawn in fixed cells so counting numbers never jitter",
    "Fanfare pitch climbs with completion; hidden names uncover with their own cue",
};

constexpr app::TourStep kTour[] = {
    {0.5f, 0, Direction::down},
    {0.25f, 0, Direction::down},
    {0.9f, action_bit(Action::jump_next), Direction::none, "cabinet"},
    {0.4f, action_bit(Action::jump_next)},
    {0.9f, 0, Direction::down},
    {0.9f, action_bit(Action::north)},
    {0.6f, action_bit(Action::confirm), Direction::none, "unlock"},
    {1.2f, action_bit(Action::back), Direction::none, "earned"},
    {1.4f, action_bit(Action::confirm)},
    {1.0f, action_bit(Action::back), Direction::none, "detail"},
    {0.5f, action_bit(Action::jump_prev)},
    {0.3f, action_bit(Action::jump_prev)},
    {0.5f, action_bit(Action::west)},
    {1.6f, 0, Direction::none, "rarest"},
    {0.3f, action_bit(Action::west)},
    {0.3f, action_bit(Action::west)},
};

// ---- content ----------------------------------------------------------------

enum Tier : int
{
    kEmber,
    kSilver,
    kGoldTier,
    kPrism,
    kTiers,
};

constexpr const char *kTierNames[kTiers] = {"Ember", "Silver", "Gold", "Prism"};
constexpr int kTierPoints[kTiers] = {10, 25, 50, 150};
constexpr int kPointsPerLevel = 100;

// The six tones a medal is painted with.
struct Metal
{
    Color light; // where the light lands
    Color mid;
    Color dark;
    Color deep; // engraved lines, the emblem's shadow
    Color face_top;
    Color face_low;
};

const Metal kMetals[kTiers] = {
    {Color::rgb(0xffd0a8), Color::rgb(0xdd7b3f), Color::rgb(0x8a3c17), Color::rgb(0x3a1507),
     Color::rgb(0xf0a068), Color::rgb(0xa9501f)},
    {Color::rgb(0xffffff), Color::rgb(0xb7c2d0), Color::rgb(0x5b6676), Color::rgb(0x222933),
     Color::rgb(0xdfe6ee), Color::rgb(0x8793a3)},
    {Color::rgb(0xfff4c2), Color::rgb(0xedbb40), Color::rgb(0x8f5f10), Color::rgb(0x3d2604),
     Color::rgb(0xf8d873), Color::rgb(0xb98220)},
    {Color::rgb(0xffffff), Color::rgb(0xbfe4ff), Color::rgb(0x6f5fd0), Color::rgb(0x261c58),
     Color::rgb(0xc4f4ff), Color::rgb(0xc9a0ff)},
};

// The light a medal throws: the highlight and the glows take it.
const Color kTierGlow[kTiers] = {Color::rgb(0xff8a4c), Color::rgb(0xcfe0ff), Color::rgb(0xffd05a),
                                 Color::rgb(0xb9a6ff)};

struct Entry
{
    const char *name;
    const char *text;
    const char *hint; // how to earn it (shown while locked)
    int tier;
    float rarity;     // per cent of players
    bool hidden;      // name and text stay secret until earned
    const char *date; // null: not earned yet
    int day;          // sort key of the date
    int have;         // progress, for the locked ones
    int need;
};

// One invented game ("Ember and Ash": two rival smiths, one forge, one
// prophecy) with 28 invented achievements, 17 of them earned.
constexpr Entry kEntries[] = {
    {"First Spark", "Light the forge for the first time.", "", kEmber, 96.2f, false, "2 Feb 2026",
     33, 1, 1},
    {"Apprentice Marks", "Finish your first blade without cracking it.", "", kEmber, 88.4f, false,
     "2 Feb 2026", 33, 1, 1},
    {"Bellows Lungs", "Keep the fire at white heat for a full minute.", "", kEmber, 71.0f, false,
     "3 Feb 2026", 34, 1, 1},
    {"A City of Customers", "Fill ten orders from the market square.", "", kEmber, 64.5f, false,
     "5 Feb 2026", 36, 10, 10},
    {"Shared Anvil", "Finish a piece your rival started.", "", kSilver, 52.3f, false, "8 Feb 2026",
     39, 1, 1},
    {"Quench", "Temper five blades in a row without a warp.", "", kEmber, 47.9f, false,
     "9 Feb 2026", 40, 5, 5},
    {"Ore Whisperer", "Find a seam in every district of the old quarry.", "", kEmber, 41.2f, false,
     "14 Feb 2026", 45, 6, 6},
    {"Night Shift", "Work the forge from dusk until the morning bell.", "", kEmber, 38.6f, false,
     "15 Feb 2026", 46, 1, 1},
    {"Folded Thirty Times", "Forge a blade with thirty layers.", "", kSilver, 29.4f, false,
     "21 Feb 2026", 52, 30, 30},
    {"The First Verse", "Hear the opening line of the prophecy.", "", kSilver, 58.8f, true,
     "22 Feb 2026", 53, 1, 1},
    {"Guild Colours", "Be admitted to the Hammerwrights' Guild.", "", kEmber, 44.0f, false,
     "27 Feb 2026", 58, 1, 1},
    {"Truce at the Trough", "Share a meal with your rival.", "", kSilver, 26.7f, false,
     "3 Mar 2026", 62, 1, 1},
    {"Heirloom", "Reforge the broken sword of the founders.", "", kGoldTier, 17.3f, false,
     "9 Mar 2026", 68, 1, 1},
    {"Soot and Starlight", "Watch the comet from the chimney roof.", "", kEmber, 33.1f, false,
     "10 Mar 2026", 69, 1, 1},
    {"Master's Stamp", "Earn the right to sign your own work.", "", kGoldTier, 12.8f, false,
     "18 Mar 2026", 77, 1, 1},
    {"Two Hammers, One Song", "Forge a whole piece in rhythm with your rival.", "", kSilver, 8.4f,
     true, "24 Mar 2026", 83, 1, 1},
    {"Ash to Ash", "Rebuild the forge after the great fire.", "", kEmber, 35.9f, false,
     "29 Mar 2026", 88, 1, 1},
    // ---- still to earn ----
    {"Thousand Strikes", "Land one thousand hammer blows.",
     "Every blow on hot metal counts, in the story and in the free forge.", kEmber, 31.5f, false,
     nullptr, 0, 742, 1000},
    {"Collector of Edges", "Forge one of every blade pattern.",
     "New patterns are sold by the travelling tinker and hidden in the quarry.", kEmber, 14.2f,
     false, nullptr, 0, 23, 36},
    {"No Wasted Iron", "Finish every chapter without scrapping a piece.",
     "Replay a chapter from the ledger; a piece saved at the last heat still counts.", kSilver,
     6.1f, false, nullptr, 0, 3, 7},
    {"Patron of the Square", "Fill one hundred orders from the market square.",
     "Orders refresh every morning bell. Rush orders count twice.", kEmber, 21.7f, false, nullptr,
     0, 37, 100},
    {"Cold Iron", "Refuse the prophecy when it is offered.",
     "A choice late in the story. Listen to the whole offer before you answer.", kGoldTier, 4.2f,
     true, nullptr, 0, 0, 1},
    {"Rival's Respect", "Win all five forge duels.",
     "Duels open at the guild hall once you carry the master's stamp.", kSilver, 9.6f, false,
     nullptr, 0, 2, 5},
    {"Song of the Seam", "Collect every miner's verse.",
     "Miners hum near a seam they have not shown you yet. Follow the sound.", kEmber, 18.9f, false,
     nullptr, 0, 11, 24},
    {"The Last Verse", "Hear the prophecy to its end.",
     "The story will bring you here if you let it.", kGoldTier, 7.7f, true, nullptr, 0, 0, 1},
    {"What the Ash Remembers", "Find the letter under the hearthstone.",
     "Something in the forge was never meant to burn. Look when the fire is out.", kSilver, 3.4f,
     true, nullptr, 0, 0, 1},
    {"Unbroken", "Finish the story without a single cracked blade.",
     "Slow heats and patient quenches. The ledger marks every crack.", kGoldTier, 1.9f, false,
     nullptr, 0, 0, 1},
    {"The Prism Blade", "Forge the blade of the prophecy from all seven ores.",
     "Two ores remain: one under the lighthouse, one the rival keeps.", kPrism, 0.8f, false,
     nullptr, 0, 5, 7},
};
constexpr int kCount = static_cast<int>(sizeof(kEntries) / sizeof(kEntries[0]));

constexpr const char *kHiddenName = "Hidden achievement";
constexpr const char *kHiddenText = "Keep playing to uncover this one.";

enum Filter : int
{
    kAll,
    kUnlocked,
    kLocked,
    kFirstTier, // + tier
    kFilters = kFirstTier + kTiers,
};
constexpr const char *kFilterNames[kFilters] = {"All",    "Unlocked", "Locked", "Ember",
                                                "Silver", "Gold",     "Prism"};
constexpr const char *kFilterNouns[kFilters] = {
    "achievements",       "earned",           "still to earn",    "in the Ember tier",
    "in the Silver tier", "in the Gold tier", "in the Prism tier"};

enum Sort : int
{
    kRecent,
    kRarity,
    kByTier,
    kSorts,
};
constexpr const char *kSortNames[kSorts] = {"Recent", "Rarity", "Tier"};

struct RarityClass
{
    float below; // per cent
    const char *name;
    std::uint32_t colour;
};
constexpr RarityClass kRarities[] = {
    {1.5f, "Mythic", 0xff8fb0},    {5.0f, "Scarce", 0xc9a2ff},    {15.0f, "Rare", 0x7fc4ff},
    {40.0f, "Uncommon", 0x8fe0b0}, {1000.0f, "Common", 0xb8b0a4},
};

const RarityClass &rarity_class(float percent)
{
    for (const RarityClass &entry : kRarities)
    {
        if (percent < entry.below)
            return entry;
    }
    return kRarities[4];
}

// ---- small drawing helpers ----------------------------------------------------

// Takes the colour out of a metal: the locked medals are pewter, a little dim
// and a little cold, but keep the modelling so they still read as objects.
Color toned(Color c, float lit)
{
    const float l = 0.3f * c.r + 0.59f * c.g + 0.11f * c.b;
    const Color pewter{l * 0.58f + 0.03f, l * 0.58f + 0.03f, l * 0.63f + 0.04f, c.a};
    return gfx::mix(pewter, c, lit);
}

// A hairline that fades out toward both ends: two gradients meeting in the
// middle. Fine gold lines organise the cabinet without boxing it in.
void hairline_h(gfx::DrawList &list, float x, float y, float w, Color c)
{
    list.gradient_rect_h({x, y, w * 0.5f, 1.5f}, 0, c.with_alpha(0.0f), c);
    list.gradient_rect_h({x + w * 0.5f, y, w * 0.5f, 1.5f}, 0, c, c.with_alpha(0.0f));
}

// One line of text whose digits all take the width of a zero. A proportional
// face can then show a number that counts without the line shifting about.
float tabular(gfx::DrawList &list, const ui::FontRef &font, std::string_view value, float x,
              float baseline, float size, Color color, gfx::Align align = gfx::Align::left)
{
    const float cell = font.measure("0", size);
    float width = 0.0f;
    for (const char c : value)
    {
        const bool digit = c >= '0' && c <= '9';
        width += digit ? cell : font.measure(std::string_view(&c, 1), size);
    }
    float cursor = x;
    if (align == gfx::Align::right)
        cursor = x - width;
    else if (align == gfx::Align::center)
        cursor = x - width * 0.5f;
    for (const char c : value)
    {
        const std::string_view one(&c, 1);
        if (c >= '0' && c <= '9')
        {
            ui::text(list, font, one, cursor + cell * 0.5f, baseline, size, color,
                     gfx::Align::center);
            cursor += cell;
        }
        else
        {
            cursor += ui::text(list, font, one, cursor, baseline, size, color);
        }
    }
    return width;
}

// A padlock from three shapes: body, shackle (half an arc) and keyhole.
void draw_lock(gfx::DrawList &list, float cx, float cy, float size, Color metal, Color hole)
{
    list.arc(cx, cy - size * 0.06f, size * 0.27f, size * 0.1f, -1.5708f, 3.1416f, metal, false);
    list.rounded_rect({cx - size * 0.36f, cy - size * 0.08f, size * 0.72f, size * 0.52f},
                      size * 0.1f, metal);
    list.circle(cx, cy + size * 0.17f, size * 0.08f, hole);
}

void draw_check(gfx::DrawList &list, float cx, float cy, float size, Color color)
{
    list.line(cx - size * 0.5f, cy, cx - size * 0.12f, cy + size * 0.38f, size * 0.22f, color);
    list.line(cx - size * 0.12f, cy + size * 0.38f, cx + size * 0.5f, cy - size * 0.4f,
              size * 0.22f, color);
}

// The kit draws circles, not ellipses, and a transform scales both axes
// alike. A medal seen at an angle therefore gets its own shape: a stack of
// polygon bands (thin near the poles, where the outline bends fastest), each
// in the gradient's colour for its height. Polygon edges are not anti-aliased,
// so every band's two outer edges are drawn again as lines, which are.
void ellipse(gfx::DrawList &list, float cx, float cy, float a, float b, Color top, Color low)
{
    constexpr int kBands = 30;
    float xs[kBands + 1];
    float ys[kBands + 1];
    for (int i = 0; i <= kBands; ++i)
    {
        const float angle = 3.14159265f * static_cast<float>(i) / static_cast<float>(kBands);
        xs[i] = a * std::sin(angle);
        ys[i] = -b * std::cos(angle);
    }
    const auto shade = [&](int i)
    { return gfx::mix(top, low, ((ys[i] + ys[i + 1]) * 0.5f + b) / (2.0f * b)); };
    for (int i = 0; i < kBands; ++i)
    {
        if (i == 0)
        {
            const float xy[] = {cx, cy + ys[0], cx + xs[1], cy + ys[1], cx - xs[1], cy + ys[1]};
            list.polygon(xy, 3, shade(i));
        }
        else if (i == kBands - 1)
        {
            const float xy[] = {cx - xs[i], cy + ys[i], cx + xs[i], cy + ys[i], cx, cy + ys[i + 1]};
            list.polygon(xy, 3, shade(i));
        }
        else
        {
            const float xy[] = {cx - xs[i],     cy + ys[i],     cx + xs[i],     cy + ys[i],
                                cx + xs[i + 1], cy + ys[i + 1], cx - xs[i + 1], cy + ys[i + 1]};
            list.polygon(xy, 4, shade(i));
        }
    }
    for (int i = 0; i < kBands; ++i)
    {
        const Color c = shade(i);
        list.line(cx + xs[i], cy + ys[i], cx + xs[i + 1], cy + ys[i + 1], 2.0f, c);
        list.line(cx - xs[i], cy + ys[i], cx - xs[i + 1], cy + ys[i + 1], 2.0f, c);
    }
}

// The outline of an ellipse, from short lines. The colour must be opaque:
// the round caps overlap at every joint.
void ellipse_ring(gfx::DrawList &list, float cx, float cy, float a, float b, float thickness,
                  Color color)
{
    constexpr int kSegments = 56;
    float px = 0.0f;
    float py = -b;
    for (int i = 1; i <= kSegments; ++i)
    {
        const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(kSegments);
        const float x = a * std::sin(angle);
        const float y = -b * std::cos(angle);
        list.line(cx + px, cy + py, cx + x, cy + y, thickness, color);
        px = x;
        py = y;
    }
}

void draw_bar(gfx::DrawList &list, const Rect &bar, float fraction, Color top, Color low)
{
    list.rounded_rect(bar, bar.h * 0.5f, kWhite.with_alpha(0.12f));
    if (fraction > 0.0f)
        list.gradient_rect({bar.x, bar.y, std::max(bar.h, bar.w * tween::clamp01(fraction)), bar.h},
                           bar.h * 0.5f, top, low);
}

// How a medal is shown.
struct MedalLook
{
    int tier = kEmber;
    float lit = 1.0f;    // 0 pewter (locked) .. 1 full colour
    float sheen = -0.4f; // where the streak sits across the face, -1 .. 1
    float squash = 1.0f; // width factor: the medal turned a little away
    float secret = 0.0f; // 1: a question mark instead of the emblem
    float lock = 0.0f;   // 1: the padlock badge
};

// One spark of an unlock burst.
struct Spark
{
    float x, y, vx, vy;
    float age, life, size;
    Color color;
};

// What changes about one achievement while the screen runs.
struct Row
{
    bool unlocked = false;
    bool today = false;   // earned in this session
    int order = 0;        // recency: higher is newer
    bool present = false; // part of the current filter
    int slot = 0;         // its place in the list
    float delay = 0.0f;   // stagger: seconds before it starts toward its slot
    float reveal_wait = 0.0f;
    tween::Spring y; // list coordinates (before scrolling)
    tween::Spring alpha;
    tween::Spring lit; // colour of the medal and brightness of the row
    tween::Spring focus;
    tween::Spring reveal; // 0: "Hidden achievement", 1: the real name
    ui::Pulse flash;
};

enum class Reflow
{
    filter, // new set: stagger, newcomers slide in, the focus returns to the top
    sort,   // same set, new order: rows travel, the focus stays on its achievement
    shrink, // an earned row leaves the "Locked" list: the focus keeps its place
};

class Trophies final : public app::Concept
{
  public:
    explicit Trophies(app::Context &context) : context_(context)
    {
        const auto &items = context.catalog.items();
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            if (std::strcmp(items[i].title, "Ember and Ash") == 0)
                game_ = i;
        }
        for (int i = 0; i < kCount; ++i)
        {
            Row &state = rows_[static_cast<std::size_t>(i)];
            state.unlocked = kEntries[i].date != nullptr;
            state.order = kEntries[i].day;
            state.lit.snap(state.unlocked ? 1.0f : 0.0f);
            state.reveal.snap(kEntries[i].hidden && !state.unlocked ? 0.0f : 1.0f);
        }
        recount();
        level_ = points_now_ / kPointsPerLevel;

        // The tabs never change, so their rectangles are measured once.
        const ui::Fonts &fonts = context.fonts;
        float x = kListX + ui::button_width(ui::Button::l2, 34.0f) + 18.0f;
        for (int i = 0; i < kFilters; ++i)
        {
            const float dot = i >= kFirstTier ? 24.0f : 0.0f;
            const float w = 40.0f + dot + fonts.semibold.measure(kFilterNames[i], 24.0f);
            tabs_[static_cast<std::size_t>(i)] = {x, kTabY, w, kTabHeight};
            x += w + 4.0f;
        }
        tabs_end_ = x + 14.0f;

        rebuild(Reflow::filter);
        settle_rows();
        highlight_.snap(slot_rect(0));
        tab_pill_.snap(tabs_[0]);
        glow_.snap(focus_glow());
        ring_.snap(fraction_done());
        points_.snap(static_cast<float>(points_now_));
        count_.snap(static_cast<float>(unlocked_));
        shown_.snap(static_cast<float>(view_.size()));
        level_fill_.snap(level_fraction());
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "trophies",
            "Trophy Room",
            "An achievements cabinet: metal medals, counting numbers, an unlock with ceremony",
            "src/concepts/trophies.cpp",
            audio::SoundSet::paper,
            kGold,
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        age_ = 0.0f;
        detail_open_ = false;
        toast_ = -1;
        toast_in_.snap(0.0f);
        sparks_.clear();
        settle_rows();
        // The summary counts up from nothing every time the cabinet opens.
        ring_.snap(0.0f);
        points_.snap(0.0f);
        count_.snap(0.0f);
        level_fill_.snap(0.0f);
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        const bool reduced = context_.settings.reduced_motion;
        age_ += dt;
        clock_ += dt;
        if (detail_open_)
            update_detail(input, feedback);
        else
            update_list(input, feedback);

        // An earned row stays in the "Locked" list long enough to be seen
        // changing, then leaves and the rows below close the gap.
        if (reflow_in_ > 0.0f)
        {
            reflow_in_ -= dt;
            if (reflow_in_ <= 0.0f)
                rebuild(Reflow::shrink);
        }

        // ---- rows ----
        const int focused = focused_entry();
        for (int i = 0; i < kCount; ++i)
        {
            Row &state = rows_[static_cast<std::size_t>(i)];
            if (state.delay > 0.0f)
                state.delay -= dt;
            if (state.present && state.delay <= 0.0f)
            {
                state.y.target = static_cast<float>(state.slot) * kRowPitch;
                state.alpha.target = 1.0f;
            }
            else if (!state.present)
            {
                state.alpha.target = 0.0f;
            }
            if (reduced)
                state.y.snap(state.y.target);
            else
                state.y.update(dt, 13.0f);
            state.alpha.update(dt, state.present ? 12.0f : 24.0f);
            state.lit.target = state.unlocked ? 1.0f : 0.0f;
            state.lit.update(dt, 5.0f);
            state.focus.target = i == focused ? 1.0f : 0.0f;
            state.focus.update(dt, reduced ? 40.0f : 15.0f);
            if (state.reveal_wait > 0.0f)
            {
                state.reveal_wait -= dt;
                if (state.reveal_wait <= 0.0f)
                {
                    // The name uncovers a beat after the fanfare, with its
                    // own small sound: a second event, not a second fanfare.
                    state.reveal.target = 1.0f;
                    feedback.play(audio::Cue::reveal, 1.0f, ui::pan_for_x(kListX + 300.0f));
                }
            }
            state.reveal.update(dt, 7.0f);
            state.flash.update(dt, 2.4f);
        }

        // ---- list furniture ----
        const int count = static_cast<int>(view_.size());
        if (count > 0)
        {
            const float start = static_cast<float>(focus_) * kRowPitch;
            scroll_.reveal(start, start + kRowHeight, kListHeight - 2.0f * kListPad,
                           kRowPitch * 0.4f,
                           static_cast<float>(count) * kRowPitch - (kRowPitch - kRowHeight));
            highlight_.target(slot_rect(focus_));
            glow_.target(focus_glow());
        }
        else
        {
            scroll_.position.target = 0.0f;
        }
        scroll_.update(dt, reduced ? 40.0f : 13.0f);
        highlight_.update(dt, reduced ? 60.0f : 20.0f);
        highlight_alpha_.target = count > 0 ? 1.0f : 0.0f;
        highlight_alpha_.update(dt, 12.0f);
        glow_.update(dt, 7.0f);
        tab_pill_.target(tabs_[static_cast<std::size_t>(filter_)]);
        tab_pill_.update(dt, reduced ? 60.0f : 18.0f);
        sort_fade_.update(dt);
        // The count under the tabs is the truth, not the number of rows: an
        // earned row that has not left the "Locked" list yet no longer counts.
        shown_.target = static_cast<float>(
            std::count_if(view_.begin(), view_.end(), [this](int i) { return matches(i); }));
        shown_.update(dt, 9.0f);
        bump_.update(dt, 9.0f);
        shake_.update(dt, 9.0f);
        tab_nudge_.update(dt, 9.0f);

        // ---- summary: everything counts toward the real state ----
        const float counting = reduced ? 16.0f : 5.5f;
        ring_.target = fraction_done();
        ring_.update(dt, counting);
        points_.target = static_cast<float>(points_now_);
        points_.update(dt, counting);
        count_.target = static_cast<float>(unlocked_);
        count_.update(dt, counting);
        level_fill_.target = level_fraction();
        level_fill_.update(dt, counting);
        for (ui::Pulse &pop : tier_pop_)
            pop.update(dt, 5.0f);
        ring_flash_.update(dt, 2.2f);
        level_pulse_.update(dt, 3.0f);

        // ---- ceremony ----
        if (toast_ >= 0)
        {
            toast_age_ += dt;
            toast_in_.target = toast_age_ < kToastHold ? 1.0f : 0.0f;
            toast_in_.update(dt, reduced ? 30.0f : 11.0f);
            if (toast_age_ >= kToastHold && toast_in_.value < 0.01f)
                toast_ = -1;
        }
        for (Spark &spark : sparks_)
        {
            spark.vy += 1500.0f * dt;
            spark.vx *= std::exp(-1.8f * dt);
            spark.x += spark.vx * dt;
            spark.y += spark.vy * dt;
            spark.age += dt;
        }
        sparks_.erase(std::remove_if(sparks_.begin(), sparks_.end(),
                                     [](const Spark &s) { return s.age >= s.life; }),
                      sparks_.end());
        confetti_.update(dt);
        detail_.target = detail_open_ ? 1.0f : 0.0f;
        detail_.update(dt, reduced ? 40.0f : 13.0f);
    }

    void draw(app::Frame &frame) const override
    {
        // The cabinet: near-black, with one warm light the shader paints
        // behind the completion ring. The other spotlights are glows in the
        // scene, so they can sit exactly where the content is.
        const bool reduced = context_.settings.reduced_motion;
        frame.backdrop.mode = gfx::BackdropMode::gradient;
        frame.backdrop.colors[0] = Color::rgb(0x120e0a);
        frame.backdrop.colors[1] = Color::rgb(0x050404);
        frame.backdrop.colors[2] = Color::rgb(0x6a4420);
        frame.backdrop.params[0] = 0.2f + (reduced ? 0.0f : 0.008f * std::sin(clock_ * 0.31f));
        frame.backdrop.params[1] = 0.4f;
        frame.backdrop.params[2] = 0.5f + 0.25f * ring_flash_.value;
        frame.backdrop.time = clock_;

        gfx::DrawList &list = frame.scene;
        const float back = detail_.value;
        list.push_transform(1.0f - 0.03f * back, 960, 540, 0, 0);
        draw_ambience(list);
        draw_summary(list);
        draw_tabs(list);
        draw_list(list);
        draw_sparks(list);
        list.pop_transform();
        if (back > 0.01f)
            list.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                              kVoid.with_alpha(0.6f * back));

        if (back > 0.01f || toast_ >= 0)
            frame.glass = true;
        if (back > 0.01f)
            draw_detail(frame.overlay, frame.glass_texture);
        if (toast_ >= 0)
            draw_toast(frame.overlay, frame.glass_texture);
        confetti_.draw(frame.overlay);
        draw_hints(back > 0.5f ? frame.overlay : frame.scene);
    }

    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    // ---- state queries --------------------------------------------------------

    const Row &row(int index) const
    {
        return rows_[static_cast<std::size_t>(index)];
    }

    int focused_entry() const
    {
        return view_.empty() ? -1 : view_[static_cast<std::size_t>(focus_)];
    }

    static float progress(int index)
    {
        const Entry &e = kEntries[index];
        return e.need > 0 ? static_cast<float>(e.have) / static_cast<float>(e.need) : 0.0f;
    }

    float fraction_done() const
    {
        return static_cast<float>(unlocked_) / static_cast<float>(kCount);
    }

    float level_fraction() const
    {
        return static_cast<float>(points_now_ % kPointsPerLevel) /
               static_cast<float>(kPointsPerLevel);
    }

    Color focus_glow() const
    {
        const int index = focused_entry();
        if (index < 0)
            return kGold;
        // A locked medal throws no coloured light: its highlight stays gold.
        return gfx::mix(kGold, kTierGlow[kEntries[index].tier], row(index).unlocked ? 0.75f : 0.2f);
    }

    bool matches(int index) const
    {
        switch (filter_)
        {
        case kAll:
            return true;
        case kUnlocked:
            return row(index).unlocked;
        case kLocked:
            return !row(index).unlocked;
        default:
            return kEntries[index].tier == filter_ - kFirstTier;
        }
    }

    // A slot's rectangle in list coordinates (before scrolling).
    static Rect slot_rect(int slot)
    {
        return {0.0f, static_cast<float>(slot) * kRowPitch, kListWidth, kRowHeight};
    }

    // List coordinates to the screen.
    Rect on_screen(float list_y) const
    {
        return {kListX, kListTop + kListPad + list_y - scroll_.offset(), kListWidth, kRowHeight};
    }

    void recount()
    {
        unlocked_ = 0;
        points_now_ = 0;
        points_all_ = 0;
        tier_unlocked_.fill(0);
        tier_total_.fill(0);
        for (int i = 0; i < kCount; ++i)
        {
            const std::size_t tier = static_cast<std::size_t>(kEntries[i].tier);
            ++tier_total_[tier];
            points_all_ += kTierPoints[tier];
            if (row(i).unlocked)
            {
                ++unlocked_;
                ++tier_unlocked_[tier];
                points_now_ += kTierPoints[tier];
            }
        }
    }

    // ---- the list: which rows, in which order ------------------------------------

    void rebuild(Reflow reason)
    {
        const int keep = focused_entry();
        view_.clear();
        for (int i = 0; i < kCount; ++i)
        {
            if (matches(i))
                view_.push_back(i);
        }
        // stable_sort over ascending indices: ties keep the table's order.
        std::stable_sort(view_.begin(), view_.end(),
                         [this](int a, int b)
                         {
                             const Row &ra = row(a);
                             const Row &rb = row(b);
                             switch (sort_)
                             {
                             case kRarity:
                                 return kEntries[a].rarity < kEntries[b].rarity;
                             case kByTier:
                                 if (kEntries[a].tier != kEntries[b].tier)
                                     return kEntries[a].tier > kEntries[b].tier;
                                 return kEntries[a].rarity < kEntries[b].rarity;
                             default:
                                 // Recent: the newest medals first, then the locked ones by
                                 // how close they are.
                                 if (ra.unlocked != rb.unlocked)
                                     return ra.unlocked;
                                 if (ra.unlocked)
                                     return ra.order > rb.order;
                                 return progress(a) > progress(b);
                             }
                         });

        const int count = static_cast<int>(view_.size());
        int focus = reason == Reflow::shrink ? focus_ : 0;
        if (reason == Reflow::sort)
        {
            for (int k = 0; k < count; ++k)
            {
                if (view_[static_cast<std::size_t>(k)] == keep)
                    focus = k;
            }
        }
        focus_ = std::clamp(focus, 0, std::max(0, count - 1));

        std::array<bool, kCount> was{};
        for (int i = 0; i < kCount; ++i)
        {
            was[static_cast<std::size_t>(i)] = rows_[static_cast<std::size_t>(i)].present;
            rows_[static_cast<std::size_t>(i)].present = false;
        }
        const bool reduced = context_.settings.reduced_motion;
        // The stagger is a delay before each row leaves for its new slot: a
        // filter change ripples down the list, a sort starts almost together.
        const float step = reduced ? 0.0f : (reason == Reflow::filter ? 0.035f : 0.012f);
        for (int k = 0; k < count; ++k)
        {
            const std::size_t index = static_cast<std::size_t>(view_[static_cast<std::size_t>(k)]);
            Row &r = rows_[index];
            r.present = true;
            r.slot = k;
            r.delay = static_cast<float>(std::min(k, 10)) * step;
            if (!was[index] || r.alpha.value < 0.02f)
            {
                // A newcomer has no old place to travel from: it rises into
                // its slot from just below.
                r.y.snap(static_cast<float>(k) * kRowPitch + (reduced ? 0.0f : 34.0f));
                r.alpha.snap(0.0f);
            }
        }
    }

    // Puts every row where it belongs, with no travel (start-up and enter()).
    void settle_rows()
    {
        for (Row &r : rows_)
        {
            r.delay = 0.0f;
            r.y.snap(static_cast<float>(r.slot) * kRowPitch);
            r.alpha.snap(r.present ? 1.0f : 0.0f);
        }
        scroll_.position.snap(scroll_.position.target);
    }

    // ---- input ---------------------------------------------------------------------

    void refuse(app::Feedback &feedback) const
    {
        feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        feedback.rumble(0.25f, 0.05f);
    }

    void update_list(const InputFrame &input, app::Feedback &feedback)
    {
        const bool prev = input.is_pressed(Action::jump_prev);
        const bool next = input.is_pressed(Action::jump_next);
        if (prev || next)
        {
            const int wanted = filter_ + (next ? 1 : -1);
            if (wanted >= 0 && wanted < kFilters)
            {
                filter_ = wanted;
                reflow_in_ = 0.0f;
                rebuild(Reflow::filter);
                feedback.play(audio::Cue::tab, 0.94f + 0.03f * static_cast<float>(wanted),
                              ui::pan_for_x(tabs_[static_cast<std::size_t>(wanted)].cx()));
            }
            else
            {
                refuse(feedback);
                tab_nudge_.trigger();
                tab_nudge_direction_ = next ? 1.0f : -1.0f;
            }
        }

        if (input.is_pressed(Action::west))
        {
            previous_sort_ = sort_;
            sort_ = (sort_ + 1) % kSorts;
            sort_fade_.start(context_.settings.reduced_motion ? 0.1f : 0.3f);
            reflow_in_ = 0.0f;
            rebuild(Reflow::sort);
            feedback.play(audio::Cue::cascade, 1.0f, ui::pan_for_x(kListX + kListWidth));
        }

        const int count = static_cast<int>(view_.size());
        if (input.nav == Direction::up || input.nav == Direction::down)
        {
            const int step = input.nav == Direction::down ? 1 : -1;
            const int wanted = focus_ + step;
            if (count > 0 && wanted >= 0 && wanted < count)
            {
                focus_ = wanted;
                // Precious metal rings higher: the tick tells the tier.
                const int tier = kEntries[focused_entry()].tier;
                feedback.play(audio::Cue::focus, 0.94f + 0.06f * static_cast<float>(tier),
                              ui::pan_for_x(kListX + kListWidth * 0.5f));
            }
            else if (!input.nav_repeat)
            {
                refuse(feedback);
                bump_.trigger();
                bump_direction_ = static_cast<float>(step);
            }
        }

        if (input.is_pressed(Action::confirm))
        {
            if (count > 0)
            {
                detail_open_ = true;
                detail_entry_ = focused_entry();
                feedback.play(audio::Cue::modal_open);
            }
            else
            {
                refuse(feedback);
            }
        }
        if (input.is_pressed(Action::north))
            earn(feedback);
    }

    // The sheet owns the input until it is closed.
    void update_detail(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.is_pressed(Action::back) || input.is_pressed(Action::confirm))
        {
            detail_open_ = false;
            feedback.play(audio::Cue::modal_close);
        }
    }

    // Triangle: the focused achievement is earned, as the game would report it.
    void earn(app::Feedback &feedback)
    {
        const int index = focused_entry();
        if (index < 0 || row(index).unlocked)
        {
            refuse(feedback);
            shake_.trigger();
            return;
        }
        const bool reduced = context_.settings.reduced_motion;
        const Entry &entry = kEntries[index];
        Row &r = rows_[static_cast<std::size_t>(index)];
        r.unlocked = true;
        r.today = true;
        r.order = 1000 + ++earned_now_;
        r.flash.trigger();
        if (entry.hidden)
            r.reveal_wait = kRevealDelay;
        recount();

        const bool complete = unlocked_ == kCount;
        const bool grand = complete || entry.tier == kPrism;
        const Rect at = on_screen(r.y.value);
        // One fanfare per unlock, and it climbs as the cabinet fills.
        feedback.play(grand ? audio::Cue::complete : audio::Cue::new_record,
                      0.96f + 0.12f * fraction_done(), ui::pan_for_x(at.x + 74.0f));
        feedback.rumble(grand ? 0.8f : 0.5f, grand ? 0.3f : 0.16f);

        tier_pop_[static_cast<std::size_t>(entry.tier)].trigger();
        ring_flash_.trigger();
        const int level = points_now_ / kPointsPerLevel;
        if (level != level_)
        {
            // The bar rolls over: it restarts from empty on the new level.
            level_ = level;
            level_fill_.snap(0.0f);
            level_pulse_.trigger();
        }
        toast_ = index;
        toast_age_ = 0.0f;
        toast_complete_ = complete;
        if (!reduced)
            burst(at.x + 74.0f, at.cy(), entry.tier, grand ? 34 : 22);
        if (complete)
            confetti_.burst(kGold, 0x7a3f91u + static_cast<std::uint32_t>(earned_now_), reduced);
        if (!matches(index))
            reflow_in_ = kReflowDelay;
    }

    float random(float low, float high)
    {
        seed_ ^= seed_ << 13;
        seed_ ^= seed_ >> 17;
        seed_ ^= seed_ << 5;
        return low + (high - low) * static_cast<float>(seed_ & 0xffffff) / 16777215.0f;
    }

    void burst(float x, float y, int tier, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            const float angle = random(0.0f, kTwoPi);
            const float speed = random(180.0f, 620.0f);
            Spark s{};
            s.x = x;
            s.y = y;
            s.vx = std::sin(angle) * speed;
            s.vy = -std::cos(angle) * speed - 220.0f; // thrown mostly upward
            s.life = random(0.7f, 1.3f);
            s.size = random(7.0f, 17.0f);
            s.color = gfx::mix(kTierGlow[tier], kWhite, random(0.3f, 0.95f));
            sparks_.push_back(s);
        }
    }

    // ---- drawing: the medal -----------------------------------------------------------

    // A medal is five stacked discs and a few streaks. Light comes from above,
    // so the rim runs light to dark; the groove inside it runs the other way,
    // which is what makes it read as cut into the metal; the face repeats the
    // rim's direction more gently. With squash below 1 the discs become a
    // little narrower than tall: the medal turned away from the viewer.
    void draw_medal(gfx::DrawList &list, float cx, float cy, float radius,
                    const MedalLook &look) const
    {
        const Metal &base = kMetals[look.tier];
        const float lit = look.lit;
        const Color light = toned(base.light, lit);
        const Color mid = toned(base.mid, lit);
        const Color dark = toned(base.dark, lit);
        const Color deep = toned(base.deep, lit);
        const float half = radius * look.squash;
        const bool round = look.squash > 0.999f;
        const float gleam = 0.45f + 0.55f * lit; // pewter still glints, only less
        const auto disc = [&](float k, Color top, Color low)
        {
            const float w = half * k;
            const float h = radius * k;
            if (round)
                list.gradient_rect({cx - w, cy - h, 2.0f * w, 2.0f * h}, w, top, low);
            else
                ellipse(list, cx, cy, w, h, top, low);
        };

        const Color face_top = toned(base.face_top, lit);
        const Color face_low = toned(base.face_low, lit);
        list.shadow({cx - half, cy - radius + radius * 0.18f, 2.0f * half, 2.0f * radius}, half,
                    radius * 0.34f, kBlack.with_alpha(0.55f));
        disc(1.0f, light, dark);
        disc(0.9f, dark, gfx::mix(mid, light, 0.4f));
        disc(0.83f, face_top, face_low);
        // An engraved circle: a dark line with a light one just below it.
        const float line = std::max(1.0f, radius * 0.022f);
        const float engraved = 0.68f;
        if (round)
        {
            list.ring(cx, cy + line, radius * engraved, line, light.with_alpha(0.35f));
            list.ring(cx, cy, radius * engraved, line, deep.with_alpha(0.45f));
            // The rim catches the lamp at its upper left and a little bounce
            // light at its lower right.
            list.arc(cx, cy, radius * 0.985f, radius * 0.05f, -1.15f, 1.0f,
                     kWhite.with_alpha(0.55f * gleam));
            list.arc(cx, cy, radius * 0.985f, radius * 0.04f, 2.2f, 0.7f,
                     light.with_alpha(0.3f * gleam));
        }
        else
        {
            const Color face_mid = gfx::mix(face_top, face_low, 0.5f);
            ellipse_ring(list, cx, cy + line, half * engraved, radius * engraved, line,
                         gfx::mix(face_mid, light, 0.35f));
            ellipse_ring(list, cx, cy, half * engraved, radius * engraved, line,
                         gfx::mix(face_mid, deep, 0.45f));
        }

        // The emblem, embossed: a shadow below, a highlight above, the body
        // between them. One shape per tier, so the tier reads without colour.
        const float e = radius * 0.4f;
        const float lift = std::max(1.0f, radius * 0.035f);
        const Color body = gfx::mix(gfx::mix(light, mid, 0.25f), kWhite, 0.6f * look.secret);
        const auto emblem = [&](float dy, Color c)
        {
            const float y = cy + dy;
            switch (look.tier)
            {
            case kEmber: // a flame: a drop made of a disc and a triangle
                list.circle(cx, y + e * 0.3f, e * 0.6f, c);
                list.triangle({cx - e * 0.56f, y - e * 1.05f, e * 1.12f, e * 1.2f}, c);
                break;
            case kSilver: // a cut stone
                list.rotated_rect({cx - e * 0.68f, y - e * 0.68f, e * 1.36f, e * 1.36f}, e * 0.12f,
                                  0.7854f, c);
                break;
            case kGoldTier:
                list.star(cx, y, e * 1.15f, c);
                break;
            default: // a prism
                list.triangle({cx - e * 0.95f, y - e * 0.9f, e * 1.9f, e * 1.65f}, c);
                break;
            }
        };
        if (look.secret < 1.0f)
        {
            list.push_opacity(1.0f - look.secret);
            emblem(lift, deep.with_alpha(0.7f));
            emblem(-lift * 0.6f, kWhite.with_alpha(0.55f * gleam));
            emblem(0.0f, body);
            if (look.tier == kEmber)
                list.circle(cx, cy + e * 0.36f, e * 0.26f, dark.with_alpha(0.55f));
            else if (look.tier == kSilver)
                list.rotated_rect({cx - e * 0.3f, cy - e * 0.3f, e * 0.6f, e * 0.6f}, e * 0.06f,
                                  0.7854f, dark.with_alpha(0.45f));
            else if (look.tier == kPrism)
                list.triangle({cx - e * 0.42f, cy - e * 0.2f, e * 0.84f, e * 0.72f},
                              dark.with_alpha(0.5f));
            list.pop_opacity();
        }
        if (look.secret > 0.0f)
        {
            const float size = radius * 0.95f;
            list.push_opacity(look.secret);
            ui::text(list, context_.fonts.semibold, "?", cx, cy + size * 0.36f + lift, size,
                     deep.with_alpha(0.7f), gfx::Align::center);
            ui::text(list, context_.fonts.semibold, "?", cx, cy + size * 0.36f, size, body,
                     gfx::Align::center);
            list.pop_opacity();
        }

        // Specular streaks, over everything: the emblem is metal too. Each is
        // a thin rounded bar leaning across the face; its length is the chord
        // of the face at its distance from the centre, so it can slide from
        // edge to edge without ever leaving the disc.
        const float face = radius * 0.8f;
        const auto streak = [&](float at, float thickness, Color c)
        {
            const float d = at * face;
            const float edge = std::fabs(d) + thickness * 0.5f;
            const float reach = face * face - edge * edge;
            if (reach <= 0.0f)
                return;
            const float length = 2.0f * std::sqrt(reach) * 0.94f;
            if (length <= thickness)
                return;
            const float fade = 1.0f - (d / face) * (d / face);
            const float x = cx + kStreakNx * d * look.squash;
            const float y = cy + kStreakNy * d;
            list.rotated_rect({x - thickness * 0.5f, y - length * 0.5f, thickness, length},
                              thickness * 0.5f, kStreakAngle, c.with_alpha(fade * gleam));
        };
        if (look.tier == kPrism)
        {
            // Glass splits the light: coloured bands ride beside the white one.
            streak(look.sheen - 0.5f, radius * 0.16f, Color::rgb(0xff8ad8, 0.4f * lit));
            streak(look.sheen + 0.62f, radius * 0.14f, Color::rgb(0x7dffd6, 0.38f * lit));
        }
        streak(look.sheen, radius * 0.34f, kWhite.with_alpha(0.14f));
        streak(look.sheen, radius * 0.16f, kWhite.with_alpha(0.2f));
        streak(look.sheen + 0.32f, radius * 0.05f, kWhite.with_alpha(0.38f));

        if (look.lock > 0.01f)
        {
            // The padlock sits on its own small plate at the medal's shoulder.
            const float bx = cx + half * 0.68f;
            const float by = cy + radius * 0.66f;
            const float br = radius * 0.36f;
            list.push_opacity(look.lock);
            list.circle(bx, by, br + 2.0f, kVoid);
            list.gradient_rect({bx - br, by - br, 2.0f * br, 2.0f * br}, br, Color::rgb(0x3a352f),
                               Color::rgb(0x1c1915));
            draw_lock(list, bx, by, br * 1.25f, Color::rgb(0xd8cfc0), Color::rgb(0x1c1915));
            list.pop_opacity();
        }
    }

    // ---- drawing: the scene --------------------------------------------------------------

    void draw_ambience(gfx::DrawList &list) const
    {
        // Two more spotlights (glows fill their interior, so each is a pool
        // of light) and the hairline that separates the two columns.
        const float in = tween::stagger(age_, 0, 0.05f, 0.9f);
        list.glow({kRingX - 40, kRingY - 40, 80, 80}, 40, 300,
                  kSpot.with_alpha((0.1f + 0.1f * ring_flash_.value) * in));
        list.glow({1100, -140, 420, 60}, 30, 360, kSpot.with_alpha(0.075f * in));
    }

    void draw_summary(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const demo::Item &game = context_.catalog[game_];
        const float right = kMargin + kLeftWidth;
        char text[64];

        // The game: cover, title, studio.
        float in = tween::stagger(age_, 0, 0.07f, 0.5f);
        list.push_opacity(in);
        const float slide = 18.0f * (1.0f - in);
        ui::text(list, fonts.semibold, "TROPHY ROOM", kMargin, 92 - slide, 16, kGold,
                 gfx::Align::left, 4.0f);
        const Rect cover{kMargin, 118 - slide, 128, 128};
        list.shadow({cover.x, cover.y + 12, cover.w, cover.h}, 14, 26, kBlack.with_alpha(0.6f));
        list.image(game.cover, cover, gfx::kCanvasUv, kWhite, 14);
        list.bordered_rect(cover.inset(-3), 17, kClear, 1.5f, kGold.with_alpha(0.7f));
        const float tx = cover.x + cover.w + 26;
        ui::text(list, fonts.display, fonts.display.font->fit(game.title, 42, right - tx), tx,
                 166 - slide, 42, kInk);
        std::snprintf(text, sizeof(text), "%s  \xC2\xB7  %s", game.studio, game.genre);
        ui::text(list, fonts.regular, fonts.regular.font->fit(text, 22, right - tx), tx,
                 202 - slide, 22, kInk.with_alpha(0.7f));
        std::snprintf(text, sizeof(text), "%d h at the forge", game.hours);
        ui::text(list, fonts.regular, text, tx, 234 - slide, 22, kInk.with_alpha(0.5f));
        list.pop_opacity();

        // The completion ring: a track, the arc, a pale inner line that gives
        // the arc a bevel, and one tick per achievement around it.
        in = tween::stagger(age_, 1, 0.07f, 0.6f);
        list.push_opacity(in);
        const float done = ring_.value;
        const float flash = ring_flash_.value;
        list.arc(kRingX, kRingY, kRingRadius, 16, 0.0f, kTwoPi, kWhite.with_alpha(0.07f), false);
        if (done > 0.002f)
        {
            list.arc(kRingX, kRingY, kRingRadius, 16, 0.0f, kTwoPi * done,
                     gfx::mix(kGold, kGoldPale, 0.6f * flash));
            list.arc(kRingX, kRingY, kRingRadius - 3, 3.5f, 0.0f, kTwoPi * done,
                     kGoldPale.with_alpha(0.75f));
            // The leading end of the arc is the brightest point on the ring.
            const float a = kTwoPi * done;
            const float ex = kRingX + std::sin(a) * (kRingRadius - 8);
            const float ey = kRingY - std::cos(a) * (kRingRadius - 8);
            list.glow({ex - 4, ey - 4, 8, 8}, 4, 22 + 26 * flash,
                      kGoldPale.with_alpha(0.5f + 0.4f * flash));
        }
        for (int i = 0; i < kCount; ++i)
        {
            const float a = kTwoPi * (static_cast<float>(i) + 0.5f) / static_cast<float>(kCount);
            const float lit = tween::clamp01(count_.value - static_cast<float>(i));
            const float sx = std::sin(a);
            const float sy = -std::cos(a);
            list.line(kRingX + sx * (kRingRadius + 14), kRingY + sy * (kRingRadius + 14),
                      kRingX + sx * (kRingRadius + 24), kRingY + sy * (kRingRadius + 24), 3.0f,
                      gfx::mix(kWhite.with_alpha(0.14f), kGold, lit));
        }
        std::snprintf(text, sizeof(text), "%d", static_cast<int>(done * 100.0f + 0.5f));
        // The big figure is set right-aligned against a fixed edge (as wide
        // as that many zeros), so the per cent sign stands still while the
        // digits count and the pair stays properly kerned once they stop.
        const float number = static_cast<float>(std::strlen(text)) * fonts.display.measure("0", 84);
        const float sign = fonts.display.measure("%", 40);
        const float edge = kRingX + (number - 8 - sign) * 0.5f;
        ui::text(list, fonts.display, text, edge, kRingY + 18, 84, kInk, gfx::Align::right);
        ui::text(list, fonts.display, "%", edge + 8, kRingY + 18, 40, kGold);
        const bool full = unlocked_ == kCount;
        ui::text(list, fonts.semibold, full ? "CABINET COMPLETE" : "COMPLETE", kRingX, kRingY + 54,
                 full ? 14 : 16, full ? kGoldPale : kInk.with_alpha(0.6f), gfx::Align::center,
                 full ? 2.5f : 4.0f);
        std::snprintf(text, sizeof(text), "%d / %d", static_cast<int>(count_.value + 0.5f), kCount);
        ui::text(list, fonts.mono, text, kRingX, kRingY + 90, 20, kInk.with_alpha(0.6f),
                 gfx::Align::center);
        list.pop_opacity();

        // Four tier counters, each under a small medal of its metal.
        const float cell = kLeftWidth / static_cast<float>(kTiers);
        for (int t = 0; t < kTiers; ++t)
        {
            in = tween::stagger(age_, 3 + t, 0.07f, 0.5f);
            const float pop = tier_pop_[static_cast<std::size_t>(t)].value;
            const float cx = kMargin + cell * (static_cast<float>(t) + 0.5f);
            const float rise = 20.0f * (1.0f - in);
            list.push_opacity(in);
            if (pop > 0.01f)
                list.glow({cx - 10, 716 - 10, 20, 20}, 10, 46, kTierGlow[t].with_alpha(0.5f * pop));
            MedalLook look;
            look.tier = t;
            look.sheen = -0.4f + 1.2f * pop;
            draw_medal(list, cx, 716 + rise, 26.0f * (1.0f + 0.3f * pop), look);
            std::snprintf(text, sizeof(text), "%d / %d",
                          tier_unlocked_[static_cast<std::size_t>(t)],
                          tier_total_[static_cast<std::size_t>(t)]);
            ui::text(list, fonts.mono, text, cx, 780 + rise, 22,
                     gfx::mix(kInk, kGoldPale, tween::clamp01(pop * 2.0f)), gfx::Align::center);
            ui::text(list, fonts.semibold, ui::upper(kTierNames[t]), cx + 1.5f, 806 + rise, 15,
                     kInk.with_alpha(0.55f), gfx::Align::center, 3.0f);
            list.pop_opacity();
        }

        // Points and the level bar.
        in = tween::stagger(age_, 7, 0.07f, 0.5f);
        list.push_opacity(in);
        hairline_h(list, kMargin, 836, kLeftWidth, kGold.with_alpha(0.35f));
        std::snprintf(text, sizeof(text), "%d", static_cast<int>(points_.value + 0.5f));
        const float w = tabular(list, fonts.semibold, text, kMargin, 898, 48, kGoldPale);
        std::snprintf(text, sizeof(text), "of %d points", points_all_);
        ui::text(list, fonts.regular, text, kMargin + w + 14, 898, 22, kInk.with_alpha(0.6f));
        const float up = level_pulse_.value;
        std::snprintf(text, sizeof(text), "LEVEL %d", level_ + 1);
        ui::text(list, fonts.semibold, text, right, 896, 16 + 4.0f * up,
                 gfx::mix(kGold, kWhite, up), gfx::Align::right, 3.5f);
        const Rect bar{kMargin, 918, kLeftWidth, 8};
        if (up > 0.01f)
            list.glow(bar, 4, 18, kGoldPale.with_alpha(0.5f * up));
        draw_bar(list, bar, level_fill_.value, kGoldPale, kGold);
        std::snprintf(text, sizeof(text), "%d / %d to level %d", points_now_ % kPointsPerLevel,
                      kPointsPerLevel, level_ + 2);
        ui::text(list, fonts.mono, text, kMargin, 958, 20, kInk.with_alpha(0.55f));
        list.pop_opacity();
    }

    void draw_tabs(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const ui::GlyphStyle style = ui::GlyphStyle::dark();
        const float in = tween::stagger(age_, 2, 0.07f, 0.5f);
        const float cy = kTabY + kTabHeight * 0.5f;
        char text[64];
        list.push_opacity(in);
        list.push_transform(1.0f, 0, 0, 0, -14.0f * (1.0f - in));

        ui::draw_button(list, fonts, style, ui::Button::l2, kListX, cy, 34);
        ui::draw_button(list, fonts, style, ui::Button::r2, tabs_end_, cy, 34);
        // One pill glides between the tabs; at either end it nudges instead.
        Rect pill = tab_pill_.value();
        pill.x += tab_nudge_direction_ * 10.0f * tab_nudge_.value;
        list.gradient_rect(pill, pill.h * 0.5f, kGold.with_alpha(0.26f), kGold.with_alpha(0.1f));
        list.bordered_rect(pill, pill.h * 0.5f, kClear, 1.5f, kGold.with_alpha(0.85f));
        for (int i = 0; i < kFilters; ++i)
        {
            const Rect &tab = tabs_[static_cast<std::size_t>(i)];
            const bool active = i == filter_;
            float x = tab.x + 20;
            if (i >= kFirstTier)
            {
                const Metal &metal = kMetals[i - kFirstTier];
                list.gradient_rect({x, cy - 7, 14, 14}, 7, metal.light, metal.dark);
                x += 24;
            }
            ui::text(list, active ? fonts.semibold : fonts.regular, kFilterNames[i], x, cy + 8.5f,
                     24, kInk.with_alpha(active ? 1.0f : 0.58f));
        }

        // Under the tabs: how many rows the filter shows, and the sort order.
        const float base = 158.0f;
        std::snprintf(text, sizeof(text), "%d", static_cast<int>(shown_.value + 0.5f));
        const float w = tabular(list, fonts.semibold, text, kListX, base, 22, kInk);
        ui::text(list, fonts.regular, kFilterNouns[filter_], kListX + w + 10, base, 22,
                 kInk.with_alpha(0.6f));
        const float right = kListX + kListWidth;
        const float t = sort_fade_.running ? sort_fade_.progress() : 1.0f;
        if (t < 0.5f)
            ui::text(list, fonts.semibold, kSortNames[previous_sort_], right, base - 14.0f * t, 22,
                     kGoldPale.with_alpha(1.0f - t * 2.0f), gfx::Align::right);
        else
            ui::text(list, fonts.semibold, kSortNames[sort_], right, base + 14.0f * (1.0f - t), 22,
                     kGoldPale.with_alpha(t * 2.0f - 1.0f), gfx::Align::right);
        ui::text(list, fonts.semibold, "SORTED BY", right - 92, base - 1, 15, kInk.with_alpha(0.5f),
                 gfx::Align::right, 3.0f);
        list.pop_transform();
        list.pop_opacity();
    }

    // The entrance progress of the row in a slot.
    float row_entrance(int slot) const
    {
        return tween::stagger(age_, 3 + std::min(slot, 8), 0.06f, 0.5f);
    }

    void draw_row_panel(gfx::DrawList &list, const Rect &r) const
    {
        list.gradient_rect(r, kRowRadius, kRowTop, kRowLow);
        list.bordered_rect(r, kRowRadius, kClear, 1.0f, kGold.with_alpha(0.14f));
    }

    void draw_row_content(gfx::DrawList &list, int index, const Rect &r) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        const Entry &entry = kEntries[index];
        const Row &state = row(index);
        const float focus = state.focus.value;
        const float lit = state.lit.value;
        const float flash = state.flash.value;
        char text[64];

        // The lift: the focused row comes a little toward the viewer.
        list.push_transform(1.0f + 0.016f * focus, r.cx(), r.cy(), 0, -2.0f * focus);
        if (flash > 0.01f)
        {
            list.glow(r, kRowRadius, 30, kTierGlow[entry.tier].with_alpha(0.5f * flash));
            list.rounded_rect(r, kRowRadius, kGoldPale.with_alpha(0.3f * flash));
        }

        // The medal. At rest its streak sits high on the left; with focus it
        // crosses toward the centre and then sways, as if held and turned
        // under the lamp. On an unlock it sweeps in from the edge.
        MedalLook look;
        look.tier = entry.tier;
        look.lit = lit;
        look.secret = 1.0f - tween::smoothstep(state.reveal.value * 2.0f);
        look.lock = 1.0f - tween::clamp01(lit * 2.5f);
        const float sway = reduced ? 0.0f : 0.2f * std::sin(clock_ * 1.7f);
        const float rest = tween::lerp(-0.42f, 0.16f, focus) + focus * sway;
        look.sheen = state.unlocked ? tween::lerp(-1.2f, rest, tween::cubic_out(lit)) : rest;
        const float mx = r.x + 74;
        if (lit > 0.5f && focus > 0.01f)
            list.glow({mx - 14, r.cy() - 14, 28, 28}, 14, 44,
                      kTierGlow[entry.tier].with_alpha(0.3f * focus * lit));
        draw_medal(list, mx, r.cy(), kMedal * (1.0f + 0.14f * focus + 0.25f * tween::ping(flash)),
                   look);

        // Locked rows are dimmed; the focused one comes up enough to read.
        list.push_opacity(tween::lerp(tween::lerp(0.6f, 0.88f, focus), 1.0f, lit));

        // Right block: rarity above, then the date or the progress. It is
        // drawn first because its two widths decide how much room the name
        // and the description get before they are cut with an ellipsis.
        const float right = r.x + r.w - 28;
        const RarityClass &rarity = rarity_class(entry.rarity);
        std::snprintf(text, sizeof(text), "%s, %.1f%% of players", rarity.name,
                      static_cast<double>(entry.rarity));
        const float rw = ui::text(list, fonts.regular, text, right, r.y + 48, 20,
                                  kInk.with_alpha(0.72f), gfx::Align::right);
        list.rotated_rect({right - rw - 22, r.y + 36, 10, 10}, 2, 0.7854f,
                          Color::rgb(rarity.colour));
        float second = 0.0f; // the wider of the two things the lower line may show
        const float earned = tween::clamp01(lit * 2.0f - 1.0f);
        if (earned > 0.0f)
        {
            list.push_opacity(earned);
            if (state.today)
                std::snprintf(text, sizeof(text), "Earned today");
            else
                std::snprintf(text, sizeof(text), "Earned %s", entry.date ? entry.date : "");
            const float dw = ui::text(list, fonts.regular, text, right, r.y + 86, 20, kGoldPale,
                                      gfx::Align::right);
            draw_check(list, right - dw - 20, r.y + 79, 14, kGold);
            second = dw + 28;
            list.pop_opacity();
        }
        if (lit < 0.5f)
        {
            list.push_opacity(1.0f - lit * 2.0f);
            if (entry.hidden)
            {
                second =
                    std::max(second, ui::text(list, fonts.semibold, "SECRET", right, r.y + 85, 16,
                                              kInk.with_alpha(0.6f), gfx::Align::right, 3.5f));
            }
            else
            {
                std::snprintf(text, sizeof(text), "%d / %d", entry.have, entry.need);
                const float fw = ui::text(list, fonts.mono, text, right, r.y + 86, 20,
                                          kInk.with_alpha(0.9f), gfx::Align::right);
                draw_bar(list, {right - fw - 16 - 130, r.y + 76, 130, 6}, progress(index),
                         kGoldPale, kGold);
                second = std::max(second, fw + 16 + 130);
            }
            list.pop_opacity();
        }

        const float tx = r.x + 136;
        const float name_width = right - rw - 22 - 32 - tx;
        const float text_width = right - second - 32 - tx;
        const float real = tween::smoothstep(state.reveal.value * 2.0f - 1.0f);
        if (real < 1.0f)
        {
            // Still secret: the placeholder leaves upward as the name arrives.
            const float gone = tween::smoothstep(state.reveal.value * 2.0f);
            list.push_opacity(1.0f - gone);
            ui::text(list, fonts.semibold, kHiddenName, tx, r.y + 50 - 10.0f * gone, 28, kInk);
            ui::text(list, fonts.regular, kHiddenText, tx, r.y + 86 - 10.0f * gone, 24,
                     kInk.with_alpha(0.68f));
            list.pop_opacity();
        }
        if (real > 0.0f)
        {
            list.push_opacity(real);
            const float rise = 12.0f * (1.0f - real);
            ui::text(list, fonts.semibold, fonts.semibold.font->fit(entry.name, 28, name_width), tx,
                     r.y + 50 + rise, 28, kInk);
            ui::text(list, fonts.regular, fonts.regular.font->fit(entry.text, 24, text_width), tx,
                     r.y + 86 + rise, 24, kInk.with_alpha(0.68f));
            list.pop_opacity();
        }
        list.pop_opacity();
        list.pop_transform();
    }

    void draw_list(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const int focused = focused_entry();
        const float bottom = kListTop + kListHeight;

        // The recess: an opaque, darker box let into the cabinet. Because it
        // is opaque, the fades at its ends can be painted in exactly its own
        // colours, whatever the backdrop is doing behind it.
        const Rect recess{kCaseX, kListTop, kCaseWidth, kListHeight};
        const float case_in = tween::stagger(age_, 2, 0.07f, 0.6f);
        list.push_opacity(case_in);
        list.gradient_rect(recess, kCaseRadius, kCaseTop, kCaseLow);
        list.pop_opacity();

        // One clip for the whole list (a clip per row would be a draw call
        // per row).
        list.push_clip(recess);
        const auto placed = [&](int index, Rect *r, float *alpha)
        {
            const Row &state = row(index);
            const float in = row_entrance(state.slot);
            *alpha = state.alpha.value * in;
            *r = on_screen(state.y.value);
            r->y += 30.0f * (1.0f - in);
            return *alpha > 0.01f && r->y + r->h > kListTop - 30 && r->y < bottom + 30;
        };
        Rect r;
        float alpha = 0.0f;
        for (int i = 0; i < kCount; ++i)
        {
            if (!placed(i, &r, &alpha))
                continue;
            list.push_opacity(alpha);
            draw_row_panel(list, r);
            list.pop_opacity();
        }

        // The highlight is its own object. It lives in list coordinates, so
        // it scrolls with the rows and only its own glide is visible.
        if (highlight_alpha_.value > 0.01f)
        {
            Rect h = on_screen(highlight_.y.value).inset(-5);
            h.x += ui::shake(shake_.value, clock_);
            h.y += bump_direction_ * 12.0f * bump_.value;
            const float breath =
                context_.settings.reduced_motion ? 0.5f : ui::breathe(clock_, 2.8f);
            const Color glow = glow_.value();
            list.push_opacity(highlight_alpha_.value * row_entrance(focus_) *
                              (1.0f - 0.6f * detail_.value));
            list.shadow({h.x, h.y + 16, h.w, h.h}, kRowRadius + 5, 32, kBlack.with_alpha(0.7f));
            list.glow(h, kRowRadius + 5, 24, glow.with_alpha(0.22f + 0.1f * breath));
            list.gradient_rect(h, kRowRadius + 5, Color::rgb(0x40321f, 0.86f),
                               Color::rgb(0x211911, 0.86f));
            list.bordered_rect(h, kRowRadius + 5, kClear, 2.0f, gfx::mix(kGold, glow, 0.35f));
            hairline_h(list, h.x + 30, h.y + 3, h.w - 60, kGoldPale.with_alpha(0.55f));
            list.pop_opacity();
        }

        for (int pass = 0; pass < 2; ++pass)
        {
            // The focused row is drawn last, on top of its neighbours.
            for (int i = 0; i < kCount; ++i)
            {
                if ((i == focused) != (pass == 1) || !placed(i, &r, &alpha))
                    continue;
                if (i == focused)
                {
                    r.x += ui::shake(shake_.value, clock_);
                    r.y += bump_direction_ * 12.0f * bump_.value;
                }
                list.push_opacity(alpha);
                draw_row_content(list, i, r);
                list.pop_opacity();
            }
        }
        list.pop_clip();

        // Edge fades say "there is more": only where there is.
        const float offset = scroll_.offset();
        const float limit =
            std::max(0.0f, static_cast<float>(view_.size()) * kRowPitch - (kRowPitch - kRowHeight) -
                               (kListHeight - 2.0f * kListPad));
        const float above = tween::clamp01(offset / 40.0f);
        const float below = tween::clamp01((limit - offset) / 40.0f);
        constexpr float kFade = 44.0f;
        list.push_opacity(case_in);
        // A little shadow under the top lip, always: the box has depth.
        list.gradient_rect({recess.x, recess.y, recess.w, 30}, kCaseRadius, kBlack.with_alpha(0.4f),
                           kBlack.with_alpha(0.0f));
        if (above > 0.0f)
            list.gradient_rect({recess.x, recess.y, recess.w, kFade}, kCaseRadius,
                               kCaseTop.with_alpha(above), kCaseTop.with_alpha(0.0f));
        if (below > 0.0f)
            list.gradient_rect({recess.x, bottom - kFade, recess.w, kFade}, kCaseRadius,
                               kCaseLow.with_alpha(0.0f), kCaseLow.with_alpha(below));
        list.bordered_rect(recess, kCaseRadius, kClear, 1.5f, kGold.with_alpha(0.24f));
        list.pop_opacity();

        // A thin scroll thumb in the gutter.
        if (limit > 0.0f)
        {
            const float in = row_entrance(0);
            const Rect track{kListX + kListWidth + 15, kListTop + kListPad + 8, 4,
                             kListHeight - 2.0f * kListPad - 16};
            const float view = kListHeight - 2.0f * kListPad;
            const float size = track.h * view / (view + limit);
            list.rounded_rect(track, 2, kWhite.with_alpha(0.07f * in));
            list.rounded_rect({track.x, track.y + (track.h - size) * tween::clamp01(offset / limit),
                               track.w, size},
                              2, kGold.with_alpha(0.7f * in));
        }

        // The empty state: only "Locked" can run dry, when everything is won.
        const float empty = 1.0f - highlight_alpha_.value;
        if (view_.empty() && empty > 0.01f)
        {
            const float cx = kListX + kListWidth * 0.5f;
            const float cy = kListTop + 300 + 20.0f * (1.0f - empty);
            list.push_opacity(empty);
            list.glow({cx - 30, cy - 30, 60, 60}, 30, 120, kSpot.with_alpha(0.14f));
            list.ring(cx, cy, 62, 2.0f, kGold.with_alpha(0.8f));
            list.ring(cx, cy, 52, 1.0f, kGold.with_alpha(0.35f));
            draw_check(list, cx, cy, 44, kGoldPale);
            ui::text(list, fonts.display, "Nothing left to earn", cx, cy + 128, 40, kInk,
                     gfx::Align::center);
            ui::text(list, fonts.regular, "Every medal in this cabinet is yours.", cx, cy + 172, 24,
                     kInk.with_alpha(0.65f), gfx::Align::center);
            list.pop_opacity();
        }
    }

    void draw_sparks(gfx::DrawList &list) const
    {
        for (const Spark &s : sparks_)
        {
            const float t = s.age / s.life;
            const float size = s.size * (1.0f - t * t);
            const float alpha = 1.0f - t * t;
            // A spark is a glint: a soft point of light, a thin cross through
            // it and a small star at its heart.
            list.glow({s.x - 1, s.y - 1, 2, 2}, 1, size * 2.2f, s.color.with_alpha(0.5f * alpha));
            list.line(s.x - size * 1.3f, s.y, s.x + size * 1.3f, s.y, size * 0.14f,
                      kWhite.with_alpha(0.8f * alpha));
            list.line(s.x, s.y - size * 1.3f, s.x, s.y + size * 1.3f, size * 0.14f,
                      kWhite.with_alpha(0.8f * alpha));
            list.star(s.x, s.y, size * 0.62f, gfx::mix(s.color, kWhite, 0.5f).with_alpha(alpha));
        }
    }

    // ---- drawing: overlays ------------------------------------------------------------------

    void draw_toast(gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        const Entry &entry = kEntries[toast_];
        const float in = toast_in_.value;
        const float age = toast_age_;
        char text[64];
        const Rect toast{1824 - kToastWidth + (reduced ? 0.0f : (kToastWidth + 120) * (1.0f - in)),
                         64, kToastWidth, kToastHeight};
        const float radius = 24.0f;
        list.push_opacity(tween::clamp01(in * 1.5f));
        list.shadow({toast.x, toast.y + 16, toast.w, toast.h}, radius, 40, kBlack.with_alpha(0.6f));
        list.glow(toast, radius, 26, kTierGlow[entry.tier].with_alpha(0.22f));
        list.glass(glass, toast, radius, kWhite);
        list.gradient_rect(toast, radius, Color::rgb(0x33281a, 0.8f), Color::rgb(0x18130d, 0.86f));
        list.bordered_rect(toast, radius, kClear, 1.5f, kGold.with_alpha(0.8f));

        // The medal pops in with overshoot, and its streak crosses once.
        const float pop = reduced ? 1.0f : tween::back_out((age - 0.12f) / 0.45f);
        MedalLook look;
        look.tier = entry.tier;
        look.sheen =
            reduced ? -0.3f : tween::lerp(-1.1f, 0.25f, tween::cubic_out((age - 0.3f) / 0.9f));
        if (pop > 0.02f)
            draw_medal(list, toast.x + 64, toast.cy(), 36.0f * pop, look);

        const float tx = toast.x + 122;
        const float tw = toast.w - 122 - 24;
        ui::text(list, fonts.semibold, toast_complete_ ? "CABINET COMPLETE" : "ACHIEVEMENT EARNED",
                 tx, toast.y + 34, 15, kGold, gfx::Align::left, 3.5f);
        ui::text(list, fonts.semibold, fonts.semibold.font->fit(entry.name, 28, tw), tx,
                 toast.y + 69, 28, kInk);
        std::snprintf(text, sizeof(text), "%s  \xC2\xB7  +%d points", kTierNames[entry.tier],
                      kTierPoints[entry.tier]);
        ui::text(list, fonts.regular, text, tx, toast.y + 97, 20, kInk.with_alpha(0.72f));

        // The shine: a narrow bright band crossing the whole plate, left to
        // right, over the medal and the text like a reflection on glass. A
        // soft edge is built from nested bars, each adding a little light.
        // The clip is a rectangle, so it is pulled in past the rounded
        // corners, and the band fades in and out at the ends of its run.
        const float run = tween::clamp01((age - 0.3f) / 0.8f);
        if (!reduced && run > 0.0f && run < 1.0f)
        {
            const float x =
                tween::lerp(toast.x - 30, toast.x + toast.w + 30, tween::smoothstep(run));
            const float strength = tween::clamp01(tween::ping(run) * 2.2f);
            list.push_clip(
                {toast.x + radius * 0.6f, toast.y + 1.5f, toast.w - radius * 1.2f, toast.h - 3});
            for (int i = 0; i < 6; ++i)
            {
                const float w = 90.0f - 15.0f * static_cast<float>(i);
                list.rotated_rect({x - w * 0.5f, toast.y - 40, w, toast.h + 80}, 0, 0.42f,
                                  kGoldPale.with_alpha(0.075f * strength));
            }
            list.rotated_rect({x + 56, toast.y - 40, 4, toast.h + 80}, 0, 0.42f,
                              kWhite.with_alpha(0.26f * strength));
            list.pop_clip();
        }
        list.pop_opacity();
    }

    void draw_detail(gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        const int index = detail_entry_;
        const Entry &entry = kEntries[index];
        const Row &state = row(index);
        const bool secret = entry.hidden && !state.unlocked;
        const float t = detail_.value;
        char text[80];

        const Rect panel{330, 192 + 44.0f * (1.0f - t), 1260, 700};
        const float radius = 40.0f;
        list.push_opacity(tween::clamp01(t * 1.4f));
        list.shadow({panel.x, panel.y + 24, panel.w, panel.h}, radius, 70, kBlack.with_alpha(0.6f));
        // Frosted glass: the blurred cabinet, a warm dark tint, a gold hairline.
        list.glass(glass, panel, radius, kWhite);
        list.gradient_rect(panel, radius, Color::rgb(0x2a2118, 0.74f), Color::rgb(0x120e0a, 0.84f));
        list.bordered_rect(panel, radius, kClear, 1.5f, kGold.with_alpha(0.55f));
        hairline_h(list, panel.x + 60, panel.y + 3, panel.w - 120, kGoldPale.with_alpha(0.5f));

        // The medal turns slowly about its vertical axis: the width follows
        // the cosine of the angle and the streak follows its sine, so the two
        // agree and the eye reads one rotating object.
        const float turn = reduced ? 0.0f : 0.5f * std::sin(clock_ * 0.9f);
        const float mx = panel.x + 230;
        const float my = panel.y + 290;
        MedalLook look;
        look.tier = entry.tier;
        look.lit = state.lit.value;
        look.squash = std::cos(turn);
        look.sheen = reduced ? -0.3f : std::sin(turn) * 1.75f;
        look.secret = secret ? 1.0f : 0.0f;
        look.lock = 1.0f - tween::clamp01(state.lit.value * 2.5f);
        const float appear = tween::stagger(t, 0, 0.1f, 0.7f);
        const Color tier_light = toned(kTierGlow[entry.tier], 0.35f + 0.65f * state.lit.value);
        list.glow({mx - 50, my - 50, 100, 100}, 50, 150,
                  tier_light.with_alpha(0.1f + 0.14f * state.lit.value));
        draw_medal(list, mx, my, 132.0f * (0.86f + 0.14f * appear), look);
        ui::text(list, fonts.semibold, ui::upper(kTierNames[entry.tier]), mx + 3, panel.y + 492, 20,
                 tier_light, gfx::Align::center, 6.0f);
        std::snprintf(text, sizeof(text), "%d points", kTierPoints[entry.tier]);
        ui::text(list, fonts.regular, text, mx, panel.y + 530, 24, kInk.with_alpha(0.75f),
                 gfx::Align::center);
        // A status plate under the medal.
        const Rect plate{mx - 92, panel.y + 566, 184, 44};
        if (state.unlocked)
        {
            list.gradient_rect(plate, 22, kGoldPale, kGold);
            draw_check(list, plate.x + 34, plate.cy(), 16, Color::rgb(0x2a1c06));
            ui::text(list, fonts.semibold, "EARNED", plate.x + 58, plate.cy() + 6.5f, 17,
                     Color::rgb(0x2a1c06), gfx::Align::left, 3.0f);
        }
        else
        {
            list.bordered_rect(plate, 22, kWhite.with_alpha(0.06f), 1.5f, kInk.with_alpha(0.35f));
            draw_lock(list, plate.x + 36, plate.cy() - 2, 22, kInk.with_alpha(0.8f),
                      Color::rgb(0x1c1915));
            ui::text(list, fonts.semibold, "LOCKED", plate.x + 60, plate.cy() + 6.5f, 17,
                     kInk.with_alpha(0.8f), gfx::Align::left, 3.0f);
        }

        // The text column arrives a beat after the panel, top to bottom.
        const float x = panel.x + 470;
        const float w = panel.w - 470 - 64;
        const auto section = [&](int order)
        {
            const float a = tween::stagger(t, order, 0.1f, 0.6f);
            list.push_opacity(a);
            return 22.0f * (1.0f - a);
        };
        float dy = section(1);
        std::snprintf(text, sizeof(text), "%s ACHIEVEMENT",
                      ui::upper(kTierNames[entry.tier]).c_str());
        ui::text(list, fonts.semibold, text, x, panel.y + 92 + dy, 17, tier_light, gfx::Align::left,
                 4.0f);
        ui::text(list, fonts.display,
                 fonts.display.font->fit(secret ? kHiddenName : entry.name, 52, w), x - 2,
                 panel.y + 158 + dy, 52, kInk);
        ui::paragraph(list, fonts.regular,
                      secret ? "This one stays secret until it is earned." : entry.text, x,
                      panel.y + 212 + dy, 26, w, 38, kInk.with_alpha(0.85f), 2);
        list.pop_opacity();

        dy = section(2);
        const RarityClass &rarity = rarity_class(entry.rarity);
        ui::text(list, fonts.semibold, "RARITY", x, panel.y + 322 + dy, 15, kInk.with_alpha(0.55f),
                 gfx::Align::left, 3.5f);
        // An honest bar: its length is the share of players, however small.
        draw_bar(list, {x, panel.y + 340 + dy, w, 10}, entry.rarity / 100.0f * appear,
                 gfx::mix(Color::rgb(rarity.colour), kWhite, 0.35f), Color::rgb(rarity.colour));
        std::snprintf(text, sizeof(text), "%s, %.1f%% of players", rarity.name,
                      static_cast<double>(entry.rarity));
        list.rotated_rect({x + 2, panel.y + 373 + dy, 12, 12}, 2, 0.7854f,
                          Color::rgb(rarity.colour));
        ui::text(list, fonts.regular, text, x + 28, panel.y + 388 + dy, 24, kInk.with_alpha(0.9f));
        list.pop_opacity();

        dy = section(3);
        hairline_h(list, x - 20, panel.y + 428, w + 40, kGold.with_alpha(0.3f));
        if (state.unlocked)
        {
            ui::text(list, fonts.semibold, "EARNED", x, panel.y + 480 + dy, 15,
                     kInk.with_alpha(0.55f), gfx::Align::left, 3.5f);
            ui::text(list, fonts.semibold, state.today ? "Today" : entry.date, x,
                     panel.y + 528 + dy, 36, kGoldPale);
            ui::text(list, fonts.regular,
                     state.today ? "Fresh from the forge. It joins the cabinet now."
                                 : "It has been in the cabinet ever since.",
                     x, panel.y + 574 + dy, 24, kInk.with_alpha(0.65f));
        }
        else
        {
            ui::text(list, fonts.semibold, "HOW TO EARN IT", x, panel.y + 480 + dy, 15,
                     kInk.with_alpha(0.55f), gfx::Align::left, 3.5f);
            ui::paragraph(list, fonts.regular, entry.hint, x, panel.y + 520 + dy, 24, w, 34,
                          kInk.with_alpha(0.85f), 2);
            if (secret)
            {
                ui::text(list, fonts.regular, "Progress stays hidden too.", x, panel.y + 630 + dy,
                         22, kInk.with_alpha(0.5f));
            }
            else
            {
                std::snprintf(text, sizeof(text), "%d / %d", entry.have, entry.need);
                const float fw = ui::text(list, fonts.mono, text, x + w, panel.y + 632 + dy, 24,
                                          kInk, gfx::Align::right);
                draw_bar(list, {x, panel.y + 618 + dy, w - fw - 24, 10}, progress(index) * appear,
                         kGoldPale, kGold);
            }
        }
        list.pop_opacity();
        list.pop_opacity();
    }

    void draw_hints(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const ui::GlyphStyle style = ui::GlyphStyle::dark();
        if (detail_open_)
        {
            list.push_opacity(detail_.value);
            const ui::Hint hints[] = {{ui::Button::circle, "Close"}};
            ui::draw_hints(list, fonts, style, hints, 1, 1824, true);
            list.pop_opacity();
        }
        else
        {
            list.push_opacity(tween::stagger(age_, 9, 0.07f, 0.5f) * (1.0f - detail_.value));
            const ui::Hint hints[] = {{ui::Button::cross, "Details"},
                                      {ui::Button::triangle, "Earn it"},
                                      {ui::Button::square, "Sort"},
                                      {ui::Button::l2, "Filter", ui::Button::r2}};
            ui::draw_hints(list, fonts, style, hints, 4, 1824, true);
            list.pop_opacity();
        }
    }

    app::Context &context_;
    std::size_t game_ = 7; // the catalogue title these achievements belong to
    std::array<Row, kCount> rows_;
    std::vector<int> view_; // the achievements the filter shows, in sort order
    int filter_ = kAll;
    int sort_ = kRecent;
    int previous_sort_ = kRecent;
    int focus_ = 0;      // a slot in view_
    int earned_now_ = 0; // unlocks in this session
    float reflow_in_ = 0.0f;

    // Totals, recomputed when something is earned.
    int unlocked_ = 0;
    int points_now_ = 0;
    int points_all_ = 0;
    int level_ = 0;
    std::array<int, kTiers> tier_unlocked_{};
    std::array<int, kTiers> tier_total_{};

    float age_ = 0.0f;   // seconds since enter(): drives the entrance
    float clock_ = 0.0f; // free-running time for idle motion
    std::array<Rect, kFilters> tabs_{};
    float tabs_end_ = 0.0f;
    ui::SpringRect tab_pill_;
    ui::Pulse tab_nudge_;
    float tab_nudge_direction_ = 0.0f;
    tween::Timer sort_fade_;
    ui::Scroller scroll_;
    ui::SpringRect highlight_; // list coordinates
    tween::Spring highlight_alpha_;
    ui::SpringColor glow_;
    ui::Pulse bump_; // a list end refused
    float bump_direction_ = 0.0f;
    ui::Pulse shake_; // Triangle refused

    // The numbers the summary shows, chasing the totals above.
    tween::Spring ring_;
    tween::Spring points_;
    tween::Spring count_;
    tween::Spring shown_;
    tween::Spring level_fill_;
    std::array<ui::Pulse, kTiers> tier_pop_;
    ui::Pulse ring_flash_;
    ui::Pulse level_pulse_;

    int toast_ = -1; // the achievement the toast announces
    float toast_age_ = 0.0f;
    bool toast_complete_ = false;
    tween::Spring toast_in_;
    std::vector<Spark> sparks_;
    std::uint32_t seed_ = 0x51f15e5du;
    ui::Confetti confetti_;

    bool detail_open_ = false;
    int detail_entry_ = 0;
    tween::Spring detail_;
};

} // namespace

std::unique_ptr<app::Concept> make_trophies(app::Context &context)
{
    return std::make_unique<Trophies>(context);
}

} // namespace hui::concepts
