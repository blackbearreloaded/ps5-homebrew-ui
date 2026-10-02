// ps5-homebrew-ui - Design "Theme Lab": one screen, thirty design languages.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The same working screen (buttons, tabs, chips, switches, check boxes, radio
// buttons, a slider, a text field, a progress bar, a card, a list and a
// dialog) drawn by ui::Painter in every built-in ui::Theme. L2 and R2 restyle
// it live. Nothing in this file knows how any theme looks: it lays out
// rectangles, keeps the widgets' values and eases them; the theme's tokens do
// the rest. That separation is the point of the design, and what makes it
// feel finished is that the *motion and the sound* change with the theme too:
//
//   - every animated value uses the theme's own speed and damping, so Candy
//     bounces, Classic and Pixel snap, Clay glides;
//   - cues play in the theme's sound set;
//   - switching theme cross-fades the widgets through the page colour, so
//     two unrelated looks never sit half-drawn on top of each other.

#include "concepts/concepts.hpp"

#include "core/tween.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace hui::concepts
{

namespace
{

using gfx::Color;
using gfx::Rect;

enum Item : int
{
    kPrimary,
    kSecondary,
    kGhost,
    kTabs,
    kChip0,
    kChip1,
    kToggle0,
    kToggle1,
    kCheck0,
    kCheck1,
    kRadio0,
    kRadio1,
    kRadio2,
    kSlider,
    kField,
    kRow0,
    kRow1,
    kRow2,
    kDialog,
    kItemCount,
};

// The focus rectangle of every item. Layout is data: the navigation below
// and the drawing both read it from here.
constexpr Rect kRects[kItemCount] = {
    {136, 332, 236, 64},  // primary button
    {396, 332, 214, 64},  // secondary button
    {634, 332, 150, 64},  // ghost button
    {136, 432, 540, 58},  // tabs
    {716, 439, 128, 44},  // chip
    {858, 439, 128, 44},  // chip
    {508, 524, 88, 46},   // switch
    {1048, 524, 88, 46},  // switch
    {136, 604, 40, 40},   // check box
    {366, 604, 40, 40},   // check box
    {656, 604, 40, 40},   // radio
    {800, 604, 40, 40},   // radio
    {966, 604, 40, 40},   // radio
    {300, 680, 720, 44},  // slider
    {136, 762, 470, 64},  // text field
    {1256, 668, 528, 56}, // list row
    {1256, 728, 528, 56}, // list row
    {1256, 788, 528, 56}, // list row
    {1256, 866, 528, 62}, // "Open dialog"
};

// The left panel is navigated as rows; the right panel is one column.
struct Row
{
    int first;
    int count;
    float x[5]; // left edge used to keep a column while moving up and down
};
constexpr Row kRows[] = {
    {kPrimary, 3, {136, 396, 634}},          {kTabs, 3, {136, 716, 858}}, {kToggle0, 2, {136, 636}},
    {kCheck0, 5, {136, 366, 656, 800, 966}}, {kSlider, 1, {136}},         {kField, 1, {136}},
};
constexpr int kRowCount = static_cast<int>(std::size(kRows));
constexpr int kColumnFirst = kRow0;
constexpr int kColumnCount = 4;

constexpr const char *kTabLabels[] = {"Overview", "Stats", "Friends"};
constexpr const char *kDemoName = "PLAYER ONE";
constexpr int kSliderSteps = 10;

constexpr const char *kTechniques[] = {
    "ui::Theme: thirty design languages as plain data (colour, shape, depth, type, motion, sound)",
    "ui::Painter: one widget set drawn in any theme; surfaces built eleven different ways",
    "Looks modelled on well-known web frameworks, measured from their own component pages",
    "Motion and sound belong to the theme: speed, bounce and sound set change with it",
    "Row and column focus navigation that keeps your column across rows of different widgets",
    "A bitmap face drawn from rectangles and hand-drawn strokes, for looks a font cannot give",
};

int row_of(int item)
{
    for (int r = 0; r < kRowCount; ++r)
    {
        if (item >= kRows[r].first && item < kRows[r].first + kRows[r].count)
            return r;
    }
    return -1; // the right column
}

class Themes final : public app::Concept
{
  public:
    explicit Themes(app::Context &context) : context_(context)
    {
        // A screen that looks used: some things on, a value part-way.
        value_[kToggle0].snap(1.0f);
        value_[kCheck0].snap(1.0f);
        value_[kChip0].snap(1.0f);
        value_[kRadio1].snap(1.0f);
        value_[kRow1].snap(1.0f);
        slider_ = 6;
        slider_value_.snap(0.6f);
        focus_amount_[0].snap(1.0f);
        build_tour();
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "themes",
            "Theme Lab",
            "One screen in thirty design languages: L2 and R2 restyle every widget",
            "src/concepts/themes.cpp",
            audio::SoundSet::glass,
            Color::rgb(0x76d6ff),
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        age_ = 0.0f;
        dialog_ = false;
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        age_ += dt;
        clock_ += dt;
        const ui::Theme &theme = ui::themes()[static_cast<std::size_t>(theme_)];

        const int count = static_cast<int>(ui::themes().size());
        if (input.is_pressed(Action::jump_next) || input.is_pressed(Action::jump_prev))
        {
            previous_theme_ = theme_;
            theme_ = (theme_ + (input.is_pressed(Action::jump_next) ? 1 : count - 1)) % count;
            switch_.start(context_.settings.reduced_motion ? 0.16f : 0.42f);
            // The new theme announces itself in its own voice.
            play(feedback, audio::Cue::tab, 1.0f, 0.0f);
        }
        else if (dialog_)
        {
            update_dialog(input, feedback);
        }
        else
        {
            update_focus(input, feedback);
            if (input.is_pressed(Action::confirm))
                activate(feedback);
        }

        // ---- animation: everything moves at the theme's pace ----
        const float omega = context_.settings.reduced_motion ? 60.0f : theme.omega;
        const float damping = context_.settings.reduced_motion ? 1.0f : theme.damping;
        switch_.update(dt);
        for (int i = 0; i < kItemCount; ++i)
        {
            focus_amount_[i].target = !dialog_ && i == focus_ ? 1.0f : 0.0f;
            focus_amount_[i].update(dt, std::max(omega, 20.0f));
            value_[i].update(dt, omega, damping);
            press_[i].update(dt, 9.0f);
        }
        tab_value_.target = static_cast<float>(tab_);
        tab_value_.update(dt, omega, damping);
        slider_value_.target = static_cast<float>(slider_) / kSliderSteps;
        slider_value_.update(dt, omega, damping);
        dialog_show_.target = dialog_ ? 1.0f : 0.0f;
        dialog_show_.update(dt, std::min(omega, 26.0f));
        dialog_focus_value_.target = static_cast<float>(dialog_focus_);
        dialog_focus_value_.update(dt, std::max(omega, 20.0f));
        refuse_.update(dt, 9.0f);
        // A download that never ends keeps the progress bar honest.
        progress_ = std::fmod(clock_ * 0.07f, 1.15f);
    }

    void draw(app::Frame &frame) const override
    {
        const auto themes = ui::themes();
        const ui::Theme &now = themes[static_cast<std::size_t>(theme_)];
        const ui::Theme &old = themes[static_cast<std::size_t>(previous_theme_)];
        const float t = switch_.running ? switch_.progress() : 1.0f;

        // The backdrop changes at the midpoint, hidden behind a veil of the
        // page colour that is opaque exactly then.
        const ui::Theme &behind = t < 0.5f ? old : now;
        frame.backdrop = behind.backdrop;
        frame.backdrop.time = clock_;
        const float veil = 1.0f - std::fabs(2.0f * t - 1.0f);
        if (behind.style == ui::SurfaceStyle::glass)
        {
            // Glass needs something behind it worth blurring.
            for (int i = 0; i < 4; ++i)
            {
                const float phase = clock_ * 0.12f + static_cast<float>(i) * 1.7f;
                frame.scene.circle(
                    480.0f + static_cast<float>(i) * 380.0f + std::sin(phase) * 120.0f,
                    560.0f + std::cos(phase * 1.3f) * 220.0f, 170.0f,
                    gfx::mix(behind.primary, behind.accent, static_cast<float>(i % 2))
                        .with_alpha(0.55f));
            }
            frame.glass = true;
        }
        if (veil > 0.001f)
            frame.scene.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                                     gfx::mix(old.page, now.page, t).with_alpha(veil));

        gfx::DrawList &list = frame.overlay;
        if (switch_.running)
        {
            draw_theme(list, old, frame.glass_texture, 1.0f - tween::smoothstep(t * 2.0f), 0.0f);
            draw_theme(list, now, frame.glass_texture, tween::smoothstep(t * 2.0f - 1.0f),
                       context_.settings.reduced_motion ? 0.0f : 18.0f * (1.0f - t));
        }
        else
        {
            draw_theme(list, now, frame.glass_texture, 1.0f, 0.0f);
        }
    }

    std::span<const app::TourStep> tour() const override
    {
        return tour_;
    }

  private:
    // The tour visits every theme: switch, use a few widgets, take its
    // picture. In reel mode the frames between two pictures become that
    // theme's clip.
    void build_tour()
    {
        const auto themes = ui::themes();
        const std::uint32_t use = action_bit(Action::confirm);
        const std::uint32_t next = action_bit(Action::jump_next);
        for (std::size_t i = 0; i < themes.size(); ++i)
        {
            tour_.push_back({0.55f, use});               // press the primary button
            tour_.push_back({0.3f, 0, Direction::down}); // tabs
            tour_.push_back({0.2f, use});
            tour_.push_back({0.3f, 0, Direction::down}); // a switch, off and on
            tour_.push_back({0.2f, use});
            tour_.push_back({0.4f, use});
            tour_.push_back({0.3f, 0, Direction::down}); // a check box, off and on
            tour_.push_back({0.2f, use});
            tour_.push_back({0.4f, use});
            tour_.push_back({0.3f, 0, Direction::down}); // the slider
            tour_.push_back({0.2f, 0, Direction::right});
            tour_.push_back({0.2f, 0, Direction::right});
            tour_.push_back({0.25f, 0, Direction::left});
            tour_.push_back({0.25f, 0, Direction::left});
            // The picture, then on to the next theme (and back to the top).
            tour_.push_back({0.6f, 0, Direction::up, themes[i].id});
            tour_.push_back({0.08f, 0, Direction::up});
            tour_.push_back({0.08f, 0, Direction::up});
            tour_.push_back({0.08f, 0, Direction::up});
            tour_.push_back({0.1f, next});
        }
        // One more picture: the dialog, in the first theme again.
        tour_.push_back({0.6f, 0, Direction::right});
        tour_.push_back({0.1f, 0, Direction::right});
        tour_.push_back({0.1f, 0, Direction::right});
        tour_.push_back({0.1f, 0, Direction::down});
        tour_.push_back({0.1f, 0, Direction::down});
        tour_.push_back({0.1f, 0, Direction::down});
        tour_.push_back({0.2f, use});
        tour_.push_back({0.8f, action_bit(Action::back), Direction::none, "dialog"});
        tour_.push_back({0.3f, 0, Direction::left});
        tour_.push_back({0.1f, 0, Direction::up});
        tour_.push_back({0.1f, 0, Direction::up});
        tour_.push_back({0.1f, 0, Direction::up});
        tour_.push_back({0.1f, 0, Direction::up});
        tour_.push_back({0.1f, 0, Direction::up});
    }

    void play(app::Feedback &feedback, audio::Cue cue, float pitch, float x) const
    {
        audio::CueEvent event;
        event.cue = cue;
        event.pitch = pitch;
        event.pan = x > 0.0f ? ui::pan_for_x(x) : 0.0f;
        event.set = ui::themes()[static_cast<std::size_t>(theme_)].sounds;
        feedback.cues.push_back(event);
    }

    void refuse(app::Feedback &feedback, const InputFrame &input)
    {
        if (input.nav_repeat)
            return;
        play(feedback, audio::Cue::error, 1.0f, 0.0f);
        feedback.cues.back().gain = 0.6f;
        feedback.rumble(0.25f, 0.05f);
        refuse_.trigger();
    }

    void move_to(int item, app::Feedback &feedback)
    {
        focus_ = item;
        play(feedback, audio::Cue::focus, 1.0f, kRects[item].cx());
    }

    void update_focus(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav == Direction::none)
            return;
        const int row = row_of(focus_);
        // The slider takes left and right for itself.
        if (focus_ == kSlider && (input.nav == Direction::left || input.nav == Direction::right))
        {
            const int next = slider_ + (input.nav == Direction::right ? 1 : -1);
            if (next < 0 || next > kSliderSteps)
            {
                refuse(feedback, input);
                return;
            }
            slider_ = next;
            // The pitch climbs with the value.
            play(feedback, audio::Cue::slider, 0.85f + 0.04f * static_cast<float>(slider_),
                 kRects[kSlider].x +
                     kRects[kSlider].w * static_cast<float>(slider_) / kSliderSteps);
            return;
        }
        if (row >= 0)
        {
            const Row &here = kRows[row];
            const int index = focus_ - here.first;
            if (input.nav == Direction::left)
            {
                if (index > 0)
                    move_to(focus_ - 1, feedback);
                else
                    refuse(feedback, input);
            }
            else if (input.nav == Direction::right)
            {
                if (index + 1 < here.count)
                    move_to(focus_ + 1, feedback);
                else
                    move_to(nearest_in_column(kRects[focus_].cy()), feedback);
            }
            else
            {
                const int target = row + (input.nav == Direction::down ? 1 : -1);
                if (target < 0 || target >= kRowCount)
                {
                    refuse(feedback, input);
                    return;
                }
                // Keep the column: the item whose left edge is nearest ours.
                const Row &there = kRows[target];
                const float x = here.x[index];
                int best = 0;
                for (int i = 1; i < there.count; ++i)
                {
                    if (std::fabs(there.x[i] - x) < std::fabs(there.x[best] - x))
                        best = i;
                }
                move_to(there.first + best, feedback);
            }
            return;
        }
        // The right column.
        const int index = focus_ - kColumnFirst;
        if (input.nav == Direction::up || input.nav == Direction::down)
        {
            const int next = index + (input.nav == Direction::down ? 1 : -1);
            if (next >= 0 && next < kColumnCount)
                move_to(kColumnFirst + next, feedback);
            else
                refuse(feedback, input);
        }
        else if (input.nav == Direction::left)
        {
            // Back into the row at the same height, at its right end.
            int best = 0;
            for (int r = 1; r < kRowCount; ++r)
            {
                if (std::fabs(kRects[kRows[r].first].cy() - kRects[focus_].cy()) <
                    std::fabs(kRects[kRows[best].first].cy() - kRects[focus_].cy()))
                    best = r;
            }
            move_to(kRows[best].first + kRows[best].count - 1, feedback);
        }
        else
        {
            refuse(feedback, input);
        }
    }

    static int nearest_in_column(float y)
    {
        int best = kColumnFirst;
        for (int i = kColumnFirst; i < kColumnFirst + kColumnCount; ++i)
        {
            if (std::fabs(kRects[i].cy() - y) < std::fabs(kRects[best].cy() - y))
                best = i;
        }
        return best;
    }

    void activate(app::Feedback &feedback)
    {
        const float x = kRects[focus_].cx();
        press_[focus_].trigger();
        switch (focus_)
        {
        case kPrimary:
        case kSecondary:
            play(feedback, audio::Cue::select, 1.0f, x);
            break;
        case kGhost:
            play(feedback, audio::Cue::back, 1.0f, x);
            break;
        case kTabs:
            tab_ = (tab_ + 1) % 3;
            play(feedback, audio::Cue::tab, 1.0f + 0.06f * static_cast<float>(tab_), x);
            break;
        case kChip0:
        case kChip1:
        case kToggle0:
        case kToggle1:
        case kCheck0:
        case kCheck1:
        {
            const bool on = value_[focus_].target < 0.5f;
            value_[focus_].target = on ? 1.0f : 0.0f;
            // On is the higher note of the pair.
            play(feedback, audio::Cue::toggle, on ? 1.08f : 0.94f, x);
            break;
        }
        case kRadio0:
        case kRadio1:
        case kRadio2:
            for (int i = kRadio0; i <= kRadio2; ++i)
                value_[i].target = i == focus_ ? 1.0f : 0.0f;
            play(feedback, audio::Cue::select, 1.0f, x);
            break;
        case kSlider:
            play(feedback, audio::Cue::tick, 1.0f, x);
            break;
        case kField:
            // Each press types the next letter of a name, then starts over.
            typed_ = typed_ >= static_cast<int>(std::char_traits<char>::length(kDemoName))
                         ? 0
                         : typed_ + 1;
            play(feedback, typed_ == 0 ? audio::Cue::erase : audio::Cue::type, 1.0f, x);
            break;
        case kRow0:
        case kRow1:
        case kRow2:
            for (int i = kRow0; i <= kRow2; ++i)
                value_[i].target = i == focus_ ? 1.0f : 0.0f;
            play(feedback, audio::Cue::select, 1.0f, x);
            break;
        case kDialog:
            dialog_ = true;
            dialog_focus_ = 0; // the safe choice
            dialog_focus_value_.snap(0.0f);
            play(feedback, audio::Cue::modal_open, 1.0f, 0.0f);
            break;
        default:
            break;
        }
    }

    void update_dialog(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.nav == Direction::left || input.nav == Direction::right)
        {
            const int next = input.nav == Direction::right ? 1 : 0;
            if (next != dialog_focus_)
            {
                dialog_focus_ = next;
                play(feedback, audio::Cue::focus, 1.0f, next == 0 ? 780.0f : 1140.0f);
            }
            else
            {
                refuse(feedback, input);
            }
        }
        if (input.is_pressed(Action::confirm) || input.is_pressed(Action::back))
        {
            dialog_ = false;
            play(feedback,
                 input.is_pressed(Action::back) || dialog_focus_ == 0 ? audio::Cue::modal_close
                                                                      : audio::Cue::select,
                 1.0f, 0.0f);
        }
    }

    ui::Look look(int item) const
    {
        return {focus_amount_[item].value, press_[item].value, false};
    }

    void draw_theme(gfx::DrawList &list, const ui::Theme &theme, std::uint32_t glass, float alpha,
                    float slide) const
    {
        if (alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        ui::Painter paint(list, fonts, theme, glass);
        const auto themes = ui::themes();
        const int index = static_cast<int>(&theme - themes.data());
        const float in = tween::cubic_out(age_ / 0.45f);
        char text[64];
        list.push_opacity(alpha * in);
        list.push_transform(1.0f, 0, 0, 0, slide + 24.0f * (1.0f - in));

        // ---- header: which language this is ----
        paint.label(theme.family, 96, 120, 21, paint.page_text_muted());
        paint.heading(theme.name, 94, 190, 66, paint.page_text());
        paint.body(theme.summary, 96, 232, 24, paint.page_text_muted());
        std::snprintf(text, sizeof(text), "%02d / %02d", index + 1,
                      static_cast<int>(themes.size()));
        paint.label(text, 1824, 124, 24, paint.page_text(), gfx::Align::right);
        // A tick per theme; the current one is long and in the primary colour
        // (or its text colour, where the primary is too pale to see on the page).
        const auto luminance = [](Color c) { return 0.299f * c.r + 0.587f * c.g + 0.114f * c.b; };
        const Color marker = std::fabs(luminance(theme.primary) - luminance(theme.page)) < 0.2f
                                 ? theme.on_primary
                                 : theme.primary;
        for (int i = 0; i < static_cast<int>(themes.size()); ++i)
        {
            const bool current = i == index;
            list.rounded_rect({1824.0f - static_cast<float>(themes.size() - 1 - i) * 14.0f - 6.0f,
                               current ? 150.0f : 158.0f, 6.0f, current ? 22.0f : 8.0f},
                              theme.radius > 2.0f ? 3.0f : 0.0f,
                              current ? marker : paint.page_text_muted().with_alpha(0.5f));
        }

        // ---- left panel: the controls ----
        const float nudge = ui::shake(refuse_.value, clock_, 10.0f, 10.0f);
        paint.panel({96, 280, 1080, 678});
        paint.button(kRects[kPrimary], "Continue", ui::ButtonKind::primary, look(kPrimary));
        paint.button(kRects[kSecondary], "Details", ui::ButtonKind::secondary, look(kSecondary));
        paint.button(kRects[kGhost], "Skip", ui::ButtonKind::ghost, look(kGhost));
        ui::Look off;
        off.disabled = true;
        paint.button({808, 332, 214, 64}, "Locked", ui::ButtonKind::secondary, off);

        paint.tabs(kRects[kTabs], kTabLabels, tab_value_.value, look(kTabs));
        paint.chip(kRects[kChip0], "New", value_[kChip0].value, look(kChip0));
        paint.chip(kRects[kChip1], "Co-op", value_[kChip1].value, look(kChip1));

        paint.body("Notifications", 136, 556, 25, theme.text);
        paint.toggle(kRects[kToggle0], value_[kToggle0].value, look(kToggle0));
        paint.body("Auto-save", 636, 556, 25, theme.text);
        paint.toggle(kRects[kToggle1], value_[kToggle1].value, look(kToggle1));

        paint.checkbox(kRects[kCheck0], value_[kCheck0].value, look(kCheck0));
        paint.body("Subtitles", 192, 633, 25, theme.text);
        paint.checkbox(kRects[kCheck1], value_[kCheck1].value, look(kCheck1));
        paint.body("Hints", 422, 633, 25, theme.text);
        const char *levels[] = {"Easy", "Normal", "Hard"};
        for (int i = 0; i < 3; ++i)
        {
            paint.radio(kRects[kRadio0 + i], value_[kRadio0 + i].value, look(kRadio0 + i));
            paint.body(levels[i], kRects[kRadio0 + i].x + 54, 633, 25, theme.text);
        }

        paint.body("Volume", 136, 711, 25, theme.text);
        Rect slider = kRects[kSlider];
        slider.x += focus_ == kSlider ? nudge : 0.0f;
        paint.slider(slider, slider_value_.value, look(kSlider));
        std::snprintf(text, sizeof(text), "%d", slider_);
        paint.label(text, 1136, 712, 26, theme.text, gfx::Align::right);

        const std::string typed(kDemoName, static_cast<std::size_t>(typed_));
        const bool caret = focus_ == kField && std::fmod(clock_, 1.0f) < 0.55f;
        paint.field(kRects[kField], typed.empty() ? "Name" : typed, caret, look(kField));
        std::snprintf(text, sizeof(text), "Downloading  %d%%",
                      static_cast<int>(tween::clamp01(progress_) * 100.0f));
        paint.body(text, 656, 786, 22, theme.text_muted);
        paint.progress({656, 800, 480, 24}, progress_);
        // Running text: how the theme reads, not only how its controls look.
        list.rounded_rect({136, 858, 1000, 1.5f}, 0, theme.text_muted.with_alpha(0.25f));
        paint.body("Body text carries a theme as much as its buttons do: size, weight and colour",
                   136, 896, 22, theme.text_muted);
        paint.body("decide how a screen reads from across the room.", 136, 926, 22,
                   theme.text_muted);

        // ---- right panel: a card, a list, the dialog button ----
        paint.panel({1216, 280, 608, 678});
        const demo::Item &item = context_.catalog[static_cast<std::size_t>(index)];
        const Rect art{1256, 320, 528, 190};
        // The cover's middle band, with the theme's corners where it can.
        list.image(item.cover, art, {0.0f, 0.7f, 1.0f, -0.36f}, Color::rgb(0xffffff),
                   theme.corner == ui::Corner::round ? std::min(theme.radius, 14.0f) : 0.0f);
        if (theme.border > 0.0f && theme.outline.a > 0.5f)
            paint.stroke(art, std::min(theme.radius, 14.0f), theme.border, theme.outline);
        paint.heading(item.title, 1256, 566, 36);
        std::snprintf(text, sizeof(text), "%s  \xC2\xB7  %d", item.genre, item.year);
        paint.body(text, 1256, 604, 22, theme.text_muted);
        const char *names[] = {"Difficulty", "Players", "Language"};
        const char *values[] = {"Normal", "2", "English"};
        for (int i = 0; i < 3; ++i)
            paint.row(kRects[kRow0 + i], names[i], values[i], value_[kRow0 + i].value,
                      look(kRow0 + i));
        paint.button(kRects[kDialog], "Open dialog", ui::ButtonKind::primary, look(kDialog));

        // ---- hints, in glyphs that suit the page ----
        const ui::GlyphStyle glyphs = theme.dark ? ui::GlyphStyle::dark() : ui::GlyphStyle::light();
        const ui::Hint hints[] = {
            {ui::Button::l2, "Theme", ui::Button::r2},
            {ui::Button::dpad, "Move"},
            {ui::Button::cross, "Use"},
        };
        ui::HintLayout layout;
        layout.cy = 1012.0f;
        ui::draw_hints(list, fonts, glyphs, hints, 3, 96, false, layout);

        // ---- the dialog ----
        const float show = dialog_show_.value;
        if (show > 0.01f)
        {
            list.rounded_rect(
                {0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                (theme.dark ? Color::rgb(0x000000) : theme.text).with_alpha(0.5f * show));
            list.push_opacity(tween::clamp01(show));
            list.push_transform(0.94f + 0.06f * tween::clamp01(show), 960, 540, 0, 0);
            paint.panel({560, 350, 800, 380});
            paint.heading("Leave without saving?", 610, 430, 40);
            paint.body("Your progress in this session will be lost.", 610, 480, 25,
                       theme.text_muted);
            ui::Look cancel;
            cancel.focus = 1.0f - tween::clamp01(dialog_focus_value_.value);
            ui::Look leave;
            leave.focus = tween::clamp01(dialog_focus_value_.value);
            paint.button({610, 620, 340, 64}, "Cancel", ui::ButtonKind::secondary, cancel);
            paint.button({970, 620, 340, 64}, "Leave", ui::ButtonKind::primary, leave);
            list.pop_transform();
            list.pop_opacity();
        }

        list.pop_transform();
        list.pop_opacity();
    }

    app::Context &context_;
    std::vector<app::TourStep> tour_;
    float age_ = 0.0f;
    float clock_ = 0.0f;
    int theme_ = 0;
    int previous_theme_ = 0;
    tween::Timer switch_;
    int focus_ = 0;
    std::array<tween::Spring, kItemCount> focus_amount_;
    std::array<tween::Bounce, kItemCount> value_; // on/off and selection amounts
    std::array<ui::Pulse, kItemCount> press_;
    int tab_ = 0;
    tween::Bounce tab_value_;
    int slider_ = 6;
    tween::Bounce slider_value_;
    int typed_ = 6;
    float progress_ = 0.0f;
    ui::Pulse refuse_;
    bool dialog_ = false;
    int dialog_focus_ = 0;
    tween::Spring dialog_show_;
    tween::Spring dialog_focus_value_;
};

} // namespace

std::unique_ptr<app::Concept> make_themes(app::Context &context)
{
    return std::make_unique<Themes>(context);
}

} // namespace hui::concepts
