// ps5-homebrew-ui - Component Library page: the components that move the player between places.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A small "console shell" built from the five navigation components: a rail
// at the left (SideNav), the path to where the player is (Breadcrumb), tabs
// whose content slides with TabBar::active_value(), a shelf of pages with its
// position under it (PageDots), and a context menu on whatever has the focus
// (Menu, Triangle). The page only moves the focus between the three regions
// and turns what the components report into the path and the status line.

#include "concepts/components/page.hpp"

#include "ui/components/breadcrumb.hpp"
#include "ui/components/menu.hpp"
#include "ui/components/page_dots.hpp"
#include "ui/components/sidenav.hpp"
#include "ui/components/tabs.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace hui::concepts::gallery
{

namespace
{

using gfx::Color;
using gfx::Rect;

// ---- layout: one column of content beside the rail --------------------------
constexpr float kRailTop = 246.0f;
constexpr float kRailHeight = 694.0f;
constexpr float kContentGap = 34.0f; // between the rail and the content
constexpr float kRight = kPageArea.x + kPageArea.w;
constexpr float kCrumbY = 246.0f;
constexpr float kCrumbHeight = 40.0f;
constexpr float kCrumbWidth = 520.0f;
constexpr float kTabsY = 302.0f;
constexpr float kTabsHeight = 56.0f;
constexpr float kPanelY = 378.0f;
constexpr float kPanelHeight = 232.0f;
constexpr float kCaptionBaseline = 650.0f;
constexpr float kStripY = 666.0f;
constexpr float kStripHeight = 190.0f;
constexpr float kDotsY = 876.0f;
constexpr float kDotsHeight = 36.0f;
constexpr float kCardGap = 20.0f;
constexpr float kBleed = 16.0f; // room around a clip for rings and shadows

constexpr int kPerPage = 3;
constexpr int kPages = 5;
constexpr int kCards = kPerPage * kPages;
constexpr int kTabs = 5;
constexpr const char *kRoot = "Hearth";
constexpr float kPi = 3.14159265f;

// ---- icons: drawn from shapes, so they take any ink and any size ------------
enum Icon : int
{
    icon_home,
    icon_grid,
    icon_bag,
    icon_people,
    icon_bubble,
    icon_download,
    icon_gear,
    icon_rail,
    icon_play,
    icon_star,
    icon_move,
    icon_cross,
    icon_lens,
};

void draw_icon(gfx::DrawList &list, int icon, const Rect &box, Color ink)
{
    const float s = std::min(box.w, box.h);
    const float x = box.cx() - s * 0.5f;
    const float y = box.cy() - s * 0.5f;
    const float cx = box.cx();
    const float cy = box.cy();
    const float t = std::max(2.0f, s * 0.09f);
    switch (icon)
    {
    case icon_home:
        list.triangle({x + s * 0.04f, y + s * 0.06f, s * 0.92f, s * 0.44f}, ink);
        list.rounded_rect({x + s * 0.2f, y + s * 0.5f, s * 0.6f, s * 0.42f}, s * 0.06f, ink);
        break;
    case icon_grid:
        for (int i = 0; i < 4; ++i)
            list.rounded_rect({x + s * (i % 2 == 0 ? 0.08f : 0.56f),
                               y + s * (i < 2 ? 0.08f : 0.56f), s * 0.36f, s * 0.36f},
                              s * 0.08f, ink);
        break;
    case icon_bag:
        list.arc(cx, y + s * 0.4f, s * 0.24f, t, -kPi * 0.5f, kPi, ink);
        list.rounded_rect({x + s * 0.1f, y + s * 0.38f, s * 0.8f, s * 0.56f}, s * 0.1f, ink);
        break;
    case icon_people:
        list.circle(cx, y + s * 0.28f, s * 0.2f, ink);
        list.rounded_rect({x + s * 0.12f, y + s * 0.56f, s * 0.76f, s * 0.38f}, s * 0.19f, ink);
        break;
    case icon_bubble:
        list.rounded_rect({x + s * 0.04f, y + s * 0.1f, s * 0.92f, s * 0.6f}, s * 0.16f, ink);
        list.triangle({x + s * 0.18f, y + s * 0.62f, s * 0.3f, s * 0.28f}, ink, 0.0f, kPi);
        break;
    case icon_download:
        list.line(cx, y + s * 0.08f, cx, y + s * 0.6f, t, ink);
        list.line(cx - s * 0.22f, y + s * 0.4f, cx, y + s * 0.62f, t, ink);
        list.line(cx + s * 0.22f, y + s * 0.4f, cx, y + s * 0.62f, t, ink);
        list.line(x + s * 0.12f, y + s * 0.9f, x + s * 0.88f, y + s * 0.9f, t, ink);
        break;
    case icon_gear:
        list.ring(cx, cy, s * 0.3f, t, ink);
        for (int i = 0; i < 6; ++i)
        {
            const float angle = static_cast<float>(i) * kPi / 3.0f;
            list.line(cx + std::sin(angle) * s * 0.32f, cy - std::cos(angle) * s * 0.32f,
                      cx + std::sin(angle) * s * 0.46f, cy - std::cos(angle) * s * 0.46f, t, ink);
        }
        break;
    case icon_rail:
        list.bordered_rect({x + s * 0.04f, y + s * 0.14f, s * 0.92f, s * 0.72f}, s * 0.12f,
                           Color{ink.r, ink.g, ink.b, 0.0f}, t, ink);
        list.rounded_rect({x + s * 0.2f, y + s * 0.3f, s * 0.14f, s * 0.4f}, s * 0.04f, ink);
        break;
    case icon_play:
        list.triangle({x + s * 0.16f, y + s * 0.16f, s * 0.68f, s * 0.68f}, ink, 0.0f, kPi * 0.5f);
        break;
    case icon_star:
        list.star(cx, cy, s * 0.48f, ink);
        break;
    case icon_move:
        list.line(x + s * 0.1f, cy, x + s * 0.86f, cy, t, ink);
        list.line(x + s * 0.6f, cy - s * 0.26f, x + s * 0.88f, cy, t, ink);
        list.line(x + s * 0.6f, cy + s * 0.26f, x + s * 0.88f, cy, t, ink);
        break;
    case icon_cross:
        list.line(x + s * 0.2f, y + s * 0.2f, x + s * 0.8f, y + s * 0.8f, t, ink);
        list.line(x + s * 0.8f, y + s * 0.2f, x + s * 0.2f, y + s * 0.8f, t, ink);
        break;
    default:
        list.ring(cx, cy, s * 0.42f, t, ink);
        list.circle(cx, cy, s * 0.16f, ink);
        break;
    }
}

// ---- the menu's actions -----------------------------------------------------
enum MenuAction : int
{
    action_open,
    action_pin,
    action_rail,
    action_move,
    action_remove,
};

// ---- variants: every component changes kind together ------------------------
struct Variant
{
    const char *name;
    ui::TabKind tabs;
    ui::TabWidth tab_width;
    bool tab_glyphs;
    bool expanded;
    ui::HighlightKind rail;
    ui::PageDotsKind dots;
    float auto_advance;
    ui::CrumbSeparator separator;
    bool chips;
    int max_segments;
    ui::HighlightKind menu;
    float scrim;
};
constexpr Variant kVariants[] = {
    {"Pill \xC2\xB7 open rail \xC2\xB7 dots", ui::TabKind::pill, ui::TabWidth::fit, false, true,
     ui::HighlightKind::tint, ui::PageDotsKind::dots, 0.0f, ui::CrumbSeparator::chevron, false, 0,
     ui::HighlightKind::tint, 0.0f},
    {"Underline \xC2\xB7 icon rail \xC2\xB7 timed dashes", ui::TabKind::underline,
     ui::TabWidth::fit, false, false, ui::HighlightKind::bar, ui::PageDotsKind::dashes, 6.0f,
     ui::CrumbSeparator::slash, false, 0, ui::HighlightKind::bar, 0.0f},
    {"Segmented \xC2\xB7 open rail \xC2\xB7 numbers", ui::TabKind::segmented, ui::TabWidth::fill,
     false, true, ui::HighlightKind::fill, ui::PageDotsKind::numbers, 0.0f, ui::CrumbSeparator::dot,
     false, 3, ui::HighlightKind::fill, 0.4f},
    {"Boxed \xC2\xB7 icon rail \xC2\xB7 timed dots", ui::TabKind::boxed, ui::TabWidth::equal, true,
     false, ui::HighlightKind::ring, ui::PageDotsKind::dots, 6.0f, ui::CrumbSeparator::chevron,
     true, 0, ui::HighlightKind::glow, 0.0f},
};
constexpr int kVariantCount = static_cast<int>(std::size(kVariants));

constexpr ui::Hint kHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Open"},
    {ui::Button::triangle, "Menu"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kMenuHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Choose"},
    {ui::Button::circle, "Close"},
};

constexpr std::uint32_t kConfirm = action_bit(Action::confirm);
constexpr std::uint32_t kBack = action_bit(Action::back);
constexpr std::uint32_t kMenu = action_bit(Action::north);
constexpr std::uint32_t kNext = action_bit(Action::west);
constexpr app::TourStep kTour[] = {
    {0.6f, 0, Direction::down},                // rail: Library
    {0.3f, kConfirm},                          // ... becomes the current place
    {0.7f, 0, Direction::right, "navigation"}, // to the tabs
    {0.3f, 0, Direction::right},               // Saves
    {0.35f, 0, Direction::right},              // Trophies
    {0.3f, 0, Direction::down},                // to the shelf
    {0.3f, 0, Direction::right},
    {0.35f, kConfirm}, // open a title: the path folds
    {0.5f, kMenu},     // its menu flips above the shelf
    {0.3f, 0, Direction::down},
    {0.3f, 0, Direction::down},
    {0.6f, 0, Direction::down, "navigation-menu"},
    {0.3f, kBack},
    {0.4f, kNext}, // underline, icon rail, timed dashes
    {0.3f, 0, Direction::right},
    {0.3f, 0, Direction::right},
    {0.3f, 0, Direction::right},
    {0.8f, kNext, Direction::none, "navigation-underline"}, // segmented, numbers
    {0.4f, 0, Direction::up},
    {0.4f, 0, Direction::left},
    {0.8f, kNext, Direction::none, "navigation-segmented"}, // boxed, chips
    {0.6f, kMenu},
    {0.8f, kBack, Direction::none, "navigation-boxed"},
    {0.4f, kNext}, // back to the first variant
    {0.4f, 0, Direction::left},
};

enum class Region : std::uint8_t
{
    rail,
    tabs,
    shelf,
};

class NavigationPage final : public Page
{
  public:
    explicit NavigationPage(app::Context &context) : context_(context)
    {
        const auto entry = [](const char *label, int icon)
        {
            ui::NavEntry out;
            out.label = label;
            out.tag = icon;
            return out;
        };
        std::vector<ui::NavEntry> entries;
        entries.push_back(entry("Home", icon_home));
        entries.push_back(entry("Library", icon_grid));
        entries.push_back(entry("Store", icon_bag));
        entries.push_back(entry("Friends", icon_people));
        entries.back().section = "Social";
        entries.push_back(entry("Messages", icon_bubble));
        entries.back().badge = "3";
        entries.push_back(entry("Downloads", icon_download));
        entries.back().separator = true;
        entries.push_back(entry("Settings", icon_gear));
        entries.push_back(entry("Collapse", icon_rail));
        entries.back().action = true; // it folds the rail; it is not a place
        rail_.set_entries(std::move(entries));
        rail_.style.footer = true;
        rail_.style.expanded_width = 290.0f;
        rail_.icon = [](ui::Canvas &canvas, const Rect &box, const ui::NavEntry &item, int, float,
                        Color ink) { draw_icon(canvas.list, item.tag, box, ink); };
        rail_.set_bounds({kPageArea.x, kRailTop, 0.0f, kRailHeight});

        tabs_.set_tabs({{"Overview", 0, false, icon_grid},
                        {"Saves", 3, false, icon_download},
                        {"Trophies", 0, false, icon_star},
                        {"Add-ons", 12, false, icon_bag},
                        {"Captures", 0, false, icon_lens}});
        tabs_.glyph =
            [](ui::Canvas &canvas, const Rect &box, const ui::TabItem &item, int, float, Color ink)
        { draw_icon(canvas.list, item.tag, {box.x, box.cy() - box.w * 0.5f, box.w, box.w}, ink); };
        tabs_.set_focused(false);

        dots_.set_count(kPages);

        const auto item = [](const char *label, const char *shortcut, int action)
        {
            ui::MenuItem out;
            out.label = label;
            out.shortcut = shortcut;
            out.tag = action;
            return out;
        };
        std::vector<ui::MenuItem> items;
        items.push_back(item("Open", "", action_open));
        items.push_back(item("Pin to home", "", action_pin));
        items.back().checkable = true;
        items.push_back(item("Compact rail", "", action_rail));
        items.back().checkable = true;
        items.push_back({});
        items.back().separator = true;
        items.push_back(item("Move to...", "Soon", action_move));
        items.back().disabled = true;
        items.push_back(item("Remove", "Hold", action_remove));
        items.back().danger = true;
        menu_.set_items(std::move(items));
        menu_.style.glyph_width = 24.0f;
        menu_.style.row_height = 54.0f;
        menu_.glyph = [](ui::Canvas &canvas, const Rect &box, const ui::MenuItem &entry, int, float,
                         Color ink)
        {
            constexpr int kIcons[] = {icon_play, icon_star, icon_rail, icon_move, icon_cross};
            draw_icon(canvas.list, kIcons[entry.tag],
                      {box.x, box.cy() - box.w * 0.5f, box.w, box.w}, ink);
        };

        sync_path(false);
        restyle(ui::default_theme(), false);
    }

    const char *title() const override
    {
        return "Navigation";
    }
    const char *summary() const override
    {
        return "SideNav, Breadcrumb, TabBar, PageDots and Menu as one small shell";
    }
    const char *variant() const override
    {
        return kVariants[variant_].name;
    }

    void restyle(const ui::Theme &theme, bool reduced_motion) override
    {
        theme_ = theme;
        reduced_ = reduced_motion;
        for (ui::ComponentStyle *style : {static_cast<ui::ComponentStyle *>(&rail_.style),
                                          static_cast<ui::ComponentStyle *>(&tabs_.style),
                                          static_cast<ui::ComponentStyle *>(&crumbs_.style),
                                          static_cast<ui::ComponentStyle *>(&dots_.style),
                                          static_cast<ui::ComponentStyle *>(&menu_.style)})
        {
            style->theme = theme;
            style->reduced_motion = reduced_motion;
        }
        // Each of these draws straight on the page, not on a panel.
        tabs_.style.on_page = true;
        crumbs_.style.on_page = true;
        dots_.style.on_page = true;
        card_style_.theme = theme;
        card_style_.reduced_motion = reduced_motion;
        apply_variant(false);
    }

    void enter() override
    {
        rail_.enter();
        menu_.dismiss();
        status_.clear();
        swap_age_ = 0.0f;
    }

    void update(const InputFrame &input, float dt, ui::Feedback &feedback) override
    {
        clock_ += dt;
        swap_age_ += dt;
        if (menu_.is_open())
            update_menu(input, feedback);
        else if (input.is_pressed(Action::west))
        {
            variant_ = (variant_ + 1) % kVariantCount;
            apply_variant(true);
            ui::play_cue(feedback, rail_.style, rail_.style.sounds.change);
        }
        else if (input.is_pressed(Action::north))
            open_menu(feedback);
        else if (region_ == Region::rail)
            update_rail(input, feedback);
        else if (region_ == Region::tabs)
            update_tabs(input, feedback);
        else
            update_shelf(input, feedback);

        rail_.set_focused(region_ == Region::rail);
        tabs_.set_focused(region_ == Region::tabs);
        // The shelf turns its own pages only while nobody is choosing on it.
        dots_.set_paused(region_ == Region::shelf || menu_.visible());

        rail_.update(dt);
        layout();
        tabs_.update(dt);
        crumbs_.update(dt);
        dots_.update(dt);
        if (dots_.take_advanced())
            card_ = dots_.page() * kPerPage;
        menu_.update(dt);

        // The ring around the focused card lives in the shelf's own space
        // (cards side by side, unscrolled), so turning a page does not make
        // it lag. While the rail changes width the cards resize under it.
        const Rect target = card_slot(card_, content_width());
        card_ring_.target(target);
        if (std::fabs(content_width() - ring_width_) > 0.01f)
            card_ring_.snap(target);
        ring_width_ = content_width();
        card_ring_.update(dt, card_style_);
        shelf_focus_.target = region_ == Region::shelf ? 1.0f : 0.0f;
        shelf_focus_.update(dt, 18.0f);
        edge_.update(dt, 9.0f);
    }

    void draw(ui::Canvas &canvas) const override
    {
        gfx::DrawList &list = canvas.list;
        ui::Painter paint(list, canvas.fonts, theme_, canvas.glass);
        const float left = content_left();
        const float width = content_width();
        const float nudge = ui::shake(edge_.value, clock_, 10.0f);

        rail_.draw(canvas);
        crumbs_.draw(canvas);
        if (!status_.empty())
            paint.body(ui::fit_body(paint, status_, 21.0f, width - kCrumbWidth - 40.0f), kRight,
                       kCrumbY + kCrumbHeight * 0.5f + 7.0f, 21.0f, paint.page_text_muted(),
                       gfx::Align::right);
        tabs_.draw(canvas);

        // ---- the tab's content: one panel per tab, slid by the bar ----
        const float pitch = width + 64.0f;
        list.push_clip({left - kBleed, kPanelY - kBleed, width + 2.0f * kBleed,
                        kPanelHeight + 2.0f * kBleed + 6.0f});
        for (int tab = 0; tab < kTabs; ++tab)
        {
            const float away = static_cast<float>(tab) - tabs_.active_value();
            if (std::fabs(away) >= 1.0f)
                continue;
            const float arrive = tween::cubic_out(swap_age_ / 0.28f);
            list.push_opacity((1.0f - std::fabs(away)) * arrive);
            list.push_transform(1.0f, 0.0f, 0.0f, reduced_ ? 0.0f : away * pitch,
                                reduced_ ? 0.0f : 12.0f * (1.0f - arrive));
            draw_content(canvas, paint, tab, {left, kPanelY, width, kPanelHeight});
            list.pop_transform();
            list.pop_opacity();
        }
        list.pop_clip();

        // ---- the shelf: pages of cards, slid by the dots ----
        paint.label("PageDots \xC2\xB7 a shelf of five pages",
                    left + (region_ == Region::shelf ? nudge : 0.0f), kCaptionBaseline, 20.0f,
                    region_ == Region::shelf ? paint.page_text() : paint.page_text_muted());
        const float turn = reduced_ ? static_cast<float>(dots_.page()) : dots_.value();
        const float scroll = turn * (width + kCardGap);
        list.push_clip({left - kBleed, kStripY - kBleed, width + 2.0f * kBleed,
                        kStripHeight + 2.0f * kBleed + 4.0f});
        // Surfaces, then the ring, then what is on the cards: a theme's ring
        // may light what it surrounds, and the text must stay on top of it.
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int card = 0; card < kCards; ++card)
            {
                Rect slot = card_slot(card, width);
                slot.x += left - scroll;
                slot.y += kStripY;
                if (slot.x + slot.w < left - kBleed || slot.x > left + width + kBleed)
                    continue;
                if (pass == 0)
                    paint.surface(slot, card_radius(slot), theme_.surface, theme_.outline, 1.0f);
                else
                    draw_card(canvas, paint, card, slot);
            }
            if (pass == 1)
                break;
            list.push_transform(1.0f, 0.0f, 0.0f,
                                left - scroll + (region_ == Region::shelf ? nudge : 0.0f), kStripY);
            ui::HighlightStyle ring;
            ring.kind = ui::HighlightKind::ring;
            ring.radius = card_radius(card_slot(0, width));
            card_ring_.draw(canvas, card_style_, ring, shelf_focus_.value);
            list.pop_transform();
        }
        list.pop_clip();
        dots_.draw(canvas);

        menu_.draw(canvas);
    }

    std::span<const ui::Hint> hints() const override
    {
        if (menu_.is_open())
            return kMenuHints;
        return kHints;
    }
    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    // ---- geometry ----
    float content_left() const
    {
        const Rect rail = rail_.rect();
        return rail.x + rail.w + kContentGap;
    }
    float content_width() const
    {
        return kRight - content_left();
    }
    // A card's place on the shelf, before scrolling: pages side by side.
    static Rect card_slot(int card, float width)
    {
        const float card_w =
            (width - kCardGap * static_cast<float>(kPerPage - 1)) / static_cast<float>(kPerPage);
        return {static_cast<float>(card) * (card_w + kCardGap), 0.0f, card_w, kStripHeight};
    }
    float card_radius(const Rect &slot) const
    {
        return std::min(theme_.radius_card, std::min(slot.w, slot.h) * 0.5f);
    }
    void layout()
    {
        const float left = content_left();
        const float width = content_width();
        crumbs_.set_bounds({left, kCrumbY, kCrumbWidth, kCrumbHeight});
        tabs_.set_bounds({left, kTabsY, width, kTabsHeight});
        dots_.set_bounds({left, kDotsY, width, kDotsHeight});
    }

    // ---- state changes ----
    void apply_variant(bool resize_rail)
    {
        const Variant &now = kVariants[variant_];
        tabs_.style.kind = now.tabs;
        tabs_.style.width = now.tab_width;
        tabs_.style.glyph_width = now.tab_glyphs ? 24.0f : 0.0f;
        rail_.style.highlight.kind = now.rail;
        dots_.style.kind = now.dots;
        dots_.style.auto_advance = now.auto_advance;
        if (now.auto_advance <= 0.0f)
            dots_.set_progress(0.0f); // no clock: nothing left to show in the mark
        crumbs_.style.separator = now.separator;
        crumbs_.style.chips = now.chips;
        crumbs_.style.max_segments = now.max_segments;
        crumbs_.style.text_size = now.chips ? 21.0f : 24.0f;
        menu_.style.highlight.kind = now.menu;
        menu_.style.scrim = now.scrim;
        // A theme change keeps the rail as the player left it; a variant
        // change is asked to show the other width.
        if (resize_rail)
            set_rail_expanded(now.expanded);
    }
    void set_rail_expanded(bool expanded)
    {
        rail_.set_expanded(expanded);
        const int last = static_cast<int>(rail_.entries().size()) - 1;
        rail_.entry(last).label = expanded ? "Collapse" : "Expand";
    }
    void sync_path(bool animate)
    {
        std::vector<std::string> path{kRoot};
        path.push_back(rail_.entries()[static_cast<std::size_t>(rail_.current())].label);
        path.push_back(tabs_.tabs()[static_cast<std::size_t>(tabs_.active())].label);
        if (opened_ >= 0)
            path.push_back(context_.catalog[static_cast<std::size_t>(opened_)].title);
        crumbs_.set_path(path, animate);
    }
    void go(Region region, ui::Feedback &feedback, float x)
    {
        region_ = region;
        ui::play_cue(feedback, rail_.style, rail_.style.sounds.move, x);
    }
    void say(const char *event, const std::string &what)
    {
        status_ = std::string(event) + "  \xC2\xB7  " + what;
    }
    Rect card_on_screen(int card) const
    {
        const float width = content_width();
        Rect slot = card_slot(card, width);
        slot.x += content_left() - static_cast<float>(dots_.page()) * (width + kCardGap);
        slot.y += kStripY;
        return slot;
    }

    // ---- input, region by region ----
    void update_rail(const InputFrame &input, ui::Feedback &feedback)
    {
        if (input.nav == Direction::right)
        {
            go(Region::tabs, feedback, content_left());
            return;
        }
        if (input.nav == Direction::left)
        {
            ui::refuse(feedback, rail_.style, input, edge_, rail_.rect().cx());
            return;
        }
        const ui::Event event = rail_.handle(input, feedback);
        const ui::NavEntry &entry = rail_.entries()[static_cast<std::size_t>(rail_.focus())];
        if (event == ui::Event::activated && entry.action)
        {
            set_rail_expanded(!rail_.expanded());
            say("SideNav: Event::activated", rail_.expanded() ? "expanded" : "collapsed");
        }
        else if (event == ui::Event::activated)
        {
            opened_ = -1;
            swap_age_ = 0.0f;
            sync_path(true);
            say("SideNav: Event::activated", entry.label);
        }
        else if (event == ui::Event::moved)
            say("SideNav: Event::moved", entry.label);
        else if (event == ui::Event::refused)
            say("SideNav: Event::refused", entry.label);
        else if (event == ui::Event::cancelled)
            status_ = "SideNav: Event::cancelled";
    }

    void update_tabs(const InputFrame &input, ui::Feedback &feedback)
    {
        if ((input.nav == Direction::left && tabs_.active() == 0 && !input.nav_repeat) ||
            input.is_pressed(Action::back))
        {
            go(Region::rail, feedback, rail_.rect().cx());
            return;
        }
        if (input.nav == Direction::down || input.is_pressed(Action::confirm))
        {
            go(Region::shelf, feedback, content_left());
            return;
        }
        if (input.nav == Direction::up)
        {
            ui::refuse(feedback, tabs_.style, input, edge_, tabs_.bounds().cx());
            return;
        }
        const ui::Event event = tabs_.handle(input, feedback);
        const std::string &label = tabs_.tabs()[static_cast<std::size_t>(tabs_.active())].label;
        if (event == ui::Event::changed)
        {
            opened_ = -1;
            sync_path(true);
            say("TabBar: Event::changed", label);
        }
        else if (event == ui::Event::refused)
            say("TabBar: Event::refused", label);
    }

    void update_shelf(const InputFrame &input, ui::Feedback &feedback)
    {
        const float x = card_on_screen(card_).cx();
        const char *title = context_.catalog[static_cast<std::size_t>(card_)].title;
        if (input.nav == Direction::up)
        {
            go(Region::tabs, feedback, tabs_.bounds().cx());
        }
        else if (input.nav == Direction::left && card_ == 0 && !input.nav_repeat)
        {
            go(Region::rail, feedback, rail_.rect().cx());
        }
        else if (input.nav == Direction::left || input.nav == Direction::right)
        {
            const int next = card_ + (input.nav == Direction::right ? 1 : -1);
            if (next < 0 || next >= kCards)
            {
                ui::refuse(feedback, dots_.style, input, edge_, x);
                say("Shelf: refused", title);
                return;
            }
            card_ = next;
            // The dots only show the page: the shelf tells them which.
            dots_.set_page(card_ / kPerPage);
            ui::play_cue(feedback, dots_.style, dots_.style.sounds.move, card_on_screen(card_).cx(),
                         tween::lerp(0.96f, 1.06f,
                                     static_cast<float>(card_) / static_cast<float>(kCards - 1)));
            char text[48];
            std::snprintf(text, sizeof(text), "page %d of %d", dots_.page() + 1, kPages);
            say("PageDots: set_page", text);
        }
        else if (input.nav == Direction::down)
        {
            ui::refuse(feedback, dots_.style, input, edge_, x);
        }
        else if (input.is_pressed(Action::confirm))
        {
            open_card(feedback);
        }
        else if (input.is_pressed(Action::back))
        {
            ui::play_cue(feedback, dots_.style, dots_.style.sounds.cancel, x);
            if (opened_ >= 0)
            {
                opened_ = -1;
                sync_path(true);
                status_ = "Breadcrumb: pop";
            }
            else
            {
                region_ = Region::tabs;
            }
        }
    }

    void open_card(ui::Feedback &feedback)
    {
        opened_ = card_;
        sync_path(true);
        ui::play_cue(feedback, dots_.style, dots_.style.sounds.activate,
                     card_on_screen(card_).cx());
        say("Breadcrumb: push", context_.catalog[static_cast<std::size_t>(card_)].title);
    }

    void open_menu(ui::Feedback &feedback)
    {
        menu_.item(action_rail).checked = !rail_.expanded();
        if (region_ == Region::rail)
        {
            menu_.style.side = ui::MenuSide::right;
            menu_.open(rail_.row_rect(rail_.focus()), feedback);
        }
        else if (region_ == Region::tabs)
        {
            menu_.style.side = ui::MenuSide::below;
            menu_.open(tabs_.tab_rect(context_.fonts, tabs_.active()), feedback);
        }
        else
        {
            // Asked to open below, where there is no room: it flips.
            menu_.style.side = ui::MenuSide::automatic;
            menu_.open(card_on_screen(card_), feedback);
        }
        status_ = "Menu: open";
    }

    void update_menu(const InputFrame &input, ui::Feedback &feedback)
    {
        const ui::Event event = menu_.handle(input, feedback);
        const ui::MenuItem &item = menu_.items()[static_cast<std::size_t>(menu_.focus())];
        if (event == ui::Event::activated)
        {
            say("Menu: Event::activated", item.label);
            if (item.tag == action_open && region_ == Region::shelf)
            {
                opened_ = card_;
                sync_path(true);
            }
        }
        else if (event == ui::Event::changed)
        {
            say("Menu: Event::changed", item.label + (item.checked ? " on" : " off"));
            if (item.tag == action_rail)
                set_rail_expanded(!item.checked);
        }
        else if (event == ui::Event::refused)
            say("Menu: Event::refused", item.label);
        else if (event == ui::Event::cancelled)
            status_ = "Menu: Event::cancelled";
    }

    // ---- drawing ----
    static void draw_cover(gfx::DrawList &list, const demo::Item &item, const Rect &art,
                           float radius)
    {
        if (item.cover != 0)
            list.image(item.cover, art, gfx::kCanvasUv, Color::rgb(0xffffff), radius);
        else
            list.gradient_rect(art, radius, item.mid, item.dark);
    }

    // What a tab shows about the title the rail's current place stands for.
    void draw_content(ui::Canvas &canvas, ui::Painter &paint, int tab, const Rect &area) const
    {
        gfx::DrawList &list = canvas.list;
        const demo::Item &item = context_.catalog[static_cast<std::size_t>(rail_.current())];
        paint.panel(area);
        const float pad = 20.0f;
        const float art_size = area.h - 2.0f * pad;
        draw_cover(list, item, {area.x + pad, area.y + pad, art_size, art_size},
                   std::min(theme_.radius, 14.0f));
        const float x = area.x + pad + art_size + 28.0f;
        const float room = area.x + area.w - pad - x;
        const Color ink = theme_.text;
        const Color quiet = theme_.text_muted;
        paint.heading(item.title, x, area.y + 58.0f, 34.0f, ink);
        char line[96];

        switch (tab)
        {
        case 0:
        {
            std::snprintf(line, sizeof(line), "%s \xC2\xB7 %s \xC2\xB7 %d", item.genre, item.studio,
                          item.year);
            paint.body(ui::fit_body(paint, line, 22.0f, room), x, area.y + 94.0f, 22.0f, quiet);
            paint.body(ui::fit_body(paint, item.blurb, 23.0f, room), x, area.y + 136.0f, 23.0f,
                       ink);
            paint.progress({x, area.y + 166.0f, std::min(room, 520.0f), 16.0f}, item.progress);
            std::snprintf(line, sizeof(line), "%d %% of the story \xC2\xB7 %d h played",
                          static_cast<int>(item.progress * 100.0f + 0.5f), item.hours);
            paint.body(line, x, area.y + 210.0f, 20.0f, quiet);
            break;
        }
        case 1:
        {
            paint.body("Three saves on this console", x, area.y + 94.0f, 22.0f, quiet);
            constexpr const char *kSlots[] = {"Autosave", "Slot 1", "Slot 2"};
            constexpr const char *kWhen[] = {"12 min ago", "Yesterday", "Last week"};
            const float slot_w = std::min((room - 2.0f * 14.0f) / 3.0f, 250.0f);
            for (int i = 0; i < 3; ++i)
            {
                const Rect slot{x + static_cast<float>(i) * (slot_w + 14.0f), area.y + 116.0f,
                                slot_w, 92.0f};
                paint.well(slot, paint.control_radius(slot), theme_.surface_high);
                paint.label(ui::fit_label(paint, kSlots[i], 22.0f, slot_w - 32.0f), slot.x + 16.0f,
                            slot.y + 38.0f, 22.0f, ink);
                paint.body(ui::fit_body(paint, kWhen[i], 19.0f, slot_w - 32.0f), slot.x + 16.0f,
                           slot.y + 70.0f, 19.0f, quiet);
            }
            break;
        }
        case 2:
        {
            const int earned = static_cast<int>(item.progress * 40.0f);
            std::snprintf(line, sizeof(line), "%d of 40 trophies earned", earned);
            paint.body(line, x, area.y + 94.0f, 22.0f, quiet);
            constexpr const char *kGrades[] = {"Platinum", "Gold", "Silver", "Bronze"};
            const int counts[] = {earned >= 40 ? 1 : 0, earned / 10, earned / 4, earned / 2};
            const float tile_w = std::min((room - 3.0f * 14.0f) / 4.0f, 190.0f);
            for (int i = 0; i < 4; ++i)
            {
                const Rect tile{x + static_cast<float>(i) * (tile_w + 14.0f), area.y + 116.0f,
                                tile_w, 92.0f};
                paint.well(tile, paint.control_radius(tile), theme_.surface_high);
                std::snprintf(line, sizeof(line), "%d", counts[i]);
                paint.heading(line, tile.x + 16.0f, tile.y + 44.0f, 32.0f, ink);
                paint.body(ui::fit_body(paint, kGrades[i], 19.0f, tile_w - 28.0f), tile.x + 16.0f,
                           tile.y + 74.0f, 19.0f, quiet);
            }
            break;
        }
        case 3:
        {
            paint.body("Twelve add-ons, two installed", x, area.y + 94.0f, 22.0f, quiet);
            constexpr const char *kPacks[] = {"Soundtrack", "Night routes", "Photo mode"};
            for (int i = 0; i < 3; ++i)
            {
                const float y = area.y + 122.0f + static_cast<float>(i) * 32.0f;
                paint.body(kPacks[i], x, y + 8.0f, 22.0f, ink);
                paint.body(i < 2 ? "Installed" : "Get", x + std::min(room, 520.0f), y + 8.0f, 20.0f,
                           i < 2 ? quiet : theme_.text, gfx::Align::right);
                list.rounded_rect({x, y + 18.0f, std::min(room, 520.0f), 1.5f}, 0.0f,
                                  quiet.with_alpha(0.22f));
            }
            break;
        }
        default:
        {
            paint.body("Four captures this week", x, area.y + 94.0f, 22.0f, quiet);
            const float shot_w = std::min((room - 3.0f * 12.0f) / 4.0f, 180.0f);
            for (int i = 0; i < 4; ++i)
            {
                const demo::Item &other =
                    context_.catalog[static_cast<std::size_t>(rail_.current() + 3 + i)];
                const Rect shot{x + static_cast<float>(i) * (shot_w + 12.0f), area.y + 116.0f,
                                shot_w, 92.0f};
                list.gradient_rect(shot, std::min(theme_.radius, 10.0f), other.mid, other.dark);
                list.circle(shot.x + shot.w * 0.7f, shot.y + shot.h * 0.36f, shot.h * 0.16f,
                            other.accent.with_alpha(0.85f));
            }
            break;
        }
        }
    }

    void draw_card(ui::Canvas &canvas, ui::Painter &paint, int card, const Rect &slot) const
    {
        gfx::DrawList &list = canvas.list;
        const demo::Item &item = context_.catalog[static_cast<std::size_t>(card)];
        const Rect &body = slot;
        const float pad = 16.0f;
        const float art_size = body.h - 2.0f * pad;
        draw_cover(list, item, {body.x + pad, body.y + pad, art_size, art_size},
                   std::min(theme_.radius, 12.0f));
        const float x = body.x + pad + art_size + 20.0f;
        const float room = body.x + body.w - pad - x;
        paint.label(ui::fit_label(paint, item.title, 24.0f, room), x, body.y + 52.0f, 24.0f,
                    theme_.text);
        paint.body(ui::fit_body(paint, item.genre, 20.0f, room), x, body.y + 84.0f, 20.0f,
                   theme_.text_muted);
        char line[48];
        std::snprintf(line, sizeof(line), "%d h \xC2\xB7 %.1f", item.hours,
                      static_cast<double>(item.rating));
        paint.body(ui::fit_body(paint, line, 20.0f, room), x, body.y + 114.0f, 20.0f,
                   theme_.text_muted);
        if (card == opened_)
        {
            const float chip_w = std::min(paint.label_width("Open", 19.0f) + 32.0f, room);
            paint.chip({x, body.y + body.h - pad - 34.0f, chip_w, 34.0f}, "Open", 1.0f, {});
        }
    }

    app::Context &context_;
    ui::Theme theme_ = ui::default_theme();
    bool reduced_ = false;
    ui::SideNav rail_;
    ui::TabBar tabs_;
    ui::Breadcrumb crumbs_;
    ui::PageDots dots_;
    ui::Menu menu_;
    ui::Highlight card_ring_;
    ui::ComponentStyle card_style_;
    Region region_ = Region::rail;
    int card_ = 0;    // the focused card on the shelf
    int opened_ = -1; // the card whose title is the last segment of the path
    int variant_ = 0;
    float clock_ = 0.0f;
    float swap_age_ = 10.0f; // since the rail's current place changed
    float ring_width_ = 0.0f;
    tween::Spring shelf_focus_;
    ui::Pulse edge_;
    std::string status_;
};

} // namespace

std::unique_ptr<Page> make_navigation_page(app::Context &context)
{
    return std::make_unique<NavigationPage>(context);
}

} // namespace hui::concepts::gallery
