// ps5-homebrew-ui - Design "Radial Dial": an in-game item wheel.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The "weapon wheel": the fastest way to pick one of eight things with a
// thumbstick, drawn over a running game world. What makes it feel finished:
//
//   - the left stick's angle picks the sector, with hysteresis on both the
//     angle and the deflection, so a thumb resting on a boundary never makes
//     the focus flicker; the D-pad steps round the same wheel;
//   - a needle follows the stick exactly (a spring on the angle that always
//     takes the shortest way round) while the focused sector snaps: springs
//     outward, takes its item's colour, glows; a ring shows the deflection;
//   - every detent ticks, pitched by the sector and panned by where it is;
//   - the world behind is desaturated, dimmed and slowed while the wheel is
//     up, and returns when Circle folds the wheel into a small HUD dial;
//   - R2 and L2 turn to a second wheel: the sectors rotate out and in one
//     after the other; the ends of the pager refuse softly;
//   - Cross equips: the sector flashes, a ripple leaves it, the icon flies to
//     the frosted "Equipped" card and the previous choice slides into a
//     short history. An empty item is dimmed, says so, and refuses.

#include "concepts/concepts.hpp"

#include "core/tween.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace hui::concepts
{

namespace
{

using gfx::Color;
using gfx::Rect;

constexpr float kPi = 3.14159265f;
constexpr float kTau = 6.2831853f;

// ---- the design language ---------------------------------------------------

const Color kInk = Color::rgb(0xf4f0e6);   // warm white: text and icon strokes
const Color kSlate = Color::rgb(0x0d131a); // resting sectors, hub, panels
const Color kScrim = Color::rgb(0x060a10); // what dims the world
const Color kAmber = Color::rgb(0xffb44a); // the design's own accent
const Color kAlert = Color::rgb(0xff6b5e); // "empty"
const Color kClear = Color::rgb(0x000000, 0.0f);

// The wheel.
constexpr int kSectors = 8;
constexpr float kStep = kTau / static_cast<float>(kSectors);
constexpr float kCx = 760.0f; // slightly left of centre: the card sits right
constexpr float kCy = 500.0f;
constexpr float kOuter = 330.0f; // outer edge of a resting sector
constexpr float kThick = 106.0f;
constexpr float kPush = 18.0f; // how far the focused sector springs out
constexpr float kGap = 0.034f; // radians between two sectors
constexpr float kHub = 180.0f;
constexpr float kDeflection = 204.0f; // the stick-magnitude ring, in the channel
constexpr float kLabel = 34.0f;       // label distance beyond a sector's edge
constexpr float kIcon = 58.0f;

// Stick selection. The deflection has two thresholds and a sector keeps the
// focus until the direction is clearly inside another one.
constexpr float kStickEngage = 0.35f;
constexpr float kStickRelease = 0.22f;
constexpr float kHysteresis = 0.10f; // radians past the boundary, about 6 degrees

// Turning to the other wheel.
constexpr int kPages = 2;
constexpr float kTurnStagger = 0.03f;
constexpr float kTurnSector = 0.36f;
constexpr float kTurnAngle = 0.55f;

// The folded wheel: a HUD dial in the bottom-left corner.
constexpr float kMiniScale = 0.24f;
constexpr float kMiniX = 176.0f;
constexpr float kMiniY = 936.0f;
constexpr float kSlowMotion = 0.15f; // world speed while the wheel is up

// The "Equipped" card.
constexpr float kPanelX = 1400.0f;
constexpr float kPanelY = 140.0f;
constexpr float kPanelW = 424.0f;
constexpr float kPanelH = 736.0f;
constexpr float kPanelRadius = 36.0f;
constexpr float kPanelPad = 32.0f;
constexpr float kTileY = 156.0f; // centre of the big icon, from the panel top
constexpr float kTile = 74.0f;
constexpr float kRowsY = 476.0f;
constexpr float kRowPitch = 60.0f;
constexpr int kHistory = 4;

constexpr float kPagerY = 948.0f;

constexpr const char *kTechniques[] = {
    "Stick-angle selection with hysteresis on angle and deflection: no flicker at a boundary",
    "Needle and focus rim on angle springs that always take the shortest way round",
    "Sectors from DrawList::arc; icons built from circles, lines, arcs, stars and triangles",
    "Detent ticks pitched by sector and panned by its position; ripple and rumble on equip",
    "World dimmed, desaturated and slowed under the wheel; frosted card through Frame::glass",
    "Wheel folds into a HUD dial with one transform; sectors rotate out and in when paging",
};

// The stick is held while a step waits, so the pictures show analog aiming:
// in "stick" it rests 12 degrees off the sector's centre and the needle shows
// it; in "empty" it points at the tonic and Cross has just been refused.
constexpr app::TourStep kTour[] = {
    {0.35f, 0, Direction::none, nullptr, 0.42f, -0.72f},
    {0.9f, action_bit(Action::confirm), Direction::none, "stick", 0.83f, 0.18f},
    {0.25f, 0, Direction::none, "equip", 0.83f, 0.18f},
    {0.9f, action_bit(Action::jump_next)},
    {0.8f, 0, Direction::right},
    {0.25f, 0, Direction::right},
    {0.9f, action_bit(Action::confirm), Direction::none, "signals"},
    {0.9f, action_bit(Action::jump_prev)},
    {0.9f, action_bit(Action::confirm), Direction::none, nullptr, -0.6f, 0.6f},
    {0.12f, 0, Direction::none, "empty", -0.6f, 0.6f},
    {0.7f, action_bit(Action::back)},
    {0.9f, action_bit(Action::confirm), Direction::none, "closed"},
    {0.6f, 0, Direction::none},
};

// ---- content ---------------------------------------------------------------

enum class Icon : std::uint8_t
{
    lantern,
    rope,
    map,
    compass,
    flare,
    tonic,
    hook,
    camera,
    beacon,
    follow,
    wait,
    danger,
    gather,
    camp,
    thanks,
    cheer,
};

struct Item
{
    const char *name;
    const char *blurb;
    int count; // -1: not counted; 0: none left, cannot be equipped
    std::uint32_t tint;
    Icon icon;
};

constexpr const char *kPageNames[kPages] = {"Tools", "Signals"};
constexpr const char *kPageLines[kPages] = {"What you carry", "What you say"};

// Sector 0 is at 12 o'clock; the others follow clockwise.
constexpr Item kItems[kPages][kSectors] = {
    {
        {"Lantern", "Lights the path for a while", -1, 0xffc061, Icon::lantern},
        {"Rope", "Climb down any marked ledge", 3, 0xdcb48a, Icon::rope},
        {"Map", "Shows the charted valley", -1, 0x8fd694, Icon::map},
        {"Compass", "Points to your next camp", -1, 0x7cc4ff, Icon::compass},
        {"Flare", "Calls friends to your spot", 5, 0xff7a59, Icon::flare},
        {"Tonic", "Restores a little stamina", 0, 0xe58bd0, Icon::tonic},
        {"Hook", "Swing across narrow gaps", 1, 0x9fe3e0, Icon::hook},
        {"Camera", "A picture for the journal", 12, 0xb9a2ff, Icon::camera},
    },
    {
        {"Over here", "Marks where you stand", -1, 0x7cc4ff, Icon::beacon},
        {"Follow", "Asks the party to come", -1, 0x8fd694, Icon::follow},
        {"Wait", "Holds everyone in place", -1, 0xffc061, Icon::wait},
        {"Danger", "Warns of trouble ahead", -1, 0xff7a59, Icon::danger},
        {"Gather", "Brings the party together", -1, 0x9fe3e0, Icon::gather},
        {"Camp", "Suggests a rest by the fire", -1, 0xffa35c, Icon::camp},
        {"Thanks", "A warm nod to a friend", -1, 0xff8fa8, Icon::thanks},
        {"Cheer", "Celebrates a good moment", -1, 0xffe07a, Icon::cheer},
    },
};

const Item &item(int id)
{
    return kItems[id / kSectors][id % kSectors];
}

// ---- geometry --------------------------------------------------------------

// Folds an angle into -pi..pi: the shortest signed way from one angle to another.
float wrap_angle(float angle)
{
    angle = std::fmod(angle + kPi, kTau);
    if (angle < 0.0f)
        angle += kTau;
    return angle - kPi;
}

// Angles are clockwise from 12 o'clock, like DrawList::arc.
float polar_x(float angle, float radius)
{
    return kCx + std::sin(angle) * radius;
}
float polar_y(float angle, float radius)
{
    return kCy - std::cos(angle) * radius;
}

float sector_angle(int sector)
{
    return static_cast<float>(sector) * kStep;
}

// The sector a stick direction selects. The current sector's zone is widened
// by kHysteresis on both sides, so the direction has to be clearly inside a
// neighbour before the focus leaves.
int pick_sector(int current, float angle)
{
    if (std::fabs(wrap_angle(angle - sector_angle(current))) <= kStep * 0.5f + kHysteresis)
        return current;
    const int nearest = static_cast<int>(std::lround(wrap_angle(angle) / kStep));
    return ((nearest % kSectors) + kSectors) % kSectors;
}

// The backdrop under the wheel: toward grey, then toward the scrim.
Color subdue(Color colour, float amount)
{
    const float grey = colour.r * 0.3f + colour.g * 0.59f + colour.b * 0.11f;
    const Color flat = gfx::mix(colour, Color{grey, grey, grey, 1.0f}, 0.5f * amount);
    return gfx::mix(flat, kScrim, 0.3f * amount);
}

// ---- icons -----------------------------------------------------------------

// One icon, centred on (x, y), designed in a 56 unit box and drawn `size`
// wide. `ink` is the stroke colour, `spot` the one coloured detail. Everything
// is a kit shape, so icons stay sharp at any scale and can take any colour.
void draw_icon(gfx::DrawList &list, Icon icon, float x, float y, float size, Color ink, Color spot)
{
    const float k = size / 56.0f;
    switch (icon)
    {
    case Icon::lantern:
        list.arc(x, y - 13 * k, 11 * k, 3 * k, -kPi * 0.5f, kPi, ink);
        list.rounded_rect({x - 12 * k, y - 14 * k, 24 * k, 6 * k}, 2 * k, ink);
        list.bordered_rect({x - 10 * k, y - 9 * k, 20 * k, 24 * k}, 4 * k, kClear, 3 * k, ink);
        list.circle(x, y + 4 * k, 5 * k, spot);
        list.rounded_rect({x - 13 * k, y + 15 * k, 26 * k, 6 * k}, 2 * k, ink);
        break;
    case Icon::rope:
        // A coil seen from the side: three turns, a loose end with a knot.
        for (int turn = 0; turn < 3; ++turn)
            list.bordered_rect(
                {x - 20 * k, y - 22 * k + static_cast<float>(turn) * 11 * k, 40 * k, 15 * k},
                7.5f * k, kClear, 4 * k, ink);
        list.line(x + 12 * k, y + 14 * k, x + 20 * k, y + 22 * k, 4 * k, ink);
        list.circle(x + 20 * k, y + 22 * k, 4.5f * k, spot);
        break;
    case Icon::map:
        list.bordered_rect({x - 24 * k, y - 18 * k, 48 * k, 36 * k}, 4 * k, kClear, 3 * k, ink);
        list.line(x - 8 * k, y - 14 * k, x - 8 * k, y + 14 * k, 2 * k, ink.with_alpha(0.55f));
        list.line(x + 8 * k, y - 14 * k, x + 8 * k, y + 14 * k, 2 * k, ink.with_alpha(0.55f));
        list.circle(x - 16 * k, y + 8 * k, 2.5f * k, ink);
        list.circle(x - 3 * k, y + 5 * k, 2.5f * k, ink);
        list.circle(x + 3 * k, y - 3 * k, 2.5f * k, ink);
        list.line(x + 12 * k, y - 11 * k, x + 19 * k, y - 4 * k, 3 * k, spot);
        list.line(x + 12 * k, y - 4 * k, x + 19 * k, y - 11 * k, 3 * k, spot);
        break;
    case Icon::compass:
        list.ring(x, y, 24 * k, 3.5f * k, ink);
        list.line(x, y, x + 9 * k, y - 12 * k, 6 * k, spot);
        list.line(x, y, x - 9 * k, y + 12 * k, 6 * k, ink);
        list.circle(x, y, 2.5f * k, kSlate);
        break;
    case Icon::flare:
        list.line(x - 20 * k, y + 20 * k, x + 2 * k, y - 2 * k, 6 * k, ink);
        list.star(x + 10 * k, y - 10 * k, 15 * k, spot);
        list.circle(x + 23 * k, y + 8 * k, 2.5f * k, ink);
        list.circle(x - 9 * k, y - 21 * k, 2.5f * k, ink);
        break;
    case Icon::tonic:
        list.rounded_rect({x - 7 * k, y - 26 * k, 14 * k, 6 * k}, 2 * k, spot);
        list.rounded_rect({x - 5 * k, y - 19 * k, 10 * k, 12 * k}, 1 * k, ink);
        list.ring(x, y + 8 * k, 17 * k, 3.5f * k, ink);
        list.circle(x, y + 11 * k, 8 * k, spot);
        break;
    case Icon::hook:
        list.ring(x, y - 20 * k, 6 * k, 3 * k, ink);
        list.line(x, y - 14 * k, x, y + 17 * k, 4.5f * k, ink);
        list.arc(x, y + 2 * k, 18 * k, 4.5f * k, kPi * 0.5f, kPi, ink);
        list.triangle({x - 21.75f * k, y - 10 * k, 12 * k, 12 * k}, spot);
        list.triangle({x + 9.75f * k, y - 10 * k, 12 * k, 12 * k}, spot);
        break;
    case Icon::camera:
        list.rounded_rect({x - 10 * k, y - 21 * k, 20 * k, 10 * k}, 3 * k, ink);
        list.bordered_rect({x - 25 * k, y - 14 * k, 50 * k, 34 * k}, 6 * k, kClear, 3.5f * k, ink);
        list.ring(x, y + 3 * k, 10 * k, 3.5f * k, ink);
        list.circle(x, y + 3 * k, 3.5f * k, spot);
        list.circle(x + 17 * k, y - 6 * k, 2.5f * k, ink);
        break;
    case Icon::beacon:
        list.arc(x, y + 12 * k, 34 * k, 3.5f * k, -kPi * 0.25f, kPi * 0.5f, ink);
        list.arc(x, y + 12 * k, 22 * k, 3.5f * k, -kPi * 0.25f, kPi * 0.5f, ink);
        list.circle(x, y + 12 * k, 7 * k, spot);
        break;
    case Icon::follow:
        list.line(x - 17 * k, y - 14 * k, x - 3 * k, y, 5 * k, ink);
        list.line(x - 3 * k, y, x - 17 * k, y + 14 * k, 5 * k, ink);
        list.line(x + 3 * k, y - 14 * k, x + 17 * k, y, 5 * k, spot);
        list.line(x + 17 * k, y, x + 3 * k, y + 14 * k, 5 * k, spot);
        break;
    case Icon::wait:
        list.ring(x, y, 25 * k, 3.5f * k, ink);
        list.rounded_rect({x - 10 * k, y - 11 * k, 7 * k, 22 * k}, 2 * k, spot);
        list.rounded_rect({x + 3 * k, y - 11 * k, 7 * k, 22 * k}, 2 * k, spot);
        break;
    case Icon::danger:
        list.triangle({x - 26 * k, y - 24 * k, 52 * k, 46 * k}, ink, 4 * k);
        list.line(x, y - 7 * k, x, y + 5 * k, 4.5f * k, spot);
        list.circle(x, y + 13 * k, 2.8f * k, spot);
        break;
    case Icon::gather:
        list.ring(x, y, 26 * k, 2.5f * k, ink.with_alpha(0.5f));
        list.circle(x, y - 13 * k, 6 * k, ink);
        list.circle(x - 12 * k, y + 8 * k, 6 * k, ink);
        list.circle(x + 12 * k, y + 8 * k, 6 * k, ink);
        list.circle(x, y + 1 * k, 3.5f * k, spot);
        break;
    case Icon::camp:
        list.triangle({x - 11 * k, y - 24 * k, 22 * k, 30 * k}, spot);
        list.line(x - 19 * k, y + 19 * k, x + 19 * k, y + 9 * k, 5 * k, ink);
        list.line(x - 19 * k, y + 9 * k, x + 19 * k, y + 19 * k, 5 * k, ink);
        break;
    case Icon::thanks:
        // A heart: a square on its corner with a disc on each upper edge.
        list.rotated_rect({x - 11 * k, y - 8 * k, 22 * k, 22 * k}, 2 * k, kPi * 0.25f, spot);
        list.circle(x - 7.8f * k, y - 4.8f * k, 11 * k, spot);
        list.circle(x + 7.8f * k, y - 4.8f * k, 11 * k, spot);
        break;
    case Icon::cheer:
        list.star(x - 4 * k, y + 3 * k, 21 * k, ink);
        list.star(x + 18 * k, y - 16 * k, 9 * k, spot);
        list.star(x + 19 * k, y + 16 * k, 5 * k, spot);
        break;
    }
}

class Radial final : public app::Concept
{
  public:
    explicit Radial(app::Context &context) : context_(context)
    {
        open_amount_.snap(1.0f);
        lift_[0].snap(1.0f);
        accent_.snap(Color::rgb(item(0).tint));
        slot_tint_.snap(Color::rgb(item(equipped_).tint));
        // The slot is never empty: the player arrives mid-journey.
        history_ = {1, 8, 3, 4, 0};
        history_count_ = kHistory;
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "radial",
            "Radial Dial",
            "An in-game item wheel: aim with the stick, equip with one press",
            "src/concepts/radial.cpp",
            audio::SoundSet::paper,
            kAmber,
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        age_ = 0.0f;
        // The wheel is this design: coming back to it finds it open.
        open_ = true;
        open_amount_.snap(1.0f);
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        const bool calm = context_.settings.reduced_motion;
        age_ += dt;
        clock_ += dt;
        // The world does not stop under the wheel, it slows: the player keeps
        // a sense of where they are and the wheel reads as "in the game".
        if (!calm)
            world_time_ += dt * tween::lerp(1.0f, kSlowMotion, open_amount_.value);

        // ---- the stick ----
        const float length = std::min(
            1.0f, std::sqrt(input.stick_x * input.stick_x + input.stick_y * input.stick_y));
        const bool was_engaged = stick_engaged_;
        stick_engaged_ = !input.focus_lost && length > (was_engaged ? kStickRelease : kStickEngage);
        if (stick_engaged_)
            stick_angle_ = std::atan2(input.stick_x, -input.stick_y);

        if (open_)
            update_open(input, feedback);
        else if ((stick_engaged_ && !was_engaged) || input.nav != Direction::none ||
                 input.is_pressed(Action::confirm))
            open_wheel(feedback);

        // ---- animation state ----
        open_amount_.target = open_ ? 1.0f : 0.0f;
        open_amount_.update(dt, calm ? 40.0f : 13.0f);

        // The needle is the stick: while it is deflected the needle shows its
        // exact direction, otherwise it rests on the focused sector. The
        // target is always the equivalent angle nearest to where the needle
        // is, so it never goes the long way round.
        const float focus_angle = sector_angle(focus_);
        chase(needle_, stick_engaged_ ? stick_angle_ : focus_angle, dt,
              stick_engaged_ ? 30.0f : 18.0f);
        chase(rim_, focus_angle, dt, 20.0f);
        deflection_.target = length;
        deflection_.update(dt, 28.0f);
        for (int i = 0; i < kSectors; ++i)
        {
            lift_[i].target = i == focus_ ? 1.0f : 0.0f;
            lift_[i].update(dt, 22.0f, calm ? 1.0f : 0.6f);
        }
        accent_.target(Color::rgb(item(focused_id()).tint));
        accent_.update(dt, 12.0f);

        if (focused_id() != hub_shown_)
        {
            hub_previous_ = hub_shown_;
            hub_shown_ = focused_id();
            hub_.start(calm ? 0.12f : 0.3f);
        }
        hub_.update(dt);
        turn_.update(dt);
        ripple_.update(dt);
        equip_.update(dt);
        flash_.update(dt, 7.0f);
        refuse_.update(dt, 9.0f);
        page_nudge_.update(dt, 9.0f);
        card_flash_.update(dt, 4.0f);
        slot_tint_.target(Color::rgb(item(equipped_).tint));
        slot_tint_.update(dt, 9.0f);
        page_position_.target = static_cast<float>(page_);
        page_position_.update(dt, 16.0f);
        history_slide_.update(dt, calm ? 40.0f : 13.0f);
    }

    void draw(app::Frame &frame) const override
    {
        const bool calm = context_.settings.reduced_motion;
        const float open = tween::clamp01(open_amount_.value);

        // A dusk valley stands in for the game. Under the wheel its colours
        // lose saturation and light, so the wheel owns the screen without
        // hiding where the player is.
        frame.backdrop.mode = gfx::BackdropMode::vista;
        frame.backdrop.colors[0] = subdue(Color::rgb(0x2f5f8f), open);
        frame.backdrop.colors[1] = subdue(Color::rgb(0xf0b27a), open);
        frame.backdrop.colors[2] = subdue(Color::rgb(0x586c8c), open);
        frame.backdrop.colors[3] = subdue(Color::rgb(0x18283a), open);
        frame.backdrop.params[0] = 1.0f;
        frame.backdrop.time = world_time_;
        if (open > 0.01f)
        {
            frame.post.mode = gfx::BackdropMode::vignette;
            frame.post.colors[0] = kScrim;
            frame.post.params[0] = 0.5f * open;
        }

        gfx::DrawList &list = frame.scene;
        list.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                          kScrim.with_alpha(0.46f * open));
        draw_header(list, open);
        if (calm)
        {
            // No flight between the corner and the centre: two wheels cross-fade.
            draw_wheel(list, 1.0f, open);
            draw_wheel(list, 0.0f, 1.0f - open);
        }
        else
        {
            draw_wheel(list, open, 1.0f);
        }
        draw_hud(list, 1.0f - open);
        draw_pager(list, open);
        draw_hints(list, open);

        if (open > 0.01f)
        {
            frame.glass = true;
            draw_panel(frame.overlay, frame.glass_texture, open);
        }
    }

    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    int focused_id() const
    {
        return page_ * kSectors + focus_;
    }

    // Moves an angle spring toward `angle` by the shortest way round, and
    // keeps its value near zero so it cannot lose precision over a session.
    static void chase(tween::Spring &spring, float angle, float dt, float omega)
    {
        spring.target = spring.value + wrap_angle(angle - spring.value);
        spring.update(dt, omega);
        if (std::fabs(spring.value) > kPi)
        {
            const float turn = spring.value > 0.0f ? -kTau : kTau;
            spring.value += turn;
            spring.target += turn;
        }
    }

    // Where a sector's icon sits at rest: used for panning and the fly-out.
    static float icon_x(int sector)
    {
        return polar_x(sector_angle(sector), kOuter - kThick * 0.5f);
    }
    static float icon_y(int sector)
    {
        return polar_y(sector_angle(sector), kOuter - kThick * 0.5f);
    }

    void set_focus(int sector, app::Feedback &feedback)
    {
        focus_ = sector;
        // One detent of the dial: each sector has its own note and its own
        // place between the speakers.
        feedback.play(audio::Cue::tick, 0.9f + 0.04f * static_cast<float>(sector),
                      ui::pan_for_x(icon_x(sector)));
    }

    void refuse(app::Feedback &feedback)
    {
        feedback.play(audio::Cue::error, 1.0f, ui::pan_for_x(icon_x(focus_)), 0.6f);
        feedback.rumble(0.25f, 0.05f);
        refuse_.trigger();
    }

    void open_wheel(app::Feedback &feedback)
    {
        open_ = true;
        // The input that opens the wheel does nothing else, except that a
        // stick already points somewhere: the wheel opens on that sector.
        if (stick_engaged_)
            focus_ = pick_sector(focus_, stick_angle_);
        feedback.play(audio::Cue::modal_open);
    }

    void update_open(const InputFrame &input, app::Feedback &feedback)
    {
        if (stick_engaged_)
        {
            // The stick owns the focus while it is deflected. The shell also
            // turns a pushed stick into `nav` presses; they are ignored here,
            // or one push would both aim and step.
            const int sector = pick_sector(focus_, stick_angle_);
            if (sector != focus_)
                set_focus(sector, feedback);
        }
        else if (input.nav != Direction::none)
        {
            step(input, feedback);
        }

        if (input.is_pressed(Action::jump_next))
            turn_page(1, feedback);
        else if (input.is_pressed(Action::jump_prev))
            turn_page(-1, feedback);

        if (input.is_pressed(Action::confirm))
            equip(feedback);
        if (input.is_pressed(Action::back))
        {
            open_ = false;
            feedback.play(audio::Cue::back);
        }
    }

    // D-pad: right and left turn the dial one detent and wrap, because a
    // wheel has no end. Up and down head for the top and the bottom sector by
    // the shorter side and refuse softly once there.
    void step(const InputFrame &input, app::Feedback &feedback)
    {
        int direction = 0;
        switch (input.nav)
        {
        case Direction::right:
            direction = 1;
            break;
        case Direction::left:
            direction = -1;
            break;
        case Direction::up:
        case Direction::down:
        {
            const int goal = input.nav == Direction::up ? 0 : kSectors / 2;
            const int clockwise = ((goal - focus_) % kSectors + kSectors) % kSectors;
            if (clockwise != 0)
                direction = clockwise <= kSectors / 2 ? 1 : -1;
            break;
        }
        case Direction::none:
            return;
        }
        if (direction != 0)
            set_focus((focus_ + direction + kSectors) % kSectors, feedback);
        else if (!input.nav_repeat)
            refuse(feedback);
    }

    void turn_page(int direction, app::Feedback &feedback)
    {
        const int next = page_ + direction;
        if (next < 0 || next >= kPages)
        {
            feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
            feedback.rumble(0.25f, 0.05f);
            page_nudge_.trigger();
            page_nudge_direction_ = static_cast<float>(direction);
            return;
        }
        turn_from_ = page_;
        turn_direction_ = static_cast<float>(direction);
        page_ = next;
        turn_.start(context_.settings.reduced_motion
                        ? 0.2f
                        : kTurnStagger * static_cast<float>(kSectors - 1) + kTurnSector);
        feedback.play(audio::Cue::tab, direction > 0 ? 1.06f : 0.94f);
    }

    void equip(app::Feedback &feedback)
    {
        const int id = focused_id();
        if (item(id).count == 0)
        {
            refuse(feedback); // nothing left: the hub already says so
            return;
        }
        flash_.trigger();
        ripple_.start(0.75f);
        ripple_sector_ = focus_;
        ripple_tint_ = item(id).tint;
        card_flash_.trigger();
        feedback.play(audio::Cue::select, 1.0f, ui::pan_for_x(icon_x(focus_)));
        feedback.rumble(0.3f, 0.06f);
        if (id == equipped_)
            return;
        // The previous choice becomes the newest line of the history.
        for (int i = kHistory; i > 0; --i)
            history_[static_cast<std::size_t>(i)] = history_[static_cast<std::size_t>(i - 1)];
        history_[0] = equipped_;
        history_count_ = std::min(history_count_ + 1, kHistory + 1);
        history_slide_.snap(-1.0f);
        history_slide_.target = 0.0f;
        equipped_previous_ = equipped_;
        equipped_ = id;
        equip_from_ = focus_;
        equip_.start(context_.settings.reduced_motion ? 0.15f : 0.5f);
    }

    // ---- drawing -----------------------------------------------------------

    void draw_header(gfx::DrawList &list, float open) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool calm = context_.settings.reduced_motion;
        const float in = tween::stagger(age_, 0, 0.05f, 0.5f) * open;
        if (in <= 0.01f)
            return;
        list.push_opacity(in);
        const float rise = calm ? 0.0f : 14.0f * (1.0f - in);
        ui::text(list, fonts.semibold, "QUICK DIAL", 96, 116 - rise, 18, accent_.value(),
                 gfx::Align::left, 4.0f);
        const auto title = [&](int page, float alpha, float slide)
        {
            if (alpha <= 0.01f)
                return;
            list.push_opacity(alpha);
            ui::text(list, fonts.display, kPageNames[page], 92 + slide, 184 - rise, 64, kInk);
            ui::text(list, fonts.regular, kPageLines[page], 96 + slide, 226 - rise, 24,
                     kInk.with_alpha(0.7f));
            list.pop_opacity();
        };
        if (turn_.running)
        {
            // The old name leaves the way the sectors turn; the new one follows.
            const float t = turn_.progress();
            const float way = calm ? 0.0f : turn_direction_;
            const float arrive = tween::clamp01((t - 0.3f) / 0.7f);
            title(turn_from_, 1.0f - tween::smoothstep(t * 2.5f),
                  way * 30.0f * tween::cubic_in(tween::clamp01(t * 2.5f)));
            title(page_, tween::smoothstep(arrive),
                  -way * 36.0f * (1.0f - tween::quint_out(arrive)));
        }
        else
        {
            title(page_, 1.0f, 0.0f);
        }
        list.pop_opacity();
    }

    // The whole wheel. `morph` runs from the folded HUD dial (0) to the open
    // wheel (1): one transform moves and scales everything, and the detail a
    // small dial cannot carry (icons, labels, hub text) fades with it.
    void draw_wheel(gfx::DrawList &list, float morph, float opacity) const
    {
        if (opacity <= 0.01f)
            return;
        const float scale = tween::lerp(kMiniScale, 1.0f, morph);
        list.push_transform(scale, kCx, kCy, (kMiniX - kCx) * (1.0f - morph),
                            (kMiniY - kCy) * (1.0f - morph));
        list.push_opacity(opacity);
        const float detail = tween::smoothstep((morph - 0.3f) / 0.7f);

        // A soft dark disc under everything lifts the wheel off the world.
        list.shadow({kCx - kOuter, kCy - kOuter + 14, kOuter * 2, kOuter * 2}, kOuter, 90,
                    Color::rgb(0x000000, 0.42f));
        for (int i = 0; i < kSectors; ++i)
        {
            if (i != focus_)
                draw_sector(list, i, detail);
        }
        draw_sector(list, focus_, detail); // last: its glow lies over its neighbours
        draw_rim(list, detail);
        draw_ripple(list);
        draw_hub(list, detail);
        list.pop_opacity();
        list.pop_transform();
    }

    // Which wheel a sector shows while the pages turn, how far it has rotated
    // and how visible it is. Each sector runs the same out-and-in a little
    // after the one before it, so the turn travels round the wheel.
    struct Pose
    {
        int page;
        float rotation;
        float alpha;
    };
    Pose pose(int sector) const
    {
        if (!turn_.running)
            return {page_, 0.0f, 1.0f};
        const bool calm = context_.settings.reduced_motion;
        const float local =
            calm ? turn_.progress()
                 : tween::clamp01((turn_.elapsed - static_cast<float>(sector) * kTurnStagger) /
                                  kTurnSector);
        const float swing = calm ? 0.0f : turn_direction_ * kTurnAngle;
        if (local < 0.5f)
        {
            const float out = tween::cubic_in(local * 2.0f);
            return {turn_from_, swing * out, 1.0f - out};
        }
        const float in = tween::cubic_out(local * 2.0f - 1.0f);
        return {page_, -swing * (1.0f - in), in};
    }

    void draw_sector(gfx::DrawList &list, int sector, float detail) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool calm = context_.settings.reduced_motion;
        const Pose p = pose(sector);
        const float in = calm ? tween::cubic_out(age_ / 0.25f)
                              : tween::stagger(age_ - 0.12f, sector, 0.045f, 0.45f);
        if (p.alpha * in <= 0.01f)
            return;
        const Item &it = kItems[p.page][sector];
        const Color tint = Color::rgb(it.tint);
        const bool focused = sector == focus_;
        const bool empty = it.count == 0;

        const float lift = lift_[sector].value * detail;
        const float lit = tween::clamp01(lift);
        float angle = sector_angle(sector) + p.rotation;
        if (focused)
            angle += ui::shake(refuse_.value, clock_, 0.05f, 9.0f); // a refusal rattles it
        const float outer = kOuter + kPush * lift - (calm ? 0.0f : 44.0f * (1.0f - in));
        const float start = angle - kStep * 0.5f + kGap * 0.5f;
        const float sweep = kStep - kGap;

        const float middle = outer - kThick * 0.5f;
        list.push_opacity(p.alpha * in);
        if (lit > 0.02f)
        {
            // Light spilling from the focused sector, in its own colour: a
            // soft pool under it, then wider, fainter copies of its shape.
            const float breath = calm ? 0.85f : 0.75f + 0.25f * ui::breathe(clock_);
            list.glow({polar_x(angle, middle) - 56, polar_y(angle, middle) - 56, 112, 112}, 56, 130,
                      tint.with_alpha(0.3f * lit * breath));
            constexpr float kSpread[] = {6.0f, 14.0f, 24.0f};
            constexpr float kStrength[] = {0.2f, 0.11f, 0.05f};
            for (int g = 0; g < 3; ++g)
            {
                const float wider = kSpread[g] / middle;
                list.arc(kCx, kCy, outer + kSpread[g], kThick + 2 * kSpread[g], start - wider,
                         sweep + 2 * wider, tint.with_alpha(kStrength[g] * lit * breath), false);
            }
        }
        // Folded into the HUD the dial is too small for icons, so it shows
        // one thing only: which sector holds what is equipped.
        const bool held = p.page * kSectors + sector == equipped_;
        const Color folded = held ? tint : Color::rgb(0x3a4654, 0.92f);
        Color fill = gfx::mix(gfx::mix(folded, kSlate.with_alpha(0.74f), detail),
                              tint.with_alpha(empty ? 0.6f : 0.97f), lit);
        if (focused)
            fill = gfx::mix(fill, kInk, 0.8f * flash_.value);
        list.arc(kCx, kCy, outer, kThick, start, sweep, fill, false);
        // A hairline of light on the outer edge gives the resting ring a form.
        list.arc(kCx, kCy, outer, 2.5f, start, sweep, kInk.with_alpha(0.14f + 0.3f * lit), false);

        if (detail > 0.01f)
        {
            // What is equipped keeps a pip on the inner edge of its sector.
            if (held && lit < 0.99f)
                list.arc(kCx, kCy, outer - kThick + 7, 4, angle - 0.16f, 0.32f,
                         tint.with_alpha(detail * (1.0f - lit)));
            // On the lit sector the icon turns dark and its coloured detail
            // white, so it keeps its contrast against the item's colour.
            const float dim = empty ? 0.38f : 1.0f;
            const Color ink = gfx::mix(kInk, kSlate, lit).with_alpha(dim);
            const Color spot = gfx::mix(tint, kInk, lit).with_alpha(dim);
            list.push_opacity(detail);
            draw_icon(list, it.icon, polar_x(angle, middle), polar_y(angle, middle),
                      kIcon * (1.0f + 0.12f * lift), ink, spot);

            // The label continues the sector's direction and is anchored on
            // the side facing the wheel, so no label ever grows into it.
            const float side = std::sin(angle);
            const float up = std::cos(angle);
            const gfx::Align align = side > 0.3f    ? gfx::Align::left
                                     : side < -0.3f ? gfx::Align::right
                                                    : gfx::Align::center;
            constexpr float kSize = 24.0f;
            ui::text(list, fonts.semibold, it.name, polar_x(angle, outer + kLabel),
                     polar_y(angle, outer + kLabel) + kSize * 0.35f * (1.0f - up), kSize,
                     kInk.with_alpha((0.7f + 0.3f * lit) * (empty ? 0.6f : 1.0f)), align);
            list.pop_opacity();
        }
        list.pop_opacity();
    }

    // The focus highlight is its own object: a bright bracket that glides
    // round the rim from sector to sector, ahead of the sectors' own springs.
    void draw_rim(gfx::DrawList &list, float detail) const
    {
        const float in = tween::stagger(age_, 11, 0.045f, 0.4f) * detail;
        if (in <= 0.01f)
            return;
        const float sweep = (kStep - kGap) * 0.82f;
        list.arc(kCx, kCy, kOuter + kPush + 15, 4, rim_.value - sweep * 0.5f, sweep,
                 gfx::mix(accent_.value(), kInk, 0.35f).with_alpha(in));
    }

    void draw_ripple(gfx::DrawList &list) const
    {
        if (!ripple_.running || context_.settings.reduced_motion)
            return;
        const Color tint = gfx::mix(Color::rgb(ripple_tint_), kInk, 0.3f);
        const float centre = sector_angle(ripple_sector_);
        // Two rings, the second a beat behind: a wave rather than a flash.
        for (int ring = 0; ring < 2; ++ring)
        {
            const float t = tween::clamp01((ripple_.progress() - 0.18f * static_cast<float>(ring)) /
                                           (1.0f - 0.18f * static_cast<float>(ring)));
            if (t <= 0.0f)
                continue;
            const float radius = kOuter + kPush + 20 + 110 * tween::cubic_out(t);
            const float sweep = (kStep - kGap) * (1.0f + 0.25f * t);
            list.arc(kCx, kCy, radius, tween::lerp(10.0f, 2.0f, t), centre - sweep * 0.5f, sweep,
                     tint.with_alpha((1.0f - t) * (ring == 0 ? 0.85f : 0.5f)));
        }
    }

    void draw_hub(gfx::DrawList &list, float detail) const
    {
        const bool calm = context_.settings.reduced_motion;
        const float in = tween::stagger(age_, 1, 0.05f, 0.5f);
        const Color accent = accent_.value();
        list.push_opacity(in);
        list.shadow({kCx - kHub, kCy - kHub + 10, kHub * 2, kHub * 2}, kHub, 36,
                    Color::rgb(0x000000, 0.5f));
        list.circle(kCx, kCy, kHub, kSlate.with_alpha(0.9f));
        list.ring(kCx, kCy, kHub, 2, kInk.with_alpha(0.14f));

        list.push_opacity(detail);
        // How far the stick is pushed: an arc that grows round the needle and
        // closes into a full ring at full deflection.
        list.ring(kCx, kCy, kDeflection, 4, kInk.with_alpha(0.1f));
        const float amount = tween::clamp01(deflection_.value);
        if (amount > 0.01f)
            list.arc(kCx, kCy, kDeflection, 4, needle_.value - kPi * amount, kTau * amount,
                     accent.with_alpha(0.9f));

        // The needle: an arrowhead in the channel between hub and sectors.
        const float angle = needle_.value;
        const float tip_x = polar_x(angle, kHub + 40);
        const float tip_y = polar_y(angle, kHub + 40);
        const float points[] = {tip_x,
                                tip_y,
                                polar_x(angle + 0.07f, kHub + 12),
                                polar_y(angle + 0.07f, kHub + 12),
                                polar_x(angle - 0.07f, kHub + 12),
                                polar_y(angle - 0.07f, kHub + 12)};
        list.glow({tip_x - 10, tip_y - 10, 20, 20}, 10, 22, accent.with_alpha(0.55f));
        list.polygon(points, 3, kInk);
        // Polygon edges are not anti-aliased; thin round lines over them are.
        for (int i = 0; i < 3; ++i)
        {
            const int j = (i + 1) % 3;
            list.line(points[2 * i], points[2 * i + 1], points[2 * j], points[2 * j + 1], 3, kInk);
        }

        if (hub_.running)
        {
            // The old item leaves quickly; the new one arrives a beat later.
            const float t = hub_.progress();
            const float arrive = tween::clamp01((t - 0.2f) / 0.8f);
            draw_hub_item(list, hub_previous_, 1.0f - tween::smoothstep(t * 2.4f),
                          calm ? 0.0f : -10.0f * tween::cubic_in(tween::clamp01(t * 2.4f)));
            draw_hub_item(list, hub_shown_, tween::smoothstep(arrive),
                          calm ? 0.0f : 14.0f * (1.0f - tween::quint_out(arrive)));
        }
        else
        {
            draw_hub_item(list, hub_shown_, 1.0f, 0.0f);
        }
        list.pop_opacity();
        list.pop_opacity();
    }

    void draw_hub_item(gfx::DrawList &list, int id, float alpha, float drop) const
    {
        if (alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const Item &it = item(id);
        const float x = kCx + (id == focused_id() ? ui::shake(refuse_.value, clock_, 8.0f) : 0.0f);
        const float y = kCy + drop;
        list.push_opacity(alpha);
        ui::text(list, fonts.semibold, ui::upper(kPageNames[id / kSectors]), x, y - 72, 16,
                 Color::rgb(it.tint), gfx::Align::center, 4.0f);
        ui::text(list, fonts.display, fonts.display.font->fit(it.name, 46, 300), x, y - 16, 46,
                 kInk, gfx::Align::center);
        ui::text(list, fonts.regular, fonts.regular.font->fit(it.blurb, 24, 336), x, y + 26, 24,
                 kInk.with_alpha(0.78f), gfx::Align::center);
        draw_quantity(list, id, x, y + 54, id == focused_id() ? refuse_.value : 0.0f);
        list.pop_opacity();
    }

    // The quantity pill under an item's description: a count for things that
    // run out, a word for the rest. An empty item says so in the alert colour.
    void draw_quantity(gfx::DrawList &list, int id, float cx, float top, float alarm) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const Item &it = item(id);
        constexpr float kHeight = 38.0f;
        char text[24];
        const bool counted = it.count > 0;
        if (counted)
            std::snprintf(text, sizeof(text), "\xC3\x97 %d", it.count);
        else if (it.count == 0)
            std::snprintf(text, sizeof(text), "NONE LEFT");
        else
            std::snprintf(text, sizeof(text), "%s", id < kSectors ? "READY" : "SIGNAL");
        const float size = counted ? 24.0f : 16.0f;
        const float tracking = counted ? 0.0f : 3.0f;
        const float width = fonts.semibold.measure(text, size, tracking) + 36;
        const Rect pill{cx - width * 0.5f, top, width, kHeight};
        const bool empty = it.count == 0;
        const Color base = empty ? kAlert : kInk;
        list.bordered_rect(pill, kHeight * 0.5f,
                           base.with_alpha(empty ? 0.16f + 0.3f * alarm : 0.09f), 1.5f,
                           base.with_alpha(empty ? 0.55f + 0.45f * alarm : 0.16f));
        // Tracking also follows the last glyph: shift half of it back to centre.
        ui::text(list, fonts.semibold, text, cx + tracking * 0.5f,
                 top + kHeight * 0.5f + size * 0.36f, size,
                 empty ? gfx::mix(kAlert, kInk, 0.35f) : kInk.with_alpha(counted ? 1.0f : 0.8f),
                 gfx::Align::center, tracking);
    }

    // The folded state: the wheel has flown into the corner as a small dial;
    // what is equipped shows in its middle, and its name beside it.
    void draw_hud(gfx::DrawList &list, float closed) const
    {
        if (closed <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const Item &it = item(equipped_);
        const float inner = (kOuter - kThick) * kMiniScale;
        list.push_opacity(tween::smoothstep((closed - 0.4f) / 0.6f));
        list.circle(kMiniX, kMiniY, inner - 5, kSlate.with_alpha(0.9f));
        draw_icon(list, it.icon, kMiniX, kMiniY, 52, kInk, Color::rgb(it.tint));
        // The game draws over any scenery: a soft shade keeps the text readable.
        const float x = kMiniX + kOuter * kMiniScale + 24;
        const float width = std::max(fonts.semibold.measure(it.name, 30), 150.0f);
        list.shadow({x, kMiniY - 26, width, 52}, 26, 48, kScrim.with_alpha(0.38f));
        ui::text(list, fonts.semibold, "EQUIPPED", x, kMiniY - 12, 16, kInk.with_alpha(0.7f),
                 gfx::Align::left, 3.0f);
        ui::text(list, fonts.semibold, it.name, x, kMiniY + 24, 30, kInk);
        list.pop_opacity();
    }

    // Two wheels, two dots: the active one is a pill that slides between
    // them, with the triggers that turn the page on either side.
    void draw_pager(gfx::DrawList &list, float open) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = tween::stagger(age_, 12, 0.045f, 0.45f) * open;
        if (in <= 0.01f)
            return;
        const ui::GlyphStyle style = ui::GlyphStyle::dark();
        const float position = page_position_.value;
        const float x =
            kCx + ui::shake(page_nudge_.value, clock_, 12.0f, 8.0f) * page_nudge_direction_;
        constexpr float kDot = 15.0f; // half the distance between the two dots
        constexpr float kName = 50.0f;
        constexpr float kGlyph = 36.0f;
        list.push_opacity(in);
        list.circle(x - kDot, kPagerY, 5, kInk.with_alpha(0.3f));
        list.circle(x + kDot, kPagerY, 5, kInk.with_alpha(0.3f));
        list.rounded_rect({x - kDot - 12 + 2 * kDot * position, kPagerY - 5, 24, 10}, 5,
                          accent_.value());

        const float left = fonts.semibold.measure(kPageNames[0], 24);
        const float right = fonts.semibold.measure(kPageNames[1], 24);
        ui::text(list, fonts.semibold, kPageNames[0], x - kName, kPagerY + 8, 24,
                 kInk.with_alpha(tween::lerp(1.0f, 0.5f, position)), gfx::Align::right);
        ui::text(list, fonts.semibold, kPageNames[1], x + kName, kPagerY + 8, 24,
                 kInk.with_alpha(tween::lerp(0.5f, 1.0f, position)));
        // A trigger that has nowhere to go is dimmed: disabled is visible.
        list.push_opacity(tween::lerp(0.35f, 1.0f, position));
        ui::draw_button(list, fonts, style, ui::Button::l2,
                        x - kName - left - 16 - ui::button_width(ui::Button::l2, kGlyph), kPagerY,
                        kGlyph);
        list.pop_opacity();
        list.push_opacity(tween::lerp(1.0f, 0.35f, position));
        ui::draw_button(list, fonts, style, ui::Button::r2, x + kName + right + 16, kPagerY,
                        kGlyph);
        list.pop_opacity();
        list.pop_opacity();
    }

    void draw_hints(gfx::DrawList &list, float open) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const ui::GlyphStyle style = ui::GlyphStyle::dark();
        const float in = tween::stagger(age_, 14, 0.045f, 0.5f);
        // The two rows share a place, so one leaves before the other arrives.
        list.push_opacity(in * tween::clamp01(open * 2.0f - 1.0f));
        const ui::Hint opened[] = {{ui::Button::left_stick, "Aim", ui::Button::dpad},
                                   {ui::Button::cross, "Equip"},
                                   {ui::Button::circle, "Close"}};
        ui::draw_hints(list, fonts, style, opened, 3, 1824, true);
        list.pop_opacity();
        list.push_opacity(in * tween::clamp01(1.0f - open * 2.0f));
        const ui::Hint closed[] = {{ui::Button::left_stick, "Open the dial", ui::Button::cross}};
        ui::draw_hints(list, fonts, style, closed, 1, 1824, true);
        list.pop_opacity();
    }

    // The frosted card: what is equipped, and what was before it. It lives in
    // the overlay so the glass can blur the world (and the scrim) behind it.
    void draw_panel(gfx::DrawList &list, std::uint32_t glass, float open) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool calm = context_.settings.reduced_motion;
        const float in = tween::stagger(age_, 8, 0.06f, 0.5f) * open;
        if (in <= 0.01f)
            return;
        const Rect panel{kPanelX + (calm ? 0.0f : 48.0f * (1.0f - in)), kPanelY, kPanelW, kPanelH};
        const Color tint = Color::rgb(item(equipped_).tint);
        list.push_opacity(in);
        list.shadow({panel.x, panel.y + 18, panel.w, panel.h}, kPanelRadius, 50,
                    Color::rgb(0x000000, 0.45f));
        if (card_flash_.value > 0.01f)
            list.glow(panel, kPanelRadius, 34, tint.with_alpha(0.4f * card_flash_.value));
        list.glass(glass, panel, kPanelRadius, Color::rgb(0xffffff));
        list.rounded_rect(panel, kPanelRadius, kSlate.with_alpha(0.56f));
        list.bordered_rect(
            panel, kPanelRadius, kClear, 1.5f,
            gfx::mix(kInk, tint, card_flash_.value).with_alpha(0.2f + 0.5f * card_flash_.value));

        ui::text(list, fonts.semibold, "EQUIPPED", panel.x + kPanelPad, panel.y + 54, 16,
                 kInk.with_alpha(0.62f), gfx::Align::left, 4.0f);
        // The slot itself stays; its colour eases to the new item's.
        const float cx = panel.cx();
        const float cy = panel.y + kTileY;
        const Color slot = slot_tint_.value();
        list.glow({cx - kTile * 0.7f, cy - kTile * 0.7f, kTile * 1.4f, kTile * 1.4f}, kTile * 0.7f,
                  70, slot.with_alpha(0.32f));
        list.circle(cx, cy, kTile, kSlate.with_alpha(0.78f));
        list.ring(cx, cy, kTile, 2.5f, slot.with_alpha(0.8f));
        if (equip_.running)
        {
            // The words change first; the icon appears when the one flying in
            // from the wheel lands, with a small pop.
            const float t = equip_.progress();
            const float landed = tween::clamp01((t - 0.55f) / 0.45f);
            const float gone = 1.0f - tween::smoothstep(t * 2.6f);
            draw_equipped(list, panel, equipped_previous_, gone, gone, 1.0f);
            draw_equipped(list, panel, equipped_, tween::smoothstep((t - 0.15f) / 0.45f),
                          calm ? tween::smoothstep(t) : tween::clamp01(landed * 4.0f),
                          calm ? 1.0f : 0.7f + 0.3f * tween::back_out(landed));
        }
        else
        {
            draw_equipped(list, panel, equipped_, 1.0f, 1.0f, 1.0f);
        }

        list.rounded_rect({panel.x + kPanelPad, panel.y + 412, panel.w - 2 * kPanelPad, 1.5f},
                          0.75f, kInk.with_alpha(0.14f));
        ui::text(list, fonts.semibold, "BEFORE THAT", panel.x + kPanelPad, panel.y + 454, 16,
                 kInk.with_alpha(0.62f), gfx::Align::left, 4.0f);

        // A new line pushes the others down: the list is drawn one row higher
        // and slides into place; the fifth row fades as it leaves the window.
        const float top = panel.y + kRowsY;
        const float slide = history_slide_.value;
        list.push_clip({panel.x, top, panel.w, kRowPitch * static_cast<float>(kHistory)});
        for (int i = 0; i < history_count_; ++i)
        {
            float alpha = 1.0f - 0.12f * static_cast<float>(i);
            if (i == 0)
                alpha *= tween::clamp01(1.0f + slide * 1.5f);
            if (i == kHistory)
                alpha *= tween::clamp01(-slide);
            if (alpha <= 0.01f)
                continue;
            const int id = history_[static_cast<std::size_t>(i)];
            const Item &it = item(id);
            const float y = top + (static_cast<float>(i) + slide) * kRowPitch;
            const float cy = y + kRowPitch * 0.5f;
            list.push_opacity(alpha);
            list.circle(panel.x + kPanelPad + 22, cy, 22, kInk.with_alpha(0.08f));
            draw_icon(list, it.icon, panel.x + kPanelPad + 22, cy, 30, kInk, Color::rgb(it.tint));
            ui::text(list, fonts.semibold, it.name, panel.x + kPanelPad + 62, cy + 9, 24, kInk);
            ui::text(list, fonts.regular, kPageNames[id / kSectors], panel.x + panel.w - kPanelPad,
                     cy + 8, 20, kInk.with_alpha(0.55f), gfx::Align::right);
            list.pop_opacity();
        }
        list.pop_clip();

        // The equipped icon's flight from its sector to the card.
        if (equip_.running && !calm)
        {
            const float t = tween::clamp01(equip_.progress() / 0.6f);
            const float u = tween::cubic_in_out(t);
            const Item &it = item(equipped_);
            const float x = tween::lerp(icon_x(equip_from_), cx, u);
            const float y = tween::lerp(icon_y(equip_from_), cy, u) -
                            70.0f * tween::ping(u); // a shallow arc, not a straight line
            const float size = tween::lerp(kIcon, 92.0f * 0.7f, u);
            list.push_opacity(tween::clamp01(t * 6.0f) *
                              (1.0f - tween::smoothstep((t - 0.9f) / 0.1f)));
            list.glow({x - size * 0.3f, y - size * 0.3f, size * 0.6f, size * 0.6f}, size * 0.3f,
                      size * 0.6f, Color::rgb(it.tint, 0.5f));
            draw_icon(list, it.icon, x, y, size, kInk, Color::rgb(it.tint));
            list.pop_opacity();
        }
        list.pop_opacity();
    }

    // One item's content in the card: its words at one opacity, its icon in
    // the slot at another, so the two can change at different moments.
    void draw_equipped(gfx::DrawList &list, const Rect &panel, int id, float words, float icon,
                       float pop) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const Item &it = item(id);
        const float cx = panel.cx();
        const float width = panel.w - 2 * kPanelPad;
        if (icon > 0.01f)
        {
            list.push_opacity(icon);
            draw_icon(list, it.icon, cx, panel.y + kTileY, 92 * pop, kInk, Color::rgb(it.tint));
            list.pop_opacity();
        }
        if (words <= 0.01f)
            return;
        list.push_opacity(words);
        ui::text(list, fonts.display, fonts.display.font->fit(it.name, 42, width), cx,
                 panel.y + 296, 42, kInk, gfx::Align::center);
        ui::text(list, fonts.regular, fonts.regular.font->fit(it.blurb, 24, width), cx,
                 panel.y + 336, 24, kInk.with_alpha(0.78f), gfx::Align::center);
        draw_quantity(list, id, cx, panel.y + 354, 0.0f);
        list.pop_opacity();
    }

    app::Context &context_;
    float age_ = 0.0f;        // seconds since enter(): drives the entrance
    float clock_ = 0.0f;      // free-running time for idle motion
    float world_time_ = 0.0f; // the backdrop's clock: slows under the wheel

    bool open_ = true;
    tween::Spring open_amount_; // 0 folded into the HUD, 1 open
    int page_ = 0;
    int focus_ = 0;

    bool stick_engaged_ = false; // the stick is past the threshold and aims
    float stick_angle_ = 0.0f;
    tween::Spring needle_;     // angle: the stick's direction, or the focus
    tween::Spring rim_;        // angle of the gliding focus bracket
    tween::Spring deflection_; // 0..1 for the ring round the hub
    tween::Bounce lift_[kSectors];
    ui::SpringColor accent_; // follows the focused item's colour

    int hub_shown_ = 0;    // the item the hub describes
    int hub_previous_ = 0; // ... and the one it is fading out
    tween::Timer hub_;

    tween::Timer turn_; // the page turn
    int turn_from_ = 0;
    float turn_direction_ = 1.0f;
    tween::Spring page_position_;
    ui::Pulse page_nudge_;
    float page_nudge_direction_ = 0.0f;

    ui::Pulse flash_;
    ui::Pulse refuse_;
    tween::Timer ripple_;
    int ripple_sector_ = 0;
    std::uint32_t ripple_tint_ = 0xffffff;

    int equipped_ = 0;
    int equipped_previous_ = 0;
    int equip_from_ = 0; // the sector the equipped icon flies from
    tween::Timer equip_;
    ui::Pulse card_flash_;
    ui::SpringColor slot_tint_;               // the card's slot ring, eased to the equipped item
    std::array<int, kHistory + 1> history_{}; // newest first; one spare row to fade out
    int history_count_ = 0;
    tween::Spring history_slide_;
};

} // namespace

std::unique_ptr<app::Concept> make_radial(app::Context &context)
{
    return std::make_unique<Radial>(context);
}

} // namespace hui::concepts
