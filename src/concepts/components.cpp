// ps5-homebrew-ui - Design "Component Library": every reusable component, in every theme.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The kit's components (src/ui/components) are whole pieces of interface:
// a list, a grid, a dialog, a form. Each owns its focus, motion and sound and
// is configured through a style struct. This design is their catalogue:
//
//   - L2 / R2 turn the pages, one per group of components;
//   - Square cycles a page's variants: the same component with other knobs;
//   - Options restyles everything in the next of the thirty themes.
//
// The file itself is only the frame around the pages (header, theme and page
// transitions, hints). The pages live in src/concepts/components/, and each
// one is the usage example of its components. What makes it feel finished:
// pages cross-slide in the direction of travel, a theme change hides behind a
// veil of the page colour so two looks never mix, and the page marker glides.

#include "concepts/concepts.hpp"

#include "concepts/components/page.hpp"
#include "core/tween.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace hui::concepts
{

namespace
{

using gfx::Color;
using gfx::Rect;

constexpr gallery::PageFactory kPages[] = {
    gallery::make_lists_page,     gallery::make_collections_page, gallery::make_navigation_page,
    gallery::make_structure_page, gallery::make_overlays_page,    gallery::make_notifications_page,
    gallery::make_actions_page,   gallery::make_forms_page,       gallery::make_pickers_page,
    gallery::make_entry_page,     gallery::make_indicators_page,  gallery::make_data_page,
    gallery::make_media_page,     gallery::make_game_page,        gallery::make_layout_page,
};

constexpr float kMargin = 96.0f;
constexpr float kRight = gfx::kVirtualWidth - kMargin;
constexpr float kPageSwitch = 0.3f;
constexpr float kThemeSwitch = 0.42f;
constexpr float kPageTravel = 46.0f;

constexpr const char *kTechniques[] = {
    "Components own their focus, motion and sound: a screen places them and reads events",
    "One style struct per component: a ui::Theme plus the component's own knobs",
    "Slots (std::function) replace parts of a component's drawing with your own",
    "Every component drawn in all thirty themes, restyled live without losing state",
    "Page changes slide in the direction of travel; theme changes hide behind a veil",
};

class Components final : public app::Concept
{
  public:
    explicit Components(app::Context &context) : context_(context)
    {
        for (const gallery::PageFactory factory : kPages)
            pages_.push_back(factory(context));
        restyle();
        marker_.snap(0.0f);
        build_tour();
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "components",
            "Component Library",
            "Reusable lists, grids, dialogs, forms and indicators, restyled by thirty themes",
            "src/concepts/components.cpp",
            audio::SoundSet::glass,
            Color::rgb(0x8be9c1),
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        age_ = 0.0f;
        page().enter();
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        age_ += dt;
        clock_ += dt;
        if (reduced_ != context_.settings.reduced_motion)
            restyle();

        const int count = static_cast<int>(pages_.size());
        if (input.is_pressed(Action::jump_next) || input.is_pressed(Action::jump_prev))
        {
            direction_ = input.is_pressed(Action::jump_next) ? 1 : -1;
            previous_page_ = page_;
            page_ = (page_ + direction_ + count) % count;
            page_switch_.start(reduced_ ? 0.16f : kPageSwitch);
            page().enter();
            feedback.play(audio::Cue::tab, 1.0f, 0.3f * static_cast<float>(direction_));
        }
        else if (input.is_pressed(Action::menu) && !theme_switch_.running)
        {
            next_theme_ = (theme_ + 1) % static_cast<int>(ui::themes().size());
            theme_switch_.start(reduced_ ? 0.16f : kThemeSwitch);
            feedback.play(audio::Cue::tab, 1.12f);
        }
        else
        {
            page().update(input, dt, feedback);
        }

        page_switch_.update(dt);
        theme_switch_.update(dt);
        // The new theme is applied at the midpoint, when the veil is opaque.
        if (next_theme_ != theme_ && theme_switch_.progress() >= 0.5f)
        {
            theme_ = next_theme_;
            restyle();
        }
        marker_.target = static_cast<float>(page_);
        marker_.update(dt, 18.0f);
    }

    void draw(app::Frame &frame) const override
    {
        const ui::Theme &theme = ui::themes()[static_cast<std::size_t>(theme_)];
        frame.backdrop = theme.backdrop;
        frame.backdrop.time = clock_;
        if (theme.style == ui::SurfaceStyle::glass)
        {
            // Glass needs something behind it worth blurring: slow pools of
            // light, soft enough not to compete with text drawn on the page.
            for (int i = 0; i < 4; ++i)
            {
                const float phase = clock_ * 0.12f + static_cast<float>(i) * 1.7f;
                const float cx = 480.0f + static_cast<float>(i) * 380.0f + std::sin(phase) * 120.0f;
                const float cy = 560.0f + std::cos(phase * 1.3f) * 220.0f;
                frame.scene.shadow({cx - 150.0f, cy - 150.0f, 300.0f, 300.0f}, 150.0f, 170.0f,
                                   gfx::mix(theme.primary, theme.accent, static_cast<float>(i % 2))
                                       .with_alpha(0.34f));
            }
            frame.glass = true;
        }
        const float t = theme_switch_.running ? theme_switch_.progress() : 1.0f;
        const float shown = std::fabs(2.0f * t - 1.0f);
        frame.scene.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                                 theme.page.with_alpha(1.0f - shown));

        gfx::DrawList &list = frame.overlay;
        // The blurred copy exists only in frames that asked for it.
        const std::uint32_t glass = frame.glass ? frame.glass_texture : 0;
        ui::Canvas canvas{list, context_.fonts, glass, clock_};
        ui::Painter paint(list, context_.fonts, theme, glass);
        list.push_opacity(tween::smoothstep(shown) * tween::cubic_out(age_ / 0.3f));
        draw_header(paint, theme);

        if (page_switch_.running)
        {
            const float p = page_switch_.progress();
            const float travel = reduced_ ? 0.0f : kPageTravel * static_cast<float>(direction_);
            draw_page(canvas, previous_page_, 1.0f - tween::smoothstep(p * 2.0f),
                      -travel * tween::cubic_in(p));
            draw_page(canvas, page_, tween::smoothstep(p * 2.0f - 1.0f),
                      travel * (1.0f - tween::cubic_out(p)));
        }
        else
        {
            draw_page(canvas, page_, 1.0f, 0.0f);
        }

        // Dialogs, sheets and toasts cover the header and the page; the hint
        // row stays readable above their scrim, since it describes them.
        // The frame's blur holds the backdrop only (the page is drawn after
        // it), so a frosted panel up here would show the page sharp through
        // it. Given no glass texture, the overlays draw solid instead.
        ui::Canvas above{list, context_.fonts, 0, clock_};
        const std::size_t before = list.instances().size();
        if (!page_switch_.running)
            pages_[static_cast<std::size_t>(page_)]->draw_modal(above);
        // Something modal may have put a scrim under the hints: give them a
        // plate of the page colour so they read in light and dark themes.
        draw_hints(list, paint, theme, list.instances().size() != before);
        list.pop_opacity();
    }

    std::span<const app::TourStep> tour() const override
    {
        return tour_;
    }

  private:
    gallery::Page &page()
    {
        return *pages_[static_cast<std::size_t>(page_)];
    }

    void restyle()
    {
        reduced_ = context_.settings.reduced_motion;
        const ui::Theme &theme = ui::themes()[static_cast<std::size_t>(theme_)];
        for (const std::unique_ptr<gallery::Page> &entry : pages_)
            entry->restyle(theme, reduced_);
    }

    void draw_page(ui::Canvas &canvas, int index, float opacity, float dx) const
    {
        if (opacity <= 0.001f)
            return;
        canvas.list.push_opacity(opacity);
        canvas.list.push_transform(1.0f, 0.0f, 0.0f, dx, 0.0f);
        pages_[static_cast<std::size_t>(index)]->draw(canvas);
        canvas.list.pop_transform();
        canvas.list.pop_opacity();
    }

    void draw_header(ui::Painter &paint, const ui::Theme &theme) const
    {
        const gallery::Page &now = *pages_[static_cast<std::size_t>(page_)];
        const Color ink = paint.page_text();
        const Color quiet = paint.page_text_muted();

        paint.label("COMPONENT LIBRARY", kMargin, 104.0f, 19.0f, quiet);
        paint.heading(now.title(), kMargin - 2.0f, 172.0f, 58.0f, ink);
        paint.body(now.summary(), kMargin, 214.0f, 23.0f, quiet);

        // Where we are among the pages: the neighbours' names either side of
        // the current one, and a row of ticks whose marker glides.
        const int count = static_cast<int>(pages_.size());
        const char *next = pages_[static_cast<std::size_t>((page_ + 1) % count)]->title();
        const char *previous =
            pages_[static_cast<std::size_t>((page_ + count - 1) % count)]->title();
        float x = kRight;
        x -= paint.label(next, x, 108.0f, 21.0f, quiet, gfx::Align::right) + 30.0f;
        const float current = paint.label(now.title(), x, 108.0f, 21.0f, ink, gfx::Align::right);
        paint.fill({x - current, 120.0f, current, 4.0f},
                   theme.corner == ui::Corner::round ? 2.0f : 0.0f,
                   theme.focus.a > 0.6f ? theme.focus : theme.primary);
        x -= current + 30.0f;
        x -= paint.label(previous, x, 108.0f, 21.0f, quiet, gfx::Align::right) + 36.0f;
        constexpr float kTick = 14.0f;
        const float ticks = x - kTick * static_cast<float>(count);
        for (int i = 0; i < count; ++i)
            paint.fill({ticks + kTick * static_cast<float>(i), 97.0f, 8.0f, 4.0f}, 0.0f,
                       quiet.with_alpha(0.45f));
        paint.fill({ticks + kTick * marker_.value - 1.0f, 95.0f, 10.0f, 8.0f}, 0.0f, ink);

        char text[48];
        std::snprintf(text, sizeof(text), "%02d / %02d", theme_ + 1,
                      static_cast<int>(ui::themes().size()));
        const float number = paint.label(text, kRight, 172.0f, 22.0f, quiet, gfx::Align::right);
        paint.label(theme.name, kRight - number - 18.0f, 172.0f, 30.0f, ink, gfx::Align::right);
        paint.body(now.variant(), kRight, 214.0f, 23.0f, quiet, gfx::Align::right);
    }

    void draw_hints(gfx::DrawList &list, const ui::Painter &paint, const ui::Theme &theme,
                    bool plate) const
    {
        ui::Hint hints[10];
        int count = 0;
        for (const ui::Hint &hint : pages_[static_cast<std::size_t>(page_)]->hints())
        {
            if (count < 8)
                hints[count++] = hint;
        }
        hints[count++] = {ui::Button::l2, "Page", ui::Button::r2};
        hints[count++] = {ui::Button::options, "Theme"};
        ui::GlyphStyle glyphs = theme.dark ? ui::GlyphStyle::dark() : ui::GlyphStyle::light();
        glyphs.label = paint.page_text();
        if (plate)
        {
            const float width = ui::measure_hints(context_.fonts, hints, count);
            list.rounded_rect({kRight - width - 26.0f, 978.0f, width + 52.0f, 64.0f},
                              theme.corner == ui::Corner::round ? 32.0f : 0.0f,
                              theme.page.with_alpha(0.9f));
        }
        ui::draw_hints(list, context_.fonts, glyphs, hints, count, kRight, true);
    }

    // Every page's own tour, then a walk through the themes on the first page.
    void build_tour()
    {
        const std::uint32_t next_page = action_bit(Action::jump_next);
        const std::uint32_t next_theme = action_bit(Action::menu);
        for (const std::unique_ptr<gallery::Page> &entry : pages_)
        {
            for (const app::TourStep &step : entry->tour())
                tour_.push_back(step);
            tour_.push_back({0.6f, next_page});
        }
        // The theme walk runs on the Forms page: switches, sliders, steppers,
        // fields and panels show the most of a theme at once.
        int walk_page = 0;
        for (std::size_t i = 0; i < pages_.size(); ++i)
        {
            if (std::string_view(pages_[i]->title()) == "Forms")
                walk_page = static_cast<int>(i);
        }
        for (int i = 0; i < walk_page; ++i)
            tour_.push_back({0.35f, next_page});
        const auto themes = ui::themes();
        for (std::size_t i = 0; i < themes.size(); ++i)
        {
            // Three very different themes get every page photographed: the
            // proof that each component holds up outside the default look.
            const bool every_page = i == 1 || i == 10 || i == 27;
            if (every_page)
            {
                const int count = static_cast<int>(pages_.size());
                for (int p = 0; p < count; ++p)
                {
                    std::string name = std::string(themes[i].id) + "-";
                    for (const char *c =
                             pages_[static_cast<std::size_t>((walk_page + p) % count)]->title();
                         *c != '\0'; ++c)
                        name += static_cast<char>(*c >= 'A' && *c <= 'Z' ? *c - 'A' + 'a' : *c);
                    capture_names_.push_back(std::move(name));
                    tour_.push_back(
                        {0.8f, next_page, Direction::none, capture_names_.back().c_str()});
                }
            }
            // A step's picture is taken before its press: it shows theme i.
            const bool pictured = i == 2 || i == 5 || i == 7 || i == 13 || i == 18;
            tour_.push_back({pictured ? 0.75f : 0.5f, next_theme, Direction::none,
                             pictured ? themes[i].id : nullptr});
        }
        for (int i = walk_page; i < static_cast<int>(pages_.size()); ++i)
            tour_.push_back({0.35f, next_page});
    }

    app::Context &context_;
    std::vector<std::unique_ptr<gallery::Page>> pages_;
    std::vector<app::TourStep> tour_;
    std::deque<std::string> capture_names_; // a deque never moves its strings
    int page_ = 0;
    int previous_page_ = 0;
    int direction_ = 1;
    int theme_ = 0;
    int next_theme_ = 0;
    bool reduced_ = false;
    float age_ = 0.0f;
    float clock_ = 0.0f;
    tween::Timer page_switch_;
    tween::Timer theme_switch_;
    tween::Spring marker_;
};

} // namespace

std::unique_ptr<app::Concept> make_components(app::Context &context)
{
    return std::make_unique<Components>(context);
}

} // namespace hui::concepts
