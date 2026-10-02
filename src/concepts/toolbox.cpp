// ps5-homebrew-ui - Design "Toolbox": the kit on one screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Not a product UI but a living style guide: every shape the renderer draws,
// the four fonts, the easing curves and springs in motion, the controller
// glyphs lighting up with real input, and a sound board that plays every cue
// of both sound sets. Start here to see what the other designs are made of.

#include "concepts/concepts.hpp"

#include "core/tween.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <cmath>
#include <cstdio>

namespace hui::concepts
{

namespace
{

using gfx::Color;
using gfx::Rect;

const Color kInk = Color::rgb(0xeef1ff);
const Color kMuted = Color::rgb(0x8f97c2);
const Color kPanel = Color::rgb(0x161a33, 0.78f);
const Color kAccent = Color::rgb(0x7cf0c8);
const Color kWarm = Color::rgb(0xffc56b);

constexpr int kColumns = 6;
constexpr float kChipW = 128.0f;
constexpr float kChipH = 48.0f;
constexpr float kChipGap = 8.0f;
constexpr float kBoardX = 1008.0f;
constexpr float kBoardY = 452.0f;

constexpr const char *kTechniques[] = {
    "Every DrawList shape: rounded and rotated rectangles, gradients, arcs, stars, shadows, glows",
    "Four baked distance-field fonts, sharp at any size, with letter tracking",
    "Easing curves and the two spring types, plotted and running",
    "Controller glyphs drawn from shapes, lit by the buttons you hold",
    "A sound board: every cue of both sound sets, panned by where it sits",
};

constexpr app::TourStep kTour[] = {
    {0.4f, 0, Direction::right},
    {0.2f, 0, Direction::right},
    {0.2f, 0, Direction::down},
    {0.3f, action_bit(Action::confirm)},
    {0.5f, action_bit(Action::west), Direction::none, "board"},
    {0.6f, action_bit(Action::confirm)},
    {0.4f, 0, Direction::none, "glass-set"},
};

Rect chip_rect(int index)
{
    const int column = index % kColumns;
    const int row = index / kColumns;
    return {kBoardX + static_cast<float>(column) * (kChipW + kChipGap),
            kBoardY + static_cast<float>(row) * (kChipH + kChipGap), kChipW, kChipH};
}

struct Curve
{
    const char *name;
    float (*ease)(float);
};
constexpr Curve kCurves[] = {
    {"cubic_out", tween::cubic_out},
    {"quint_out", tween::quint_out},
    {"back_out", tween::back_out},
    {"elastic_out", tween::elastic_out},
};

class Toolbox final : public app::Concept
{
  public:
    explicit Toolbox(app::Context &context) : context_(context)
    {
        ring_.snap(chip_rect(0));
        set_blend_.snap(1.0f);
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "toolbox",
            "Toolbox",
            "The kit on one screen: shapes, type, motion, glyphs and every sound",
            "src/concepts/toolbox.cpp",
            audio::SoundSet::glass,
            kAccent,
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        age_ = 0.0f;
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        age_ += dt;
        clock_ += dt;
        held_ = input.held;
        stick_x_ = input.stick_x;
        stick_y_ = input.stick_y;

        const int count = static_cast<int>(audio::kCueCount);
        int next = focus_;
        switch (input.nav)
        {
        case Direction::left:
            next = focus_ % kColumns > 0 ? focus_ - 1 : focus_;
            break;
        case Direction::right:
            next = focus_ % kColumns < kColumns - 1 && focus_ + 1 < count ? focus_ + 1 : focus_;
            break;
        case Direction::up:
            next = focus_ >= kColumns ? focus_ - kColumns : focus_;
            break;
        case Direction::down:
            next = focus_ + kColumns < count ? focus_ + kColumns : focus_;
            break;
        case Direction::none:
            break;
        }
        if (next != focus_)
        {
            focus_ = next;
            feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(chip_rect(focus_).cx()));
        }
        else if (input.nav != Direction::none && !input.nav_repeat)
        {
            // The edge of the grid answers too: a soft refusal and a nudge.
            feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.5f);
            edge_.trigger();
        }
        if (input.is_pressed(Action::confirm))
        {
            audio::CueEvent event;
            event.cue = static_cast<audio::Cue>(focus_);
            event.pan = ui::pan_for_x(chip_rect(focus_).cx());
            event.set = set_;
            feedback.cues.push_back(event);
            played_ = focus_;
            flash_.trigger();
        }
        if (input.is_pressed(Action::west))
        {
            set_ = set_ == audio::SoundSet::paper ? audio::SoundSet::glass : audio::SoundSet::paper;
            audio::CueEvent event;
            event.cue = audio::Cue::toggle;
            event.set = set_;
            feedback.cues.push_back(event);
        }

        ring_.target(chip_rect(focus_));
        ring_.update(dt, 22.0f);
        flash_.update(dt, 5.0f);
        edge_.update(dt, 10.0f);
        set_blend_.target = set_ == audio::SoundSet::glass ? 1.0f : 0.0f;
        set_blend_.update(dt, 14.0f);

        // The spring demo: its target hops every 1.4 seconds.
        const bool right = std::fmod(clock_, 2.8f) > 1.4f;
        spring_.target = right ? 1.0f : 0.0f;
        bounce_.target = spring_.target;
        spring_.update(dt, 10.0f);
        bounce_.update(dt, 14.0f, 0.35f);
    }

    void draw(app::Frame &frame) const override
    {
        frame.backdrop.mode = gfx::BackdropMode::gradient;
        frame.backdrop.colors[0] = Color::rgb(0x0b0e20);
        frame.backdrop.colors[1] = Color::rgb(0x161032);
        frame.backdrop.colors[2] = Color::rgb(0x2a3a8c);
        frame.backdrop.params[0] = 0.2f;
        frame.backdrop.params[1] = 0.1f;
        frame.backdrop.params[2] = 0.55f;
        frame.backdrop.time = clock_;

        gfx::DrawList &list = frame.scene;
        const ui::Fonts &fonts = context_.fonts;
        const float intro = tween::cubic_out(age_ / 0.5f);
        list.push_opacity(intro);

        ui::text(list, fonts.display, "Toolbox", 96, 150, 60, kInk);
        ui::text(list, fonts.regular, "Everything the other designs are built from.", 98, 192, 24,
                 kMuted);

        draw_shapes(list);
        draw_type(list);
        draw_motion(list);
        draw_controller(list);
        draw_board(list);

        const ui::Hint hints[] = {
            {ui::Button::dpad, "Choose a sound"},
            {ui::Button::cross, "Play"},
            {ui::Button::square, "Sound set"},
            {ui::Button::touchpad, "About this design"},
        };
        ui::draw_hints(list, fonts, ui::GlyphStyle::dark(), hints, 4, 1824, true);
        list.pop_opacity();
    }

    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    void section(gfx::DrawList &list, const char *label, float x, float y) const
    {
        ui::text(list, context_.fonts.semibold, label, x, y, 18, kAccent, gfx::Align::left, 3.0f);
    }

    void caption(gfx::DrawList &list, const char *label, float cx, float y) const
    {
        ui::text(list, context_.fonts.regular, label, cx, y, 15, kMuted, gfx::Align::center);
    }

    void draw_shapes(gfx::DrawList &list) const
    {
        section(list, "SHAPES", 96, 262);
        const float t = clock_;
        struct Cell
        {
            const char *name;
        };
        constexpr Cell kTop[] = {{"rounded"}, {"gradient"}, {"gradient_h"}, {"bordered"},
                                 {"rotated"}, {"circle"},   {"ring"},       {"arc"}};
        constexpr Cell kBottom[] = {{"line"}, {"triangle"}, {"star"},    {"shadow"},
                                    {"glow"}, {"image"},    {"polygon"}, {"clip"}};
        for (int i = 0; i < 8; ++i)
        {
            const float cx = 136.0f + static_cast<float>(i) * 100.0f;
            const float cy = 320.0f;
            const Rect r{cx - 32, cy - 32, 64, 64};
            switch (i)
            {
            case 0:
                list.rounded_rect(r, 16, kAccent);
                break;
            case 1:
                list.gradient_rect(r, 16, kAccent, Color::rgb(0x2b6cff));
                break;
            case 2:
                list.gradient_rect_h(r, 16, kWarm, Color::rgb(0xff5fa2));
                break;
            case 3:
                list.bordered_rect(r, 16, Color::rgb(0x1d2347), 3, kAccent);
                break;
            case 4:
                list.rotated_rect(r.inset(6), 12, t * 0.8f, kWarm);
                break;
            case 5:
                list.circle(cx, cy, 32, Color::rgb(0xff5fa2));
                break;
            case 6:
                list.ring(cx, cy, 32, 6, kAccent);
                break;
            default:
                list.arc(cx, cy, 32, 8, 0.0f, 6.2831853f, Color::rgb(0x2a3060));
                list.arc(cx, cy, 32, 8, 0.0f, 6.2831853f * (0.5f + 0.5f * std::sin(t * 0.9f)),
                         kAccent);
                break;
            }
            caption(list, kTop[i].name, cx, 384);
        }
        for (int i = 0; i < 8; ++i)
        {
            const float cx = 136.0f + static_cast<float>(i) * 100.0f;
            const float cy = 446.0f;
            const Rect r{cx - 32, cy - 32, 64, 64};
            switch (i)
            {
            case 0:
                list.line(cx - 28, cy + 24, cx + 28, cy - 24, 8, kAccent);
                break;
            case 1:
                list.triangle(r.inset(4), kWarm);
                break;
            case 2:
                list.star(cx, cy, 34, kWarm);
                break;
            case 3:
                list.shadow({r.x, r.y + 8, r.w, r.h}, 16, 18, Color::rgb(0x000000, 0.7f));
                list.rounded_rect(r, 16, Color::rgb(0x2a3060));
                break;
            case 4:
                list.glow(r.inset(8), 16, 22, kAccent.with_alpha(0.5f + 0.3f * std::sin(t * 2.0f)));
                list.rounded_rect(r.inset(8), 12, kAccent);
                break;
            case 5:
                list.image(context_.catalog[1].cover, r, gfx::kCanvasUv, Color::rgb(0xffffff), 14);
                break;
            case 6:
            {
                const float points[] = {cx - 30, cy + 26, cx - 12, cy - 28, cx + 4,
                                        cy + 4,  cx + 20, cy - 22, cx + 32, cy + 26};
                list.polygon(points, 5, Color::rgb(0x2b6cff));
                break;
            }
            default:
                list.push_clip(r);
                list.circle(cx - 18 + 22 * std::sin(t), cy - 6, 34, Color::rgb(0xff5fa2));
                list.circle(cx + 20, cy + 18, 26, kAccent);
                list.pop_clip();
                list.bordered_rect(r, 0, Color::rgb(0x000000, 0.0f), 1.5f, kMuted.with_alpha(0.6f));
                break;
            }
            caption(list, kBottom[i].name, cx, 510);
        }
    }

    void draw_type(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        section(list, "TYPE", 96, 574);
        ui::text(list, fonts.display, "Montserrat Medium", 96, 632, 46, kInk);
        ui::text(list, fonts.semibold, "Inter SemiBold for titles and buttons", 96, 678, 30, kInk);
        ui::text(list, fonts.regular, "Inter Regular for body text, readable from the sofa", 96,
                 718, 24, kInk.with_alpha(0.86f));
        ui::text(list, fonts.mono, "DejaVu Sans Mono 0123456789 16.67 ms", 96, 754, 21,
                 kWarm.with_alpha(0.95f));
        ui::text(list, fonts.semibold, "TRACKED CAPS FOR SMALL LABELS", 96, 788, 16, kMuted,
                 gfx::Align::left, 4.0f);
    }

    void draw_motion(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        section(list, "MOTION", 96, 842);
        const float phase = std::fmod(clock_, 2.2f) / 1.6f; // 1.6 s run, then a rest
        for (int i = 0; i < 4; ++i)
        {
            const float x = 96.0f + static_cast<float>(i) * 150.0f;
            const Rect r{x, 862, 132, 84};
            list.rounded_rect(r, 12, kPanel);
            // The curve, as short segments.
            float px = r.x + 12;
            float py = r.y + r.h - 16;
            for (int s = 1; s <= 24; ++s)
            {
                const float u = static_cast<float>(s) / 24.0f;
                const float nx = r.x + 12 + u * (r.w - 24);
                const float ny = r.y + r.h - 16 - kCurves[i].ease(u) * (r.h - 36);
                list.line(px, py, nx, ny, 2.0f, kMuted);
                px = nx;
                py = ny;
            }
            const float u = tween::clamp01(phase);
            list.circle(r.x + 12 + u * (r.w - 24), r.y + r.h - 16 - kCurves[i].ease(u) * (r.h - 36),
                        6, kAccent);
            ui::text(list, fonts.regular, kCurves[i].name, r.cx(), r.y + r.h + 22, 15, kMuted,
                     gfx::Align::center);
        }
        // Critically damped spring against the bouncy one, same target.
        const float track_x = 716.0f;
        const float track_w = 190.0f;
        for (int i = 0; i < 2; ++i)
        {
            const float y = 884.0f + static_cast<float>(i) * 44.0f;
            const float value = i == 0 ? spring_.value : bounce_.value;
            list.rounded_rect({track_x, y - 3, track_w, 6}, 3, Color::rgb(0x2a3060));
            list.circle(track_x + value * track_w, y, 12, i == 0 ? kAccent : kWarm);
        }
        ui::text(list, fonts.regular, "Spring / Bounce", track_x + track_w * 0.5f, 968, 15, kMuted,
                 gfx::Align::center);
    }

    void draw_controller(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        section(list, "CONTROLLER", kBoardX, 262);
        struct Key
        {
            ui::Button button;
            std::uint32_t actions;
        };
        const Key keys[] = {
            {ui::Button::cross, action_bit(Action::confirm)},
            {ui::Button::circle, action_bit(Action::back)},
            {ui::Button::square, action_bit(Action::west)},
            {ui::Button::triangle, action_bit(Action::north)},
            {ui::Button::l1, 0},
            {ui::Button::r1, 0},
            {ui::Button::l2, action_bit(Action::jump_prev)},
            {ui::Button::r2, action_bit(Action::jump_next)},
            {ui::Button::options, action_bit(Action::menu)},
            {ui::Button::dpad, action_bit(Action::up) | action_bit(Action::down) |
                                   action_bit(Action::left) | action_bit(Action::right)},
            {ui::Button::left_stick, action_bit(Action::l3)},
            {ui::Button::right_stick, action_bit(Action::r3)},
            {ui::Button::touchpad, 0},
        };
        const ui::GlyphStyle style = ui::GlyphStyle::dark();
        float x = kBoardX;
        for (const Key &key : keys)
        {
            const float w = ui::button_width(key.button, 44);
            float dx = 0.0f;
            float dy = 0.0f;
            if (key.button == ui::Button::left_stick)
            {
                dx = stick_x_ * 8.0f; // the glyph leans with the real stick
                dy = stick_y_ * 8.0f;
            }
            if ((held_ & key.actions) != 0)
                list.glow({x + dx - 4, 316 + dy - 26, w + 8, 52}, 26, 16, kAccent.with_alpha(0.8f));
            ui::draw_button(list, fonts, style, key.button, x + dx, 316 + dy, 44);
            x += w + 12;
        }
        ui::text(list, fonts.regular, "Hold a button: its glyph lights up.", kBoardX, 378, 20,
                 kMuted);
    }

    void draw_board(gfx::DrawList &list) const
    {
        const ui::Fonts &fonts = context_.fonts;
        section(list, "SOUND BOARD", kBoardX, 428);
        // The set switch: a pill whose thumb springs between the two names.
        const Rect toggle{1560, 404, 264, 34};
        list.rounded_rect(toggle, 17, kPanel);
        list.rounded_rect({toggle.x + 3 + set_blend_.value * 130, toggle.y + 3, 128, 28}, 14,
                          gfx::mix(kWarm, kAccent, set_blend_.value));
        ui::text(list, fonts.semibold, "paper", toggle.x + 67, toggle.y + 24, 18,
                 gfx::mix(Color::rgb(0x10122a), kMuted, set_blend_.value), gfx::Align::center);
        ui::text(list, fonts.semibold, "glass", toggle.x + 197, toggle.y + 24, 18,
                 gfx::mix(kMuted, Color::rgb(0x10122a), set_blend_.value), gfx::Align::center);

        const float nudge = ui::shake(edge_.value, clock_, 6.0f, 11.0f);
        for (int i = 0; i < static_cast<int>(audio::kCueCount); ++i)
        {
            const Rect r = chip_rect(i);
            const float appear = tween::stagger(age_, i, 0.012f, 0.3f);
            list.push_opacity(appear);
            list.rounded_rect({r.x, r.y + 10 * (1.0f - appear), r.w, r.h}, 12, kPanel);
            if (i == played_ && flash_.value > 0.0f)
                list.rounded_rect(r, 12, kAccent.with_alpha(0.55f * flash_.value));
            ui::text(
                list, fonts.regular,
                fonts.regular.font->fit(audio::cue_name(static_cast<audio::Cue>(i)), 18, r.w - 16),
                r.cx(), r.y + 31 + 10 * (1.0f - appear), 18,
                i == focus_ ? kInk : kInk.with_alpha(0.72f), gfx::Align::center);
            list.pop_opacity();
        }
        Rect ring = ring_.value();
        ring.x += nudge;
        list.glow(ring, 12, 14, kAccent.with_alpha(0.35f));
        list.bordered_rect(ring.inset(-3), 14, Color::rgb(0x000000, 0.0f), 3, kAccent);
    }

    app::Context &context_;
    float age_ = 0.0f;   // seconds since the design was entered
    float clock_ = 0.0f; // seconds since the app started showing it at all
    std::uint32_t held_ = 0;
    float stick_x_ = 0.0f;
    float stick_y_ = 0.0f;
    int focus_ = 0;
    int played_ = -1;
    audio::SoundSet set_ = audio::SoundSet::glass;
    ui::SpringRect ring_;
    ui::Pulse flash_;
    ui::Pulse edge_;
    tween::Spring set_blend_;
    tween::Spring spring_;
    tween::Bounce bounce_;
};

} // namespace

std::unique_ptr<app::Concept> make_toolbox(app::Context &context)
{
    return std::make_unique<Toolbox>(context);
}

} // namespace hui::concepts
