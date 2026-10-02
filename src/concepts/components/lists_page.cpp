// ps5-homebrew-ui - Component Library page: ListView in three configurations.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One component, three sets of knobs: a menu with sections, values and badges;
// cards with a leading slot drawn by the page; a compact wrapped list inside
// a panel. Square swaps the highlight of all three. The page itself only
// moves the focus between the lists and reports what they return.

#include "concepts/components/page.hpp"

#include "ui/components/list.hpp"

#include <array>
#include <cstdio>
#include <string>

namespace hui::concepts::gallery
{

namespace
{

using gfx::Color;
using gfx::Rect;

constexpr int kLists = 3;
constexpr Rect kBounds[kLists] = {
    {96.0f, 290.0f, 548.0f, 590.0f},
    {690.0f, 290.0f, 548.0f, 590.0f},
    {1284.0f, 290.0f, 528.0f, 590.0f},
};
constexpr const char *kCaptions[kLists] = {
    "Sections, values, badges, a disabled row",
    "Cards and a leading slot",
    "Compact, in a panel, wraps around",
};

struct Variant
{
    const char *name;
    ui::HighlightKind kind;
};
constexpr Variant kVariants[] = {
    {"Highlight: tint", ui::HighlightKind::tint},
    {"Highlight: fill", ui::HighlightKind::fill},
    {"Highlight: bar", ui::HighlightKind::bar},
    {"Highlight: theme ring", ui::HighlightKind::ring},
    {"Highlight: underline", ui::HighlightKind::underline},
    {"Highlight: glow", ui::HighlightKind::glow},
};
constexpr int kVariantCount = static_cast<int>(std::size(kVariants));

constexpr ui::Hint kHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Choose"},
    {ui::Button::square, "Highlight"},
};

constexpr app::TourStep kTour[] = {
    {0.5f, 0, Direction::down},
    {0.2f, 0, Direction::down},
    {0.2f, 0, Direction::down},
    {0.5f, action_bit(Action::confirm), Direction::none, "lists"},
    {0.4f, 0, Direction::right},
    {0.25f, 0, Direction::down},
    {0.25f, action_bit(Action::west)},
    {0.6f, 0, Direction::right, "lists-fill"},
    {0.25f, action_bit(Action::west)},
    {0.2f, 0, Direction::down},
    {0.2f, 0, Direction::down},
    {0.2f, 0, Direction::down},
    {0.2f, 0, Direction::down},
    {0.6f, action_bit(Action::west), Direction::none, "lists-bar"},
    {0.5f, action_bit(Action::west)},
    {0.5f, action_bit(Action::west)},
    {0.6f, action_bit(Action::west), Direction::none, "lists-glow"},
    {0.2f, 0, Direction::left},
    {0.2f, 0, Direction::left},
};

class ListsPage final : public Page
{
  public:
    explicit ListsPage(app::Context &context) : context_(context)
    {
        std::vector<ui::ListItem> menu;
        const auto header = [&menu](const char *title)
        {
            ui::ListItem item;
            item.title = title;
            item.header = true;
            menu.push_back(item);
        };
        const auto row = [&menu](const char *title, const char *subtitle, const char *value,
                                 bool chevron) -> ui::ListItem &
        {
            ui::ListItem item;
            item.title = title;
            item.subtitle = subtitle;
            item.value = value;
            item.chevron = chevron;
            menu.push_back(item);
            return menu.back();
        };
        header("Play");
        row("Continue", "Chapter 8, Lantern Pass", "63 h", false);
        row("New journey", "Start again from the first spark", "", false);
        row("Challenges", "Three new this week", "", false).badge = "NEW";
        header("System");
        row("Display", "", "2160p", true);
        row("Sound", "", "Surround", true);
        row("Controller", "", "", true);
        row("Network play", "Sign in to use it", "", false).disabled = true;
        header("Extras");
        row("Gallery", "", "42 / 60", true);
        row("Soundtrack", "", "", true);
        row("Credits", "", "", true);
        lists_[0].set_items(std::move(menu));

        std::vector<ui::ListItem> people;
        for (std::size_t i = 0; i < 7 && i < context_.catalog.size(); ++i)
        {
            const demo::Item &title = context_.catalog[i];
            ui::ListItem item;
            item.title = title.title;
            item.subtitle = std::string(title.genre) + " \xC2\xB7 " + title.studio;
            people.push_back(item);
        }
        lists_[1].set_items(std::move(people));
        lists_[1].leading = [this](ui::Canvas &canvas, const Rect &box, const ui::ListItem &,
                                   int index, float focus)
        {
            // The page draws the cover; the list only reserves the room.
            const float size = 52.0f + 6.0f * focus;
            const Rect art{box.cx() - size * 0.5f, box.cy() - size * 0.5f, size, size};
            const demo::Item &title = context_.catalog[static_cast<std::size_t>(index)];
            if (title.cover != 0)
                canvas.list.image(title.cover, art, gfx::kCanvasUv, Color::rgb(0xffffff), 12.0f);
            else
                canvas.list.gradient_rect(art, 12.0f, title.mid, title.dark);
        };

        const char *names[] = {"Ember", "Coral", "Amber", "Moss",  "Teal",
                               "Sky",   "Iris",  "Plum",  "Slate", "Bone"};
        const std::uint32_t inks[] = {0xe5484d, 0xff7a59, 0xf5a524, 0x30a46c, 0x12a594,
                                      0x3e9eff, 0x6e56cf, 0xab4aba, 0x8b8d98, 0xe8e2d0};
        std::vector<ui::ListItem> colours;
        for (std::size_t i = 0; i < std::size(names); ++i)
        {
            ui::ListItem item;
            item.title = names[i];
            char hex[16];
            std::snprintf(hex, sizeof(hex), "#%06x", inks[i]);
            item.value = hex;
            item.swatch = Color::rgb(inks[i]);
            colours.push_back(item);
        }
        lists_[2].set_items(std::move(colours));

        for (int i = 0; i < kLists; ++i)
            lists_[static_cast<std::size_t>(i)].set_bounds(kBounds[i]);
        restyle(ui::default_theme(), false);
    }

    const char *title() const override
    {
        return "Lists";
    }
    const char *summary() const override
    {
        return "ui::ListView: one gliding highlight, spring scrolling, soft refusals, three slots";
    }
    const char *variant() const override
    {
        return kVariants[variant_].name;
    }

    void restyle(const ui::Theme &theme, bool reduced_motion) override
    {
        theme_ = theme;
        for (ui::ListView &list : lists_)
        {
            list.style.theme = theme;
            list.style.reduced_motion = reduced_motion;
            list.style.highlight.kind = kVariants[variant_].kind;
        }
        // The same component, three sets of knobs.
        ui::ListStyle &cards = lists_[1].style;
        cards.cards = true;
        cards.row_height = 84.0f;
        cards.gap = 12.0f;
        cards.leading_width = 60.0f;
        cards.title_size = 26.0f;
        ui::ListStyle &compact = lists_[2].style;
        compact.panel = true;
        compact.dividers = true;
        compact.wrap = true;
        compact.row_height = 58.0f;
        compact.gap = 2.0f;
        compact.title_size = 24.0f;
        compact.value_size = 21.0f;
        compact.focus_shift = 0.0f;
        // Sizes changed: have the lists lay their rows out again.
        for (int i = 0; i < kLists; ++i)
        {
            ui::ListView &list = lists_[static_cast<std::size_t>(i)];
            list.set_items(list.items());
            list.set_active(i == column_);
        }
    }

    void enter() override
    {
        for (ui::ListView &list : lists_)
            list.enter();
        status_.clear();
    }

    void update(const InputFrame &input, float dt, ui::Feedback &feedback) override
    {
        clock_ += dt;
        ui::ListView &active = lists_[static_cast<std::size_t>(column_)];
        if (input.is_pressed(Action::west))
        {
            variant_ = (variant_ + 1) % kVariantCount;
            for (ui::ListView &list : lists_)
                list.style.highlight.kind = kVariants[variant_].kind;
            ui::play_cue(feedback, active.style, active.style.sounds.change);
        }
        else if (input.nav == Direction::left || input.nav == Direction::right)
        {
            const int next = column_ + (input.nav == Direction::right ? 1 : -1);
            if (next < 0 || next >= kLists)
            {
                ui::refuse(feedback, active.style, input, edge_, kBounds[column_].cx());
            }
            else
            {
                column_ = next;
                for (int i = 0; i < kLists; ++i)
                    lists_[static_cast<std::size_t>(i)].set_active(i == column_);
                ui::play_cue(feedback, active.style, active.style.sounds.move,
                             kBounds[column_].cx());
            }
        }
        else
        {
            const ui::Event event = active.handle(input, feedback);
            const std::string &name =
                active.items()[static_cast<std::size_t>(active.focus())].title;
            if (event == ui::Event::activated)
                status_ = "Event::activated  \xC2\xB7  " + name;
            else if (event == ui::Event::refused)
                status_ = "Event::refused  \xC2\xB7  " + name;
            else if (event == ui::Event::cancelled)
                status_ = "Event::cancelled";
            else if (event == ui::Event::moved)
                status_ = "Event::moved  \xC2\xB7  " + name;
        }
        for (ui::ListView &list : lists_)
            list.update(dt);
        edge_.update(dt, 9.0f);
    }

    void draw(ui::Canvas &canvas) const override
    {
        ui::Painter paint(canvas.list, canvas.fonts, theme_, canvas.glass);
        const float nudge = ui::shake(edge_.value, clock_, 10.0f);
        for (int i = 0; i < kLists; ++i)
        {
            const bool active = i == column_;
            paint.label(kCaptions[i], kBounds[i].x + (active ? nudge : 0.0f), 268.0f, 20.0f,
                        active ? paint.page_text() : paint.page_text_muted());
            lists_[static_cast<std::size_t>(i)].draw(canvas);
        }
        if (!status_.empty())
            paint.body(status_, kPageArea.x, 930.0f, 22.0f, paint.page_text_muted());
    }

    std::span<const ui::Hint> hints() const override
    {
        return kHints;
    }
    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    app::Context &context_;
    ui::Theme theme_ = ui::default_theme();
    std::array<ui::ListView, kLists> lists_;
    int column_ = 0;
    int variant_ = 0;
    float clock_ = 0.0f;
    ui::Pulse edge_;
    std::string status_;
};

} // namespace

std::unique_ptr<Page> make_lists_page(app::Context &context)
{
    return std::make_unique<ListsPage>(context);
}

} // namespace hui::concepts::gallery
