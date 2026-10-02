// ps5-homebrew-ui - Design "Launch Sequence": everything before the main menu.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The first minute of a game: studio splash, title screen, profile select,
// loading screen and the hand-over to the game. Homebrew usually skips this
// part; it is also where the first impression is made. What makes it feel
// finished:
//
//   - the studio logo assembles itself: strokes fly in on staggered springs,
//     a ring draws itself, the wordmark arrives letter by letter while its
//     tracking settles, and the chime lands when the pieces lock;
//   - the title logo is built in layers (bloom, halo, a banded gradient fill)
//     and a clipped band of light crosses the letters every few seconds;
//   - the loader is honest: progress is uneven, every stage has a name, a
//     spinner covers the stalls, and the screen only asks for a button when
//     the bar is really full;
//   - each change of screen is its own designed move (through black, a slide
//     with the sky panning, a push into the artwork, an iris), and all of
//     them fall back to plain fades under "Reduce motion";
//   - no screen is a dead end: the splash can be skipped, every later screen
//     can be left, and a move reversed in flight turns round where it is.

#include "concepts/concepts.hpp"

#include "core/tween.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace hui::concepts
{

namespace
{

using gfx::Color;
using gfx::Rect;

constexpr float kTau = 6.2831853f;
constexpr float kCentreX = gfx::kVirtualWidth * 0.5f;
constexpr float kCentreY = gfx::kVirtualHeight * 0.5f;
constexpr float kMargin = 96.0f;
constexpr float kRight = gfx::kVirtualWidth - kMargin;

const Color kInk = Color::rgb(0xfff4dc);   // warm white: all text
const Color kNight = Color::rgb(0x030308); // the black the moves pass through
const Color kClear = Color::rgb(0x000000, 0.0f);
const Color kStudioInk = Color::rgb(0xf3ede2); // the studio's own two colours
const Color kStudioEmber = Color::rgb(0xff6a48);

constexpr int kGame = 18; // catalogue index of the game this sequence opens

// ---- studio splash --------------------------------------------------------
constexpr float kSplashSeconds = 2.6f; // then it leaves by itself
constexpr float kEmblemY = 452.0f;
constexpr float kEmblemRadius = 118.0f;
constexpr int kPieces = 5; // four strokes and the ember
constexpr float kPieceStart = 0.10f;
constexpr float kPieceStep = 0.09f;
constexpr float kRingStart = 0.20f;
constexpr float kRingSeconds = 0.75f;
constexpr float kWelcomeAt = 0.55f; // the chime blooms as the ring closes
constexpr float kWordStart = 0.60f;
constexpr float kLetterStep = 0.045f;
constexpr float kWordSize = 40.0f;
constexpr float kWordTracking = 14.0f;
constexpr float kWordTrackingWide = 30.0f;
constexpr float kPresentsAt = 1.25f;

// ---- title ----------------------------------------------------------------
constexpr float kLogoSize = 184.0f;
constexpr float kLogoTracking = 10.0f;
constexpr float kLogoMaxWidth = 1400.0f;
constexpr float kLogoBaseline = 560.0f;
constexpr int kLogoBands = 12;      // slices of the gradient fill
constexpr float kSweepFirst = 1.8f; // seconds after the title arrives
constexpr float kSweepPeriod = 5.0f;
constexpr float kSweepSeconds = 1.2f;
constexpr int kSweepStrips = 9;
constexpr int kSweepRows = 5;
constexpr float kSweepStrip = 15.0f; // width of one strip of the band
constexpr float kSweepLean = 9.0f;   // how far each row is shifted: the slant
constexpr float kSweepProfile[kSweepStrips] = {0.08f, 0.22f, 0.42f, 0.64f, 0.80f,
                                               0.64f, 0.42f, 0.22f, 0.08f};
constexpr float kCameraDrop = 2.4f; // how far the sky pans for the profiles

// ---- profile select -------------------------------------------------------
constexpr int kCards = 5;
constexpr int kAddCard = kCards - 1;
constexpr float kCardW = 264.0f;
constexpr float kCardH = 340.0f;
constexpr float kCardGrowW = 64.0f; // what the focused card gains
constexpr float kCardGrowH = 136.0f;
constexpr float kCardRise = 48.0f;
constexpr float kCardGap = 28.0f;
constexpr float kCardTop = 428.0f;
constexpr float kCardRadius = 28.0f;

struct Profile
{
    const char *name;
    const char *last_played;
    float progress;
    int hours;
    const char *chapter;
    const char *area;
    std::uint32_t colour;
};

constexpr Profile kProfiles[kCards] = {
    {"Mara", "Today", 0.62f, 41, "Chapter 5", "The Ember Road", 0xffb347},
    {"Tobias", "Yesterday", 0.35f, 18, "Chapter 3", "The Ashen Coast", 0x7fb4ff},
    {"June", "3 days ago", 0.90f, 63, "Chapter 8", "Lantern Pass", 0xff8fb3},
    {"Old Fen", "2 weeks ago", 0.08f, 2, "Chapter 1", "The Unlit Shore", 0x9be8b0},
    {"Add profile", "New journey", 0.0f, 0, "Prologue", "The First Spark", 0xfff4dc},
};

// ---- loading --------------------------------------------------------------
// The simulated loader: each stage ends at a share of the whole and takes its
// own time, so the bar runs, stalls and runs again like a real one.
struct Stage
{
    const char *name;
    float end;     // progress when the stage is done
    float seconds; // how long it takes
    bool stall;    // little to show for the time: the spinner speaks up
};

constexpr Stage kStages[] = {
    {"Reading save data", 0.10f, 0.45f, false}, {"Loading textures", 0.44f, 1.25f, false},
    {"Compiling shaders", 0.50f, 1.50f, true},  {"Building world", 0.84f, 1.30f, false},
    {"Lighting lanterns", 0.89f, 1.20f, true},  {"Waking the wind", 1.00f, 0.60f, false},
};
constexpr int kStageCount = static_cast<int>(sizeof(kStages) / sizeof(kStages[0]));

constexpr int kTips = 6;
constexpr const char *kTipText[kTips] = {
    "A candle burns faster in the wind. Shelter it behind walls and tall grass.",
    "Share your flame with a cold lantern. Lit lanterns are safe places to rest.",
    "Moths gather over hidden paths. Follow them after dusk.",
    "Rain will not put the candle out, but running in it will. Walk.",
    "Wax is currency in the lowlands. Never spend your last stub.",
    "If the dark feels closer than before, it is. Turn around slowly.",
};
constexpr float kTipSeconds = 3.5f;
constexpr float kBarY = 928.0f;
constexpr float kBarH = 8.0f;
constexpr float kArtScale = 1.3f; // the key art overscans, so it can drift and zoom

// ---- in game --------------------------------------------------------------
constexpr float kSavedAt = 1.4f;
constexpr float kIrisRadius = 1160.0f; // past the corners of the screen

constexpr const char *kTechniques[] = {
    "Logo assembly: staggered tween::Bounce strokes, a ring drawn with arc(), settling tracking",
    "Light sweep: the logo redrawn in white inside a moving, slanted stack of clip strips",
    "Gradient lettering from twelve clipped bands, over a halo of offset copies",
    "An honest loader: timed stages of uneven speed, a spring-smoothed bar, a spinner for stalls",
    "Tips that cross-fade on a timer and step by hand; key art cropped and drifted through uv",
    "Five designed screen changes (through black, slide, push, iris) that reverse in flight",
};

constexpr app::TourStep kTour[] = {
    {0.4f, action_bit(Action::confirm)},                            // skip the splash
    {2.85f, action_bit(Action::confirm), Direction::none, "title"}, // mid-sweep
    {0.6f, 0, Direction::right},
    {0.3f, 0, Direction::right},
    {1.0f, action_bit(Action::confirm), Direction::none, "profiles"},
    {2.3f, 0, Direction::right, "loading"}, // in the first stall
    {5.4f, action_bit(Action::confirm), Direction::none, "ready"},
    {2.6f, action_bit(Action::menu), Direction::none, "ingame"},
    {1.6f, 0, Direction::none},
};

enum class Screen : std::uint8_t
{
    splash,
    title,
    profiles,
    loading,
    game,
    count,
};

// How one screen hands over to the next.
enum class Move : std::uint8_t
{
    none,
    black, // fade through black: the backdrop changes underneath
    slide, // title <-> profiles: content slides while the sky pans
    zoom,  // profiles <-> loading: a push into (or out of) the artwork
    iris,  // loading -> game: a closing and opening circle
};

bool any_button(const InputFrame &input)
{
    return input.pressed != 0 || (input.nav != Direction::none && !input.nav_repeat);
}

// Progress of the simulated loader after `time` seconds, and the stage it is
// in (kStageCount when done).
float loader_value(float time, int *stage)
{
    float start = 0.0f;
    float from = 0.0f;
    for (int i = 0; i < kStageCount; ++i)
    {
        const Stage &s = kStages[i];
        if (time < start + s.seconds)
        {
            const float u = (time - start) / s.seconds;
            // A stall gives its small share at once and then sits there; a
            // working stage arrives in bursts, the way files of different
            // sizes do.
            const float shaped =
                s.stall ? tween::cubic_out(u * 2.2f) : u + 0.035f * std::sin(u * kTau * 3.0f);
            *stage = i;
            return tween::lerp(from, s.end, tween::clamp01(shaped));
        }
        start += s.seconds;
        from = s.end;
    }
    *stage = kStageCount;
    return 1.0f;
}

// A drop shape from a disc and a triangle: flames and the candle emblem.
void teardrop(gfx::DrawList &list, float cx, float cy, float r, Color colour)
{
    list.triangle({cx - r * 0.93f, cy - r * 2.3f, r * 1.86f, r * 2.05f}, colour);
    list.circle(cx, cy, r, colour);
}

void flame(gfx::DrawList &list, float cx, float cy, float r, Color outer, Color inner)
{
    teardrop(list, cx, cy, r, outer);
    teardrop(list, cx, cy + r * 0.32f, r * 0.5f, inner);
}

// A check mark in a box of size s centred on (cx, cy).
void check_mark(gfx::DrawList &list, float cx, float cy, float s, float thickness, Color colour)
{
    list.line(cx - s * 0.42f, cy + s * 0.02f, cx - s * 0.12f, cy + s * 0.32f, thickness, colour);
    list.line(cx - s * 0.12f, cy + s * 0.32f, cx + s * 0.44f, cy - s * 0.30f, thickness, colour);
}

class Boot final : public app::Concept
{
  public:
    explicit Boot(app::Context &context)
        : context_(context), game_(context.catalog[static_cast<std::size_t>(kGame)])
    {
        for (int i = 0; i < kCards; ++i)
            grow_[i].snap(i == focus_ ? 1.0f : 0.0f);
        ring_.snap(focus_rect(focus_));
        tint_.snap(Color::rgb(kProfiles[focus_].colour));
        for (int i = 0; i < kTips; ++i)
            dot_[i].snap(i == tip_ ? 1.0f : 0.0f);
        enter();
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "boot",
            "Launch Sequence",
            "Before the menu: studio splash, title, profiles and an honest loading screen",
            "src/concepts/boot.cpp",
            audio::SoundSet::glass,
            Color::rgb(0xffd166),
            kTechniques,
        };
        return kInfo;
    }

    // The sequence starts again from the splash; the chosen profile stays.
    void enter() override
    {
        screen_ = from_ = Screen::splash;
        move_ = Move::none;
        transition_ = {};
        age_[index(Screen::splash)] = 0.0f;
        for (tween::Bounce &piece : piece_)
            piece.snap(0.0f);
        welcomed_ = false;
        locked_ = false;
        lock_.value = 0.0f;
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        const bool reduced = context_.settings.reduced_motion;
        clock_ += dt;
        if (!reduced)
            drift_ += dt;
        age_[index(screen_)] += dt;
        transition_.update(dt);

        // Input always belongs to the screen that is arriving: a press is
        // never held back until a move has finished. The one exception is a
        // screen still hidden behind black (its age is negative until the
        // cover lifts): a press there would answer something nobody has seen.
        switch (screen_)
        {
        case Screen::splash:
            update_splash(input, feedback);
            break;
        case Screen::title:
            if (any_button(input) && age(Screen::title) >= 0.0f)
            {
                feedback.play(audio::Cue::select);
                go(Screen::profiles, Move::slide, 0.5f);
            }
            break;
        case Screen::profiles:
            update_profiles(input, feedback);
            break;
        case Screen::loading:
            update_loading(input, dt, feedback);
            break;
        case Screen::game:
            update_game(input, feedback);
            break;
        case Screen::count:
            break;
        }

        // ---- animation state ----
        for (int i = 0; i < kPieces; ++i)
        {
            const float start = kPieceStart + kPieceStep * static_cast<float>(i);
            if (reduced)
                piece_[i].snap(1.0f); // the logo is simply there
            else
            {
                piece_[i].target = age(Screen::splash) >= start ? 1.0f : 0.0f;
                piece_[i].update(dt, 13.0f, 0.62f);
            }
        }
        lock_.update(dt, 3.5f);

        const bool below = screen_ == Screen::profiles || screen_ == Screen::loading;
        camera_.target = below ? kCameraDrop : 0.0f;
        camera_.update(dt, reduced ? 40.0f : 3.2f);

        for (int i = 0; i < kCards; ++i)
        {
            grow_[i].target = i == focus_ ? 1.0f : 0.0f;
            grow_[i].update(dt, reduced ? 40.0f : 14.0f);
        }
        ring_.target(focus_rect(focus_));
        ring_.update(dt, 18.0f);
        tint_.target(Color::rgb(kProfiles[focus_].colour));
        tint_.update(dt, 8.0f);
        nudge_.update(dt, 9.0f);

        bar_.update(dt, 12.0f);
        flash_.update(dt, 2.6f);
        deny_.update(dt, 9.0f);
        stage_fade_.update(dt);
        tip_fade_.update(dt);
        spinner_.update(dt, 8.0f);
        for (int i = 0; i < kTips; ++i)
        {
            dot_[i].target = i == tip_ ? 1.0f : 0.0f;
            dot_[i].update(dt, 16.0f);
        }
    }

    void draw(app::Frame &frame) const override
    {
        const bool reduced = context_.settings.reduced_motion;
        const bool moving = transition_.running;
        const float t = moving ? transition_.progress() : 1.0f;
        Move move = moving ? move_ : Move::none;
        if (reduced && move == Move::iris)
            move = Move::black;
        // Black and iris moves hide the screen at their midpoint; that is
        // where the backdrop (there is only one per frame) changes hands.
        const bool covered = move == Move::black || move == Move::iris;
        const Screen behind = covered && t < 0.5f ? from_ : screen_;
        set_backdrop(frame, behind, moving && !covered);

        gfx::DrawList &list = frame.scene;
        const Rect full{0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight};
        const float travel = reduced ? 0.0f : 1.0f; // plain fades under reduced motion
        switch (move)
        {
        case Move::none:
            draw_screen(list, screen_, 1.0f, 1.0f, 0.0f);
            break;
        case Move::black:
        {
            draw_screen(list, behind, 1.0f, 1.0f, 0.0f);
            const float cover = t < 0.5f ? tween::smoothstep(t * 2.0f)
                                         : 1.0f - tween::smoothstep((t - 0.5f) * 2.0f);
            list.rounded_rect(full, 0, kNight.with_alpha(cover));
            break;
        }
        case Move::iris:
        {
            draw_screen(list, behind, 1.0f, 1.0f, 0.0f);
            // One very thick ring is a screen of black with a round hole.
            const float open = t < 0.5f ? 1.0f - tween::cubic_in_out(t / 0.42f)
                                        : tween::cubic_in_out((t - 0.5f) * 2.0f);
            const float hole = kIrisRadius * open;
            list.ring(kCentreX, kCentreY, kIrisRadius, kIrisRadius - hole, kNight);
            if (hole > 6.0f)
                list.ring(kCentreX, kCentreY, hole + 4.0f, 4.0f,
                          game_.accent.with_alpha(0.55f * (1.0f - open)));
            break;
        }
        case Move::slide:
        {
            // Down to the profiles, back up to the title; the old screen
            // leaves faster than the new one arrives.
            const float direction = screen_ == Screen::profiles ? 1.0f : -1.0f;
            draw_screen(list, from_, 1.0f - tween::smoothstep(t * 1.8f), 1.0f,
                        -120.0f * direction * travel * tween::cubic_in(t));
            const float arrive = tween::clamp01((t - 0.15f) / 0.85f);
            draw_screen(list, screen_, tween::smoothstep(arrive), 1.0f,
                        140.0f * direction * travel * (1.0f - tween::quint_out(arrive)));
            break;
        }
        case Move::zoom:
        {
            // Into the loading screen the cards fly past the camera and the
            // artwork settles from behind them; cancelling plays it backwards.
            const float direction = screen_ == Screen::loading ? 1.0f : -1.0f;
            draw_screen(list, from_, 1.0f - tween::smoothstep(t * 1.6f),
                        1.0f + 0.10f * direction * travel * tween::cubic_in(t), 0.0f);
            draw_screen(list, screen_, tween::smoothstep(t),
                        1.0f - 0.07f * direction * travel * (1.0f - tween::quint_out(t)), 0.0f);
            break;
        }
        }

        frame.post.mode = gfx::BackdropMode::vignette;
        frame.post.colors[0] = kNight;
        frame.post.params[0] = 0.45f;
    }

    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    static std::size_t index(Screen screen)
    {
        return static_cast<std::size_t>(screen);
    }

    float age(Screen screen) const
    {
        return age_[index(screen)];
    }

    ui::Button confirm_button() const
    {
        return context_.settings.swap_confirm ? ui::Button::circle : ui::Button::cross;
    }

    ui::Button back_button() const
    {
        return context_.settings.swap_confirm ? ui::Button::cross : ui::Button::circle;
    }

    // ---- flow ---------------------------------------------------------------

    void go(Screen next, Move move, float seconds)
    {
        if (context_.settings.reduced_motion)
            seconds = std::min(seconds, 0.4f);
        const bool covered = move == Move::black || move == Move::iris;
        // Turning round in the middle of a slide or push: start the way back
        // from where the move is, so nothing jumps.
        float lead = 0.0f;
        if (transition_.running && next == from_ && move == move_ && !covered)
            lead = 1.0f - transition_.progress();
        from_ = screen_;
        screen_ = next;
        move_ = move;
        transition_.start(seconds);
        transition_.elapsed = lead * seconds;
        // The entrance of the new screen waits until it can be seen.
        age_[index(next)] = covered ? -0.5f * seconds : -0.12f * (1.0f - lead);
    }

    void update_splash(const InputFrame &input, app::Feedback &feedback)
    {
        const bool reduced = context_.settings.reduced_motion;
        const float t = age(Screen::splash);
        if (!welcomed_ && t >= (reduced ? 0.15f : kWelcomeAt))
        {
            welcomed_ = true;
            feedback.play(audio::Cue::welcome);
        }
        if (!locked_ && t >= kRingStart + kRingSeconds)
        {
            locked_ = true;
            if (!reduced)
                lock_.trigger();
        }
        if (any_button(input))
        {
            feedback.play(audio::Cue::tab);
            go(Screen::title, Move::black, 0.7f);
        }
        else if (t >= kSplashSeconds)
        {
            go(Screen::title, Move::black, 0.9f);
        }
    }

    void update_profiles(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav == Direction::left || input.nav == Direction::right)
        {
            const int next = focus_ + (input.nav == Direction::right ? 1 : -1);
            if (next >= 0 && next < kCards)
            {
                focus_ = next;
                feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(focus_rect(next).cx()));
            }
            else if (!input.nav_repeat)
            {
                feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
                feedback.rumble(0.25f, 0.05f);
                nudge_.trigger();
                nudge_direction_ = input.nav == Direction::right ? 1.0f : -1.0f;
            }
        }
        if (input.is_pressed(Action::confirm))
        {
            profile_ = focus_;
            load_time_ = 0.0f;
            ready_ = false;
            bar_.snap(0.0f);
            flash_.value = 0.0f;
            stage_ = previous_stage_ = 0;
            tip_clock_ = 0.0f;
            spinner_.snap(0.45f);
            feedback.play(audio::Cue::open);
            go(Screen::loading, Move::zoom, 0.55f);
        }
        else if (input.is_pressed(Action::back))
        {
            feedback.play(audio::Cue::back);
            go(Screen::title, Move::slide, 0.5f);
        }
    }

    void step_tip(int direction)
    {
        previous_tip_ = tip_;
        tip_ = (tip_ + direction + kTips) % kTips;
        tip_direction_ = static_cast<float>(direction);
        tip_fade_.start(context_.settings.reduced_motion ? 0.2f : 0.45f);
        tip_clock_ = 0.0f;
    }

    void update_loading(const InputFrame &input, float dt, app::Feedback &feedback)
    {
        if (input.nav == Direction::left || input.nav == Direction::right)
        {
            const int direction = input.nav == Direction::right ? 1 : -1;
            step_tip(direction);
            // The tips form a ring, so there is no end to refuse at; the
            // pitch says which of the six is up.
            feedback.play(audio::Cue::tab, 0.94f + 0.03f * static_cast<float>(tip_),
                          0.3f * static_cast<float>(direction));
        }
        if (input.is_pressed(Action::confirm))
        {
            if (ready_)
            {
                feedback.play(audio::Cue::launch);
                feedback.rumble(0.7f, 0.18f);
                saved_ = false;
                go(Screen::game, Move::iris, 1.1f);
                return;
            }
            // Not yet: the percentage answers, quietly.
            feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.5f);
            deny_.trigger();
        }
        else if (input.is_pressed(Action::back))
        {
            feedback.play(audio::Cue::back);
            go(Screen::profiles, Move::zoom, 0.5f);
            return;
        }

        // The "loader" is only a clock: the frame never waits for it.
        load_time_ += dt;
        int stage = 0;
        bar_.target = loader_value(load_time_, &stage);
        if (stage != stage_ && !ready_)
        {
            previous_stage_ = stage_;
            stage_ = stage;
            stage_fade_.start(0.3f);
        }
        // Ready is announced when the bar the player sees is full, not when
        // the number behind it is.
        if (!ready_ && stage == kStageCount && bar_.value > 0.99f)
        {
            ready_ = true;
            ready_age_ = 0.0f;
            flash_.trigger();
            feedback.play(audio::Cue::resume);
        }
        if (ready_)
            ready_age_ += dt;
        const bool stalled = stage < kStageCount && kStages[stage].stall;
        spinner_.target = ready_ ? 0.0f : stalled ? 1.0f : 0.45f;

        tip_clock_ += dt;
        if (tip_clock_ >= kTipSeconds)
            step_tip(1);
    }

    void update_game(const InputFrame &input, app::Feedback &feedback)
    {
        if (!saved_ && age(Screen::game) >= kSavedAt)
        {
            saved_ = true;
            feedback.play(audio::Cue::saved, 1.0f, 0.4f);
        }
        if (input.is_pressed(Action::menu) && age(Screen::game) >= 0.0f)
        {
            feedback.play(audio::Cue::back);
            go(Screen::title, Move::black, 0.9f);
        }
    }

    // ---- geometry -----------------------------------------------------------

    // Where a card sits for a given set of growth values: the row stays
    // centred while one card widens and another narrows.
    static Rect card_rect(int card, const float *grow)
    {
        float total =
            static_cast<float>(kCards) * kCardW + static_cast<float>(kCards - 1) * kCardGap;
        for (int i = 0; i < kCards; ++i)
            total += kCardGrowW * grow[i];
        float x = kCentreX - total * 0.5f;
        for (int i = 0; i < card; ++i)
            x += kCardW + kCardGrowW * grow[i] + kCardGap;
        const float g = grow[card];
        return {x, kCardTop - kCardRise * g, kCardW + kCardGrowW * g, kCardH + kCardGrowH * g};
    }

    // The settled rectangle of a focused card: what the ring glides to.
    static Rect focus_rect(int card)
    {
        float grow[kCards] = {};
        grow[card] = 1.0f;
        return card_rect(card, grow);
    }

    // ---- drawing ------------------------------------------------------------

    void set_backdrop(app::Frame &frame, Screen screen, bool crossing) const
    {
        gfx::BackdropSpec &spec = frame.backdrop;
        spec.time = drift_;
        switch (screen)
        {
        case Screen::splash:
        {
            const float lit =
                tween::cubic_in_out((age(Screen::splash) - kRingStart) / kRingSeconds);
            spec.mode = gfx::BackdropMode::gradient;
            spec.colors[0] = Color::rgb(0x050507);
            spec.colors[1] = Color::rgb(0x0c0a0f);
            spec.colors[2] = kStudioEmber;
            spec.params[0] = 0.5f;
            spec.params[1] = kEmblemY / gfx::kVirtualHeight;
            spec.params[2] = 0.07f * lit;
            break;
        }
        case Screen::loading:
            // The key art covers everything, so the sky only has to be there
            // while a move shows through it.
            if (!crossing)
            {
                spec.mode = gfx::BackdropMode::gradient;
                spec.colors[0] = kNight;
                spec.colors[1] = game_.dark;
                break;
            }
            [[fallthrough]];
        case Screen::title:
        case Screen::profiles:
            spec.mode = gfx::BackdropMode::stars;
            spec.colors[0] = gfx::mix(game_.dark, kNight, 0.55f);
            spec.colors[1] = gfx::mix(game_.dark, game_.mid, 0.3f);
            spec.colors[2] = game_.mid;
            spec.colors[3] = gfx::mix(game_.mid, game_.accent, 0.4f);
            spec.params[0] = drift_ * 0.05f; // a slow pan keeps the stars alive
            spec.params[1] = camera_.value;
            break;
        case Screen::game:
        case Screen::count:
            spec.mode = gfx::BackdropMode::vista;
            spec.colors[0] = gfx::mix(game_.dark, game_.mid, 0.6f);
            spec.colors[1] = gfx::mix(game_.mid, game_.accent, 0.62f);
            spec.colors[2] = gfx::mix(game_.mid, game_.dark, 0.25f);
            spec.colors[3] = gfx::mix(game_.dark, kNight, 0.35f);
            spec.params[0] = 1.0f;
            break;
        }
    }

    void draw_screen(gfx::DrawList &list, Screen screen, float alpha, float scale, float dy) const
    {
        if (alpha <= 0.004f)
            return;
        list.push_opacity(alpha);
        list.push_transform(scale, kCentreX, kCentreY, 0.0f, dy);
        switch (screen)
        {
        case Screen::splash:
            draw_splash(list);
            break;
        case Screen::title:
            draw_title(list);
            break;
        case Screen::profiles:
            draw_profiles(list);
            break;
        case Screen::loading:
            draw_loading(list);
            break;
        case Screen::game:
        case Screen::count:
            draw_game(list);
            break;
        }
        list.pop_transform();
        list.pop_opacity();
    }

    // One word, a letter at a time: each letter has its own fade and rise,
    // and the spacing of the whole word is animated by the caller.
    void draw_wordmark(gfx::DrawList &list, const std::string &word, float baseline, float size,
                       float tracking, float elapsed) const
    {
        const ui::FontRef &font = context_.fonts.display;
        const bool reduced = context_.settings.reduced_motion;
        float total = 0.0f;
        for (char c : word)
            total += font.measure(std::string_view(&c, 1), size);
        total += tracking * static_cast<float>(word.size() > 0 ? word.size() - 1 : 0);
        float x = kCentreX - total * 0.5f;
        for (std::size_t i = 0; i < word.size(); ++i)
        {
            const std::string_view letter(&word[i], 1);
            const float a =
                reduced ? 1.0f
                        : tween::cubic_out((elapsed - static_cast<float>(i) * kLetterStep) / 0.32f);
            if (a > 0.0f && word[i] != ' ')
                ui::text(list, font, letter, x, baseline + 14.0f * (1.0f - a), size,
                         kStudioInk.with_alpha(a));
            x += font.measure(letter, size) + tracking;
        }
    }

    void draw_splash(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        const float t = std::max(0.0f, age(Screen::splash));
        const float cx = kCentreX;
        const float cy = kEmblemY;
        const float ring = reduced ? 1.0f : tween::cubic_in_out((t - kRingStart) / kRingSeconds);

        // Under reduced motion nothing assembles: the finished logo fades in.
        list.push_opacity(reduced ? tween::cubic_out(t / 0.4f) : 1.0f);

        // The bloom grows with the ring and swells once when the pieces lock.
        list.glow({cx - 70, cy - 70, 140, 140}, 70, 250,
                  kStudioEmber.with_alpha(ring * (0.09f + 0.03f * ui::breathe(clock_, 3.2f)) +
                                          0.14f * lock_.value));

        list.push_transform(1.0f + 0.035f * lock_.value, cx, cy, 0, 0);
        list.arc(cx, cy, kEmblemRadius, 6, 0.0f, kTau * ring, kStudioInk);
        // A hairline drawn the other way round closes at the same moment.
        list.arc(cx, cy, kEmblemRadius + 18, 2, kTau * (1.0f - ring), kTau * ring,
                 kStudioInk.with_alpha(0.3f));

        // The monogram: four strokes, each arriving from its own side.
        struct Stroke
        {
            float x1, y1, x2, y2; // relative to the emblem centre
            float from_x, from_y; // where it flies in from
        };
        constexpr Stroke kStrokes[kPieces - 1] = {
            {-50, 46, -50, -42, -150, 0},
            {-50, -42, 0, 18, -70, -150},
            {0, 18, 50, -42, 70, -150},
            {50, -42, 50, 46, 150, 0},
        };
        for (int i = 0; i < kPieces - 1; ++i)
        {
            const Stroke &s = kStrokes[i];
            const float p = piece_[i].value;
            const float ox = cx + s.from_x * (1.0f - p);
            const float oy = cy + s.from_y * (1.0f - p);
            list.line(ox + s.x1, oy + s.y1, ox + s.x2, oy + s.y2, 13,
                      kStudioInk.with_alpha(tween::clamp01(p * 1.5f)));
        }
        // ... and the ember drops in last, with the most bounce to show.
        const float ember = piece_[kPieces - 1].value;
        list.glow({cx - 6, cy - 70 - 6, 12, 12}, 6, 26,
                  kStudioEmber.with_alpha(0.5f * tween::clamp01(ember)));
        list.circle(cx, cy - 70 - 80.0f * (1.0f - ember), 9,
                    kStudioEmber.with_alpha(tween::clamp01(ember * 1.5f)));
        list.pop_transform();

        const float settle = reduced ? 1.0f : tween::expo_out((t - kWordStart) / 1.3f);
        draw_wordmark(list, ui::upper(game_.studio), cy + 232, kWordSize,
                      tween::lerp(kWordTrackingWide, kWordTracking, settle), t - kWordStart);

        const float presents = reduced ? 1.0f : tween::cubic_out((t - kPresentsAt) / 0.45f);
        const float rule = 150.0f * presents;
        list.gradient_rect_h({cx - rule, cy + 266, rule, 2}, 1, kClear,
                             kStudioInk.with_alpha(0.4f * presents));
        list.gradient_rect_h({cx, cy + 266, rule, 2}, 1, kStudioInk.with_alpha(0.4f * presents),
                             kClear);
        ui::text(list, fonts.semibold, "PRESENTS", cx + 3, cy + 306, 16,
                 kStudioInk.with_alpha(0.6f * presents), gfx::Align::center, 6.0f);
        list.pop_opacity();

        list.push_opacity(0.75f * tween::cubic_out((t - 0.5f) / 0.5f));
        const ui::Hint hints[] = {{confirm_button(), "Skip"}};
        ui::draw_hints(list, fonts, ui::GlyphStyle::dark(), hints, 1, kRight, true);
        list.pop_opacity();
    }

    void draw_title(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        const float t = std::max(0.0f, age(Screen::title));
        const Color gold = game_.accent;
        const Color pale = gfx::mix(gold, Color::rgb(0xffffff), 0.6f);
        const Color amber = gfx::mix(gold, Color::rgb(0xd9781f), 0.5f);

        // The logo is the catalogue title, scaled down if it is a long one.
        const std::string logo = ui::upper(game_.title);
        float size = kLogoSize;
        float tracking = kLogoTracking;
        float width = fonts.display.measure(logo, size, tracking);
        if (width > kLogoMaxWidth)
        {
            size *= kLogoMaxWidth / width;
            tracking *= kLogoMaxWidth / width;
            width = fonts.display.measure(logo, size, tracking);
        }
        const float x = kCentreX - width * 0.5f;
        const float baseline = kLogoBaseline;
        const float top = baseline - size * 0.76f; // cap height of the display face
        const float height = size * 0.80f;
        const auto letters = [&](float dx, float dy, Color colour)
        {
            ui::text(list, fonts.display, logo, x + dx, baseline + dy, size, colour,
                     gfx::Align::left, tracking);
        };

        // ---- candle emblem and its two rules ----
        const float emblem = tween::stagger(t, 0, 0.1f, 0.7f);
        const float flicker =
            reduced ? 0.0f : 0.03f * std::sin(clock_ * 7.0f) + 0.02f * std::sin(clock_ * 11.3f);
        const float fy = top - 74.0f + 20.0f * (1.0f - emblem);
        list.push_opacity(emblem);
        list.glow({kCentreX - 10, fy - 26, 20, 30}, 10, 70,
                  gold.with_alpha(0.32f + 0.10f * ui::breathe(clock_, 3.0f)));
        flame(list, kCentreX, fy, 17.0f * (1.0f + flicker), gold, pale);
        const float rule = 260.0f * emblem;
        list.gradient_rect_h({kCentreX - 44 - rule, fy - 9, rule, 2}, 1, kClear,
                             gold.with_alpha(0.7f));
        list.gradient_rect_h({kCentreX + 44, fy - 9, rule, 2}, 1, gold.with_alpha(0.7f), kClear);
        list.pop_opacity();

        // ---- the logo, back to front ----
        const float arrive = tween::quint_out((t - 0.15f) / 0.9f);
        list.push_opacity(tween::clamp01(arrive * 1.2f));
        list.push_transform(1.0f + (reduced ? 0.0f : 0.06f * (1.0f - arrive)), kCentreX,
                            baseline - size * 0.38f, 0, 0);
        // 1. bloom: light spilling far around the word
        list.glow({x + 60, top + height * 0.25f, width - 120, height * 0.5f}, height * 0.25f, 150,
                  gold.with_alpha(0.10f + 0.04f * ui::breathe(clock_, 4.0f)));
        // 2. a dark copy below gives the letters weight
        letters(0.0f, 9.0f, kNight.with_alpha(0.55f));
        // 3. halo: faint copies on a circle read as a soft glow hugging the glyphs
        for (int i = 0; i < 8; ++i)
        {
            const float angle = kTau * static_cast<float>(i) / 8.0f;
            letters(std::sin(angle) * 5.0f, std::cos(angle) * 5.0f, gold.with_alpha(0.09f));
        }
        // 4. fill: text cannot take a gradient, but it can be drawn in slices.
        //    Each band clips the word and paints it one step further from
        //    pale to gold.
        for (int band = 0; band < kLogoBands; ++band)
        {
            const float slice = height / static_cast<float>(kLogoBands);
            Rect clip{x - 40, top + slice * static_cast<float>(band), width + 80, slice + 1.0f};
            if (band == 0) // accents above the capitals belong to the first band
            {
                clip.y -= size * 0.3f;
                clip.h += size * 0.3f;
            }
            if (band == kLogoBands - 1) // descenders to the last
                clip.h += size * 0.3f;
            list.push_clip(clip);
            letters(0.0f, 0.0f,
                    gfx::mix(pale, amber,
                             static_cast<float>(band) / static_cast<float>(kLogoBands - 1)));
            list.pop_clip();
        }
        // 5. the light sweep: a band of white that exists only where there
        //    are letters. Rows shifted sideways make the band lean.
        const float cycle = std::fmod(t + kSweepPeriod - kSweepFirst, kSweepPeriod) / kSweepSeconds;
        if (!reduced && cycle < 1.0f)
        {
            const float band_width = kSweepStrip * static_cast<float>(kSweepStrips);
            const float centre =
                tween::lerp(x - band_width, x + width + band_width, tween::smoothstep(cycle));
            const float row_height = (height + size * 0.1f) / static_cast<float>(kSweepRows);
            for (int row = 0; row < kSweepRows; ++row)
            {
                const float lean = kSweepLean * static_cast<float>(kSweepRows - 1 - row);
                for (int strip = 0; strip < kSweepStrips; ++strip)
                {
                    list.push_clip({centre + lean - band_width * 0.5f +
                                        kSweepStrip * static_cast<float>(strip),
                                    top - size * 0.05f + row_height * static_cast<float>(row),
                                    kSweepStrip, row_height});
                    letters(0.0f, 0.0f, Color::rgb(0xffffff, kSweepProfile[strip]));
                    list.pop_clip();
                }
            }
        }
        list.pop_transform();
        list.pop_opacity();

        // ---- the plate under the logo: it opens from the middle ----
        const float plate = tween::cubic_out((t - 0.5f) / 0.5f);
        const float opened = tween::quint_out((t - 0.5f) / 0.8f);
        const float half = (width * 0.5f + 20.0f) * opened;
        const Rect left{kCentreX - half, baseline + 40, half, 46};
        const Rect right{kCentreX, baseline + 40, half, 46};
        list.push_opacity(plate);
        list.gradient_rect_h(left, 0, kClear, gold.with_alpha(0.20f));
        list.gradient_rect_h(right, 0, gold.with_alpha(0.20f), kClear);
        for (float edge : {left.y, left.y + left.h - 1.5f})
        {
            list.gradient_rect_h({left.x, edge, half, 1.5f}, 0, kClear, gold.with_alpha(0.6f));
            list.gradient_rect_h({right.x, edge, half, 1.5f}, 0, gold.with_alpha(0.6f), kClear);
        }
        ui::text(list, fonts.semibold, "THE LAST CANDLE", kCentreX + 5, left.y + 31, 22,
                 kInk.with_alpha(tween::cubic_out((t - 0.8f) / 0.5f)), gfx::Align::center, 10.0f);
        list.pop_opacity();

        // ---- the prompt breathes; the small print does not ----
        const float prompt = tween::cubic_out((t - 1.1f) / 0.6f);
        ui::text(list, fonts.semibold, "Press any button", kCentreX, 868, 30,
                 kInk.with_alpha(prompt * (0.45f + 0.55f * ui::breathe(clock_))),
                 gfx::Align::center, 1.0f);

        const float print = tween::cubic_out((t - 1.3f) / 0.6f);
        char line[96];
        ui::text(list, fonts.mono, "v1.4.2  build 2048", kMargin, 1016, 18,
                 kInk.with_alpha(0.5f * print));
        std::snprintf(line, sizeof(line), "\xC2\xA9 2026 %s. A work of fiction.", game_.studio);
        ui::text(list, fonts.regular, line, kRight, 1016, 18, kInk.with_alpha(0.5f * print),
                 gfx::Align::right);
    }

    // A profile picture from shapes: a disc in the profile's colour with one
    // emblem. Sizes are in units of the radius, so it scales with the card.
    void draw_avatar(gfx::DrawList &list, int card, float cx, float cy, float r) const
    {
        const Color tone = Color::rgb(kProfiles[card].colour);
        const Color base = gfx::mix(tone, game_.dark, 0.66f);
        if (card == kAddCard)
        {
            // An empty slot: a dashed outline that turns slowly, and a plus.
            constexpr int kDashes = 14;
            for (int i = 0; i < kDashes; ++i)
                list.arc(cx, cy, r, r * 0.07f,
                         drift_ * 0.25f + kTau * static_cast<float>(i) / kDashes,
                         kTau / kDashes * 0.5f, kInk.with_alpha(0.5f));
            list.line(cx - r * 0.3f, cy, cx + r * 0.3f, cy, r * 0.11f, kInk);
            list.line(cx, cy - r * 0.3f, cx, cy + r * 0.3f, r * 0.11f, kInk);
            return;
        }
        list.circle(cx, cy, r, base);
        list.ring(cx, cy, r, std::max(2.0f, r * 0.06f), tone);
        switch (card)
        {
        case 0: // flame
            flame(list, cx, cy + r * 0.24f, r * 0.34f, tone, gfx::mix(tone, kInk, 0.7f));
            break;
        case 1: // crescent: a disc with a bite in the base colour
            list.circle(cx - r * 0.04f, cy, r * 0.48f, tone);
            list.circle(cx + r * 0.16f, cy - r * 0.12f, r * 0.42f, base);
            break;
        case 2: // star
            list.star(cx, cy + r * 0.02f, r * 0.54f, tone);
            break;
        default: // two peaks and a low sun
            list.circle(cx - r * 0.34f, cy - r * 0.32f, r * 0.13f, tone);
            list.triangle({cx - r * 0.64f, cy - r * 0.08f, r * 0.8f, r * 0.5f},
                          gfx::mix(tone, base, 0.45f));
            list.triangle({cx - r * 0.26f, cy - r * 0.4f, r * 0.9f, r * 0.82f}, tone);
            break;
        }
    }

    void draw_card(gfx::DrawList &list, int card, const Rect &r, float g, float in) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const Profile &p = kProfiles[card];
        const Color tone = Color::rgb(p.colour);
        const float cx = r.cx();
        char text[64];

        // Body: a dark pane that takes a little of the profile's colour as
        // it comes forward.
        list.gradient_rect(r, kCardRadius,
                           gfx::mix(gfx::mix(game_.dark, game_.mid, 0.42f), tone, 0.05f + 0.13f * g)
                               .with_alpha(0.90f),
                           gfx::mix(game_.dark, kNight, 0.25f).with_alpha(0.90f));
        list.bordered_rect(r, kCardRadius, kClear, 1.5f, kInk.with_alpha(0.12f + 0.08f * g));

        draw_avatar(list, card, cx, r.y + 100 + 6 * g, 54.0f * (1.0f + 0.12f * g));
        ui::text(list, fonts.semibold, p.name, cx, r.y + 214 + 12 * g, 30,
                 kInk.with_alpha(0.82f + 0.18f * g), gfx::Align::center);
        ui::text(list, fonts.regular, p.last_played, cx, r.y + 248 + 12 * g, 21,
                 kInk.with_alpha(0.6f), gfx::Align::center);

        const float row = r.y + 296 + 14 * g;
        if (card == kAddCard)
        {
            ui::text(list, fonts.semibold, "EMPTY SLOT", cx + 1.5f, row + 6, 15,
                     kInk.with_alpha(0.45f), gfx::Align::center, 3.0f);
        }
        else
        {
            // The small progress ring fills as the card arrives.
            list.arc(cx - 40, row, 20, 5, 0.0f, kTau, kInk.with_alpha(0.15f));
            list.arc(cx - 40, row, 20, 5, 0.0f, kTau * p.progress * in, tone);
            std::snprintf(text, sizeof(text), "%d%%", static_cast<int>(p.progress * 100.0f + 0.5f));
            ui::text(list, fonts.mono, text, cx - 8, row + 8, 22, kInk.with_alpha(0.9f));
        }

        // Details exist only on the focused card. They sit at fixed offsets
        // and the growing card uncovers them, so they are clipped to it.
        const float details = tween::clamp01((g - 0.35f) / 0.65f);
        if (details <= 0.01f)
            return;
        const float y = r.y + 362;
        list.push_clip(r);
        list.push_opacity(details);
        list.gradient_rect_h({r.x + 28, y, r.w * 0.5f - 28, 1.5f}, 0, kClear,
                             kInk.with_alpha(0.3f));
        list.gradient_rect_h({cx, y, r.w * 0.5f - 28, 1.5f}, 0, kInk.with_alpha(0.3f), kClear);
        ui::text(list, fonts.semibold, ui::upper(p.chapter), cx + 1.5f, y + 34, 15, tone,
                 gfx::Align::center, 3.0f);
        ui::text(list, fonts.semibold, fonts.semibold.font->fit(p.area, 23, r.w - 40), cx, y + 66,
                 23, kInk, gfx::Align::center);
        if (card == kAddCard)
            std::snprintf(text, sizeof(text), "Start from the beginning");
        else
            std::snprintf(text, sizeof(text), "%d h played", p.hours);
        ui::text(list, fonts.regular, text, cx, y + 96, 20, kInk.with_alpha(0.6f),
                 gfx::Align::center);
        list.pop_opacity();
        list.pop_clip();
    }

    void draw_profiles(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const float t = std::max(0.0f, age(Screen::profiles));
        const Color tone = tint_.value();

        const float head = tween::stagger(t, 0, 0.07f, 0.5f);
        list.push_opacity(head);
        ui::text(list, fonts.semibold, ui::upper(game_.title), kCentreX + 3, 204 - 14 * (1 - head),
                 18, game_.accent, gfx::Align::center, 6.0f);
        ui::text(list, fonts.display, "Who is playing?", kCentreX, 282 - 14 * (1 - head), 64, kInk,
                 gfx::Align::center);
        list.pop_opacity();

        float grow[kCards];
        for (int i = 0; i < kCards; ++i)
            grow[i] = grow_[i].value;
        const auto entrance = [&](int card) { return tween::stagger(t, 2 + card, 0.07f, 0.5f); };
        const auto placed = [&](int card)
        {
            Rect r = card_rect(card, grow);
            r.y += 44.0f * (1.0f - entrance(card));
            return r;
        };
        for (int i = 0; i < kCards; ++i)
        {
            if (i == focus_)
                continue; // drawn last, on top of its neighbours
            list.push_opacity(entrance(i));
            draw_card(list, i, placed(i), grow[i], entrance(i));
            list.pop_opacity();
        }

        // The focused card: lifted, lit in its own colour, ringed. The ring
        // is its own spring, so it glides while the cards change size.
        const float in = entrance(focus_);
        const float nudge = ui::shake(nudge_.value, clock_, 16.0f, 8.0f) * nudge_direction_;
        Rect card = placed(focus_);
        card.x += nudge;
        Rect ring = ring_.value();
        ring.x += nudge;
        ring.y += 44.0f * (1.0f - in);
        list.push_opacity(in);
        list.shadow({card.x, card.y + 22, card.w, card.h}, kCardRadius, 44,
                    Color::rgb(0x000000, 0.55f));
        list.glow(ring, kCardRadius, 34, tone.with_alpha(0.26f + 0.12f * ui::breathe(clock_)));
        draw_card(list, focus_, card, grow[focus_], in);
        list.bordered_rect(ring.inset(-7), kCardRadius + 7, kClear, 3.5f, tone);
        list.pop_opacity();

        list.push_opacity(tween::stagger(t, 8, 0.07f, 0.5f));
        const ui::Hint hints[] = {{confirm_button(), "Select"}, {back_button(), "Back"}};
        ui::draw_hints(list, fonts, ui::GlyphStyle::dark(), hints, 2, kRight, true);
        list.pop_opacity();
    }

    // A line of text that fades and slides a few pixels (for cross-fades).
    void draw_stage(gfx::DrawList &list, int stage, float x, float baseline, float alpha,
                    float dy) const
    {
        if (alpha <= 0.01f)
            return;
        const char *name = stage >= kStageCount ? "Ready" : kStages[stage].name;
        ui::text(list, context_.fonts.semibold, ui::upper(name), x, baseline + dy, 17,
                 kInk.with_alpha(0.85f * alpha), gfx::Align::left, 3.5f);
    }

    void draw_tip(gfx::DrawList &list, int tip, float alpha, float dx) const
    {
        if (alpha <= 0.01f)
            return;
        ui::paragraph(list, context_.fonts.regular, kTipText[tip], kMargin + dx, 800, 28, 800, 40,
                      kInk.with_alpha(0.92f * alpha), 2);
    }

    void draw_loading(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        const float t = std::max(0.0f, age(Screen::loading));
        const Profile &profile = kProfiles[profile_];
        const Color gold = game_.accent;
        const ui::GlyphStyle style = ui::GlyphStyle::dark();
        char text[64];

        // ---- key art ----
        // A wide window into the square cover, between the studio line at
        // its top and the title at its foot. Moving the window by a few
        // texels over half a minute is the drift (drift_ stands still under
        // reduced motion).
        constexpr float kWindow = 0.94f;
        constexpr float kWindowH = kWindow * 9.0f / 16.0f;
        const float u = 0.014f * (0.5f + 0.5f * std::sin(drift_ * 0.11f));
        const float v = 0.125f + 0.02f * (0.5f + 0.5f * std::sin(drift_ * 0.083f + 1.3f));
        // The rectangle is larger than the screen and sits off-centre, which
        // puts the cover's emblem in the right half and leaves the left for
        // text. The overscan also keeps the edges out of sight while the
        // screen zooms in.
        const Rect art{-80, -300, gfx::kVirtualWidth * kArtScale, gfx::kVirtualHeight * kArtScale};
        // Covers are canvases, stored bottom row first, so a positive uv
        // height shows the window upside down. That is wanted here: the
        // cover's orbit lines would cross the text at the bottom, and turned
        // over they arc across the empty top instead.
        list.image(game_.cover, art, {u, 1.0f - v - kWindowH, kWindow, kWindowH},
                   Color::rgb(0xd8d2cc));
        // Scrims only where the text is: a short one from the left edge and
        // a tall one from below. A gradient laid over the flat, bright part
        // of the art would show as bands, so none reaches it.
        const Color shade = gfx::mix(game_.dark, kNight, 0.6f);
        list.gradient_rect_h({art.x, art.y, 660 - art.x, art.h}, 0, shade.with_alpha(0.7f),
                             shade.with_alpha(0.0f));
        // (Two ramps that start at different heights: one long ramp begins
        // with a visible edge where it meets the art.)
        list.gradient_rect({art.x, 400, art.w, art.y + art.h - 400}, 0, shade.with_alpha(0.0f),
                           shade.with_alpha(0.6f));
        list.gradient_rect({art.x, 640, art.w, art.y + art.h - 640}, 0, shade.with_alpha(0.0f),
                           shade.with_alpha(0.92f));

        // ---- top line: the game, and who is playing ----
        const float top = tween::stagger(t, 0, 0.08f, 0.5f);
        list.push_opacity(top);
        ui::text(list, fonts.semibold, ui::upper(game_.title), kMargin, 112, 20,
                 kInk.with_alpha(0.8f), gfx::Align::left, 6.0f);
        draw_avatar(list, profile_, kRight - 24, 104, 24);
        ui::text(list, fonts.semibold, profile_ == kAddCard ? "New profile" : profile.name,
                 kRight - 64, 113, 24, kInk.with_alpha(0.9f), gfx::Align::right);
        list.pop_opacity();

        // ---- where we are going ----
        const float place = tween::stagger(t, 1, 0.08f, 0.6f);
        list.push_opacity(place);
        ui::text(list, fonts.semibold, ui::upper(profile.chapter), kMargin + 2,
                 606 + 16 * (1 - place), 20, gold, gfx::Align::left, 5.0f);
        ui::text(list, fonts.display, profile.area, kMargin - 3, 690 + 16 * (1 - place), 76, kInk);
        list.pop_opacity();

        // ---- the tip: label, dots, and two texts while one replaces the other ----
        const float tips = tween::stagger(t, 2, 0.08f, 0.6f);
        list.push_opacity(tips);
        const float label = ui::text(list, fonts.semibold, "TRAVELLER'S TIP", kMargin + 1, 754, 16,
                                     kInk.with_alpha(0.6f), gfx::Align::left, 4.0f);
        float dot_x = kMargin + label + 28;
        for (int i = 0; i < kTips; ++i)
        {
            const float on = dot_[i].value;
            const float w = 8.0f + 18.0f * on; // the current dot stretches into a pill
            list.rounded_rect({dot_x, 744, w, 8}, 4,
                              gfx::mix(kInk.with_alpha(0.3f), gold, tween::clamp01(on)));
            dot_x += w + 8.0f;
        }
        if (tip_fade_.running)
        {
            const float f = tip_fade_.progress();
            const float slide = reduced ? 0.0f : 28.0f * tip_direction_;
            draw_tip(list, previous_tip_, 1.0f - tween::smoothstep(f * 2.0f),
                     -slide * tween::cubic_in(tween::clamp01(f * 2.0f)));
            const float arrive = tween::clamp01((f - 0.3f) / 0.7f);
            draw_tip(list, tip_, tween::smoothstep(arrive),
                     slide * (1.0f - tween::quint_out(arrive)));
        }
        else
        {
            draw_tip(list, tip_, 1.0f, 0.0f);
        }
        list.pop_opacity();

        // ---- the loader ----
        const float loader = tween::stagger(t, 3, 0.08f, 0.6f);
        const float shown = tween::clamp01(bar_.value);
        const float ready = ready_ ? tween::cubic_out(ready_age_ / 0.4f) : 0.0f;
        list.push_opacity(loader);

        // Spinner: always turning, brighter and wider in a stall, and it
        // resolves into a check mark when the work is done.
        const float spin_x = kMargin + 11;
        const float spin_y = 900;
        const float busy = spinner_.value;
        if (ready < 1.0f)
        {
            list.arc(spin_x, spin_y, 11, 3, 0.0f, kTau, kInk.with_alpha(0.14f * (1.0f - ready)));
            list.arc(spin_x, spin_y, 11, 3, clock_ * 5.5f,
                     1.3f + 1.5f * busy + 0.5f * std::sin(clock_ * 2.3f),
                     gfx::mix(kInk, gold, busy).with_alpha((0.5f + 0.5f * busy) * (1.0f - ready)));
        }
        if (ready > 0.0f)
            check_mark(list, spin_x, spin_y, 22.0f * tween::back_out(ready), 3.5f,
                       gold.with_alpha(ready));

        if (stage_fade_.running)
        {
            const float f = stage_fade_.progress();
            draw_stage(list, previous_stage_, kMargin + 36, 906, 1.0f - tween::smoothstep(f * 2.0f),
                       -8.0f * f);
            draw_stage(list, stage_, kMargin + 36, 906, tween::smoothstep(f), 8.0f * (1.0f - f));
        }
        else
        {
            draw_stage(list, stage_, kMargin + 36, 906, 1.0f, 0.0f);
        }

        // The number is set in the mono face so it does not shuffle sideways.
        // It never claims 100 before the bar is full.
        const int percent = ready_ ? 100 : std::min(99, static_cast<int>(shown * 100.0f));
        std::snprintf(text, sizeof(text), "%d%%", percent);
        ui::text(list, fonts.mono, text, kRight + ui::shake(deny_.value, clock_, 8.0f, 10.0f), 908,
                 24, gfx::mix(kInk, gold, std::max(ready, deny_.value)), gfx::Align::right);

        const Rect track{kMargin, kBarY, kRight - kMargin, kBarH};
        const Rect fill{track.x, track.y, std::max(kBarH, track.w * shown), track.h};
        list.rounded_rect(track, kBarH * 0.5f, kInk.with_alpha(0.16f));
        list.glow(fill, kBarH * 0.5f, 14 + 22 * flash_.value,
                  gold.with_alpha(0.22f + 0.5f * flash_.value));
        list.gradient_rect_h(fill, kBarH * 0.5f, gfx::mix(gold, game_.mid, 0.45f), gold);
        // A glint runs along the filled part, so the bar is alive even
        // while the number stands still.
        const float glint = std::fmod(clock_, 1.8f) / 1.8f;
        const float glint_x = fill.x - 200.0f + (fill.w + 200.0f) * glint;
        list.push_clip(fill);
        list.gradient_rect_h({glint_x, fill.y, 100, fill.h}, 0, Color::rgb(0xffffff, 0.0f),
                             Color::rgb(0xffffff, 0.65f));
        list.gradient_rect_h({glint_x + 100, fill.y, 100, fill.h}, 0, Color::rgb(0xffffff, 0.65f),
                             Color::rgb(0xffffff, 0.0f));
        list.pop_clip();
        // The head of the bar, and the flash when it arrives.
        if (!ready_)
            list.circle(fill.x + fill.w - 2, fill.y + fill.h * 0.5f, 6, gfx::mix(gold, kInk, 0.6f));
        list.rounded_rect(track, kBarH * 0.5f, Color::rgb(0xffffff, 0.85f * flash_.value));
        list.pop_opacity();

        // ---- bottom row: the prompt when ready, and the hints ----
        if (ready > 0.0f)
        {
            list.push_opacity(ready * (0.6f + 0.4f * ui::breathe(clock_)));
            float x = kMargin;
            const float cy = 1010.0f + 10.0f * (1.0f - ready);
            x += ui::text(list, fonts.semibold, "Press", x, cy + 10, 28, kInk) + 14;
            ui::draw_button(list, fonts, style, confirm_button(), x, cy, 40);
            x += ui::button_width(confirm_button(), 40) + 14;
            ui::text(list, fonts.semibold, "to continue", x, cy + 10, 28, kInk);
            list.pop_opacity();
        }
        list.push_opacity(tween::stagger(t, 4, 0.08f, 0.6f));
        const ui::Hint hints[] = {{ui::Button::dpad, "Tips"}, {back_button(), "Cancel"}};
        ui::draw_hints(list, fonts, style, hints, 2, kRight, true);
        list.pop_opacity();
    }

    // A dark pill for HUD pieces, sized by the caller.
    static void hud_pill(gfx::DrawList &list, const Rect &r)
    {
        list.shadow({r.x, r.y + 8, r.w, r.h}, r.h * 0.5f, 24, Color::rgb(0x000000, 0.35f));
        list.rounded_rect(r, r.h * 0.5f, kNight.with_alpha(0.62f));
        list.bordered_rect(r, r.h * 0.5f, kClear, 1.5f, kInk.with_alpha(0.16f));
    }

    void draw_game(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        const bool reduced = context_.settings.reduced_motion;
        const float t = std::max(0.0f, age(Screen::game));
        const Profile &profile = kProfiles[profile_];
        const ui::GlyphStyle style = ui::GlyphStyle::dark();

        // The area's name card: arrives once, stays a few seconds, leaves.
        const float card =
            tween::cubic_out((t - 0.3f) / 0.8f) * (1.0f - tween::smoothstep((t - 5.0f) / 1.0f));
        if (card > 0.01f)
        {
            const float rise =
                reduced ? 0.0f : 18.0f * (1.0f - tween::cubic_out((t - 0.3f) / 0.8f));
            list.push_opacity(card);
            ui::text(list, fonts.semibold, ui::upper(profile.chapter), kCentreX + 3, 250 + rise, 20,
                     game_.accent, gfx::Align::center, 6.0f);
            ui::text(list, fonts.display, profile.area, kCentreX, 330 + rise, 72, kInk,
                     gfx::Align::center);
            const float rule = 220.0f * tween::quint_out((t - 0.5f) / 0.9f);
            list.gradient_rect_h({kCentreX - rule, 362 + rise, rule, 2}, 1, kClear,
                                 kInk.with_alpha(0.7f));
            list.gradient_rect_h({kCentreX, 362 + rise, rule, 2}, 1, kInk.with_alpha(0.7f), kClear);
            list.pop_opacity();
        }

        // "Progress saved": the toast that tells the player the hand-over
        // from the loader really finished.
        const float toast = tween::cubic_out((t - kSavedAt) / 0.4f) *
                            (1.0f - tween::smoothstep((t - kSavedAt - 3.0f) / 0.5f));
        if (toast > 0.01f)
        {
            const float w = 72 + fonts.semibold.measure("Progress saved", 22);
            const Rect pill{kRight - w + (reduced ? 0.0f : 30.0f * (1.0f - toast)), 76, w, 52};
            list.push_opacity(toast);
            hud_pill(list, pill);
            check_mark(list, pill.x + 30, pill.cy(), 18.0f * tween::back_out((t - kSavedAt) / 0.5f),
                       3.0f, game_.accent);
            ui::text(list, fonts.semibold, "Progress saved", pill.x + 52, pill.cy() + 8, 22, kInk);
            list.pop_opacity();
        }

        // The HUD hint: one pill, one sentence, one glyph.
        const float in = tween::stagger(t, 2, 0.1f, 0.6f);
        const float said = fonts.semibold.measure("You are in.", 24);
        const float glyph = ui::button_width(ui::Button::options, 36);
        const float action = fonts.regular.measure("Back to title", 24);
        const Rect pill{kMargin, 976 + 20 * (1.0f - in), 28 + said + 22 + glyph + 12 + action + 28,
                        60};
        list.push_opacity(in);
        hud_pill(list, pill);
        float x = pill.x + 28;
        ui::text(list, fonts.semibold, "You are in.", x, pill.cy() + 8.5f, 24, kInk);
        x += said + 22;
        ui::draw_button(list, fonts, style, ui::Button::options, x, pill.cy(), 36);
        x += glyph + 12;
        ui::text(list, fonts.regular, "Back to title", x, pill.cy() + 8.5f, 24,
                 kInk.with_alpha(0.85f));
        list.pop_opacity();
    }

    app::Context &context_;
    const demo::Item &game_;

    // The flow.
    Screen screen_ = Screen::splash;
    Screen from_ = Screen::splash; // the screen a running move leaves
    Move move_ = Move::none;
    tween::Timer transition_;
    float age_[static_cast<std::size_t>(Screen::count)] = {}; // per screen: drives entrances
    float clock_ = 0.0f; // free-running: breathing, spinner, glints
    float drift_ = 0.0f; // free-running unless motion is reduced: every drift

    // Splash.
    tween::Bounce piece_[kPieces];
    ui::Pulse lock_; // the swell when the ring closes
    bool welcomed_ = false;
    bool locked_ = false;

    // Title and profiles.
    tween::Spring camera_; // vertical position of the sky
    int focus_ = 0;
    tween::Spring grow_[kCards];
    ui::SpringRect ring_;
    ui::SpringColor tint_;
    ui::Pulse nudge_;
    float nudge_direction_ = 0.0f;

    // Loading.
    int profile_ = 0;        // who is loading
    float load_time_ = 0.0f; // the loader's clock
    bool ready_ = false;
    float ready_age_ = 0.0f;
    tween::Spring bar_; // what the bar shows: the loader's value, smoothed
    ui::Pulse flash_;
    ui::Pulse deny_;
    int stage_ = 0;
    int previous_stage_ = 0;
    tween::Timer stage_fade_;
    tween::Spring spinner_; // how insistent the spinner is
    int tip_ = 0;
    int previous_tip_ = 0;
    float tip_direction_ = 1.0f;
    float tip_clock_ = 0.0f;
    tween::Timer tip_fade_;
    tween::Spring dot_[kTips];

    // In game.
    bool saved_ = false;
};

} // namespace

std::unique_ptr<app::Concept> make_boot(app::Context &context)
{
    return std::make_unique<Boot>(context);
}

} // namespace hui::concepts
