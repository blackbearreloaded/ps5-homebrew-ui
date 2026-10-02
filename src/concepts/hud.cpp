// ps5-homebrew-ui - Design "Field HUD": an in-game HUD and its pause menu.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The screen a player looks at for most of a game: vitals, a compass, the
// current objective, four abilities and a feed of events, laid over a moving
// world, plus the pause menu that takes over when Options is pressed. The
// face buttons stand in for the game: Cross takes a hit, Square heals,
// Triangle advances the objective. What makes it feel finished:
//
//   - the health bar snaps to its new value and leaves a pale "ghost" of the
//     lost part behind, which drains a moment later on a slower spring, so the
//     eye reads how hard the hit was; healing fills on a spring with a green
//     flash, and low health makes the bar pulse under a breathing red vignette;
//   - a hit is felt everywhere at once: the number pops and drifts up, the
//     whole HUD group shakes with a decaying transform, the controller rumbles;
//   - ability tiles show their cooldown as a pie that sweeps away (an arc over
//     the tile) with the seconds in a monospaced face; a tile on cooldown
//     refuses softly instead of ignoring the press;
//   - an objective line ticks itself: the check mark is two lines that grow,
//     the text is struck through, and a finished card flashes and hands over
//     to the next objective with a cross-fade;
//   - gameplay runs on its own clock. Pausing stops that clock (cooldowns,
//     compass, toasts, the world), blurs the world into glass and keeps the
//     HUD readable at 30 % behind a menu with one gliding highlight.

#include "concepts/concepts.hpp"

#include "core/tween.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace hui::concepts
{

namespace
{

using gfx::Color;
using gfx::Rect;

// ---- the design language ----------------------------------------------------

const Color kInk = Color::rgb(0xf4f6fb);
const Color kNight = Color::rgb(0x070b12); // scrims, tiles, text on the accent
const Color kGlassTint = Color::rgb(0x0e1726);
const Color kAccent = Color::rgb(0xffc14d); // the one highlight colour: amber
const Color kHealth = Color::rgb(0xee4b44);
const Color kHealthLight = Color::rgb(0xff9580);
const Color kGhost = Color::rgb(0xffe9cf); // the part of the bar just lost
const Color kHeal = Color::rgb(0x7df0a2);
const Color kStamina = Color::rgb(0x86d4ff);
const Color kDanger = Color::rgb(0xc20f1e); // the low-health vignette
const Color kClear = Color::rgb(0x000000, 0.0f);

constexpr float kTau = 6.2831853f;
constexpr float kMargin = 96.0f; // safe area, left and right
constexpr float kTop = 64.0f;    // top edge of the HUD's upper row
constexpr float kRight = gfx::kVirtualWidth - kMargin;

// Vitals (top-left).
constexpr float kBarX = 188.0f;
constexpr float kHealthY = 90.0f;
constexpr float kHealthW = 340.0f;
constexpr float kHealthH = 22.0f;
constexpr float kStaminaY = 124.0f;
constexpr float kStaminaW = 260.0f;
constexpr float kStaminaH = 10.0f;
constexpr int kMaxHealth = 100;
constexpr int kLowHealth = 25; // below this the bar pulses
constexpr int kHealAmount = 25;
constexpr float kGhostHold = 0.4f;   // seconds the ghost waits before draining
constexpr float kDownSeconds = 2.0f; // the "DOWN" state before the revive

// Compass (top-centre).
constexpr float kCompassW = 520.0f;
constexpr float kCompassH = 52.0f;
constexpr float kPixelsPerDegree = 4.4f;

// Objective card (top-right).
constexpr float kCardW = 420.0f;
constexpr float kCardH = 224.0f;
constexpr int kLines = 3;
constexpr float kNextObjectiveDelay = 1.3f;

// Ability tiles (bottom-right).
constexpr int kAbilityCount = 4;
constexpr float kTile = 96.0f;
constexpr float kTileGap = 16.0f;
constexpr float kTileY = 824.0f;
constexpr float kBadgeY = kTileY + kTile + 22.0f; // the button glyph under each tile
constexpr float kTileRadius = 22.0f;

// Toast feed (bottom-left).
constexpr int kMaxToasts = 4;
constexpr float kToastH = 56.0f;
constexpr float kToastPitch = 66.0f;
constexpr float kToastBottom = 962.0f;
constexpr float kToastLife = 4.2f;
constexpr float kAmbientEvery = 6.5f;

// Floating numbers (centre).
constexpr int kMaxFloaters = 8;
constexpr float kFloaterLife = 0.95f;

// Pause menu.
constexpr int kMenuItems = 5;
constexpr float kPanelY = 172.0f; // below the HUD's top row, which stays visible
constexpr float kPanelW = 480.0f;
constexpr float kPanelH = 764.0f;
constexpr float kPanelRadius = 36.0f;
constexpr float kMenuY = kPanelY + 214.0f;
constexpr float kRowH = 68.0f;
constexpr float kRowPitch = 78.0f;
constexpr float kSessionW = 460.0f;
constexpr float kSessionH = 392.0f;
const Rect kDialog{600.0f, 366.0f, 720.0f, 348.0f};

constexpr const char *kTechniques[] = {
    "Health bar with a delayed damage ghost: one value snaps, a slower spring drains behind it",
    "Radial cooldown sweeps (DrawList::arc as a pie) with monospaced seconds and a soft refusal",
    "A check mark that draws itself, a strike-through and a card that hands over by cross-fade",
    "Hit feedback in layers: popping numbers, a decaying HUD shake, a vignette, rumble",
    "A separate gameplay clock: pausing freezes cooldowns, compass, toasts and the world",
    "Pause menu on frosted glass over the blurred world, the HUD kept readable at 30 %",
};

constexpr app::TourStep kTour[] = {
    // A flurry that takes the player down, the revive, then a few more hits.
    {0.5f, action_bit(Action::confirm)},
    {0.25f, action_bit(Action::confirm)},
    {0.25f, action_bit(Action::confirm)},
    {0.25f, action_bit(Action::confirm)},
    {0.25f, action_bit(Action::confirm)},
    {0.25f, action_bit(Action::confirm)},
    {0.25f, action_bit(Action::confirm)},
    {0.25f, action_bit(Action::confirm)},
    {0.7f, 0, Direction::none, "down"},
    {2.4f, action_bit(Action::confirm)},
    {0.3f, action_bit(Action::confirm)},
    {0.3f, action_bit(Action::confirm)},
    {0.3f, action_bit(Action::confirm)},
    {0.3f, action_bit(Action::confirm)},
    {0.3f, action_bit(Action::confirm)},
    {0.22f, 0, Direction::none, "hit"},
    // Heal, work through the objective, fire two abilities; the picture
    // catches the last tick, the card's flash and both cooldown sweeps.
    {0.9f, action_bit(Action::west)},
    {0.5f, action_bit(Action::north)},
    {0.5f, action_bit(Action::north)},
    {0.4f, action_bit(Action::back)},
    {0.3f, 0, Direction::up},
    {0.4f, action_bit(Action::north)},
    {0.3f, 0, Direction::none, "abilities"},
    // The pause menu, then its confirmation; both are backed out of.
    {3.0f, action_bit(Action::menu)},
    {0.5f, 0, Direction::down},
    {0.6f, 0, Direction::down, "pause"},
    {0.2f, 0, Direction::down},
    {0.2f, 0, Direction::down},
    {0.3f, action_bit(Action::confirm)},
    {0.8f, action_bit(Action::back), Direction::none, "quit"},
    {0.5f, action_bit(Action::back)},
};

struct Objective
{
    const char *title;
    const char *lines[kLines];
    float bearing; // degrees: where the compass marker sits
};
constexpr Objective kObjectives[] = {
    {"Reach the Signal Tower",
     {"Cross the rope bridge", "Scout the ridge camp", "Light the beacon"},
     38.0f},
    {"The Ferryman's Favour",
     {"Find the lost oar", "Speak with the ferryman", "Cross the Greywater"},
     286.0f},
    {"Echoes Below",
     {"Enter the old mine", "Collect three ember shards", "Return to the camp"},
     164.0f},
};
constexpr int kObjectiveCount = static_cast<int>(sizeof(kObjectives) / sizeof(kObjectives[0]));

struct Ability
{
    const char *name;
    float cooldown; // seconds
    audio::Cue cue;
};
// In D-pad order: up, right, down, left. The same order as the face buttons
// Triangle, Circle, Cross, Square that the tiles show.
constexpr Ability kAbilities[kAbilityCount] = {
    {"Dash", 3.0f, audio::Cue::slide},
    {"Flare", 6.0f, audio::Cue::spawn},
    {"Ward", 10.0f, audio::Cue::connect},
    {"Volley", 14.0f, audio::Cue::cascade},
};

enum class ToastKind : std::uint8_t
{
    pickup,
    area,
    achievement,
    revive,
};

struct Ambient
{
    ToastKind kind;
    const char *text;
};
// Things that "happen" in the world while the player watches.
constexpr Ambient kAmbient[] = {
    {ToastKind::pickup, "Picked up Rope \xC3\x97"
                        "2"},
    {ToastKind::area, "Area discovered: Hollow Reach"},
    {ToastKind::pickup, "Picked up Ember Shard"},
    {ToastKind::area, "Waystone attuned: Greywater Ford"},
    {ToastKind::pickup, "Picked up Field Tonic \xC3\x97"
                        "1"},
};
constexpr int kAmbientCount = static_cast<int>(sizeof(kAmbient) / sizeof(kAmbient[0]));

struct MenuItem
{
    const char *label;
    const char *meta;  // small figure at the right of the row
    const char *about; // shown under the list while the row is focused
};
constexpr MenuItem kMenu[kMenuItems] = {
    {"Resume", "", "Back to the field. Everything is where you left it."},
    {"Inventory", "14/30", "Rope, field tonics and ember shards. 14 of 30 slots used."},
    {"Map", "3/7", "Hollow Reach and its roads. 3 of 7 areas discovered."},
    {"Settings", "", "Sound, display and controls."},
    {"Quit to title", "", "Progress since the last waystone will be lost."},
};
constexpr const char *kNotInDemo = "Not part of this demo: the pause menu itself is the exhibit.";

// A signed angle difference in -180..180 degrees.
float wrap_degrees(float degrees)
{
    degrees = std::fmod(degrees + 180.0f, 360.0f);
    if (degrees < 0.0f)
        degrees += 360.0f;
    return degrees - 180.0f;
}

Rect tile_rect(int index)
{
    const float width = kAbilityCount * kTile + (kAbilityCount - 1) * kTileGap;
    return {kRight - width + static_cast<float>(index) * (kTile + kTileGap), kTileY, kTile, kTile};
}

// Rows of the pause menu, in the panel's resting position.
Rect row_rect(int index)
{
    return {kMargin + 20.0f, kMenuY + static_cast<float>(index) * kRowPitch, kPanelW - 40.0f,
            kRowH};
}

Rect dialog_button(int index)
{
    return {kDialog.x + 40.0f + static_cast<float>(index) * 340.0f,
            kDialog.y + kDialog.h - 40.0f - 68.0f, 300.0f, 68.0f};
}

class Hud final : public app::Concept
{
  public:
    explicit Hud(app::Context &context) : context_(context)
    {
        reset_session();
        highlight_.snap(row_rect(0));
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "hud",
            "Field HUD",
            "An in-game HUD over a moving world, and the pause menu behind Options",
            "src/concepts/hud.cpp",
            audio::SoundSet::paper,
            kAccent,
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        age_ = 0.0f;
        paused_ = false;
        dialog_ = false;
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        age_ += dt;
        clock_ += dt;
        session_time_ += dt;
        const bool reduced = context_.settings.reduced_motion;

        float sprint = 0.0f; // how far R2 is pulled
        float turn = 0.0f;
        if (dialog_)
        {
            update_dialog(input, feedback);
        }
        else if (paused_)
        {
            update_pause(input, feedback);
        }
        else
        {
            update_play(input, feedback);
            sprint = std::max(input.trigger_r, input.is_held(Action::jump_next) ? 1.0f : 0.0f);
            turn = input.stick_x;
        }

        // Gameplay has its own clock. While the menu is up it receives no
        // time at all, so nothing in the world can move behind the glass.
        advance_game(paused_ ? 0.0f : dt, sprint, turn, feedback);

        // ---- interface animation: always on real time ----
        pause_.target = paused_ ? 1.0f : 0.0f;
        pause_.update(dt, reduced ? 40.0f : 13.0f);
        if (paused_)
            pause_age_ += dt;
        highlight_.target(row_rect(item_));
        highlight_.update(dt, 20.0f);
        menu_nudge_.update(dt, 9.0f);
        dialog_value_.target = dialog_ ? 1.0f : 0.0f;
        dialog_value_.update(dt, reduced ? 40.0f : 16.0f);
        dialog_position_.target = static_cast<float>(dialog_focus_);
        dialog_position_.update(dt, 22.0f);
        const int about = item_ * 2 + (note_ ? 1 : 0);
        if (about != about_)
        {
            about_previous_ = about_;
            about_ = about;
            about_fade_.start(0.28f);
        }
        about_fade_.update(dt);
        curtain_.update(dt, 2.6f);
    }

    void draw(app::Frame &frame) const override
    {
        // The stand-in game world. It scrolls on the gameplay clock, so it
        // stops under the pause menu and runs faster during a sprint.
        frame.backdrop.mode = gfx::BackdropMode::vista;
        frame.backdrop.colors[0] = Color::rgb(0x1f4a7a);
        frame.backdrop.colors[1] = Color::rgb(0xf2b680);
        frame.backdrop.colors[2] = Color::rgb(0x4b5f8c);
        frame.backdrop.colors[3] = Color::rgb(0x101927);
        frame.backdrop.params[0] = 1.0f;
        frame.backdrop.time = world_time_;

        // Two edge scrims belong to the world layer: they give the HUD's
        // light text a floor to stand on whatever the sky is doing.
        gfx::DrawList &scene = frame.scene;
        scene.gradient_rect({0, 0, gfx::kVirtualWidth, 300}, 0, kNight.with_alpha(0.5f), kClear);
        scene.gradient_rect({0, 760, gfx::kVirtualWidth, 320}, 0, kClear, kNight.with_alpha(0.62f));

        const float pause = pause_.value;
        const bool veiled = pause > 0.01f;
        if (veiled)
        {
            // The whole world goes out of focus: the blurred copy of the
            // frame is painted over it, then dimmed. The HUD moves into the
            // overlay for as long as the veil is up, so it stays sharp (and
            // out of the blur) while it fades back to 30 %.
            frame.glass = true;
            const Rect screen{0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight};
            frame.overlay.glass(frame.glass_texture, screen, 0, kInk.with_alpha(pause));
            frame.overlay.rounded_rect(screen, 0, kNight.with_alpha(0.38f * pause));
        }
        draw_hud(veiled ? frame.overlay : frame.scene, tween::lerp(1.0f, 0.3f, pause));
        if (veiled)
        {
            draw_pause(frame.overlay, frame.glass_texture);
            draw_session(frame.overlay, frame.glass_texture);
            if (dialog_value_.value > 0.01f)
                draw_dialog(frame.overlay, frame.glass_texture);
        }
        draw_hints(frame);

        // "Quit to title" drops a curtain that lifts on the fresh session.
        if (curtain_.value > 0.0f)
            frame.overlay.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                                       kNight.with_alpha(std::min(1.0f, curtain_.value * 1.4f)));

        // Danger is a property of the whole picture, not of one widget: a
        // red vignette that breathes while health is low, kicks on every hit
        // and closes in while the player is down.
        const float beat = context_.settings.reduced_motion ? 0.5f : ui::breathe(game_time_, 1.1f);
        const float strength =
            (danger_.value * (0.3f + 0.22f * beat) + hit_.value * 0.3f + down_.value * 0.42f) *
            (1.0f - 0.6f * pause);
        if (strength > 0.01f)
        {
            frame.post.mode = gfx::BackdropMode::vignette;
            frame.post.colors[0] = kDanger;
            frame.post.params[0] = std::min(0.95f, strength);
        }
    }

    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    struct Toast
    {
        bool alive = false;
        ToastKind kind = ToastKind::pickup;
        char text[80] = {};
        float age = 0.0f;
        std::uint32_t serial = 0; // newer toasts have higher numbers
        tween::Spring slot;       // 0 is the bottom of the stack
    };

    struct Floater
    {
        bool alive = false;
        bool heal = false;
        int amount = 0;
        float age = 0.0f;
        float x = 0.0f;
        float y = 0.0f;
    };

    // ---- rules --------------------------------------------------------------

    std::uint32_t random()
    {
        rng_ = rng_ * 1664525u + 1013904223u;
        return rng_ >> 8;
    }

    bool is_down() const
    {
        return down_left_ > 0.0f;
    }

    // The glyph a logical action wears: Cross and Circle trade places when
    // the player swapped confirm and back in the settings.
    ui::Button confirm_glyph() const
    {
        return context_.settings.swap_confirm ? ui::Button::circle : ui::Button::cross;
    }
    ui::Button back_glyph() const
    {
        return context_.settings.swap_confirm ? ui::Button::cross : ui::Button::circle;
    }
    ui::Button ability_glyph(int index) const
    {
        const ui::Button glyphs[kAbilityCount] = {ui::Button::triangle, back_glyph(),
                                                  confirm_glyph(), ui::Button::square};
        return glyphs[index];
    }

    void reset_session()
    {
        health_ = kMaxHealth;
        fill_.snap(1.0f);
        ghost_.snap(1.0f);
        ghost_hold_ = 0.0f;
        stamina_ = 1.0f;
        stamina_rest_ = 0.0f;
        exhausted_ = false;
        speed_.snap(1.0f);
        down_left_ = 0.0f;
        heading_ = 12.0f;
        objective_ = previous_objective_ = 0;
        ticked_ = 0;
        next_delay_ = 0.0f;
        swap_ = tween::Timer{};
        for (float &tick : tick_)
            tick = 0.0f;
        for (float &cooldown : cooldown_)
            cooldown = 0.0f;
        for (Toast &toast : toasts_)
            toast.alive = false;
        for (Floater &floater : floaters_)
            floater.alive = false;
        ambient_timer_ = 0.7f;
        ambient_next_ = 0;
        stat_objectives_ = stat_damage_ = stat_abilities_ = stat_downs_ = 0;
        session_time_ = 0.0f;
        xp_.snap(0.42f);
    }

    void refuse_while_down(app::Feedback &feedback)
    {
        feedback.play(audio::Cue::invalid, 0.9f, 0.0f, 0.4f);
        down_nudge_.trigger();
    }

    void update_play(const InputFrame &input, app::Feedback &feedback)
    {
        // Options pauses. So does losing the controller: a game must not run
        // on while nobody can steer it.
        if (input.is_pressed(Action::menu) || input.focus_lost)
        {
            paused_ = true;
            pause_age_ = 0.0f;
            item_ = 0;
            note_ = false;
            highlight_.snap(row_rect(0));
            feedback.play(audio::Cue::modal_open);
            return;
        }

        // Abilities sit on the D-pad. `nav` also carries the left stick, which
        // steers here, so a direction that came from the stick is not a press.
        constexpr std::uint32_t kDpad = action_bit(Action::up) | action_bit(Action::down) |
                                        action_bit(Action::left) | action_bit(Action::right);
        const bool tilted = std::fabs(input.stick_x) > 0.3f || std::fabs(input.stick_y) > 0.3f;
        const bool from_stick = ((input.held | input.pressed) & kDpad) == 0 && tilted;
        if (input.nav != Direction::none && !from_stick)
        {
            const int index = input.nav == Direction::up      ? 0
                              : input.nav == Direction::right ? 1
                              : input.nav == Direction::down  ? 2
                                                              : 3;
            trigger_ability(index, !input.nav_repeat, feedback);
        }

        // Holding L2 turns the face buttons into the four tiles, the way an
        // action game's ability palette works; without it they drive the demo.
        const bool palette = input.is_held(Action::jump_prev) || input.trigger_l > 0.5f;
        palette_.target = palette ? 1.0f : 0.0f;
        if (palette)
        {
            if (input.is_pressed(Action::north))
                trigger_ability(0, true, feedback);
            if (input.is_pressed(Action::back))
                trigger_ability(1, true, feedback);
            if (input.is_pressed(Action::confirm))
                trigger_ability(2, true, feedback);
            if (input.is_pressed(Action::west))
                trigger_ability(3, true, feedback);
            return;
        }
        if (input.is_pressed(Action::confirm))
            take_damage(feedback);
        if (input.is_pressed(Action::west))
            heal(feedback);
        if (input.is_pressed(Action::north))
            advance_objective(feedback);
        if (input.is_pressed(Action::back))
            trigger_ability(1, true, feedback);
    }

    void take_damage(app::Feedback &feedback)
    {
        if (is_down())
        {
            refuse_while_down(feedback);
            return;
        }
        const int amount = std::min(health_, 8 + static_cast<int>(random() % 13u));
        // The ghost keeps showing what the bar showed before the hit, and
        // every further hit restarts its wait: a flurry reads as one loss.
        ghost_.snap(std::max(ghost_.value, fill_.value));
        ghost_hold_ = kGhostHold;
        health_ -= amount;
        fill_.snap(static_cast<float>(health_) / kMaxHealth);
        stat_damage_ += amount;
        spawn_floater(amount, false);
        hit_.trigger();
        if (!context_.settings.reduced_motion)
            shake_.trigger();
        if (health_ == 0)
        {
            down_left_ = kDownSeconds;
            ++stat_downs_;
            feedback.play(audio::Cue::game_over, 1.0f, 0.0f, 0.8f);
            feedback.rumble(0.8f, 0.25f);
        }
        else
        {
            // Hits sound heavier as health runs out.
            feedback.play(audio::Cue::explode, 0.95f + 0.3f * fill_.value, 0.0f, 0.4f);
            feedback.rumble(0.45f, 0.09f);
        }
    }

    void heal(app::Feedback &feedback)
    {
        if (is_down())
        {
            refuse_while_down(feedback);
            return;
        }
        if (health_ >= kMaxHealth)
        {
            feedback.play(audio::Cue::invalid, 1.0f, ui::pan_for_x(kBarX), 0.5f);
            feedback.rumble(0.25f, 0.05f);
            vitals_nudge_.trigger();
            return;
        }
        const int amount = std::min(kHealAmount, kMaxHealth - health_);
        health_ += amount;
        fill_.target = static_cast<float>(health_) / kMaxHealth;
        heal_.trigger();
        spawn_floater(amount, true);
        // The cue rises with the bar it fills.
        feedback.play(audio::Cue::reveal, 0.9f + 0.3f * fill_.target, ui::pan_for_x(kBarX + 170));
    }

    void advance_objective(app::Feedback &feedback)
    {
        if (is_down())
        {
            refuse_while_down(feedback);
            return;
        }
        const float pan = ui::pan_for_x(kRight - kCardW * 0.5f);
        if (ticked_ >= kLines)
        {
            // The finished card is only waiting to hand over: skip the wait.
            if (next_delay_ > 0.0f)
            {
                next_objective();
                feedback.play(audio::Cue::slide, 1.0f, pan);
            }
            return;
        }
        tick_[ticked_] = 0.0f;
        ++ticked_;
        if (ticked_ < kLines)
        {
            // Each line of a card ticks a step higher than the one before.
            feedback.play(audio::Cue::mark, 0.94f + 0.08f * static_cast<float>(ticked_), pan);
            return;
        }
        ++stat_objectives_;
        next_delay_ = kNextObjectiveDelay;
        card_flash_.trigger();
        xp_.target = std::min(1.0f, xp_.target + 0.2f);
        feedback.play(audio::Cue::complete, 1.0f, pan * 0.5f);
        feedback.rumble(0.4f, 0.12f);
        char text[80];
        std::snprintf(text, sizeof(text), "Objective complete: %s", kObjectives[objective_].title);
        post_toast(ToastKind::achievement, text, nullptr);
    }

    void next_objective()
    {
        previous_objective_ = objective_;
        objective_ = (objective_ + 1) % kObjectiveCount;
        ticked_ = 0;
        next_delay_ = 0.0f;
        for (float &tick : tick_)
            tick = 0.0f;
        swap_.start(context_.settings.reduced_motion ? 0.2f : 0.6f);
    }

    // `fresh` is false for a held D-pad direction: repeats never complain.
    void trigger_ability(int index, bool fresh, app::Feedback &feedback)
    {
        const float pan = ui::pan_for_x(tile_rect(index).cx());
        if (is_down() || cooldown_[index] > 0.0f)
        {
            if (fresh)
            {
                feedback.play(audio::Cue::invalid, 1.0f, pan, 0.6f);
                feedback.rumble(0.25f, 0.05f);
                ability_refusal_[index].trigger();
            }
            return;
        }
        cooldown_[index] = kAbilities[index].cooldown;
        ability_flash_[index].trigger();
        ++stat_abilities_;
        feedback.play(kAbilities[index].cue, 1.0f, pan);
        feedback.rumble(0.3f, 0.06f);
        if (index == 0)
            speed_.value += 3.0f; // Dash: the world lurches forward and eases back
    }

    void update_pause(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.is_pressed(Action::back) || input.is_pressed(Action::menu))
        {
            paused_ = false;
            feedback.play(audio::Cue::modal_close);
            return;
        }
        const float pan = ui::pan_for_x(kMargin + kPanelW * 0.5f);
        if (input.nav == Direction::up || input.nav == Direction::down)
        {
            const int next = item_ + (input.nav == Direction::down ? 1 : -1);
            if (next >= 0 && next < kMenuItems)
            {
                item_ = next;
                note_ = false;
                // Rows lower in the list sound a little lower.
                feedback.play(audio::Cue::focus, 1.08f - 0.04f * static_cast<float>(item_), pan);
            }
            else if (!input.nav_repeat)
            {
                feedback.play(audio::Cue::error, 1.0f, pan, 0.6f);
                feedback.rumble(0.25f, 0.05f);
                menu_nudge_.trigger();
                nudge_vertical_ = true;
            }
        }
        if (!input.is_pressed(Action::confirm))
            return;
        if (item_ == 0)
        {
            paused_ = false;
            feedback.play(audio::Cue::modal_close);
        }
        else if (item_ == kMenuItems - 1)
        {
            // Destructive: ask first, and put the focus on the safe answer.
            dialog_ = true;
            dialog_focus_ = 0;
            dialog_position_.snap(0.0f);
            feedback.play(audio::Cue::modal_open, 0.92f);
        }
        else
        {
            // Screens this demo does not have: refuse, and say why.
            note_ = true;
            feedback.play(audio::Cue::error, 1.0f, pan, 0.6f);
            feedback.rumble(0.25f, 0.05f);
            menu_nudge_.trigger();
            nudge_vertical_ = false;
        }
    }

    void update_dialog(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav == Direction::left || input.nav == Direction::right)
        {
            const int next = dialog_focus_ + (input.nav == Direction::right ? 1 : -1);
            if (next >= 0 && next <= 1)
            {
                dialog_focus_ = next;
                feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(dialog_button(next).cx()));
            }
            else if (!input.nav_repeat)
            {
                feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
                feedback.rumble(0.25f, 0.05f);
                menu_nudge_.trigger();
                nudge_vertical_ = false;
            }
        }
        if (input.is_pressed(Action::back) || input.is_pressed(Action::menu))
        {
            dialog_ = false;
            feedback.play(audio::Cue::back);
        }
        else if (input.is_pressed(Action::confirm))
        {
            dialog_ = false;
            if (dialog_focus_ == 0)
            {
                feedback.play(audio::Cue::modal_close);
                return;
            }
            // There is no title screen here: quitting starts a fresh session
            // behind a curtain, and the HUD assembles again.
            reset_session();
            paused_ = false;
            age_ = 0.0f;
            curtain_.trigger();
            feedback.play(audio::Cue::restart);
            feedback.rumble(0.5f, 0.12f);
        }
    }

    void spawn_floater(int amount, bool heal)
    {
        Floater *slot = &floaters_[0];
        for (Floater &floater : floaters_)
        {
            if (!floater.alive)
            {
                slot = &floater;
                break;
            }
            if (floater.age > slot->age)
                slot = &floater;
        }
        slot->alive = true;
        slot->heal = heal;
        slot->amount = amount;
        slot->age = 0.0f;
        // Each number takes the next place of a fixed scatter around the
        // centre, so a flurry of hits fans out instead of stacking into a blob.
        constexpr float kScatterX[] = {-150.0f, 70.0f, -40.0f, 180.0f, -110.0f, 120.0f, 10.0f};
        constexpr float kScatterY[] = {0.0f, -46.0f, 38.0f};
        slot->x = 960.0f + kScatterX[floater_serial_ % 7u];
        slot->y = 580.0f + kScatterY[floater_serial_ % 3u];
        ++floater_serial_;
    }

    void post_toast(ToastKind kind, const char *text, app::Feedback *feedback)
    {
        Toast *slot = &toasts_[0];
        for (Toast &toast : toasts_)
        {
            if (!toast.alive)
            {
                slot = &toast;
                break;
            }
            if (toast.serial < slot->serial)
                slot = &toast; // the stack is full: the oldest one makes room
        }
        slot->alive = true;
        slot->kind = kind;
        std::snprintf(slot->text, sizeof(slot->text), "%s", text);
        slot->age = 0.0f;
        slot->serial = ++toast_serial_;
        slot->slot.snap(0.0f);
        if (feedback != nullptr)
        {
            const float pan = ui::pan_for_x(kMargin + 160.0f);
            if (kind == ToastKind::pickup)
                feedback->play(audio::Cue::pickup, 1.0f, pan, 0.55f);
            else
                feedback->play(audio::Cue::notify, 1.0f, pan, 0.55f);
        }
    }

    // Everything that belongs to the game rather than to the interface. dt
    // is zero while paused.
    void advance_game(float dt, float sprint, float turn, app::Feedback &feedback)
    {
        game_time_ += dt;

        // Down, then back up: the tour and a leaning player never dead-end.
        if (is_down())
        {
            down_left_ -= dt;
            if (down_left_ <= 0.0f)
            {
                down_left_ = 0.0f;
                health_ = kMaxHealth;
                fill_.target = 1.0f;
                heal_.trigger();
                feedback.play(audio::Cue::spawn);
                post_toast(ToastKind::revive, "Revived at the last waystone", nullptr);
            }
        }

        // The ghost waits, then follows the bar on a much softer spring.
        if (ghost_hold_ > 0.0f)
            ghost_hold_ -= dt;
        else
            ghost_.target = fill_.target;
        fill_.update(dt, 9.0f);
        ghost_.update(dt, 5.0f);

        // Stamina drains while R2 is held (by how far it is pulled), rests a
        // moment, then refills. An empty bar must recover before it sprints.
        const bool sprinting = sprint > 0.15f && !exhausted_ && !is_down();
        if (sprinting)
        {
            stamina_ -= 0.3f * sprint * dt;
            stamina_rest_ = 0.5f;
            if (stamina_ <= 0.0f)
            {
                stamina_ = 0.0f;
                exhausted_ = true;
                vitals_nudge_.trigger();
                feedback.play(audio::Cue::invalid, 0.85f, ui::pan_for_x(kBarX), 0.5f);
            }
        }
        else
        {
            stamina_rest_ -= dt;
            if (stamina_rest_ <= 0.0f)
                stamina_ = std::min(1.0f, stamina_ + 0.22f * dt);
            if (stamina_ > 0.3f)
                exhausted_ = false;
        }
        speed_.target = sprinting ? 1.0f + 1.8f * sprint : 1.0f;
        speed_.update(dt, 4.0f);
        world_time_ += dt * (is_down() ? 0.15f : speed_.value + turn * 1.5f);
        heading_ += dt * (is_down() ? 0.0f : 3.0f * speed_.value + 55.0f * turn);
        heading_ = std::fmod(heading_ + 360.0f, 360.0f);

        for (int i = 0; i < kAbilityCount; ++i)
        {
            if (cooldown_[i] > 0.0f)
            {
                cooldown_[i] -= dt;
                if (cooldown_[i] <= 0.0f)
                {
                    cooldown_[i] = 0.0f;
                    ability_ready_[i].trigger();
                    feedback.play(audio::Cue::tick, 1.2f, ui::pan_for_x(tile_rect(i).cx()), 0.6f);
                }
            }
            ability_flash_[i].update(dt, 5.0f);
            ability_ready_[i].update(dt, 4.0f);
            ability_refusal_[i].update(dt, 9.0f);
        }
        palette_.update(dt, 16.0f);

        // The objective card: ticks draw themselves, a finished card waits a
        // beat (long enough to be read) and then hands over.
        for (int i = 0; i < ticked_; ++i)
            tick_[i] = std::min(1.0f, tick_[i] + dt / 0.36f);
        if (next_delay_ > 0.0f)
        {
            next_delay_ -= dt;
            if (next_delay_ <= 0.0f)
                next_objective();
        }
        swap_.update(dt);
        card_flash_.update(dt, 3.5f);
        xp_.update(dt, 5.0f);

        // Toasts: a scripted trickle of world events, newest at the bottom.
        ambient_timer_ -= dt;
        if (ambient_timer_ <= 0.0f && dt > 0.0f)
        {
            ambient_timer_ = kAmbientEvery;
            const Ambient &event = kAmbient[ambient_next_];
            ambient_next_ = (ambient_next_ + 1) % kAmbientCount;
            post_toast(event.kind, event.text, &feedback);
        }
        for (Toast &toast : toasts_)
        {
            if (!toast.alive)
                continue;
            toast.age += dt;
            if (toast.age >= kToastLife)
            {
                toast.alive = false;
                continue;
            }
            int below = 0; // newer toasts sit under this one
            for (const Toast &other : toasts_)
            {
                if (other.alive && other.serial > toast.serial)
                    ++below;
            }
            toast.slot.target = static_cast<float>(below);
            toast.slot.update(dt, 14.0f);
        }
        for (Floater &floater : floaters_)
        {
            if (!floater.alive)
                continue;
            floater.age += dt;
            if (floater.age >= kFloaterLife)
                floater.alive = false;
        }

        hit_.update(dt, 5.0f);
        heal_.update(dt, 3.0f);
        shake_.update(dt, 7.0f);
        vitals_nudge_.update(dt, 9.0f);
        down_nudge_.update(dt, 9.0f);
        danger_.target = health_ > 0 && health_ < kLowHealth ? 1.0f : 0.0f;
        danger_.update(dt, 6.0f);
        down_.target = is_down() ? 1.0f : 0.0f;
        down_.update(dt, 10.0f);
    }

    // ---- drawing: the HUD ---------------------------------------------------

    // Text over the world carries its own small shadow so it reads on sky,
    // haze and ridge alike.
    float world_text(gfx::DrawList &list, const ui::FontRef &font, std::string_view value, float x,
                     float baseline, float size, Color color, gfx::Align align = gfx::Align::left,
                     float tracking = 0.0f) const
    {
        ui::text(list, font, value, x, baseline + std::clamp(size * 0.06f, 1.5f, 4.0f), size,
                 kNight.with_alpha(0.6f * color.a), align, tracking);
        return ui::text(list, font, value, x, baseline, size, color, align, tracking);
    }

    // The objective card and the toast feed sit where the pause menu's two
    // panels land. They step aside completely instead of showing through.
    float uncovered() const
    {
        return 1.0f - tween::clamp01(pause_.value * 1.8f);
    }

    void draw_hud(gfx::DrawList &list, float opacity) const
    {
        const bool reduced = context_.settings.reduced_motion;
        list.push_opacity(opacity);
        if (down_.value > 0.01f)
            list.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                              Color::rgb(0x1a0306, 0.45f * down_.value));

        // The hit shake moves the HUD as one group, the way a camera kick
        // would: two detuned sines so the path is not a straight line.
        const float kick = reduced ? 0.0f : shake_.value;
        list.push_transform(1.0f, 0, 0, std::sin(game_time_ * 61.0f) * 11.0f * kick,
                            std::cos(game_time_ * 47.0f) * 8.0f * kick);
        draw_vitals(list);
        draw_compass(list);
        draw_objective(list);
        draw_abilities(list);
        draw_toasts(list);
        list.pop_transform();
        draw_floaters(list);
        draw_down(list);
        list.pop_opacity();
    }

    void draw_vitals(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        const float in = tween::stagger(age_, 0, 0.07f, 0.5f);
        char text[32];
        list.push_opacity(in);
        list.push_transform(1.0f, 0, 0,
                            (reduced ? 0.0f : -48.0f * (1.0f - in)) +
                                ui::shake(vitals_nudge_.value, game_time_, 8.0f, 11.0f),
                            0);
        list.shadow({kMargin - 8, kTop - 6, 560, 100}, 40, 56, kNight.with_alpha(0.42f));

        // Level emblem; the ring around it is experience toward the next one.
        const float ex = kMargin + 36.0f;
        const float ey = kTop + 42.0f;
        list.circle(ex, ey, 36, kNight.with_alpha(0.72f));
        list.arc(ex, ey, 36, 4, 0.0f, kTau, kInk.with_alpha(0.16f));
        list.arc(ex, ey, 36, 4, 0.0f, kTau * xp_.value, kAccent);
        ui::text(list, fonts.semibold, "LV", ex, ey - 8, 13, kInk.with_alpha(0.6f),
                 gfx::Align::center, 2.0f);
        ui::text(list, fonts.mono, "12", ex, ey + 17, 24, kInk, gfx::Align::center);

        world_text(list, fonts.semibold, "WARDEN", kBarX, kHealthY - 12, 15, kInk.with_alpha(0.78f),
                   gfx::Align::left, 3.0f);

        // ---- health ----
        const Rect bar{kBarX, kHealthY, kHealthW, kHealthH};
        const float fill = tween::clamp01(fill_.value);
        const float ghost = tween::clamp01(ghost_.value);
        const float low = danger_.value * (reduced ? 0.5f : ui::breathe(game_time_, 0.7f));
        const Color body =
            gfx::mix(gfx::mix(kHealth, kHealthLight, 0.7f * low), kHeal, heal_.value);
        if (low > 0.01f || heal_.value > 0.01f)
            list.glow({bar.x, bar.y, std::max(12.0f, bar.w * fill), bar.h}, 6, 18,
                      body.with_alpha(0.5f * std::max(low, heal_.value)));
        list.rounded_rect(bar.inset(-3), 9, kNight.with_alpha(0.7f));
        // The ghost is drawn first and the live bar over it, so only the part
        // that was just lost shows.
        if (ghost > fill + 0.002f)
            list.rounded_rect({bar.x, bar.y, bar.w * ghost, bar.h}, 6, kGhost.with_alpha(0.92f));
        if (bar.w * fill >= 2.0f)
        {
            const float radius = std::min(6.0f, bar.w * fill * 0.5f);
            list.gradient_rect({bar.x, bar.y, bar.w * fill, bar.h}, radius,
                               gfx::mix(body, kInk, 0.28f), body);
        }
        for (int i = 1; i < 4; ++i) // quarter marks: a hit is judged against them
            list.rounded_rect({bar.x + bar.w * 0.25f * static_cast<float>(i) - 1, bar.y, 2, bar.h},
                              0, kNight.with_alpha(0.45f));
        list.bordered_rect(bar.inset(-3), 9, kClear, 1.5f, kInk.with_alpha(0.26f + 0.3f * low));
        std::snprintf(text, sizeof(text), "%3d", health_);
        const float number_x = bar.x + bar.w + 16;
        const float w = world_text(list, fonts.mono, text, number_x, bar.y + 20, 26,
                                   gfx::mix(kInk, kHealthLight, danger_.value));
        world_text(list, fonts.mono, "/100", number_x + w + 2, bar.y + 20, 18,
                   kInk.with_alpha(0.6f));

        // ---- stamina ----
        const Rect run{kBarX, kStaminaY, kStaminaW, kStaminaH};
        const float flicker = reduced ? 1.0f : 0.5f + 0.5f * std::sin(game_time_ * 14.0f);
        const float blink = exhausted_ ? flicker : 0.0f;
        const Color wind = gfx::mix(kStamina, kHealthLight, blink);
        list.rounded_rect(run.inset(-3), 8, kNight.with_alpha(0.7f));
        if (run.w * stamina_ >= 2.0f)
            list.rounded_rect({run.x, run.y, run.w * stamina_, run.h},
                              std::min(5.0f, run.w * stamina_ * 0.5f), wind);
        list.bordered_rect(run.inset(-3), 8, kClear, 1.5f, kInk.with_alpha(0.2f + 0.3f * blink));
        std::snprintf(text, sizeof(text), "%3d", static_cast<int>(stamina_ * 100.0f + 0.5f));
        world_text(list, fonts.mono, text, run.x + run.w + 16, run.y + 12, 20,
                   kInk.with_alpha(0.8f));

        list.pop_transform();
        list.pop_opacity();
    }

    void draw_compass(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float in = tween::stagger(age_, 1, 0.07f, 0.5f);
        const float drop = context_.settings.reduced_motion ? 0.0f : -36.0f * (1.0f - in);
        const Rect band{960.0f - kCompassW * 0.5f, kTop + drop, kCompassW, kCompassH};
        list.push_opacity(in);
        list.shadow({band.x, band.y + 6, band.w, band.h}, 14, 30, kNight.with_alpha(0.4f));
        list.gradient_rect_h({band.x, band.y, band.w * 0.5f, band.h}, 0, kClear,
                             kNight.with_alpha(0.5f));
        list.gradient_rect_h({band.x + band.w * 0.5f, band.y, band.w * 0.5f, band.h}, 0,
                             kNight.with_alpha(0.5f), kClear);

        // The strip is a ring of 24 marks, 15 degrees apart; each is placed by
        // its signed distance from the heading and fades toward the ends, so
        // letters slide in and out instead of being cut by the clip.
        static constexpr const char *kPoints[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
        list.push_clip(band);
        const float half = kCompassW * 0.5f;
        for (int i = 0; i < 24; ++i)
        {
            const float delta = wrap_degrees(static_cast<float>(i) * 15.0f - heading_);
            const float x = 960.0f + delta * kPixelsPerDegree;
            const float edge = tween::clamp01((half - std::fabs(x - 960.0f)) / 90.0f);
            if (edge <= 0.0f)
                continue;
            if (i % 6 == 0)
                world_text(list, fonts.semibold, kPoints[i / 3], x, band.y + 36, 28,
                           (i == 0 ? kAccent : kInk).with_alpha(edge), gfx::Align::center);
            else if (i % 3 == 0)
                world_text(list, fonts.semibold, kPoints[i / 3], x, band.y + 33, 18,
                           kInk.with_alpha(0.7f * edge), gfx::Align::center, 1.0f);
            else
                list.rounded_rect({x - 1, band.y + 20, 2, 12}, 1, kInk.with_alpha(0.5f * edge));
        }
        list.pop_clip();
        list.gradient_rect_h({band.x, band.y, half, 1.5f}, 0, kClear, kInk.with_alpha(0.4f));
        list.gradient_rect_h({960, band.y, half, 1.5f}, 0, kInk.with_alpha(0.4f), kClear);
        list.gradient_rect_h({band.x, band.y + band.h - 1.5f, half, 1.5f}, 0, kClear,
                             kInk.with_alpha(0.4f));
        list.gradient_rect_h({960, band.y + band.h - 1.5f, half, 1.5f}, 0, kInk.with_alpha(0.4f),
                             kClear);

        // Where the objective lies: a diamond riding the top edge. Off the
        // strip it waits at the nearer end, smaller, as "that way".
        const float bearing = wrap_degrees(kObjectives[objective_].bearing - heading_);
        const float reach = half - 22.0f;
        const float mx = std::clamp(bearing * kPixelsPerDegree, -reach, reach);
        const bool off = std::fabs(bearing * kPixelsPerDegree) > reach;
        const float size = off ? 9.0f : 13.0f;
        list.rotated_rect({960.0f + mx - size * 0.5f, band.y - size * 0.5f, size, size}, 2,
                          kTau / 8.0f, kAccent.with_alpha(off ? 0.7f : 1.0f));

        // The fixed marker and the heading it points at.
        list.triangle({960 - 9, band.y + band.h + 5, 18, 13}, kInk);
        char text[16];
        std::snprintf(text, sizeof(text), "%03d\xC2\xB0", static_cast<int>(heading_) % 360);
        world_text(list, fonts.mono, text, 960 + 4, band.y + band.h + 44, 20,
                   kInk.with_alpha(0.88f), gfx::Align::center);
        list.pop_opacity();
    }

    // One objective's title and checklist, at an opacity and offset so two
    // can cross-fade. `finished` draws every line ticked (the outgoing card).
    void draw_objective_body(gfx::DrawList &list, const Rect &card, int index, bool finished,
                             float alpha, float slide) const
    {
        if (alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const Objective &objective = kObjectives[index];
        const float x = card.x + 24 + slide;
        list.push_opacity(alpha);
        ui::text(list, fonts.semibold, fonts.semibold.font->fit(objective.title, 28, card.w - 48),
                 x, card.y + 82, 28, kInk);
        const int done = finished ? kLines : ticked_;
        for (int i = 0; i < kLines; ++i)
        {
            const float p = finished ? 1.0f : (i < done ? tick_[i] : 0.0f);
            const float cy = card.y + 118 + static_cast<float>(i) * 36.0f;
            const Rect box{x, cy - 12, 24, 24};
            list.bordered_rect(box, 7, kAccent.with_alpha(p), 2,
                               gfx::mix(kInk.with_alpha(0.55f), kAccent, p));
            // The tick is two strokes that grow one after the other: the
            // short down-stroke first, then the long one up.
            const float first = tween::clamp01(p / 0.35f);
            const float second = tween::cubic_out((p - 0.35f) / 0.65f);
            const float ax = box.x + 6, ay = box.y + 12.5f;
            const float bx = box.x + 10.5f, by = box.y + 17;
            const float cx = box.x + 18.5f, cy2 = box.y + 7.5f;
            if (first > 0.0f)
                list.line(ax, ay, tween::lerp(ax, bx, first), tween::lerp(ay, by, first), 3.2f,
                          kNight);
            if (second > 0.0f)
                list.line(bx, by, tween::lerp(bx, cx, second), tween::lerp(by, cy2, second), 3.2f,
                          kNight);

            // The next thing to do is the brightest line on the card.
            const float rest = i == done ? 1.0f : 0.72f;
            const std::string label = fonts.regular.font->fit(objective.lines[i], 24, card.w - 88);
            const float w = ui::text(list, fonts.regular, label, x + 38, cy + 8, 24,
                                     kInk.with_alpha(tween::lerp(rest, 0.5f, p)));
            if (p > 0.0f)
                list.rounded_rect({x + 36, cy - 0.5f, (w + 4) * tween::cubic_out(p), 2}, 1,
                                  kInk.with_alpha(0.7f));
        }
        list.pop_opacity();
    }

    void draw_objective(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        const float in = tween::stagger(age_, 2, 0.07f, 0.5f);
        const float flash = card_flash_.value;
        const Rect card{kRight - kCardW + (reduced ? 0.0f : 48.0f * (1.0f - in)), kTop, kCardW,
                        kCardH};
        list.push_opacity(in * uncovered());
        list.push_transform(1.0f + (reduced ? 0.0f : 0.03f * flash), card.cx(), card.cy(), 0, 0);
        list.shadow({card.x, card.y + 12, card.w, card.h}, 22, 34, kNight.with_alpha(0.5f));
        if (flash > 0.01f)
            list.glow(card, 22, 30, kAccent.with_alpha(0.55f * flash));
        list.rounded_rect(card, 22, kNight.with_alpha(0.66f));
        list.rounded_rect(card, 22, kAccent.with_alpha(0.3f * flash));
        list.bordered_rect(card, 22, kClear, 1.5f,
                           gfx::mix(kInk.with_alpha(0.18f), kAccent, std::min(1.0f, flash * 1.5f)));

        // Header: the label turns to "COMPLETE" as the last tick lands.
        const float complete = ticked_ >= kLines ? tick_[kLines - 1] : 0.0f;
        ui::text(list, fonts.semibold, "OBJECTIVE", card.x + 24, card.y + 40, 15,
                 kAccent.with_alpha(1.0f - complete), gfx::Align::left, 3.0f);
        ui::text(list, fonts.semibold, "COMPLETE", card.x + 24, card.y + 40, 15,
                 kHeal.with_alpha(complete), gfx::Align::left, 3.0f);
        char text[16];
        std::snprintf(text, sizeof(text), "%d/%d", ticked_, kLines);
        ui::text(list, fonts.mono, text, card.x + card.w - 24, card.y + 41, 20,
                 kInk.with_alpha(0.7f), gfx::Align::right);

        list.push_clip(card);
        if (swap_.running)
        {
            // The finished objective leaves quickly; the next arrives a beat later.
            const float t = swap_.progress();
            draw_objective_body(list, card, previous_objective_, true,
                                1.0f - tween::smoothstep(t * 2.4f),
                                reduced ? 0.0f : -40.0f * tween::cubic_in(t * 2.4f));
            const float arrive = tween::clamp01((t - 0.3f) / 0.7f);
            draw_objective_body(list, card, objective_, false, tween::smoothstep(arrive),
                                reduced ? 0.0f : 56.0f * (1.0f - tween::quint_out(arrive)));
        }
        else
        {
            draw_objective_body(list, card, objective_, false, 1.0f, 0.0f);
        }
        list.pop_clip();
        list.pop_transform();
        list.pop_opacity();
    }

    // The four ability icons, built from kit shapes around (cx, cy).
    static void draw_ability_icon(gfx::DrawList &list, int index, float cx, float cy, Color ink)
    {
        switch (index)
        {
        case 0: // Dash: a double chevron
            for (int i = 0; i < 2; ++i)
            {
                const float x = cx - 15.0f + static_cast<float>(i) * 18.0f;
                list.line(x, cy - 15, x + 13, cy, 5.5f, ink);
                list.line(x + 13, cy, x, cy + 15, 5.5f, ink);
            }
            break;
        case 1: // Flare: a star
            list.star(cx, cy + 1, 25, ink);
            break;
        case 2: // Ward: a ringed core
            list.ring(cx, cy, 22, 5, ink);
            list.circle(cx, cy, 8, ink);
            break;
        default: // Volley: three arrowheads in flight
            list.triangle({cx - 9, cy - 24, 18, 20}, ink);
            list.triangle({cx - 25, cy - 2, 18, 20}, ink);
            list.triangle({cx + 7, cy - 2, 18, 20}, ink);
            break;
        }
    }

    void draw_abilities(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        const ui::GlyphStyle style = ui::GlyphStyle::dark();
        char text[16];

        // The row under the tiles reads as a sentence: "L2 + (button)". The
        // face buttons are live while L2 is held, and the tag lights up then.
        const float tag = tween::stagger(age_, 3, 0.07f, 0.5f);
        const Rect first = tile_rect(0);
        list.push_opacity(tag * (0.7f + 0.3f * palette_.value));
        if (palette_.value > 0.01f)
            list.glow({first.x - 58, kBadgeY - 15, 42, 30}, 10, 14,
                      kAccent.with_alpha(0.7f * palette_.value));
        ui::draw_button(list, fonts, style, ui::Button::l2, first.x - 58, kBadgeY, 32);
        world_text(list, fonts.semibold, "+", first.x + 6, kBadgeY + 8, 24, kInk,
                   gfx::Align::center);
        list.pop_opacity();

        for (int i = 0; i < kAbilityCount; ++i)
        {
            const float in = tween::stagger(age_, 3 + i, 0.07f, 0.5f);
            const float flash = ability_flash_[i].value;
            const float ready = ability_ready_[i].value;
            const bool cooling = cooldown_[i] > 0.0f;
            Rect tile = tile_rect(i);
            tile.x += ui::shake(ability_refusal_[i].value, game_time_, 9.0f, 12.0f);
            tile.y += (reduced ? 0.0f : 36.0f * (1.0f - in)) - 8.0f * palette_.value;
            const float cx = tile.cx();
            const float cy = tile.cy();
            list.push_opacity(in);
            // A triggered tile punches out and settles; a refreshed one swells.
            list.push_transform(1.0f + (reduced ? 0.0f : 0.1f * flash + 0.05f * ready), cx, cy, 0,
                                0);
            list.shadow({tile.x, tile.y + 10, tile.w, tile.h}, kTileRadius, 26,
                        kNight.with_alpha(0.55f));
            if (flash > 0.01f || ready > 0.01f)
                list.glow(tile, kTileRadius, 22, kAccent.with_alpha(0.7f * std::max(flash, ready)));
            list.gradient_rect(tile, kTileRadius, Color::rgb(0x2a3950, 0.92f),
                               Color::rgb(0x121b2a, 0.92f));
            // Dimmed while it cannot be used: cooling down, or the player is down.
            draw_ability_icon(list, i, cx, cy,
                              kInk.with_alpha(cooling ? 0.13f : 0.96f - 0.6f * down_.value));
            if (cooling)
            {
                // The sweep: a pie (an arc as thick as its radius) covering
                // the part of the cooldown still to run, wiping away
                // clockwise, edged by a thin bright arc.
                const float left = cooldown_[i] / kAbilities[i].cooldown;
                const float start = kTau * (1.0f - left);
                list.arc(cx, cy, 40, 40, start, kTau * left, kNight.with_alpha(0.74f), false);
                list.arc(cx, cy, 42, 4, start, kTau * left, kAccent);
                if (cooldown_[i] >= 1.0f)
                    std::snprintf(text, sizeof(text), "%d",
                                  static_cast<int>(std::ceil(cooldown_[i])));
                else
                    std::snprintf(text, sizeof(text), "%.1f", static_cast<double>(cooldown_[i]));
                ui::text(list, fonts.mono, text, cx, cy + 11, 30, kInk, gfx::Align::center);
            }
            list.rounded_rect(tile, kTileRadius, kInk.with_alpha(0.55f * flash));
            list.bordered_rect(tile, kTileRadius, kClear, 2,
                               gfx::mix(kInk.with_alpha(cooling ? 0.16f : 0.34f), kAccent,
                                        std::max(palette_.value, std::max(flash, ready))));
            list.pop_transform();
            ui::draw_button(list, fonts, style, ability_glyph(i), cx - 16, kBadgeY, 32);
            list.pop_opacity();
        }
    }

    void draw_toasts(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        for (const Toast &toast : toasts_)
        {
            if (!toast.alive)
                continue;
            const float appear = tween::cubic_out(toast.age / 0.35f);
            const float leave = tween::clamp01((toast.age - (kToastLife - 0.45f)) / 0.45f);
            const bool gold = toast.kind == ToastKind::achievement;
            const std::string label = fonts.regular.font->fit(toast.text, 24, 520);
            const float width = 64.0f + fonts.regular.measure(label, 24) + 24.0f;
            const float slide =
                reduced ? 0.0f : -72.0f * (1.0f - appear) - 40.0f * tween::cubic_in(leave);
            const float top = kToastBottom - kToastH - toast.slot.value * kToastPitch;
            const Rect pill{kMargin + slide, top, width, kToastH};
            // Older toasts step back as they climb the stack.
            list.push_opacity(appear * (1.0f - leave) * uncovered() *
                              (1.0f - 0.14f * std::min(3.0f, toast.slot.value)));
            list.shadow({pill.x, pill.y + 8, pill.w, pill.h}, 18, 24, kNight.with_alpha(0.5f));
            list.rounded_rect(pill, 18, kNight.with_alpha(0.74f));
            list.bordered_rect(pill, 18, kClear, 1.5f,
                               gold ? kAccent.with_alpha(0.85f) : kInk.with_alpha(0.16f));
            const float ix = pill.x + 32;
            const float iy = pill.cy();
            switch (toast.kind)
            {
            case ToastKind::pickup: // a plus: something was added
                list.circle(ix, iy, 15, kStamina.with_alpha(0.22f));
                list.line(ix - 7, iy, ix + 7, iy, 3.5f, kStamina);
                list.line(ix, iy - 7, ix, iy + 7, 3.5f, kStamina);
                break;
            case ToastKind::area: // a map pin's diamond
                list.circle(ix, iy, 15, kHeal.with_alpha(0.2f));
                list.rotated_rect({ix - 7, iy - 7, 14, 14}, 2, kTau / 8.0f, kHeal);
                break;
            case ToastKind::revive:
                list.circle(ix, iy, 15, kHealthLight.with_alpha(0.24f));
                list.triangle({ix - 8, iy - 9, 16, 15}, kHealthLight);
                break;
            case ToastKind::achievement: // the star pops as the toast lands
                list.star(ix, iy, 9.0f + 9.0f * tween::back_out(toast.age / 0.45f), kAccent);
                break;
            }
            ui::text(list, fonts.regular, label, pill.x + 64, pill.cy() + 8, 24,
                     gold ? gfx::mix(kInk, kAccent, 0.25f) : kInk);
            list.pop_opacity();
        }
    }

    void draw_floaters(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        char text[16];
        for (const Floater &floater : floaters_)
        {
            if (!floater.alive)
                continue;
            const float t = floater.age / kFloaterLife;
            // Pop past full size and settle (back_out), drift up on an
            // easing that slows, fade over the last third.
            const float scale = reduced ? 1.0f : 0.5f + 0.5f * tween::back_out(floater.age / 0.2f);
            const float y = floater.y - (reduced ? 0.0f : 110.0f * tween::cubic_out(t));
            const float alpha = 1.0f - tween::smoothstep((t - 0.62f) / 0.38f);
            std::snprintf(text, sizeof(text), "%c%d", floater.heal ? '+' : '-', floater.amount);
            list.push_transform(scale, floater.x, y, 0, 0);
            ui::text(list, fonts.semibold, text, floater.x, y + 4, 60,
                     kNight.with_alpha(0.6f * alpha), gfx::Align::center);
            ui::text(list, fonts.semibold, text, floater.x, y, 60,
                     (floater.heal ? kHeal : gfx::mix(kHealthLight, kInk, 0.35f)).with_alpha(alpha),
                     gfx::Align::center);
            list.pop_transform();
        }
    }

    void draw_down(gfx::DrawList &list) const
    {
        const float d = down_.value;
        if (d <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        list.push_opacity(d);
        list.push_transform(reduced ? 1.0f : 1.0f + 0.35f * (1.0f - d), 960, 470,
                            ui::shake(down_nudge_.value, clock_, 12.0f, 11.0f), 0);
        world_text(list, fonts.display, "DOWN", 960 + 14, 508, 132,
                   gfx::mix(kInk, kHealthLight, 0.35f), gfx::Align::center, 28.0f);
        list.pop_transform();
        world_text(list, fonts.semibold, "REVIVING", 960 + 2, 566, 18, kInk.with_alpha(0.8f),
                   gfx::Align::center, 4.0f);
        const Rect bar{960 - 140, 588, 280, 6};
        list.rounded_rect(bar, 3, kNight.with_alpha(0.6f));
        const float done = 1.0f - tween::clamp01(down_left_ / kDownSeconds);
        list.rounded_rect({bar.x, bar.y, std::max(6.0f, bar.w * done), bar.h}, 3, kInk);
        list.pop_opacity();
    }

    // ---- drawing: the pause menu --------------------------------------------

    // Frosted panel: the blurred frame, a tint, then a hairline of light.
    static void glass_panel(gfx::DrawList &list, std::uint32_t glass, const Rect &r, float radius,
                            float tint)
    {
        list.shadow({r.x, r.y + 22, r.w, r.h}, radius, 60, kNight.with_alpha(0.55f));
        list.glass(glass, r, radius, kInk);
        list.gradient_rect(r, radius, kGlassTint.with_alpha(tint - 0.1f),
                           kNight.with_alpha(tint + 0.06f));
        list.bordered_rect(r, radius, kClear, 1.5f, kInk.with_alpha(0.22f));
    }

    void draw_pause(gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        const float t = pause_.value;
        // The panel rides the pause spring in from beyond the left edge.
        const float slide = reduced ? 0.0f : -(kMargin + kPanelW + 40.0f) * (1.0f - t);
        const Rect panel{kMargin + slide, kPanelY, kPanelW, kPanelH};
        list.push_opacity(tween::clamp01(t * 1.5f) * (1.0f - 0.5f * dialog_value_.value));
        glass_panel(list, glass, panel, kPanelRadius, 0.62f);

        const float x = panel.x + 40;
        ui::text(list, fonts.semibold, "PAUSED", x, panel.y + 72, 18, kAccent, gfx::Align::left,
                 4.0f);
        ui::text(list, fonts.display, "Hollow Reach", x - 2, panel.y + 132, 48, kInk);
        ui::text(list, fonts.regular,
                 fonts.regular.font->fit(kObjectives[objective_].title, 24, panel.w - 80), x,
                 panel.y + 172, 24, kInk.with_alpha(0.68f));
        list.rounded_rect({x, panel.y + 198, panel.w - 80, 1.5f}, 0, kInk.with_alpha(0.14f));

        // One highlight, sprung between the rows; it nudges when refused.
        Rect bar = highlight_.value();
        bar.x += slide + (nudge_vertical_ ? 0.0f : ui::shake(menu_nudge_.value, clock_, 10.0f));
        bar.y += nudge_vertical_ ? ui::shake(menu_nudge_.value, clock_, 8.0f) : 0.0f;
        const float lit = tween::stagger(pause_age_, 1, 0.05f, 0.3f);
        list.glow(bar, kRowH * 0.5f, 22,
                  kAccent.with_alpha((0.3f + 0.14f * ui::breathe(clock_)) * lit));
        list.rounded_rect(bar, kRowH * 0.5f, kAccent.with_alpha(lit));
        for (int i = 0; i < kMenuItems; ++i)
        {
            // Rows arrive one after another, behind the panel that carries them.
            const float in = tween::stagger(pause_age_, i + 1, 0.05f, 0.34f);
            const Rect row = row_rect(i);
            const float rx = row.x + slide + (reduced ? 0.0f : -28.0f * (1.0f - in));
            // Ink follows the highlight's position, not the focus index, so
            // a row darkens as the bar reaches it.
            const float under =
                tween::clamp01(1.0f - std::fabs(highlight_.y.value - row.y) / (kRowPitch * 0.6f));
            const Color ink = gfx::mix(kInk.with_alpha(0.86f), kNight, under);
            list.push_opacity(in);
            ui::text(list, fonts.semibold, kMenu[i].label, rx + 28, row.y + 44, 28, ink);
            if (kMenu[i].meta[0] != '\0')
                ui::text(list, fonts.mono, kMenu[i].meta, rx + row.w - 28, row.y + 42, 20,
                         gfx::mix(kInk.with_alpha(0.55f), kNight.with_alpha(0.8f), under),
                         gfx::Align::right);
            list.pop_opacity();
        }

        // What the focused row is for; it cross-fades as the focus moves.
        const float about_y = kMenuY + kMenuItems * kRowPitch + 12.0f;
        list.rounded_rect({x, about_y, panel.w - 80, 1.5f}, 0, kInk.with_alpha(0.14f));
        const float fade = about_fade_.running ? about_fade_.progress() : 1.0f;
        const auto about = [&](int id, float alpha)
        {
            if (alpha <= 0.01f)
                return;
            ui::paragraph(list, fonts.regular, id % 2 == 1 ? kNotInDemo : kMenu[id / 2].about, x,
                          about_y + 48, 24, panel.w - 80, 34,
                          (id % 2 == 1 ? kAccent : kInk).with_alpha(0.8f * alpha), 2);
        };
        about(about_previous_, 1.0f - tween::smoothstep(fade * 2.0f));
        about(about_, tween::smoothstep((fade - 0.3f) / 0.7f));
        list.pop_opacity();
    }

    void draw_session(gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        const float t = pause_.value;
        const float in = tween::stagger(pause_age_, 2, 0.06f, 0.45f);
        const Rect card{kRight - kSessionW + (reduced ? 0.0f : 60.0f * (1.0f - in)), kPanelY,
                        kSessionW, kSessionH};
        char text[32];
        list.push_opacity(std::min(in, tween::clamp01(t * 1.5f)) *
                          (1.0f - 0.5f * dialog_value_.value));
        glass_panel(list, glass, card, 30, 0.6f);
        const float x = card.x + 36;
        ui::text(list, fonts.semibold, "SESSION", x, card.y + 60, 18, kAccent, gfx::Align::left,
                 4.0f);
        // Play time in the monospaced face: the digits tick without the
        // line changing width.
        const int seconds = static_cast<int>(session_time_);
        std::snprintf(text, sizeof(text), "%02d:%02d:%02d", seconds / 3600, seconds / 60 % 60,
                      seconds % 60);
        ui::text(list, fonts.mono, text, x - 2, card.y + 130, 56, kInk);
        ui::text(list, fonts.regular, "Play time", x, card.y + 166, 22, kInk.with_alpha(0.6f));
        list.rounded_rect({x, card.y + 194, card.w - 72, 1.5f}, 0, kInk.with_alpha(0.14f));

        const char *labels[] = {"Objectives done", "Damage taken", "Abilities used", "Times down"};
        const int values[] = {stat_objectives_, stat_damage_, stat_abilities_, stat_downs_};
        for (int i = 0; i < 4; ++i)
        {
            const float appear = tween::stagger(pause_age_, 4 + i, 0.06f, 0.4f);
            const float y = card.y + 242 + static_cast<float>(i) * 42.0f +
                            (reduced ? 0.0f : 14.0f * (1.0f - appear));
            list.push_opacity(appear);
            ui::text(list, fonts.regular, labels[i], x, y, 24, kInk.with_alpha(0.74f));
            std::snprintf(text, sizeof(text), "%d", values[i]);
            ui::text(list, fonts.mono, text, card.x + card.w - 36, y, 26, kInk, gfx::Align::right);
            list.pop_opacity();
        }
        list.pop_opacity();
    }

    void draw_dialog(gfx::DrawList &list, std::uint32_t glass) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        const float t = dialog_value_.value;
        list.push_opacity(tween::clamp01(t * 1.4f));
        list.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                          kNight.with_alpha(0.3f));
        list.push_transform(reduced ? 1.0f : 0.93f + 0.07f * t, kDialog.cx(), kDialog.cy(), 0, 0);
        glass_panel(list, glass, kDialog, 36, 0.72f);
        ui::text(list, fonts.display, "Quit to title?", kDialog.x + 40, kDialog.y + 86, 46, kInk);
        ui::paragraph(list, fonts.regular,
                      "Progress since the last waystone will be lost and a new session begins.",
                      kDialog.x + 40, kDialog.y + 138, 24, kDialog.w - 80, 34,
                      kInk.with_alpha(0.8f), 2);

        // Two answers and one highlight that slides between them. It opens
        // on Cancel: the safe answer is the one a stray press confirms.
        const Rect from = dialog_button(0);
        const Rect to = dialog_button(1);
        Rect bar = from;
        bar.x = tween::lerp(from.x, to.x, dialog_position_.value) +
                ui::shake(menu_nudge_.value, clock_, 10.0f);
        const Color lit = gfx::mix(kAccent, kHealthLight, dialog_position_.value);
        list.glow(bar, 34, 20, lit.with_alpha(0.3f + 0.14f * ui::breathe(clock_)));
        list.rounded_rect(bar, 34, lit);
        const char *labels[] = {"Cancel", "Quit to title"};
        for (int i = 0; i < 2; ++i)
        {
            const Rect button = dialog_button(i);
            const float under =
                tween::clamp01(1.0f - std::fabs(dialog_position_.value - static_cast<float>(i)));
            list.bordered_rect(button, 34, kInk.with_alpha(0.06f * (1.0f - under)), 1.5f,
                               kInk.with_alpha(0.24f * (1.0f - under)));
            ui::text(list, fonts.semibold, labels[i], button.cx(), button.cy() + 10, 28,
                     gfx::mix(kInk.with_alpha(0.9f), kNight, under), gfx::Align::center);
        }
        list.pop_transform();
        list.pop_opacity();
    }

    void draw_hints(app::Frame &frame) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const ui::GlyphStyle style = ui::GlyphStyle::dark();
        const float pause = pause_.value;

        // The play hints stay in the scene: they blur away with the world.
        frame.scene.push_opacity(tween::stagger(age_, 8, 0.07f, 0.5f) * (1.0f - pause));
        const ui::Hint play[] = {
            {ui::Button::dpad, "Abilities"}, {confirm_glyph(), "Take a hit"},
            {ui::Button::square, "Heal"},    {ui::Button::triangle, "Objective"},
            {ui::Button::r2, "Sprint"},      {ui::Button::left_stick, "Turn"},
            {ui::Button::options, "Pause"},
        };
        ui::draw_hints(frame.scene, fonts, style, play, 7, kRight, true);
        frame.scene.pop_opacity();

        if (pause <= 0.01f)
            return;
        const float choice = dialog_value_.value;
        frame.overlay.push_opacity(pause * (1.0f - choice));
        const ui::Hint menu[] = {
            {ui::Button::dpad, "Move"}, {confirm_glyph(), "Choose"}, {back_glyph(), "Resume"}};
        ui::draw_hints(frame.overlay, fonts, style, menu, 3, kRight, true);
        frame.overlay.pop_opacity();
        frame.overlay.push_opacity(pause * choice);
        const ui::Hint ask[] = {
            {ui::Button::dpad, "Move"}, {confirm_glyph(), "Choose"}, {back_glyph(), "Cancel"}};
        ui::draw_hints(frame.overlay, fonts, style, ask, 3, kRight, true);
        frame.overlay.pop_opacity();
    }

    app::Context &context_;
    float age_ = 0.0f;        // seconds since enter(): drives the entrance
    float clock_ = 0.0f;      // free-running interface time
    float game_time_ = 0.0f;  // gameplay time: stands still while paused
    float world_time_ = 0.0f; // the backdrop's clock: gameplay time times speed
    float session_time_ = 0.0f;
    std::uint32_t rng_ = 0x51f15eedu;

    // Vitals.
    int health_ = kMaxHealth;
    tween::Spring fill_;  // the live bar: snaps down, springs up
    tween::Spring ghost_; // what the bar showed before the last hit
    float ghost_hold_ = 0.0f;
    float stamina_ = 1.0f;
    float stamina_rest_ = 0.0f;
    bool exhausted_ = false;
    float down_left_ = 0.0f; // seconds of the DOWN state still to run
    tween::Spring down_;
    tween::Spring danger_;
    tween::Spring speed_; // how fast the world scrolls
    tween::Spring xp_;
    ui::Pulse hit_;
    ui::Pulse heal_;
    ui::Pulse shake_;
    ui::Pulse vitals_nudge_;
    ui::Pulse down_nudge_;
    float heading_ = 0.0f; // degrees, 0 is north

    // Objective.
    int objective_ = 0;
    int previous_objective_ = 0;
    int ticked_ = 0;
    float tick_[kLines] = {}; // 0..1: how much of each tick is drawn
    float next_delay_ = 0.0f;
    tween::Timer swap_;
    ui::Pulse card_flash_;

    // Abilities.
    float cooldown_[kAbilityCount] = {};
    ui::Pulse ability_flash_[kAbilityCount];
    ui::Pulse ability_ready_[kAbilityCount];
    ui::Pulse ability_refusal_[kAbilityCount];
    tween::Spring palette_; // L2 held

    // Feed and numbers.
    Toast toasts_[kMaxToasts];
    std::uint32_t toast_serial_ = 0;
    float ambient_timer_ = 0.0f;
    int ambient_next_ = 0;
    Floater floaters_[kMaxFloaters];
    std::uint32_t floater_serial_ = 0;

    // Session figures for the pause menu.
    int stat_objectives_ = 0;
    int stat_damage_ = 0;
    int stat_abilities_ = 0;
    int stat_downs_ = 0;

    // Pause menu.
    bool paused_ = false;
    tween::Spring pause_;
    float pause_age_ = 0.0f; // seconds since the menu opened: staggers its rows
    int item_ = 0;
    ui::SpringRect highlight_;
    ui::Pulse menu_nudge_;
    bool nudge_vertical_ = false;
    bool note_ = false; // the focused row was refused: explain instead
    int about_ = 0;
    int about_previous_ = 0;
    tween::Timer about_fade_;
    bool dialog_ = false;
    tween::Spring dialog_value_;
    int dialog_focus_ = 0;
    tween::Spring dialog_position_;
    ui::Pulse curtain_;
};

} // namespace

std::unique_ptr<app::Concept> make_hud(app::Context &context)
{
    return std::make_unique<Hud>(context);
}

} // namespace hui::concepts
