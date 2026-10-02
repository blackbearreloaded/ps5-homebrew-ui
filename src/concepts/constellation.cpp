// ps5-homebrew-ui - Design "Constellation": a skill tree drawn as a star map.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A progression screen that is larger than the TV: twenty-four stars in three
// branches, laid out by hand in a world of about 3000 x 1800, seen through a
// camera that glides after the focus. What makes it feel finished:
//
//   - the focus moves freely in two dimensions: one small scoring function
//     picks the best star in the pressed direction, and a compile-time check
//     proves that no star can be stranded by the layout;
//   - the camera is a soft spring on the focused star, the sky behind it moves
//     in parallax, and L2 / R2 step between three zoom levels down to a view
//     of the whole tree with the branch names written across it;
//   - unlocking is a small event: light runs along the link from the star it
//     came from, the star ignites with a ripple, the points count down, and
//     the cue climbs one step of a scale for every star lit in that branch;
//   - a frosted card explains the focused star and cross-fades as the focus
//     moves; refusals say why, in the card, for two seconds. The HUD and the
//     hint row are glass too, because the map slides under all of them;
//   - refunding asks first (the safe choice is focused), then the stars go
//     dark one after another, newest first, as the points come back.

#include "concepts/concepts.hpp"

#include "core/tween.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace hui::concepts
{

namespace
{

using gfx::Color;
using gfx::Rect;

// ---- the design language ---------------------------------------------------

const Color kWhite = Color::rgb(0xffffff);
const Color kInk = Color::rgb(0xe9eeff);   // text and resting line work
const Color kMuted = Color::rgb(0x93a0d0); // secondary text
const Color kVoid = Color::rgb(0x090d20);  // the inside of an unlit star
const Color kWarm = Color::rgb(0xff9f7a);  // refusals and the refund prompt
const Color kClear = Color::rgb(0x000000, 0.0f);

constexpr float kMargin = 96.0f;
constexpr float kTau = 6.2831853f;

// Where the camera's target lands on screen: left of centre, because the
// info card owns the right-hand quarter.
constexpr float kAnchorX = 730.0f;
constexpr float kAnchorY = 548.0f;
// The middle of the tree, which the far zoom looks at instead of the focus.
constexpr float kTreeX = 1500.0f;
constexpr float kTreeY = 895.0f;
constexpr float kZoomLevels[] = {0.48f, 0.9f, 1.3f};
constexpr int kZoomCount = 3;
constexpr float kCameraOmega = 6.0f; // soft: travel should feel like gliding
constexpr float kZoomOmega = 7.0f;
// Backdrop units per world pixel. The star layers scale this by 0.05 to 0.25,
// so the far sky moves at about a third of the speed of the map.
constexpr float kParallax = 1.4f / 1080.0f;

constexpr float kStarRadius = 30.0f;
constexpr float kKeystoneRadius = 44.0f;
constexpr float kOriginRadius = 38.0f;
constexpr float kOuterRing = 10.0f;    // keystones: distance to the second ring
constexpr float kTravel = 0.55f;       // seconds for light to run along a link
constexpr float kRipple = 0.9f;        // seconds the unlock ripple lives
constexpr float kRefundStep = 0.085f;  // seconds between stars going dark
constexpr float kReasonSeconds = 2.0f; // how long a refusal explains itself
constexpr float kPointStep = 0.07f;    // the counter moves one point at a time
constexpr int kStartPoints = 9;

const Rect kCard{1396.0f, 172.0f, 428.0f, 664.0f};
constexpr float kCardRadius = 36.0f;
constexpr float kCardPad = 36.0f;
constexpr float kRowHeight = 64.0f;
constexpr float kRowGap = 12.0f;
const Rect kHud{64.0f, 40.0f, 652.0f, 148.0f};
constexpr float kHudRadius = 30.0f;
constexpr float kHintPad = 28.0f;                    // glass around the hint row
const Rect kHintRow{900.0f, 976.0f, 952.0f, 104.0f}; // where that glass can be, to the edge

constexpr const char *kTechniques[] = {
    "Free-form focus navigation: one scoring function, proven complete by a static_assert",
    "A spring camera inside push_transform, with the star backdrop moving in parallax",
    "Three zoom levels on a spring; line widths divided by the zoom so they stay crisp",
    "Unlocks that travel: light runs along the link, then the star ignites with a ripple",
    "A cue that climbs a scale as a branch fills, panned by where the star is on screen",
    "A frosted info card that cross-fades, explains refusals and hosts the refund prompt",
};

constexpr app::TourStep kTour[] = {
    {0.6f, 0, Direction::left},
    {0.5f, action_bit(Action::confirm)},
    {0.7f, 0, Direction::up},
    {0.5f, action_bit(Action::confirm)},
    {0.7f, 0, Direction::left},
    {0.6f, action_bit(Action::confirm)},
    {0.36f, 0, Direction::none, "unlock"}, // light still travelling to the keystone
    {0.9f, 0, Direction::up},
    {0.5f, 0, Direction::up},
    {0.7f, action_bit(Action::confirm)}, // a locked star: refused, with the reason
    {0.45f, 0, Direction::none, "refusal"},
    {0.7f, action_bit(Action::jump_prev)},
    {1.4f, action_bit(Action::jump_next), Direction::none, "overview"},
    {1.0f, action_bit(Action::north)},
    {0.7f, action_bit(Action::back), Direction::none, "refund"},
    {0.5f, 0, Direction::none},
};

// ---- the tree --------------------------------------------------------------

struct Branch
{
    const char *name;
    Color colour;
    float label_x; // where the far zoom writes the name, in world space
    float label_y;
};

const Branch kBranches[] = {
    {"Origin", Color::rgb(0xffe7b0), 0.0f, 0.0f},
    {"Voyager", Color::rgb(0x5ad1ff), 560.0f, 1320.0f},
    {"Artisan", Color::rgb(0xffb152), 2440.0f, 1320.0f},
    {"Sentinel", Color::rgb(0xb596ff), 1500.0f, 1790.0f},
};
constexpr int kBranchCount = 4;

struct Node
{
    const char *name;
    const char *effect;
    float x; // world position, placed by hand
    float y;
    int branch;
    int cost;
    bool keystone;
    int needs[2]; // prerequisites: any one of them opens the star (-1: none)
};

// Listed so that a star's prerequisites always come before it. Columns:
// name, effect / x, y, branch, cost, keystone, prerequisites.
// clang-format off
constexpr Node kNodes[] = {
    {"First Light", "Where every path begins. Three roads lead out from here.",
     1500, 900, 0, 0, false, {-1, -1}},
    // Voyager: up and to the left.
    {"Drift Sails", "Move 10% faster outside of combat.",
     1210, 800, 1, 1, false, {0, -1}},
    {"Star Chart", "Unexplored regions appear on the map.",
     960, 610, 1, 1, false, {1, -1}},
    {"Slipstream", "Sprinting costs no stamina for the first 4 seconds.",
     940, 940, 1, 1, false, {1, -1}},
    {"Far Horizon", "Fast travel from anywhere to any beacon you have lit.",
     660, 770, 1, 3, true, {2, 3}},
    {"Tidal Lock", "Dodging toward a target closes the gap twice as fast.",
     760, 370, 1, 2, false, {2, -1}},
    {"Dark Passage", "Travel unseen at night until you strike.",
     430, 520, 1, 2, false, {4, 5}},
    {"Wayfinder", "Hidden paths glow faintly when you are near.",
     540, 1060, 1, 1, false, {4, -1}},
    {"Long Night", "Night lasts twice as long, and the dark makes you faster.",
     360, 310, 1, 3, true, {6, -1}},
    // Artisan: up and to the right.
    {"Steady Hands", "Crafting uses 10% fewer materials.",
     1790, 790, 2, 1, false, {0, -1}},
    {"Tempered Edge", "Blades you forge keep their edge twice as long.",
     2030, 590, 2, 1, false, {9, -1}},
    {"Fine Tools", "Every tool gains a second trinket slot.",
     2080, 920, 2, 1, false, {9, -1}},
    {"Masterwork", "Once a day, forge an item with no flaws at all.",
     2350, 740, 2, 3, true, {10, 11}},
    {"Hidden Seams", "Armour you repair gains a concealed pocket.",
     2240, 350, 2, 2, false, {10, -1}},
    {"Rare Alloys", "Ore veins sometimes yield starmetal.",
     2600, 480, 2, 2, false, {12, 13}},
    {"Salvager", "Broken gear returns half of its materials.",
     2480, 1040, 2, 1, false, {12, -1}},
    {"Living Forge", "Your forge keeps working while you are away.",
     2700, 250, 2, 3, true, {14, -1}},
    // Sentinel: straight down. Its seven stars cost exactly the nine points
    // a new tree starts with, so one branch can always be completed.
    {"Watchfire", "Camps you build warn you of danger nearby.",
     1500, 1130, 3, 1, false, {0, -1}},
    {"Bulwark", "Blocking absorbs 15% more force.",
     1230, 1270, 3, 1, false, {17, -1}},
    {"Second Wind", "Recover a little health after every guard break.",
     1770, 1270, 3, 1, false, {17, -1}},
    {"Iron Vigil", "Nothing staggers you while you hold your ground.",
     1500, 1370, 3, 2, true, {18, 19}},
    {"Stone Skin", "The first hit of every fight does less damage.",
     960, 1420, 3, 1, false, {18, -1}},
    {"Riposte", "A perfect block opens the attacker to a counter.",
     2040, 1420, 3, 1, false, {19, -1}},
    {"Last Stand", "At death's door, hold on for five more seconds.",
     1500, 1610, 3, 2, true, {20, -1}},
};
// clang-format on
constexpr int kNodeCount = static_cast<int>(sizeof(kNodes) / sizeof(kNodes[0]));

constexpr bool linked(int a, int b)
{
    for (int need : kNodes[a].needs)
    {
        if (need == b)
            return true;
    }
    for (int need : kNodes[b].needs)
    {
        if (need == a)
            return true;
    }
    return false;
}

// ---- free-form focus navigation --------------------------------------------
//
// Which star does "left" mean, when the stars sit anywhere? Split the vector
// to each candidate into the distance travelled `along` the pressed direction
// and the `side` offset across it, then:
//
//   1. drop everything behind the focus (along <= 0);
//   2. drop everything outside a cone: side / along is the tangent of the
//      angle off the axis. Strangers get 45 degrees; stars linked to the focus
//      get about 65, because following a drawn line is what the player means;
//   3. pick the lowest score = along + kSideCost * side. Counting the side
//      offset double prefers "nearly straight ahead, a little further" over
//      "closer, but well off to the side". Linked stars score at a discount,
//      so the tree's own edges win ties against a star from another branch.
//
// Returns -1 when nothing lies that way; the caller refuses softly.
constexpr float kStrangerCone = 1.0f; // tan(45 degrees)
constexpr float kLinkedCone = 2.2f;   // tan(about 65 degrees)
constexpr float kSideCost = 2.0f;
constexpr float kLinkedDiscount = 0.45f;

constexpr int neighbour(int from, Direction direction)
{
    if (direction == Direction::none)
        return -1;
    const float dx = direction == Direction::right  ? 1.0f
                     : direction == Direction::left ? -1.0f
                                                    : 0.0f;
    const float dy = direction == Direction::down ? 1.0f
                     : direction == Direction::up ? -1.0f
                                                  : 0.0f;
    int best = -1;
    float best_score = 0.0f;
    for (int i = 0; i < kNodeCount; ++i)
    {
        if (i == from)
            continue;
        const float vx = kNodes[i].x - kNodes[from].x;
        const float vy = kNodes[i].y - kNodes[from].y;
        const float along = vx * dx + vy * dy;
        const float cross = vx * dy - vy * dx;
        const float side = cross < 0.0f ? -cross : cross;
        if (along <= 0.0f)
            continue;
        const bool is_linked = linked(from, i);
        if (side > along * (is_linked ? kLinkedCone : kStrangerCone))
            continue;
        const float score = (along + kSideCost * side) * (is_linked ? kLinkedDiscount : 1.0f);
        if (best < 0 || score < best_score)
        {
            best = i;
            best_score = score;
        }
    }
    return best;
}

// A hand-placed layout can strand a star: reachable on paper, but no
// direction from any neighbour selects it. So the layout is checked where it
// is written: every star must be reachable from the origin, and the origin
// from every star, using nothing but neighbour(). Move a star badly and the
// build stops here.
constexpr bool every_star_is_reachable()
{
    constexpr Direction kDirections[] = {Direction::up, Direction::down, Direction::left,
                                         Direction::right};
    int next[kNodeCount][4] = {};
    for (int i = 0; i < kNodeCount; ++i)
    {
        for (int d = 0; d < 4; ++d)
            next[i][d] = neighbour(i, kDirections[d]);
    }
    for (int pass = 0; pass < 2; ++pass) // 0: outward from the origin, 1: back to it
    {
        bool seen[kNodeCount] = {};
        seen[0] = true;
        for (int round = 0; round < kNodeCount; ++round)
        {
            for (int a = 0; a < kNodeCount; ++a)
            {
                for (int d = 0; d < 4; ++d)
                {
                    const int b = next[a][d];
                    if (b < 0)
                        continue;
                    if (pass == 0 && seen[a])
                        seen[b] = true;
                    if (pass == 1 && seen[b])
                        seen[a] = true;
                }
            }
        }
        for (bool reached : seen)
        {
            if (!reached)
                return false;
        }
    }
    return true;
}
static_assert(every_star_is_reachable(), "the layout strands a star: adjust kNodes");

constexpr int branch_size(int branch)
{
    int count = 0;
    for (const Node &node : kNodes)
        count += node.branch == branch ? 1 : 0;
    return count;
}

constexpr float radius_of(int index)
{
    return index == 0 ? kOriginRadius : kNodes[index].keystone ? kKeystoneRadius : kStarRadius;
}

// The edge of everything a star draws: where links stop and the reticle sits.
constexpr float extent_of(int index)
{
    return radius_of(index) + (kNodes[index].keystone ? kOuterRing : 0.0f);
}

// A major scale starting low, so eight steps stay inside a pleasant range
// (0.7 to 1.4): each star lit in a branch plays the next degree.
float scale_pitch(int step)
{
    constexpr int kSemitones[] = {0, 2, 4, 5, 7, 9, 11, 12};
    const int index = std::clamp(step, 0, 7);
    return 0.7f * std::pow(2.0f, static_cast<float>(kSemitones[index]) / 12.0f);
}

Rect disc(float cx, float cy, float radius)
{
    return {cx - radius, cy - radius, radius * 2.0f, radius * 2.0f};
}

class Constellation final : public app::Concept
{
  public:
    explicit Constellation(app::Context &context) : context_(context)
    {
        for (int i = 1; i < kNodeCount; ++i)
            depth_[i] = depth_[kNodes[i].needs[0]] + 1;
        unlocked_[0] = true;
        unlock_age_[0] = 60.0f;
        lit_[0].snap(1.0f);
        camera_x_.snap(kNodes[0].x);
        camera_y_.snap(kNodes[0].y);
        zoom_.snap(kZoomLevels[zoom_level_]);
        reticle_x_.snap(kNodes[0].x);
        reticle_y_.snap(kNodes[0].y);
        reticle_r_.snap(extent_of(0));
        nebula_.snap(nebula_for(0));
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "constellation",
            "Constellation",
            "A skill tree as a star map: free 2D focus, a gliding camera, progression",
            "src/concepts/constellation.cpp",
            audio::SoundSet::paper,
            Color::rgb(0x5ad1ff),
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        age_ = 0.0f;
        prompt_open_ = false;
        reason_time_ = 0.0f;
        // Arrive from a little further out, so the map settles toward the
        // player as the stars appear.
        if (!context_.settings.reduced_motion)
            zoom_.value = zoom_.target * 0.84f;
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        const bool calm = context_.settings.reduced_motion;
        age_ += dt;
        clock_ += dt;
        if (!calm)
            sky_time_ += dt;

        if (prompt_open_)
            update_prompt(input, feedback);
        else
            update_map(input, feedback);
        update_refund(dt, feedback);

        // ---- animation state ----
        if (focus_ != shown_)
        {
            previous_ = shown_;
            shown_ = focus_;
            card_.start(calm ? 0.12f : 0.36f);
            nebula_.target(nebula_for(kNodes[focus_].branch));
        }
        card_.update(dt);
        nebula_.update(dt, 2.5f);

        // The camera chases the focus; at the far zoom it rests on the middle
        // of the tree instead, so the whole map is in view.
        const bool far = zoom_level_ == 0;
        camera_x_.target = far ? kTreeX : kNodes[focus_].x;
        camera_y_.target = far ? kTreeY : kNodes[focus_].y;
        camera_x_.update(dt, calm ? 30.0f : kCameraOmega);
        camera_y_.update(dt, calm ? 30.0f : kCameraOmega);
        zoom_.target = kZoomLevels[zoom_level_];
        zoom_.update(dt, calm ? 30.0f : kZoomOmega);

        reticle_x_.target = kNodes[focus_].x;
        reticle_y_.target = kNodes[focus_].y;
        reticle_r_.target = extent_of(focus_);
        reticle_x_.update(dt, 17.0f);
        reticle_y_.update(dt, 17.0f);
        reticle_r_.update(dt, 17.0f);

        for (int i = 0; i < kNodeCount; ++i)
        {
            if (unlocked_[i])
                unlock_age_[i] += dt;
            // A star ignites when the light running along its link arrives.
            const bool arrived = unlocked_[i] && (calm || unlock_age_[i] >= kTravel * 0.85f);
            lit_[i].target = arrived ? 1.0f : 0.0f;
            lit_[i].update(dt, 10.0f);
        }
        for (int b = 1; b < kBranchCount; ++b)
        {
            gauge_[b].target =
                static_cast<float>(count_unlocked(b)) / static_cast<float>(branch_size(b));
            gauge_[b].update(dt, 8.0f);
            flash_[b].update(dt, 1.6f);
        }

        // The counter walks to the real value one point at a time, popping on
        // each step, so spending three points reads as three.
        if (shown_points_ != points_)
        {
            point_step_ -= dt;
            if (point_step_ <= 0.0f)
            {
                shown_points_ += points_ > shown_points_ ? 1 : -1;
                points_pop_.trigger();
                point_step_ = calm ? 0.0f : kPointStep;
            }
        }
        else
        {
            point_step_ = 0.0f;
        }
        points_pop_.update(dt, 7.0f);

        nudge_.update(dt, 9.0f);
        tap_.update(dt, 6.0f);
        reason_time_ = std::max(0.0f, reason_time_ - dt);
        prompt_.target = prompt_open_ ? 1.0f : 0.0f;
        prompt_.update(dt, calm ? 40.0f : 13.0f);
        choice_position_.target = static_cast<float>(choice_);
        choice_position_.update(dt, 22.0f);
    }

    void draw(app::Frame &frame) const override
    {
        frame.backdrop.mode = gfx::BackdropMode::stars;
        frame.backdrop.colors[0] = Color::rgb(0x03050d);
        frame.backdrop.colors[1] = Color::rgb(0x0a1026);
        frame.backdrop.colors[2] = nebula_.value();
        frame.backdrop.colors[3] = Color::rgb(0x2c2f86);
        // The sky is told where the camera is: its three star layers slide at
        // different fractions of that, which is all parallax is.
        frame.backdrop.params[0] = camera_x_.value * kParallax;
        frame.backdrop.params[1] = camera_y_.value * kParallax;
        frame.backdrop.time = sky_time_;

        gfx::DrawList &list = frame.scene;
        const float zoom = zoom_value();
        // 1 at the far zoom, 0 from the middle zoom inward: names and costs
        // hand over to the branch labels as the map shrinks.
        const float overview =
            1.0f - tween::inverse_lerp(kZoomLevels[0] + 0.06f, kZoomLevels[1] - 0.1f, zoom);

        // World layer: everything inside is in world coordinates. The camera
        // is one transform: scale about the origin, then move the camera's
        // position onto the anchor.
        list.push_transform(zoom, 0, 0, kAnchorX - camera_x_.value * zoom,
                            kAnchorY - camera_y_.value * zoom);
        draw_branch_names(list, overview);
        draw_links(list, overview);
        draw_stars(list, overview);
        draw_reticle(list);
        list.pop_transform();

        // The map runs off every edge of the screen: shade the top and bottom
        // so it sinks away there, and dim all of it under the refund prompt.
        list.gradient_rect({0, 0, gfx::kVirtualWidth, 220}, 0, Color::rgb(0x03050d, 0.6f),
                           Color::rgb(0x03050d, 0.0f));
        list.gradient_rect({0, 900, gfx::kVirtualWidth, 180}, 0, Color::rgb(0x03050d, 0.0f),
                           Color::rgb(0x03050d, 0.6f));
        const float prompt = prompt_.value;
        if (prompt > 0.01f)
            list.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                              Color::rgb(0x03050d, 0.5f * prompt));

        // Screen layer. Stars pass under all three panels as the camera
        // moves, so each one is frosted glass in the overlay: the map stays
        // visible as blurred light, and never tangles with the text.
        frame.glass = true;
        draw_hud(frame.overlay, frame.glass_texture);
        draw_card(frame.overlay, frame.glass_texture);
        draw_hints(frame.overlay, frame.glass_texture);
    }

    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    // ---- rules ----

    bool available(int index) const
    {
        if (unlocked_[index])
            return false;
        for (int need : kNodes[index].needs)
        {
            if (need >= 0 && unlocked_[need])
                return true;
        }
        return false;
    }

    int count_unlocked(int branch) const
    {
        int count = 0;
        for (int i = 0; i < kNodeCount; ++i)
            count += unlocked_[i] && kNodes[i].branch == branch ? 1 : 0;
        return count;
    }

    static Color nebula_for(int branch)
    {
        // The backdrop adds this colour, so keep it well below full strength.
        return gfx::mix(Color::rgb(0x0a1030), kBranches[branch].colour, 0.42f);
    }

    float zoom_value() const
    {
        return std::max(0.2f, zoom_.value);
    }

    float screen_x(float world_x) const
    {
        return kAnchorX + (world_x - camera_x_.value) * zoom_value();
    }

    float screen_y(float world_y) const
    {
        return kAnchorY + (world_y - camera_y_.value) * zoom_value();
    }

    // 0 while a star is under the HUD or the hint row, 1 in the open. The
    // glass hides the star but its name would hang out below the panel, an
    // orphan; so the name fades as its star slides under.
    float clear_of_panels(float world_x, float world_y) const
    {
        const float sx = screen_x(world_x);
        const float sy = screen_y(world_y);
        const auto outside = [sx, sy](const Rect &r)
        {
            const float dx = std::max(r.x - sx, sx - (r.x + r.w));
            const float dy = std::max(r.y - sy, sy - (r.y + r.h));
            return tween::clamp01(std::max(dx, dy) / 30.0f);
        };
        return std::min(outside(kHud), outside(kHintRow));
    }

    float pan_of(int index) const
    {
        return ui::pan_for_x(screen_x(kNodes[index].x));
    }

    // ---- input ----

    void update_map(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav != Direction::none)
        {
            const int next = neighbour(focus_, input.nav);
            if (next >= 0)
            {
                focus_ = next;
                reason_time_ = 0.0f;
                // Panned by where the star is now, before the camera catches
                // up: the tick comes from the side the focus flew to. Stars
                // higher on the map sound a little higher.
                feedback.play(audio::Cue::focus, 1.08f - 0.14f * kNodes[next].y / 1800.0f,
                              pan_of(next));
            }
            else if (!input.nav_repeat)
            {
                feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
                feedback.rumble(0.25f, 0.05f);
                nudge(input.nav == Direction::left || input.nav == Direction::right);
            }
        }
        if (input.is_pressed(Action::confirm))
        {
            finish_refund(); // a press never waits for the cascade
            try_unlock(feedback);
        }
        if (input.is_pressed(Action::north))
        {
            finish_refund();
            if (order_count_ > 0)
            {
                prompt_open_ = true;
                choice_ = 0; // the safe choice
                choice_position_.snap(0.0f);
                feedback.play(audio::Cue::modal_open);
            }
            else
            {
                refuse("Nothing to refund yet", feedback);
            }
        }
        if (input.is_pressed(Action::back))
        {
            if (focus_ != 0)
            {
                focus_ = 0;
                reason_time_ = 0.0f;
                feedback.play(audio::Cue::back, 1.0f, pan_of(0));
            }
            else
            {
                tap_.trigger();
            }
        }
        const int step = (input.is_pressed(Action::jump_next) ? 1 : 0) -
                         (input.is_pressed(Action::jump_prev) ? 1 : 0);
        if (step != 0)
        {
            const int next = zoom_level_ + step;
            if (next >= 0 && next < kZoomCount)
            {
                zoom_level_ = next;
                feedback.play(audio::Cue::slide, 0.9f + 0.1f * static_cast<float>(next));
            }
            else
            {
                // The end of the zoom range answers like the end of a list,
                // and the lens gives a little in the direction it was pushed.
                feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
                if (!context_.settings.reduced_motion)
                    zoom_.velocity += 0.7f * static_cast<float>(step) * zoom_.target;
            }
        }
    }

    void update_prompt(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav == Direction::up || input.nav == Direction::down)
        {
            const int next = std::clamp(choice_ + (input.nav == Direction::down ? 1 : -1), 0, 1);
            if (next != choice_)
            {
                choice_ = next;
                feedback.play(audio::Cue::focus, 1.0f, 0.4f);
            }
            else if (!input.nav_repeat)
            {
                feedback.play(audio::Cue::error, 1.0f, 0.4f, 0.6f);
            }
        }
        if (input.is_pressed(Action::confirm))
        {
            prompt_open_ = false;
            if (choice_ == 1)
            {
                refunding_ = true;
                refund_clock_ = 0.0f;
                refund_index_ = 0;
                feedback.play(audio::Cue::undo);
                feedback.rumble(0.35f, 0.08f);
            }
            else
            {
                feedback.play(audio::Cue::modal_close);
            }
        }
        else if (input.is_pressed(Action::back))
        {
            prompt_open_ = false;
            feedback.play(audio::Cue::back);
        }
    }

    void try_unlock(app::Feedback &feedback)
    {
        const Node &node = kNodes[focus_];
        char text[96];
        if (unlocked_[focus_])
        {
            // Nothing to do, but the press is still acknowledged.
            tap_.trigger();
            feedback.play(audio::Cue::tick, 1.0f, pan_of(focus_));
            return;
        }
        if (!available(focus_))
        {
            if (node.needs[1] >= 0)
                std::snprintf(text, sizeof(text), "Unlock %s or %s first",
                              kNodes[node.needs[0]].name, kNodes[node.needs[1]].name);
            else
                std::snprintf(text, sizeof(text), "Unlock %s first", kNodes[node.needs[0]].name);
            refuse(text, feedback);
            return;
        }
        if (points_ < node.cost)
        {
            const int missing = node.cost - points_;
            std::snprintf(text, sizeof(text), "%d more point%s needed", missing,
                          missing == 1 ? "" : "s");
            refuse(text, feedback);
            return;
        }

        const int step = count_unlocked(node.branch);
        unlocked_[focus_] = true;
        unlock_age_[focus_] = 0.0f;
        order_[order_count_++] = focus_;
        points_ -= node.cost;
        reason_time_ = 0.0f;
        if (step + 1 == branch_size(node.branch))
        {
            // The last star of a branch: the fanfare replaces the scale note.
            feedback.play(audio::Cue::complete);
            feedback.rumble(0.7f, 0.18f);
            flash_[node.branch].trigger();
        }
        else
        {
            feedback.play(audio::Cue::connect, scale_pitch(step), pan_of(focus_));
            feedback.rumble(0.45f, 0.08f);
        }
    }

    void refuse(const char *reason, app::Feedback &feedback)
    {
        std::snprintf(reason_, sizeof(reason_), "%s", reason);
        reason_time_ = kReasonSeconds;
        feedback.play(audio::Cue::invalid, 1.0f, pan_of(focus_), 0.8f);
        feedback.rumble(0.25f, 0.05f);
        nudge(true);
    }

    void nudge(bool horizontal)
    {
        nudge_.trigger();
        nudge_horizontal_ = horizontal;
    }

    // The reverse cascade: the newest star goes dark first, one per step, and
    // its points come back with it.
    void update_refund(float dt, app::Feedback &feedback)
    {
        if (!refunding_)
            return;
        if (context_.settings.reduced_motion)
        {
            finish_refund();
            return;
        }
        refund_clock_ -= dt;
        while (refunding_ && refund_clock_ <= 0.0f)
        {
            const int index = order_[order_count_ - 1];
            // Each star answers as it dims, falling in pitch: the scale the
            // player climbed, walked back down.
            feedback.play(audio::Cue::tick, 1.25f - 0.06f * static_cast<float>(refund_index_),
                          pan_of(index), 0.7f);
            refund_one();
            ++refund_index_;
            refund_clock_ += kRefundStep;
        }
    }

    void refund_one()
    {
        const int index = order_[--order_count_];
        unlocked_[index] = false;
        points_ += kNodes[index].cost;
        if (order_count_ == 0)
            refunding_ = false;
    }

    void finish_refund()
    {
        while (refunding_)
            refund_one();
    }

    // ---- drawing: the world ----

    float appear(int index) const
    {
        return tween::stagger(age_, depth_[index], 0.07f, 0.5f);
    }

    // The branch colour, washed toward white while the branch celebrates.
    Color tinted(int branch) const
    {
        return gfx::mix(kBranches[branch].colour, kWhite, 0.75f * flash_[branch].value);
    }

    float travel(float age) const
    {
        return context_.settings.reduced_motion ? 1.0f : tween::smoothstep(age / kTravel);
    }

    void draw_branch_names(gfx::DrawList &list, float overview) const
    {
        if (overview <= 0.01f)
            return;
        const float in = tween::stagger(age_, 3, 0.08f, 0.6f);
        for (int b = 1; b < kBranchCount; ++b)
        {
            const Branch &branch = kBranches[b];
            // World units: at the far zoom this is about 50 px on screen.
            ui::text(list, context_.fonts.display, ui::upper(branch.name), branch.label_x,
                     branch.label_y, 100,
                     tinted(b).with_alpha((0.42f + 0.5f * flash_[b].value) * overview * in),
                     gfx::Align::center, 16.0f);
        }
    }

    void draw_links(gfx::DrawList &list, float overview) const
    {
        // Widths are given in screen pixels and divided by the zoom, so a
        // line is as crisp at the far zoom as it is up close.
        const float px = 1.0f / zoom_value();
        const bool calm = context_.settings.reduced_motion;
        for (int n = 1; n < kNodeCount; ++n)
        {
            const Node &child = kNodes[n];
            const Color colour = tinted(child.branch);
            for (int p : child.needs)
            {
                if (p < 0)
                    continue;
                const Node &parent = kNodes[p];
                const float in = appear(n);
                // Links run from rim to rim, not centre to centre.
                const float dx = child.x - parent.x;
                const float dy = child.y - parent.y;
                const float length = std::sqrt(dx * dx + dy * dy);
                const float ux = dx / length;
                const float uy = dy / length;
                // A link that leaves a star steeply downward would run through
                // the name written under it, so it starts below the name.
                const float steep =
                    64.0f * (1.0f - overview) * tween::smoothstep((std::fabs(uy) - 0.75f) / 0.2f);
                const float gap_p = extent_of(p) + 9.0f + (uy > 0.0f ? steep : 0.0f);
                const float gap_n = extent_of(n) + 9.0f + (uy < 0.0f ? steep : 0.0f);
                const float ax = parent.x + ux * gap_p;
                const float ay = parent.y + uy * gap_p;
                const float bx = child.x - ux * gap_n;
                const float by = child.y - uy * gap_n;

                list.line(ax, ay, bx, by, 2.0f * px, kInk.with_alpha(0.14f * in));
                if (unlocked_[p] && unlocked_[n])
                {
                    // Light runs from the star that was lit first to the one
                    // just unlocked: a line whose far end is animated.
                    const bool forward = unlock_age_[p] >= unlock_age_[n];
                    const float t = travel(std::min(unlock_age_[p], unlock_age_[n]));
                    const float sx = forward ? ax : bx;
                    const float sy = forward ? ay : by;
                    const float hx = tween::lerp(sx, forward ? bx : ax, t);
                    const float hy = tween::lerp(sy, forward ? by : ay, t);
                    list.line(sx, sy, hx, hy, 10.0f * px, colour.with_alpha(0.2f * in));
                    list.line(sx, sy, hx, hy, 3.5f * px,
                              gfx::mix(colour, kWhite, 0.25f).with_alpha(in));
                    if (t < 1.0f)
                    {
                        list.glow(disc(hx, hy, 5.0f * px), 5.0f * px, 22.0f * px,
                                  colour.with_alpha(0.9f));
                        list.circle(hx, hy, 5.0f * px, kWhite);
                    }
                    continue;
                }
                // A refunded link fades with the star that went dark.
                const float fade = std::min(lit_[p].value, lit_[n].value);
                if (fade > 0.01f)
                {
                    list.line(ax, ay, bx, by, 3.5f * px, colour.with_alpha(fade * in));
                }
                else if (unlocked_[p])
                {
                    // An open road: tinted, with a slow comet travelling
                    // toward the star it would unlock.
                    list.line(ax, ay, bx, by, 2.0f * px, colour.with_alpha(0.5f * in));
                    if (calm)
                        continue;
                    const float phase = clock_ * 0.42f + static_cast<float>(n) * 0.37f;
                    const float t = phase - std::floor(phase);
                    for (int tail = 2; tail >= 0; --tail)
                    {
                        const float u = tween::clamp01(t - 0.035f * static_cast<float>(tail));
                        const float alpha =
                            tween::ping(u) * (1.0f - 0.33f * static_cast<float>(tail));
                        const float size = (4.5f - static_cast<float>(tail)) * px;
                        list.circle(tween::lerp(ax, bx, u), tween::lerp(ay, by, u), size,
                                    gfx::mix(colour, kWhite, tail == 0 ? 0.7f : 0.2f)
                                        .with_alpha(alpha * in));
                    }
                }
            }
        }
    }

    // The mark inside a lit star: one shape per branch, a star for keystones.
    void draw_icon(gfx::DrawList &list, int index, float cx, float cy, float r, Color ink) const
    {
        const Node &node = kNodes[index];
        if (node.keystone || index == 0)
        {
            list.star(cx, cy, r * 0.56f, ink);
            return;
        }
        switch (node.branch)
        {
        case 1: // Voyager: a heading
            list.triangle({cx - r * 0.4f, cy - r * 0.46f, r * 0.8f, r * 0.8f}, ink);
            break;
        case 2: // Artisan: a cut stone
            list.rotated_rect(disc(cx, cy, r * 0.32f), r * 0.06f, kTau / 8.0f, ink);
            break;
        default: // Sentinel: a shield boss
            list.ring(cx, cy, r * 0.44f, r * 0.16f, ink);
            list.circle(cx, cy, r * 0.12f, ink);
            break;
        }
    }

    void draw_stars(gfx::DrawList &list, float overview) const
    {
        const float near = 1.0f - overview;
        const ui::Fonts &fonts = context_.fonts;
        const float px = 1.0f / zoom_value();
        const bool calm = context_.settings.reduced_motion;
        const float breath = calm ? 0.5f : ui::breathe(clock_, 2.6f);
        char text[8];
        for (int i = 0; i < kNodeCount; ++i)
        {
            const Node &node = kNodes[i];
            const float in = appear(i);
            if (in <= 0.01f)
                continue;
            const Color colour = tinted(node.branch);
            const float lit = lit_[i].value;
            // A star just bought keeps its open look until the light arrives.
            const bool open = available(i) || unlocked_[i];
            const float r = radius_of(i) * (0.7f + 0.3f * in);
            list.push_opacity(in);

            // The ripple: one ring leaving the star as it ignites.
            const float ripple = (unlock_age_[i] - kTravel * 0.85f) / kRipple;
            if (unlocked_[i] && !calm && ripple > 0.0f && ripple < 1.0f)
            {
                const float grow = tween::cubic_out(ripple);
                list.ring(node.x, node.y, r + 12.0f + 110.0f * grow,
                          (1.0f + 4.0f * (1.0f - grow)) * px,
                          colour.with_alpha((1.0f - ripple) * (1.0f - ripple)));
            }
            if (i == focus_ && tap_.value > 0.01f)
                list.ring(node.x, node.y, r + 8.0f + 22.0f * (1.0f - tap_.value), 2.0f * px,
                          colour.with_alpha(0.7f * tap_.value));

            // Light: lit stars glow steadily, open ones breathe.
            const float glow = std::max(0.55f * lit, open ? 0.14f + 0.16f * breath : 0.0f);
            if (glow > 0.01f)
                list.glow(disc(node.x, node.y, r), r, 30.0f + 14.0f * lit, colour.with_alpha(glow));

            // The body: dark glass that fills with the branch colour.
            const Color body = gfx::mix(kVoid, colour, open ? 0.12f : 0.0f);
            list.gradient_rect(disc(node.x, node.y, r), r,
                               gfx::mix(body, gfx::mix(colour, kWhite, 0.45f), lit),
                               gfx::mix(body, colour, lit));
            const Color rim =
                gfx::mix(open ? colour.with_alpha(0.62f + 0.38f * breath) : kInk.with_alpha(0.26f),
                         gfx::mix(colour, kWhite, 0.7f), lit);
            list.ring(node.x, node.y, r, 3.0f * px, rim);
            if (node.keystone)
                list.ring(node.x, node.y, r + kOuterRing, 2.0f * px, rim.with_alpha(0.75f));

            // Inside: the cost while it can still be bought, the mark once lit.
            if (lit < 0.99f && node.cost > 0 && near > 0.01f)
            {
                std::snprintf(text, sizeof(text), "%d", node.cost);
                ui::text(list, fonts.semibold, text, node.x, node.y + 0.35f * 28.0f, 28,
                         (open ? colour : kInk.with_alpha(0.42f)).with_alpha((1.0f - lit) * near),
                         gfx::Align::center);
            }
            if (lit > 0.01f)
                draw_icon(list, i, node.x, node.y, r, kVoid.with_alpha(0.86f * lit));

            // The name, under the star; hidden at the far zoom, where it
            // would be too small to read.
            if (near > 0.01f)
            {
                // The reticle needs the space right under the star: the name
                // steps down as the reticle closes in, and back as it leaves.
                const float rx = reticle_x_.value - node.x;
                const float ry = reticle_y_.value - node.y;
                const float held = 1.0f - tween::smoothstep(std::sqrt(rx * rx + ry * ry) / 90.0f);
                const float rest = unlocked_[i] ? 1.0f : available(i) ? 0.82f : 0.5f;
                const float strength =
                    std::max(held, rest) * near * clear_of_panels(node.x, node.y);
                const float baseline = node.y + extent_of(i) + 36.0f + 24.0f * held * px;
                // A soft dark halo, as on a printed map: links that pass
                // behind a name go under it instead of striking it through.
                const float width = fonts.semibold.measure(node.name, 24);
                list.shadow({node.x - width * 0.5f, baseline - 20.0f, width, 24.0f}, 12, 12,
                            Color::rgb(0x04060f, 0.85f * strength));
                ui::text(list, fonts.semibold, node.name, node.x, baseline, 24,
                         kInk.with_alpha(strength), gfx::Align::center);
            }
            list.pop_opacity();
        }
    }

    // The focus: a ring and four ticks (short arcs) that turn slowly, drawn in
    // the world so it flies between the stars and grows to fit the one it
    // lands on.
    void draw_reticle(gfx::DrawList &list) const
    {
        const float px = 1.0f / zoom_value();
        const bool calm = context_.settings.reduced_motion;
        const float in = tween::stagger(age_, 2, 0.08f, 0.5f);
        const float shake = ui::shake(nudge_.value, clock_, 12.0f, 9.0f) * px;
        const float cx = reticle_x_.value + (nudge_horizontal_ ? shake : 0.0f);
        const float cy = reticle_y_.value + (nudge_horizontal_ ? 0.0f : shake);
        const float breath = calm ? 0.5f : ui::breathe(clock_);
        const float r = reticle_r_.value + (15.0f + 2.5f * breath) * px;
        const float spin = calm ? 0.0f : clock_ * 0.45f;
        const Color colour = tinted(kNodes[focus_].branch);

        list.push_opacity(in);
        list.glow(disc(cx, cy, r), r, 26.0f * px, colour.with_alpha(0.16f + 0.1f * breath));
        list.ring(cx, cy, r, 1.5f * px, kWhite.with_alpha(0.4f));
        constexpr float kSweep = 0.44f;
        for (int k = 0; k < 4; ++k)
        {
            // arc() takes the outer radius: the ticks ride just outside the ring.
            const float angle = spin + static_cast<float>(k) * kTau / 4.0f - kSweep * 0.5f;
            list.arc(cx, cy, r + 10.0f * px, 5.0f * px, angle, kSweep, kWhite);
        }
        list.pop_opacity();
    }

    // ---- drawing: the screen ----

    // A frosted panel: the blurred map, a tint, then a hairline of light.
    static void draw_glass(gfx::DrawList &list, std::uint32_t glass, const Rect &r, float radius,
                           Color top, Color bottom, Color edge)
    {
        list.glass(glass, r, radius, kWhite);
        list.gradient_rect(r, radius, top, bottom);
        list.bordered_rect(r, radius, kClear, 1.5f, edge);
    }

    void draw_hud(gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = context_.fonts;
        char text[16];
        float in = tween::stagger(age_, 0, 0.08f, 0.5f);
        // The prompt is the only thing to read while it is up.
        list.push_opacity(1.0f - 0.55f * prompt_.value);
        list.push_opacity(in);
        const Rect plate{kHud.x, kHud.y - 24.0f * (1.0f - in), kHud.w, kHud.h};
        list.shadow({plate.x, plate.y + 12, plate.w, plate.h}, kHudRadius, 36,
                    Color::rgb(0x000000, 0.4f));
        draw_glass(list, glass, plate, kHudRadius, Color::rgb(0x0c1230, 0.62f),
                   Color::rgb(0x070a1c, 0.72f), kWhite.with_alpha(0.16f));
        ui::text(list, fonts.semibold, "SKILL POINTS", kMargin, 82, 16, kMuted, gfx::Align::left,
                 3.5f);
        // The number pops about its own centre each time it changes.
        std::snprintf(text, sizeof(text), "%d", shown_points_);
        const float pop = points_pop_.value;
        list.push_transform(1.0f + 0.22f * pop, kMargin + 20, 128, 0, 0);
        ui::text(list, fonts.mono, text, kMargin - 3, 152, 68,
                 gfx::mix(shown_points_ > 0 ? kInk : kMuted, kBranches[0].colour, pop));
        list.pop_transform();
        ui::text(list, fonts.regular, "to spend", kMargin + 54, 152, 22, kMuted);
        list.pop_opacity();

        // One gauge per branch: how much of it is lit.
        for (int b = 1; b < kBranchCount; ++b)
        {
            in = tween::stagger(age_, b, 0.08f, 0.5f);
            const float cx = 286.0f + static_cast<float>(b) * 116.0f;
            const float cy = 106.0f - 10.0f * (1.0f - in);
            const Color colour = tinted(b);
            list.push_opacity(in);
            if (flash_[b].value > 0.01f)
                list.glow(disc(cx, cy, 34), 34, 26, colour.with_alpha(0.6f * flash_[b].value));
            list.arc(cx, cy, 34, 6, 0.0f, kTau, kInk.with_alpha(0.14f));
            list.arc(cx, cy, 34, 6, 0.0f, kTau * gauge_[b].value * in, colour);
            std::snprintf(text, sizeof(text), "%d", count_unlocked(b));
            ui::text(list, fonts.semibold, text, cx, cy + 0.35f * 26.0f, 26, kInk,
                     gfx::Align::center);
            ui::text(list, fonts.semibold, ui::upper(kBranches[b].name), cx + 1, cy + 60, 15,
                     colour.with_alpha(0.9f), gfx::Align::center, 2.0f);
            list.pop_opacity();
        }
        list.pop_opacity();
    }

    // One star's description, drawn at an opacity and a horizontal offset so
    // two of them can cross-fade.
    void draw_info(gfx::DrawList &list, const Rect &card, int index, float alpha, float slide) const
    {
        if (alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const Node &node = kNodes[index];
        const Color colour = kBranches[node.branch].colour;
        const float x = card.x + kCardPad + slide;
        const float w = card.w - 2.0f * kCardPad;
        const bool lit = unlocked_[index];
        const bool open = available(index);
        const bool affordable = points_ >= node.cost;
        char text[96];
        list.push_opacity(alpha);

        float cursor = x + ui::text(list, fonts.semibold, ui::upper(kBranches[node.branch].name), x,
                                    card.y + 64, 17, colour, gfx::Align::left, 4.0f);
        if (node.keystone)
        {
            list.star(cursor + 20, card.y + 58, 8, colour);
            ui::text(list, fonts.semibold, "KEYSTONE", cursor + 38, card.y + 64, 17,
                     kInk.with_alpha(0.62f), gfx::Align::left, 4.0f);
        }
        ui::text(list, fonts.display, fonts.display.font->fit(node.name, 46, w), x - 2,
                 card.y + 124, 46, kWhite);
        list.rounded_rect({x, card.y + 150, w, 1.5f}, 0.75f, kInk.with_alpha(0.16f));
        ui::paragraph(list, fonts.regular, node.effect, x, card.y + 200, 25, w, 36,
                      kInk.with_alpha(0.88f), 3);

        // Cost: one pip per point, then the words.
        ui::text(list, fonts.semibold, "COST", x, card.y + 344, 15, kMuted, gfx::Align::left, 3.5f);
        for (int i = 0; i < 3; ++i)
        {
            const float px = x + 10.0f + static_cast<float>(i) * 28.0f;
            if (i < node.cost)
                list.circle(px, card.y + 374, 9, lit || affordable ? colour : kMuted);
            else
                list.ring(px, card.y + 374, 9, 2, kInk.with_alpha(0.2f));
        }
        if (node.cost > 0)
            std::snprintf(text, sizeof(text), "%d point%s", node.cost, node.cost == 1 ? "" : "s");
        else
            std::snprintf(text, sizeof(text), "Free");
        ui::text(list, fonts.semibold, text, x + 100, card.y + 383, 26, kInk);

        // State: a lamp, a word, and one line of detail. A refusal takes the
        // detail line over for two seconds, then hands it back.
        ui::text(list, fonts.semibold, "STATE", x, card.y + 446, 15, kMuted, gfx::Align::left,
                 3.5f);
        const Color lamp = lit ? colour : open ? kWhite : kMuted;
        if (lit)
            list.glow(disc(x + 10, card.y + 476, 8), 8, 12, colour.with_alpha(0.7f));
        if (lit || open)
            list.circle(x + 10, card.y + 476, 8, lamp);
        else
            list.ring(x + 10, card.y + 476, 8, 2, lamp);
        ui::text(list, fonts.semibold,
                 lit    ? "Unlocked"
                 : open ? "Available"
                        : "Locked",
                 x + 32, card.y + 485, 26,
                 lit    ? colour
                 : open ? kWhite
                        : kInk.with_alpha(0.7f));

        const float reason = index == focus_
                                 ? std::min(tween::clamp01((kReasonSeconds - reason_time_) / 0.12f),
                                            tween::clamp01(reason_time_ / 0.3f))
                                 : 0.0f;
        if (lit)
            std::snprintf(text, sizeof(text), index == 0 ? "Always lit" : "Active on your path");
        else if (open && affordable)
            std::snprintf(text, sizeof(text), "Linked to your path");
        else if (open)
            std::snprintf(text, sizeof(text), "Not enough points");
        else if (node.needs[1] >= 0)
            std::snprintf(text, sizeof(text), "Requires: %s or %s", kNodes[node.needs[0]].name,
                          kNodes[node.needs[1]].name);
        else
            std::snprintf(text, sizeof(text), "Requires: %s", kNodes[node.needs[0]].name);
        ui::paragraph(list, fonts.regular, text, x, card.y + 524, 22, w, 30,
                      kInk.with_alpha(0.72f * (1.0f - reason)), 2);
        if (reason > 0.01f)
            ui::paragraph(list, fonts.semibold, reason_, x, card.y + 524, 22, w, 30,
                          kWarm.with_alpha(reason), 2);

        // The action. Only a star that can be bought gets the bright button.
        const Rect pill{x, card.y + card.h - kCardPad - kRowHeight, w, kRowHeight};
        if (open && affordable)
        {
            list.glow(pill, 32, 18, colour.with_alpha(0.35f));
            list.rounded_rect(pill, 32, kWhite);
            ui::draw_button(list, fonts, ui::GlyphStyle::light(), ui::Button::cross, pill.x + 20,
                            pill.cy(), 34);
            ui::text(list, fonts.semibold, "Unlock", pill.x + 68, pill.cy() + 10, 28,
                     Color::rgb(0x0a0e20));
        }
        else
        {
            list.bordered_rect(pill, 32, kWhite.with_alpha(0.05f), 1.5f, kWhite.with_alpha(0.16f));
            ui::text(list, fonts.semibold,
                     lit    ? "Unlocked  \xE2\x9C\x93"
                     : open ? "Not enough points"
                            : "Locked",
                     pill.cx(), pill.cy() + 9, 26, lit ? colour : kInk.with_alpha(0.5f),
                     gfx::Align::center);
        }
        list.pop_opacity();
    }

    // The refund question, in the same card: the scene dims, the hints
    // change, and the highlight starts on the choice that changes nothing.
    void draw_prompt(gfx::DrawList &list, const Rect &card, float alpha) const
    {
        if (alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const float x = card.x + kCardPad;
        const float w = card.w - 2.0f * kCardPad;
        char text[96];
        list.push_opacity(alpha);
        ui::text(list, fonts.semibold, "REFUND", x, card.y + 64, 17, kWarm, gfx::Align::left, 4.0f);
        ui::text(list, fonts.display, "Start over?", x - 2, card.y + 124, 46, kWhite);
        list.rounded_rect({x, card.y + 150, w, 1.5f}, 0.75f, kInk.with_alpha(0.16f));
        const int spent = kStartPoints - points_;
        std::snprintf(text, sizeof(text),
                      "%d star%s will go dark and %d point%s will come back to you.", order_count_,
                      order_count_ == 1 ? "" : "s", spent, spent == 1 ? "" : "s");
        ui::paragraph(list, fonts.regular, text, x, card.y + 200, 25, w, 36, kInk.with_alpha(0.88f),
                      3);

        // What comes back, branch by branch.
        ui::text(list, fonts.semibold, "COMES BACK", x, card.y + 344, 15, kMuted, gfx::Align::left,
                 3.5f);
        float row_y = card.y + 386;
        for (int b = 1; b < kBranchCount; ++b)
        {
            int cost = 0;
            for (int i = 0; i < kNodeCount; ++i)
                cost += unlocked_[i] && kNodes[i].branch == b ? kNodes[i].cost : 0;
            if (cost == 0)
                continue;
            list.circle(x + 10, row_y - 8, 7, kBranches[b].colour);
            ui::text(list, fonts.regular, kBranches[b].name, x + 32, row_y, 24,
                     kInk.with_alpha(0.9f));
            std::snprintf(text, sizeof(text), "+%d", cost);
            ui::text(list, fonts.mono, text, x + w, row_y, 24, kBranches[b].colour,
                     gfx::Align::right);
            row_y += 38;
        }

        const char *choices[2] = {"Keep my path", "Refund everything"};
        const float top = card.y + card.h - kCardPad - 2.0f * kRowHeight - kRowGap;
        list.rounded_rect({x, top + choice_position_.value * (kRowHeight + kRowGap), w, kRowHeight},
                          32, kWhite);
        for (int i = 0; i < 2; ++i)
        {
            const bool focused = i == choice_;
            const Rect row{x, top + static_cast<float>(i) * (kRowHeight + kRowGap), w, kRowHeight};
            if (!focused)
                list.bordered_rect(row, 32, kWhite.with_alpha(0.05f), 1.5f,
                                   kWhite.with_alpha(0.18f));
            ui::text(list, fonts.semibold, choices[i], row.x + 30, row.cy() + 9, 26,
                     focused ? Color::rgb(0x0a0e20) : kInk.with_alpha(0.9f));
        }
        list.pop_opacity();
    }

    void draw_card(gfx::DrawList &list, std::uint32_t glass) const
    {
        const float in = tween::stagger(age_, 4, 0.08f, 0.55f);
        const float prompt = prompt_.value;
        Rect card = kCard;
        card.x += 56.0f * (1.0f - in);
        const Color colour = gfx::mix(kBranches[kNodes[shown_].branch].colour, kWarm, prompt);

        list.push_opacity(in);
        list.shadow({card.x, card.y + 18, card.w, card.h}, kCardRadius, 50,
                    Color::rgb(0x000000, 0.5f));
        // The tint leans toward the branch of the star it describes.
        draw_glass(list, glass, card, kCardRadius,
                   gfx::mix(Color::rgb(0x0c1230), colour, 0.16f).with_alpha(0.7f),
                   Color::rgb(0x070a1c, 0.8f),
                   gfx::mix(kWhite, colour, 0.4f).with_alpha(0.24f + 0.2f * prompt));

        const float info = 1.0f - tween::smoothstep(prompt * 1.6f);
        if (card_.running)
        {
            // The old star leaves quickly; the new one arrives a beat later.
            const float t = card_.progress();
            draw_info(list, card, previous_, info * (1.0f - tween::smoothstep(t * 2.4f)),
                      -24.0f * tween::cubic_in(tween::clamp01(t * 2.4f)));
            const float arrive = tween::clamp01((t - 0.22f) / 0.78f);
            draw_info(list, card, shown_, info * tween::smoothstep(arrive),
                      30.0f * (1.0f - tween::quint_out(arrive)));
        }
        else
        {
            draw_info(list, card, shown_, info, 0.0f);
        }
        draw_prompt(list, card, tween::smoothstep((prompt - 0.35f) / 0.65f));
        list.pop_opacity();
    }

    // The width ui::draw_hints will take. The plate behind the row has to be
    // drawn before the row, so the row is measured the way the kit lays it out.
    float hints_width(const ui::Hint *hints, int count) const
    {
        const ui::HintLayout layout;
        float total = 0.0f;
        for (int i = 0; i < count; ++i)
        {
            const ui::Hint &hint = hints[i];
            total += ui::button_width(hint.button, layout.size) + 12.0f +
                     context_.fonts.regular.measure(hint.label, layout.text_size);
            if (hint.second != ui::Button::none)
                total += 6.0f + ui::button_width(hint.second, layout.size);
            if (i + 1 < count)
                total += layout.item_gap;
        }
        return total;
    }

    void draw_hints(gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const ui::GlyphStyle style = ui::GlyphStyle::dark();
        const ui::HintLayout layout;
        const ui::Hint map_hints[] = {{ui::Button::dpad, "Move"},
                                      {ui::Button::cross, "Unlock"},
                                      {ui::Button::triangle, "Refund"},
                                      {ui::Button::l2, "Zoom", ui::Button::r2},
                                      {ui::Button::circle, "Origin"}};
        const ui::Hint prompt_hints[] = {{ui::Button::cross, "Choose"},
                                         {ui::Button::circle, "Back"}};
        // One plate for both rows: it changes width as they cross-fade.
        const float prompt = tween::smoothstep(prompt_.value);
        const float width =
            tween::lerp(hints_width(map_hints, 5), hints_width(prompt_hints, 2), prompt);
        const float in = tween::stagger(age_, 7, 0.08f, 0.5f);
        const float right = gfx::kVirtualWidth - kMargin;
        const Rect plate{right - width - kHintPad, layout.cy - 34.0f + 24.0f * (1.0f - in),
                         width + 2.0f * kHintPad, 68.0f};
        list.push_opacity(in);
        draw_glass(list, glass, plate, 34, Color::rgb(0x0c1230, 0.6f), Color::rgb(0x070a1c, 0.7f),
                   kWhite.with_alpha(0.14f));
        list.push_opacity(1.0f - tween::clamp01(prompt * 2.0f));
        ui::draw_hints(list, fonts, style, map_hints, 5, right, true);
        list.pop_opacity();
        list.push_opacity(tween::clamp01(prompt * 2.0f - 1.0f));
        ui::draw_hints(list, fonts, style, prompt_hints, 2, right, true);
        list.pop_opacity();
        list.pop_opacity();
    }

    app::Context &context_;
    float age_ = 0.0f;      // seconds since enter(): drives the entrance
    float clock_ = 0.0f;    // free-running time for idle motion
    float sky_time_ = 0.0f; // the backdrop's clock: stops under reduced motion

    // Progression.
    bool unlocked_[kNodeCount] = {};
    int order_[kNodeCount] = {}; // unlock order, so a refund can run backwards
    int order_count_ = 0;
    int points_ = kStartPoints;
    int shown_points_ = kStartPoints; // what the counter says while it catches up
    float point_step_ = 0.0f;
    bool refunding_ = false;
    float refund_clock_ = 0.0f;
    int refund_index_ = 0;

    // Per-star animation.
    int depth_[kNodeCount] = {};        // links from the origin: entrance order
    float unlock_age_[kNodeCount] = {}; // seconds since each star was unlocked
    tween::Spring lit_[kNodeCount];     // 0 dark .. 1 burning
    tween::Spring gauge_[kBranchCount];
    ui::Pulse flash_[kBranchCount]; // a branch was completed

    // Focus, camera, card.
    int focus_ = 0;
    int zoom_level_ = 1;
    tween::Spring camera_x_, camera_y_, zoom_;
    tween::Spring reticle_x_, reticle_y_, reticle_r_;
    ui::SpringColor nebula_;
    int shown_ = 0;    // the star the card describes
    int previous_ = 0; // ... and the one it is fading out
    tween::Timer card_;
    ui::Pulse points_pop_;
    ui::Pulse nudge_; // a refusal shakes the reticle
    bool nudge_horizontal_ = true;
    ui::Pulse tap_; // a press with nothing to do
    char reason_[96] = "";
    float reason_time_ = 0.0f; // seconds the refusal stays in the card

    // The refund prompt.
    bool prompt_open_ = false;
    tween::Spring prompt_;
    int choice_ = 0;
    tween::Spring choice_position_;
};

} // namespace

std::unique_ptr<app::Concept> make_constellation(app::Context &context)
{
    return std::make_unique<Constellation>(context);
}

} // namespace hui::concepts
