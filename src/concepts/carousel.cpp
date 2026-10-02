// ps5-homebrew-ui - Design "Cover Wheel": a carousel of covers you can spin.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The whole collection stands in one row on a dark, reflecting floor. The
// carousel is not a list with an index: it is a wheel with a position and a
// velocity, and everything on screen is drawn from that one number. What
// makes it feel finished:
//
//   - the left stick scrubs the wheel, releasing it lets it coast on its own
//     inertia, and a spring then pulls it onto the nearest cover; the D-pad
//     and the triggers only move the spring's target, so every way of moving
//     blends into every other;
//   - depth without 3D: a cover's place, size and brightness are smooth
//     functions of its distance from the wheel position, and covers are drawn
//     far to near so the near ones overlap the far ones;
//   - each cover is mirrored in the floor: the same texture with its v range
//     reversed, squashed, darkened and veiled by a gradient of the floor's
//     own colour;
//   - the two ends are rubber: scrubbing into one stretches a little, coasting
//     into one bounces back, and pressing against one gives a soft refusal;
//   - the caption follows the wheel but only shows once it settles, so fast
//     scrubbing blurs past titles instead of strobing them;
//   - Cross lifts the centred cover out of the row toward the player while
//     the rest slide away, and a frosted panel with its details fades in.

#include "concepts/concepts.hpp"

#include "core/tween.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace hui::concepts
{

namespace
{

using gfx::Color;
using gfx::Rect;

const Color kWhite = Color::rgb(0xffffff);
const Color kClear = Color::rgb(0x000000, 0.0f);
const Color kBlack = Color::rgb(0x000000);
const Color kNight = Color::rgb(0x030408); // what every dark tone is mixed toward
const Color kInk = Color::rgb(0x0b0d16);   // text on white buttons
const Color kGold = Color::rgb(0xffd166);

// ---- the row: where a cover sits at distance d from the wheel position ----
constexpr float kCentreX = 960.0f;
constexpr float kCover = 440.0f;   // size of the centred cover
constexpr float kRadius = 28.0f;   // its corner radius; scales with the cover
constexpr float kBaseY = 592.0f;   // the floor line under the centred cover
constexpr float kVanishY = 516.0f; // far covers stand closer to this height
constexpr float kSpreadA = 560.0f; // x = A * (1 - exp(-k |d|)) + B * |d|: the first
constexpr float kSpreadK = 0.95f;  //   step is wide, later ones crowd together
constexpr float kSpreadB = 40.0f;
constexpr float kFarScale = 0.36f; // size of the farthest covers
constexpr float kScaleFall = 1.1f;
constexpr float kFarLight = 0.26f; // brightness of the farthest covers
constexpr float kLightFall = 0.8f;
constexpr float kSoftAbs = 0.3f; // rounds |d| at 0 so size has no sharp peak
constexpr float kReach = 6.0f;   // covers farther than this are not drawn

// ---- the floor and the reflections ----
constexpr float kFloorY = 536.0f;      // the floor is opaque from here down
constexpr float kHorizon = 120.0f;     // ... and fades in over this height
constexpr float kReflectPart = 0.42f;  // how much of a cover is mirrored
constexpr float kReflectSquash = 0.8f; // the mirror image is a little flatter
constexpr float kReflectLight = 0.3f;  // and much darker
constexpr float kReflectHaze = 0.18f;  // veil opacity right under the cover

// ---- the wheel: units are covers and seconds ----
constexpr int kStart = 3;
constexpr int kJump = 5;               // L2 / R2
constexpr float kStickDead = 0.12f;    // below this the stick does not scrub
constexpr float kScrubSpeed = 11.0f;   // covers per second at full deflection
constexpr float kScrubResponse = 5.0f; // how quickly the wheel takes the stick's speed
constexpr float kFriction = 1.8f;      // coasting: speed decays by e every 1/1.8 s
constexpr float kSnapSpeed = 1.4f;     // below this a coasting wheel is caught
constexpr float kSnapLook = 0.18f;     // ... on the cover it would reach in this time
constexpr float kSnapOmega = 13.0f;
constexpr float kSnapDamping = 0.8f; // a touch under critical: weight, not wobble
constexpr float kRubber = 0.3f;      // how far a held stick stretches past an end
constexpr float kRubberRate = 12.0f;
constexpr float kRefuseKick = 3.2f;     // speed given to the wheel by a refused press
constexpr float kMaxEndSpeed = 8.0f;    // a coasting wheel hits an end no faster
constexpr float kEndSlack = 0.02f;      // this far past an end counts as hitting it
constexpr float kTickInterval = 0.085f; // at most about twelve ticks a second

// ---- the caption and the scrub bar ----
constexpr float kHeaderY = 94.0f;
constexpr float kTitleY = 808.0f;
constexpr float kMetaY = 856.0f;
constexpr float kBarX = 560.0f;
constexpr float kBarW = 800.0f;
constexpr float kBarY = 918.0f;
constexpr float kCalmFrom = 5.5f; // faster than this the caption starts to hide
constexpr float kCalmSpan = 2.5f;

// ---- the opened cover and its panel ----
const Rect kOpenRect{176.0f, 250.0f, 520.0f, 520.0f};
const Rect kPanel{760.0f, 210.0f, 1064.0f, 600.0f};
constexpr float kPanelRadius = 40.0f;
constexpr float kPanelPad = 56.0f;
constexpr float kOpenSlide = 520.0f; // how far the other covers move away

constexpr const char *kTechniques[] = {
    "A wheel with position and velocity: stick scrubbing, inertia, spring snap to a cover",
    "Depth without 3D: place, size and light from distance; covers drawn far to near",
    "Floor reflections: the cover texture with reversed v, squashed and veiled by a gradient",
    "Rubber-band ends: stretch while scrubbing, bounce when coasting, soft refusal on a press",
    "A caption that waits for the wheel to settle, so fast scrubbing never strobes text",
    "The centred cover lifts out of the row beside a frosted details panel (Frame::glass)",
};

constexpr app::TourStep kTour[] = {
    {0.4f, 0, Direction::right},
    {0.4f, 0, Direction::right},
    {0.85f, 0, Direction::none, "scrub", 1.0f}, // the stick is held, then the picture
    {2.2f, action_bit(Action::north)},          // let go: coast, settle, then favourite
    {0.9f, action_bit(Action::confirm), Direction::none, "settled"},
    {1.1f, action_bit(Action::confirm), Direction::none, "details"},
    {1.3f, action_bit(Action::back)},
    {0.7f, action_bit(Action::jump_next)},
    {1.4f, 0, Direction::none, "end", 1.0f}, // leaning on the last cover
    {0.9f, action_bit(Action::jump_prev)},
    {0.4f, action_bit(Action::jump_prev)},
};

// How the wheel is being driven right now.
enum class Drive
{
    snap,  // a spring pulls it onto target_
    scrub, // the stick sets its speed
    coast, // nothing touches it: friction only
};

// Where one cover is drawn.
struct Pose
{
    Rect rect;
    float light = 1.0f; // brightness of the artwork
    float alpha = 1.0f;
    float reflect = 1.0f; // strength of its mirror image
};

class CoverFlow final : public app::Concept
{
  public:
    explicit CoverFlow(app::Context &context)
        : context_(context), count_(static_cast<int>(context.catalog.size()))
    {
        favorite_.assign(static_cast<std::size_t>(count_), false);
        target_ = centre_ = opened_ = std::min(kStart, count_ - 1);
        wheel_.snap(static_cast<float>(target_));
        calm_.snap(1.0f);
        apply_palette(true);
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "carousel",
            "Cover Wheel",
            "A carousel with weight: scrub it, let it coast, open a cover",
            "src/concepts/carousel.cpp",
            audio::SoundSet::glass,
            Color::rgb(0x38e8ff),
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        age_ = 0.0f;
        open_ = false;
        launch_.running = false;
        // Coming back mid-spin would be odd: the wheel is at rest on a cover.
        settle_on(current());
        quiet_ = true;
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        age_ += dt;
        clock_ += dt;
        tick_wait_ = std::max(0.0f, tick_wait_ - dt);
        if (open_)
            update_details(input, feedback);
        else
            update_wheel(input, feedback);
        spin(dt, feedback);

        // ---- animation state ----
        const float speed = std::fabs(wheel_.velocity);
        calm_.target = 1.0f - tween::clamp01((speed - kCalmFrom) / kCalmSpan);
        calm_.update(dt, 14.0f);
        for (ui::SpringColor &colour : palette_)
            colour.update(dt, 4.5f);
        lift_.target = open_ ? 1.0f : 0.0f;
        lift_.update(dt, reduced() ? 40.0f : 11.0f);
        launch_.update(dt);
        star_.update(dt, 5.0f);
    }

    void draw(app::Frame &frame) const override
    {
        // A spotlight in the centred cover's body colour, behind the row.
        frame.backdrop.mode = gfx::BackdropMode::gradient;
        frame.backdrop.colors[0] = palette_[0].value();
        frame.backdrop.colors[1] = palette_[1].value();
        frame.backdrop.colors[2] = palette_[2].value();
        frame.backdrop.params[0] = 0.5f;
        frame.backdrop.params[1] = 0.34f;
        frame.backdrop.params[2] = 0.5f + (reduced() ? 0.0f : 0.06f * ui::breathe(clock_, 5.0f));
        frame.backdrop.time = clock_;

        gfx::DrawList &list = frame.scene;
        const float lift = tween::clamp01(lift_.value);
        draw_floor(list);
        draw_header(list);
        draw_row(list, lift);
        draw_caption(list, lift);
        draw_scrub_bar(list, lift);
        if (lift > 0.01f)
        {
            // The row steps back: a scrim over everything but the lifted cover.
            list.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                              kNight.with_alpha(0.42f * lift));
            // Launching: the cover flares in its own accent and the light
            // washes over the whole screen for a moment.
            const float flare = launch_.running ? 1.0f - launch_.progress() : 0.0f;
            draw_cover(list, opened_, pose(opened_), lift * (1.0f + 1.6f * flare * flare));
            frame.glass = true;
            draw_details(frame.overlay, frame.glass_texture, lift);
            if (flare > 0.0f)
                frame.overlay.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                                           gfx::mix(item(opened_).accent, kWhite, 0.55f)
                                               .with_alpha(0.26f * lift * flare * flare * flare));
        }
        draw_hints(lift > 0.5f ? frame.overlay : frame.scene, lift);
    }

    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    bool reduced() const
    {
        return context_.settings.reduced_motion;
    }

    const demo::Item &item(int index) const
    {
        return context_.catalog[static_cast<std::size_t>(index)];
    }

    float last() const
    {
        return static_cast<float>(count_ - 1);
    }

    // The cover closest to the wheel position.
    int nearest() const
    {
        return std::clamp(static_cast<int>(std::lround(wheel_.value)), 0, count_ - 1);
    }

    // The cover the player means: where the wheel is going, or where it is.
    int current() const
    {
        return drive_ == Drive::snap ? target_ : nearest();
    }

    // How far the wheel is past an end: negative before the first cover,
    // positive after the last, zero in between.
    float beyond() const
    {
        if (wheel_.value < 0.0f)
            return wheel_.value;
        return std::max(0.0f, wheel_.value - last());
    }

    // The stick is scrubbing outward from the first or the last cover.
    bool pushing_at_end() const
    {
        return drive_ == Drive::scrub && ((wheel_.value <= 0.0f && stick_ < 0.0f) ||
                                          (wheel_.value >= last() && stick_ > 0.0f));
    }

    // 1 when the wheel rests on a cover, 0 half-way between two. The caption,
    // the focus ring and the glow all fade with it. A wheel stretched past an
    // end still counts as resting on the end cover.
    float settled() const
    {
        const float inside = std::clamp(wheel_.value, 0.0f, last());
        const float off = std::fabs(inside - static_cast<float>(centre_));
        return 1.0f - tween::smoothstep((off - 0.08f) / 0.34f);
    }

    void settle_on(int index)
    {
        target_ = std::clamp(index, 0, count_ - 1);
        drive_ = Drive::snap;
    }

    // The backdrop, the floor and the accents take the centred cover's colours.
    void apply_palette(bool snap)
    {
        const demo::Item &it = item(centre_);
        const Color sky = gfx::mix(it.dark, it.mid, 0.3f);
        const Color ground = gfx::mix(it.dark, it.mid, 0.12f);
        const Color targets[5] = {
            gfx::mix(it.dark, kNight, 0.6f), // backdrop, top
            gfx::mix(sky, kNight, 0.35f),    // backdrop, bottom
            it.mid,                          // the spotlight behind the row
            gfx::mix(ground, kNight, 0.5f),  // the floor at its horizon
            it.accent,                       // the scrub bar's thumb
        };
        for (int i = 0; i < 5; ++i)
        {
            if (snap)
                palette_[i].snap(targets[i]);
            else
                palette_[i].target(targets[i]);
        }
    }

    // ---- input ----

    void update_wheel(const InputFrame &input, app::Feedback &feedback)
    {
        // The stick also produces nav repeats; while it scrubs, those are not
        // steps. A D-pad press arrives with the stick at rest.
        const bool scrubbing = std::fabs(input.stick_x) >= kStickDead && !input.focus_lost;
        if (scrubbing)
        {
            drive_ = Drive::scrub;
            stick_ = input.stick_x;
            quiet_ = false;
        }
        else if (drive_ == Drive::scrub)
        {
            release();
        }

        if (!scrubbing && (input.nav == Direction::left || input.nav == Direction::right))
        {
            const int direction = input.nav == Direction::right ? 1 : -1;
            if (move_target(direction))
            {
                // Left and right sound left and right, and a hair apart in pitch.
                feedback.play(audio::Cue::focus, 1.0f + 0.025f * static_cast<float>(direction),
                              0.3f * static_cast<float>(direction));
            }
            else if (!input.nav_repeat)
            {
                refuse(direction, feedback);
            }
        }
        for (const int direction : {-1, 1})
        {
            if (!input.is_pressed(direction < 0 ? Action::jump_prev : Action::jump_next))
                continue;
            if (move_target(direction * kJump))
                feedback.play(audio::Cue::tab, 1.0f + 0.06f * static_cast<float>(direction),
                              0.4f * static_cast<float>(direction));
            else
                refuse(direction, feedback);
        }
        if (input.is_pressed(Action::north))
            toggle_favorite(feedback);
        if (input.is_pressed(Action::confirm))
        {
            opened_ = current();
            settle_on(opened_);
            quiet_ = true;
            open_ = true;
            feedback.play(audio::Cue::open);
        }
    }

    void update_details(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.is_pressed(Action::confirm))
        {
            feedback.play(audio::Cue::launch);
            feedback.rumble(0.7f, 0.18f);
            launch_.start(0.9f);
        }
        if (input.is_pressed(Action::north))
            toggle_favorite(feedback);
        if (input.is_pressed(Action::back))
        {
            feedback.play(audio::Cue::back);
            open_ = false;
        }
    }

    // Moves the spring's target by `delta` covers, stopping at the ends.
    // Returns false when it could not move at all.
    bool move_target(int delta)
    {
        const int from = current();
        const int to = std::clamp(from + delta, 0, count_ - 1);
        if (to == from)
            return false;
        settle_on(to);
        quiet_ = true; // the press had its own sound: no ticks on the way
        return true;
    }

    // The end of the row answers: a quiet cue, a light pulse, and the wheel
    // is pushed against its stop so the spring shows the bump.
    void refuse(int direction, app::Feedback &feedback)
    {
        feedback.play(audio::Cue::error, 1.0f, 0.4f * static_cast<float>(direction), 0.6f);
        feedback.rumble(0.25f, 0.05f);
        quiet_ = true;
        if (reduced())
            return;
        settle_on(current());
        wheel_.kick(kRefuseKick * static_cast<float>(direction));
    }

    // The stick was let go.
    void release()
    {
        if (beyond() != 0.0f)
        {
            settle_on(nearest());
        }
        else if (reduced() || std::fabs(wheel_.velocity) < kSnapSpeed)
        {
            catch_wheel();
        }
        else
        {
            drive_ = Drive::coast;
        }
    }

    // Hands a slow wheel to the spring, aimed at the cover it was heading for.
    // The velocity is kept, so the hand-over cannot be seen.
    void catch_wheel()
    {
        settle_on(static_cast<int>(std::lround(wheel_.value + wheel_.velocity * kSnapLook)));
    }

    void toggle_favorite(app::Feedback &feedback)
    {
        const std::size_t index = static_cast<std::size_t>(open_ ? opened_ : current());
        favorite_[index] = !favorite_[index];
        feedback.play(favorite_[index] ? audio::Cue::favorite_on : audio::Cue::favorite_off);
        if (favorite_[index])
            star_.trigger();
    }

    // ---- physics ----

    // Advances the wheel by dt according to how it is driven, then reports
    // what the movement caused: an end reached, a new cover in the centre.
    void spin(float dt, app::Feedback &feedback)
    {
        switch (drive_)
        {
        case Drive::scrub:
        {
            // A response curve gives fine control near the centre of the stick.
            const float push = std::copysign(std::pow(std::fabs(stick_), 1.6f), stick_);
            if (pushing_at_end())
            {
                // Rubber: past an end the stick no longer sets a speed, it
                // sets how far the row is stretched, and the row eases there.
                const float end = push < 0.0f ? 0.0f : last();
                const float stretch = end + (reduced() ? 0.0f : kRubber * push);
                wheel_.value += (stretch - wheel_.value) * (1.0f - std::exp(-kRubberRate * dt));
                wheel_.velocity = 0.0f;
            }
            else
            {
                // The wheel is heavy: it takes a moment to reach the stick's speed.
                wheel_.velocity += (push * kScrubSpeed - wheel_.velocity) *
                                   (1.0f - std::exp(-kScrubResponse * dt));
                wheel_.value += wheel_.velocity * dt;
            }
            break;
        }
        case Drive::coast:
            wheel_.velocity *= std::exp(-kFriction * dt);
            wheel_.value += wheel_.velocity * dt;
            if (beyond() != 0.0f)
            {
                // Ran into an end: the spring takes the remaining speed and
                // turns it into the bounce.
                wheel_.velocity = std::clamp(wheel_.velocity, -kMaxEndSpeed, kMaxEndSpeed);
                settle_on(nearest());
            }
            else if (std::fabs(wheel_.velocity) < kSnapSpeed)
            {
                catch_wheel();
            }
            break;
        case Drive::snap:
            wheel_.target = static_cast<float>(target_);
            wheel_.update(dt, reduced() ? 18.0f : kSnapOmega, reduced() ? 1.0f : kSnapDamping);
            break;
        }
        if (reduced())
            wheel_.value = std::clamp(wheel_.value, 0.0f, last());

        // Reaching an end by stick or by momentum is refused once per visit.
        // A press that was refused has had its sound already (quiet_), and the
        // last ripples of the spring around an end are not a new visit.
        const bool at_end = std::fabs(beyond()) > kEndSlack || pushing_at_end();
        if (at_end && !against_end_ && !quiet_)
        {
            const float side = wheel_.value < last() * 0.5f ? -1.0f : 1.0f;
            feedback.play(audio::Cue::error, 1.0f, 0.4f * side, 0.6f);
            feedback.rumble(0.25f, 0.05f);
        }
        against_end_ = at_end;

        const int centre = nearest();
        if (centre != centre_)
        {
            const float direction = centre > centre_ ? 1.0f : -1.0f;
            centre_ = centre;
            apply_palette(false);
            // Detents: one soft tick per cover that passes the centre, faster
            // spins a little higher, and never more than the ear can separate.
            if (!quiet_ && tick_wait_ <= 0.0f)
            {
                const float rate = tween::clamp01(std::fabs(wheel_.velocity) / kScrubSpeed);
                feedback.play(audio::Cue::tick, 0.94f + 0.22f * rate, 0.25f * direction, 0.8f);
                tick_wait_ = kTickInterval;
            }
        }
    }

    // ---- geometry ----

    // Everything about one cover follows from d, its distance from the wheel
    // position: there is no "selected" state to switch, so scrubbing, coasting
    // and stepping all look right for free.
    Pose pose(int index) const
    {
        const float d = static_cast<float>(index) - wheel_.value;
        const float away = std::fabs(d);
        const float side = d < 0.0f ? -1.0f : 1.0f;
        const float soft = std::sqrt(d * d + kSoftAbs * kSoftAbs) - kSoftAbs;
        const float lift = tween::clamp01(lift_.value);

        float offset = side * (kSpreadA * (1.0f - std::exp(-away * kSpreadK)) + kSpreadB * away);
        float scale = kFarScale + (1.0f - kFarScale) * std::exp(-soft * kScaleFall);
        Pose result;
        result.light = kFarLight + (1.0f - kFarLight) * std::exp(-soft * kLightFall);
        result.alpha = tween::clamp01(kReach - away);

        // Entrance: the row closes in from both sides, centre first.
        const int rank = std::min(static_cast<int>(away + 0.5f), static_cast<int>(kReach));
        const float in = tween::stagger(age_, rank, 0.055f, 0.55f);
        result.alpha *= in;
        if (!reduced())
        {
            offset *= 1.0f + 0.3f * (1.0f - in);
            scale *= 0.92f + 0.08f * in;
        }
        // While a cover is open the others make room and fade.
        if (index != opened_)
        {
            offset += side * kOpenSlide * tween::cubic_in_out(lift);
            result.alpha *= 1.0f - lift;
        }

        // Bases rise toward the vanishing height as covers shrink, so the row
        // stands on a floor that recedes instead of hanging from a line.
        const float size = kCover * scale;
        const float base = kVanishY + (kBaseY - kVanishY) * scale;
        result.rect = {kCentreX + offset - size * 0.5f, base - size, size, size};

        if (index == opened_ && lift > 0.0f)
        {
            const float t = tween::cubic_in_out(lift);
            Rect open = kOpenRect;
            if (launch_.running && !reduced())
                open = open.inset(-14.0f * tween::ping(tween::cubic_out(launch_.progress())));
            result.rect = {
                tween::lerp(result.rect.x, open.x, t), tween::lerp(result.rect.y, open.y, t),
                tween::lerp(result.rect.w, open.w, t), tween::lerp(result.rect.h, open.h, t)};
            result.light = tween::lerp(result.light, 1.0f, t);
            // It left the floor: its mirror image goes first.
            result.reflect = 1.0f - tween::clamp01(lift * 3.0f);
        }
        return result;
    }

    // The floor's colour at a height: the reflections are veiled with it.
    Color floor_at(float y) const
    {
        const float t = tween::clamp01((y - kFloorY) / (gfx::kVirtualHeight - kFloorY));
        return gfx::mix(palette_[3].value(), kNight, t);
    }

    // ---- drawing ----

    void draw_floor(gfx::DrawList &list) const
    {
        const Color top = floor_at(kFloorY);
        list.gradient_rect({0, kFloorY - kHorizon, gfx::kVirtualWidth, kHorizon}, 0,
                           top.with_alpha(0.0f), top);
        list.gradient_rect({0, kFloorY, gfx::kVirtualWidth, gfx::kVirtualHeight - kFloorY}, 0, top,
                           floor_at(gfx::kVirtualHeight));
    }

    void draw_header(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = tween::stagger(age_, 0, 0.05f, 0.5f);
        char text[64];
        std::snprintf(text, sizeof(text), "YOUR COLLECTION  \xC2\xB7  %d TITLES", count_);
        ui::text(list, fonts.semibold, text, kCentreX, kHeaderY - 10.0f * (1.0f - in), 18,
                 kWhite.with_alpha(0.6f * in), gfx::Align::center, 4.0f);
    }

    // One cover with its mirror image, shadow, glow and star. `focus` is how
    // much of the centre's light it gets.
    void draw_cover(gfx::DrawList &list, int index, const Pose &p, float focus) const
    {
        if (p.alpha <= 0.01f)
            return;
        const demo::Item &it = item(index);
        const Rect &r = p.rect;
        const float k = r.w / kCover;
        const float radius = kRadius * k;
        list.push_opacity(p.alpha);

        if (p.reflect > 0.01f)
        {
            // The mirror image: the lower part of the artwork with its v range
            // running the other way (a canvas texture's top row is v = 1, so
            // 0 -> part puts the cover's bottom edge at the top). It is drawn
            // opaque and dark so overlapping reflections hide each other like
            // the covers do, then veiled with the floor's own gradient until
            // nothing is left.
            const Rect mirror{r.x, r.y + r.h + 3.0f * k, r.w, r.h * kReflectPart * kReflectSquash};
            const float light = p.light * kReflectLight;
            list.image(it.cover, mirror, {0.0f, 0.0f, 1.0f, kReflectPart},
                       {light, light, light, 1.0f}, radius);
            const Rect veil{mirror.x - 1.0f, mirror.y, mirror.w + 2.0f, mirror.h + 2.0f};
            const float haze = 1.0f - p.reflect * (1.0f - kReflectHaze);
            list.gradient_rect(veil, 0, floor_at(veil.y).with_alpha(haze),
                               floor_at(veil.y + veil.h));
        }

        // The shadow separates overlapping covers and grounds each one.
        const float height = 1.0f - p.reflect; // 0 on the floor, 1 lifted
        list.shadow({r.x, r.y + (14.0f + 16.0f * height) * k, r.w, r.h}, radius,
                    (30.0f + 20.0f * height) * k, kBlack.with_alpha(0.55f));
        if (focus > 0.01f)
        {
            const float breath = reduced() ? 0.5f : ui::breathe(clock_);
            list.glow(r.inset(24.0f), radius, 80.0f,
                      it.accent.with_alpha((0.24f + 0.1f * breath) * focus));
        }
        list.image(it.cover, r, gfx::kCanvasUv, {p.light, p.light, p.light, 1.0f}, radius);
        list.bordered_rect(r, radius, kClear, 1.5f, kWhite.with_alpha(0.16f * p.light));

        if (favorite_[static_cast<std::size_t>(index)])
        {
            // A badge on the corner; it pops when it is given.
            const bool fresh = index == (open_ ? opened_ : current());
            const float pop = fresh ? star_.value : 0.0f;
            const float cx = r.x + r.w - 40.0f * k;
            const float cy = r.y + 40.0f * k;
            list.circle(cx, cy, (24.0f + 6.0f * pop) * k, kInk.with_alpha(0.72f));
            list.star(cx, cy, (14.0f + 9.0f * pop) * k, gfx::mix(kInk, kGold, p.light));
        }
        list.pop_opacity();
    }

    void draw_row(gfx::DrawList &list, float lift) const
    {
        // Far covers first, so nearer ones overlap them.
        struct Entry
        {
            int index;
            float away;
        };
        Entry order[2 * static_cast<int>(kReach) + 3];
        int entries = 0;
        const int first = std::max(0, static_cast<int>(std::floor(wheel_.value - kReach)));
        const int end = std::min(count_ - 1, static_cast<int>(std::ceil(wheel_.value + kReach)));
        for (int i = first; i <= end && entries < static_cast<int>(std::size(order)); ++i)
            order[entries++] = {i, std::fabs(static_cast<float>(i) - wheel_.value)};
        std::sort(order, order + entries,
                  [](const Entry &a, const Entry &b) { return a.away > b.away; });

        // The centre's light and ring wait, like the caption, for a slow wheel.
        const float focus = settled() * calm_.value * (1.0f - lift);
        for (int i = 0; i < entries; ++i)
        {
            const int index = order[i].index;
            if (index == opened_ && lift > 0.01f)
                continue; // drawn above the scrim
            draw_cover(list, index, pose(index), index == centre_ ? focus : 0.0f);
        }

        if (focus <= 0.01f)
            return;
        const Pose centre = pose(centre_);
        const float in = tween::stagger(age_, 0, 0.055f, 0.55f);
        // The focus ring: it belongs to the centre of the row, and appears on
        // whichever cover comes to rest there.
        list.bordered_rect(centre.rect.inset(-7.0f), kRadius + 7.0f, kClear, 3.0f,
                           kWhite.with_alpha(0.95f * focus * in));
    }

    // Five stars filled up to `rating`; the partial one is a clipped full star.
    static void draw_stars(gfx::DrawList &list, float x, float cy, float rating, float radius,
                           float pitch, float alpha)
    {
        for (int i = 0; i < 5; ++i)
        {
            const float cx = x + radius + static_cast<float>(i) * pitch;
            const float fill = tween::clamp01(rating - static_cast<float>(i));
            list.star(cx, cy, radius, kWhite.with_alpha(0.2f * alpha));
            if (fill >= 0.99f)
            {
                list.star(cx, cy, radius, kGold.with_alpha(alpha));
            }
            else if (fill > 0.01f)
            {
                list.push_clip({cx - radius, cy - radius, 2.0f * radius * fill, 2.0f * radius});
                list.star(cx, cy, radius, kGold.with_alpha(alpha));
                list.pop_clip();
            }
        }
    }

    // Title and facts of the centred cover. There is one caption, always for
    // the nearest cover; it is opaque only while the wheel rests, so passing
    // titles fade through instead of flickering.
    void draw_caption(gfx::DrawList &list, float lift) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = tween::stagger(age_, 5, 0.08f, 0.5f);
        const float shown = in * (1.0f - tween::clamp01(lift * 2.5f));
        // During a fast spin a counter takes the caption's place: digits in a
        // monospaced face can change every frame without flickering.
        const float blur = shown * (1.0f - calm_.value);
        if (blur > 0.01f)
        {
            char place[8];
            std::snprintf(place, sizeof(place), "%02d", centre_ + 1);
            ui::text(list, fonts.mono, place, kCentreX, kTitleY, 64, kWhite.with_alpha(0.5f * blur),
                     gfx::Align::center);
            std::snprintf(place, sizeof(place), "OF %d", count_);
            ui::text(list, fonts.semibold, place, kCentreX, kMetaY - 4.0f, 18,
                     kWhite.with_alpha(0.4f * blur), gfx::Align::center, 4.0f);
        }
        const float alpha = shown * settled() * calm_.value;
        if (alpha <= 0.01f)
            return;
        const demo::Item &it = item(centre_);
        // The caption belongs to its cover: it stays under it, so a step
        // carries the old title out one side as the new one arrives from the
        // other, and a row stretched past its end takes its caption along.
        const float x = reduced() ? kCentreX : pose(centre_).rect.cx();
        const float rise = reduced() ? 0.0f : 14.0f * (1.0f - in);
        char text[96];

        list.push_opacity(alpha);
        ui::text(list, fonts.display, fonts.display.font->fit(it.title, 64, 1200), x,
                 kTitleY + rise, 64, kWhite, gfx::Align::center);

        // Genre, year and studio, then the rating, centred as one line.
        std::snprintf(text, sizeof(text), "%s  \xC2\xB7  %d  \xC2\xB7  %s", it.genre, it.year,
                      it.studio);
        char score[8];
        std::snprintf(score, sizeof(score), "%.1f", static_cast<double>(it.rating));
        constexpr float kStar = 11.0f;
        constexpr float kStarPitch = 27.0f;
        const float facts = fonts.regular.measure(text, 26);
        const float stars = 4.0f * kStarPitch + 2.0f * kStar;
        const float total = facts + 36.0f + stars + 14.0f + fonts.semibold.measure(score, 26);
        float cursor = x - total * 0.5f;
        ui::text(list, fonts.regular, text, cursor, kMetaY + rise, 26, kWhite.with_alpha(0.74f));
        cursor += facts + 36.0f;
        draw_stars(list, cursor, kMetaY + rise - 9.0f, it.rating, kStar, kStarPitch, alpha);
        cursor += stars + 14.0f;
        ui::text(list, fonts.semibold, score, cursor, kMetaY + rise, 26, kWhite);
        list.pop_opacity();
    }

    // Where the wheel is within the collection. The thumb is drawn from the
    // same position as the covers, so it shows the coasting and the stretch.
    void draw_scrub_bar(gfx::DrawList &list, float lift) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = tween::stagger(age_, 7, 0.08f, 0.5f);
        list.push_opacity(in * (1.0f - tween::clamp01(lift * 2.5f)));
        list.rounded_rect({kBarX, kBarY - 2.0f, kBarW, 4.0f}, 2.0f, kWhite.with_alpha(0.14f));
        for (int i = 0; i < count_; ++i)
        {
            const float x = kBarX + kBarW * static_cast<float>(i) / last();
            if (favorite_[static_cast<std::size_t>(i)])
                list.circle(x, kBarY, 4.5f, kGold);
            else
                list.circle(x, kBarY, 2.5f, kWhite.with_alpha(0.36f));
        }

        // Past an end the thumb is squeezed against it instead of leaving.
        const float over = beyond();
        const float squeeze = 1.0f - std::min(0.55f, std::fabs(over) * 1.5f);
        const float width = 44.0f * squeeze;
        const float at = kBarX + kBarW * tween::clamp01(wheel_.value / last());
        float left = at - width * 0.5f;
        if (over < 0.0f)
            left = at - 22.0f;
        else if (over > 0.0f)
            left = at + 22.0f - width;
        const Rect thumb{left, kBarY - 8.0f, width, 16.0f};
        const Color accent = palette_[4].value();
        list.glow(thumb, 8.0f, 16.0f, accent.with_alpha(0.45f));
        list.rounded_rect(thumb, 8.0f, gfx::mix(accent, kWhite, 0.35f));

        // The place in numbers; during a fast spin the large counter says it.
        char text[8];
        std::snprintf(text, sizeof(text), "%02d", centre_ + 1);
        ui::text(list, fonts.mono, text, kBarX - 30.0f, kBarY + 8.0f, 22,
                 kWhite.with_alpha(0.9f * calm_.value), gfx::Align::right);
        std::snprintf(text, sizeof(text), "%02d", count_);
        ui::text(list, fonts.mono, text, kBarX + kBarW + 30.0f, kBarY + 8.0f, 22,
                 kWhite.with_alpha(0.5f * calm_.value));
        list.pop_opacity();
    }

    void draw_details(gfx::DrawList &list, std::uint32_t glass, float lift) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const demo::Item &it = item(opened_);
        char text[96];

        // The panel waits until the cover has moved out of its way, and on
        // the way back it is gone before the cover returns.
        const float appear_panel = tween::clamp01((lift - 0.45f) / 0.5f);
        Rect panel = kPanel;
        if (!reduced())
            panel.x += 80.0f * (1.0f - tween::cubic_out(appear_panel));
        list.push_opacity(appear_panel);
        list.shadow({panel.x, panel.y + 22, panel.w, panel.h}, kPanelRadius, 60,
                    kBlack.with_alpha(0.5f));
        // Frosted panel: the blurred screen, a tint in the cover's own dark
        // tone, then a hairline of light.
        list.glass(glass, panel, kPanelRadius, kWhite);
        const Color tint = gfx::mix(gfx::mix(it.dark, it.mid, 0.18f), kInk, 0.35f);
        list.rounded_rect(panel, kPanelRadius, tint.with_alpha(0.64f));
        list.bordered_rect(panel, kPanelRadius, kClear, 1.5f, kWhite.with_alpha(0.2f));

        const float x = panel.x + kPanelPad;
        const float width = panel.w - 2.0f * kPanelPad;
        ui::text(list, fonts.semibold, ui::upper(it.genre), x, panel.y + 84, 20, it.accent,
                 gfx::Align::left, 4.0f);
        ui::text(list, fonts.display, fonts.display.font->fit(it.title, 60, width), x - 2,
                 panel.y + 152, 60, kWhite);
        std::snprintf(text, sizeof(text), "%d  \xC2\xB7  %s", it.year, it.studio);
        ui::text(list, fonts.regular, text, x, panel.y + 198, 26, kWhite.with_alpha(0.72f));
        ui::paragraph(list, fonts.regular, it.blurb, x, panel.y + 254, 26, width, 38,
                      kWhite.with_alpha(0.88f), 2);

        // Four facts, arriving one after the other once the panel is there.
        constexpr float kTileGap = 16.0f;
        const float tile_w = (width - 3.0f * kTileGap) / 4.0f;
        const char *labels[] = {"RATING", "PLAYED", "PLAYERS", "STORY"};
        for (int i = 0; i < 4; ++i)
        {
            const float appear = tween::stagger(appear_panel, i, 0.1f, 0.6f);
            const Rect tile{x + static_cast<float>(i) * (tile_w + kTileGap),
                            panel.y + 334 + (reduced() ? 0.0f : 22.0f * (1.0f - appear)), tile_w,
                            116};
            list.push_opacity(appear);
            list.rounded_rect(tile, 22, kWhite.with_alpha(0.08f));
            ui::text(list, fonts.semibold, labels[i], tile.x + 22, tile.y + 36, 15,
                     kWhite.with_alpha(0.55f), gfx::Align::left, 3.0f);
            switch (i)
            {
            case 0:
                std::snprintf(text, sizeof(text), "%.1f", static_cast<double>(it.rating));
                draw_stars(list, tile.x + 96, tile.y + 73, it.rating, 8.0f, 19.0f, appear);
                break;
            case 1:
                if (it.hours > 0)
                    std::snprintf(text, sizeof(text), "%d h", it.hours);
                else
                    std::snprintf(text, sizeof(text), "New");
                break;
            case 2:
                if (it.players > 1)
                    std::snprintf(text, sizeof(text), "1\xE2\x80\x93%d", it.players);
                else
                    std::snprintf(text, sizeof(text), "1");
                break;
            default:
            {
                std::snprintf(text, sizeof(text), "%d%%",
                              static_cast<int>(it.progress * 100.0f + 0.5f));
                const Rect bar{tile.x + 22, tile.y + 98, tile.w - 44, 4};
                list.rounded_rect(bar, 2, kWhite.with_alpha(0.16f));
                if (it.progress > 0.0f)
                    list.rounded_rect(
                        {bar.x, bar.y, std::max(4.0f, bar.w * it.progress * appear), 4}, 2,
                        it.accent);
                break;
            }
            }
            ui::text(list, fonts.semibold, text, tile.x + 22, tile.y + 86, 36, kWhite);
            list.pop_opacity();
        }

        // The one focused thing on the panel: the Play button, lit in the
        // cover's accent. It swells when pressed.
        const float press = launch_.running ? tween::ping(launch_.progress()) : 0.0f;
        const float breath = reduced() ? 0.5f : ui::breathe(clock_);
        const Rect play = Rect{x, panel.y + 478, 250, 68}.inset(reduced() ? 0.0f : -4.0f * press);
        list.glow(play, 34, 22, it.accent.with_alpha(0.3f + 0.2f * breath + 0.4f * press));
        list.rounded_rect(play, 34, kWhite);
        ui::draw_button(list, fonts, ui::GlyphStyle::light(), ui::Button::cross, play.x + 24,
                        play.cy(), 34);
        const char *verb =
            launch_.running ? "Starting \xE2\x80\xA6" : (it.progress > 0.0f ? "Resume" : "Play");
        ui::text(list, fonts.semibold, verb, play.x + 74, play.cy() + 10, 28, kInk);

        // Beside it, the favourite state: not focusable, Triangle toggles it.
        const bool starred = favorite_[static_cast<std::size_t>(opened_)];
        const float chip_x = x + 250 + 28;
        const float chip_cy = panel.y + 512;
        list.star(chip_x + 16, chip_cy, 15.0f + 8.0f * star_.value,
                  starred ? kGold : kWhite.with_alpha(0.7f), starred ? 0.0f : 2.5f);
        ui::text(list, fonts.regular, starred ? "In your favorites" : "Not a favorite yet",
                 chip_x + 44, chip_cy + 9, 24, kWhite.with_alpha(starred ? 0.9f : 0.6f));
        list.pop_opacity();
    }

    void draw_hints(gfx::DrawList &list, float lift) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const ui::GlyphStyle style = ui::GlyphStyle::dark();
        if (open_)
        {
            const ui::Hint hints[] = {{ui::Button::cross, "Play"},
                                      {ui::Button::triangle, "Favorite"},
                                      {ui::Button::circle, "Back"}};
            list.push_opacity(tween::clamp01(lift * 2.0f));
            ui::draw_hints(list, fonts, style, hints, 3, 1824, true);
            list.pop_opacity();
        }
        else
        {
            const ui::Hint hints[] = {{ui::Button::left_stick, "Spin"},
                                      {ui::Button::dpad, "Step"},
                                      {ui::Button::l2, "Jump", ui::Button::r2},
                                      {ui::Button::cross, "Open"},
                                      {ui::Button::triangle, "Favorite"}};
            list.push_opacity(tween::stagger(age_, 9, 0.08f, 0.5f) * (1.0f - lift));
            ui::draw_hints(list, fonts, style, hints, 5, 1824, true);
            list.pop_opacity();
        }
    }

    app::Context &context_;
    int count_;
    std::vector<bool> favorite_;
    float age_ = 0.0f;   // seconds since enter(): drives the entrance
    float clock_ = 0.0f; // free-running time for idle motion

    // The wheel. value is the position in covers (3.5 is half-way between the
    // fourth and fifth), velocity is covers per second. In Drive::snap it is
    // a spring toward target_; otherwise update() moves it directly.
    tween::Bounce wheel_;
    Drive drive_ = Drive::snap;
    int target_ = 0;
    float stick_ = 0.0f;       // the stick's deflection while scrubbing
    int centre_ = 0;           // the cover nearest the position, as last announced
    bool quiet_ = true;        // the current movement already had its sound
    bool against_end_ = false; // the wheel is past an end (refuse once)
    float tick_wait_ = 0.0f;   // seconds until the next tick may play

    tween::Spring calm_; // 1 while the wheel is slow enough to read a caption
    ui::SpringColor palette_[5];
    ui::Pulse star_;

    bool open_ = false;
    int opened_ = 0;     // the cover the details belong to
    tween::Spring lift_; // 0 in the row, 1 lifted beside the panel
    tween::Timer launch_;
};

} // namespace

std::unique_ptr<app::Concept> make_carousel(app::Context &context)
{
    return std::make_unique<CoverFlow>(context);
}

} // namespace hui::concepts
