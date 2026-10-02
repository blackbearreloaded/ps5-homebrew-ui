// ps5-homebrew-ui - Component Library page: the components that organise and reach content.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Three columns. Left, a TreeView of a made-up library. In the middle, a long
// ListView of the catalogue's titles with a JumpBar beside it: the bar jumps
// the list to a letter and follows it when the list moves by itself. Right, a
// Wizard that drives itself through its button row and an Accordion with text
// bodies and one section drawn by the page through the content slot. Triangle
// opens a RadialMenu over everything (in the last variant it is a quick wheel:
// up while Triangle is held, releasing it chooses). The page only moves the
// focus between the five components, through the edges they hand it back on.

#include "concepts/components/page.hpp"

#include "ui/components/accordion.hpp"
#include "ui/components/jump_bar.hpp"
#include "ui/components/list.hpp"
#include "ui/components/radial.hpp"
#include "ui/components/tree.hpp"
#include "ui/components/wizard.hpp"

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

// ---- layout -----------------------------------------------------------------
constexpr float kCaptionY = 268.0f;
constexpr float kTop = 292.0f;
constexpr float kHeight = 612.0f;
constexpr float kTreeX = 96.0f;
constexpr float kTreeWidth = 430.0f;
constexpr float kListX = 566.0f;
constexpr float kListWidth = 470.0f;
constexpr float kRightX = 1076.0f;
constexpr float kRightWidth = 748.0f;
constexpr float kStripWidth = 48.0f; // the vertical jump bar beside the list
constexpr float kStripGap = 12.0f;
constexpr float kStatusY = 938.0f;
constexpr float kThumbRoom = 14.0f; // beside a component that shows a scroll thumb
constexpr float kPi = 3.14159265f;

enum Zone : int
{
    zone_tree,
    zone_list,
    zone_jump,
    zone_wizard,
    zone_accordion,
    zone_count,
};

// ---- variants: every component changes together -----------------------------
struct Variant
{
    const char *name;
    int wedges;
    ui::RadialLabels labels;
    ui::RadialMode mode;
    ui::WizardKind wizard;
    bool single;        // the accordion keeps one section open
    bool guides;        // the tree draws its indent guides
    bool jump_vertical; // the jump bar stands beside the list, or lies above it
    ui::HighlightKind highlight;
};
constexpr Variant kVariants[] = {
    {"Horizontal \xC2\xB7 single \xC2\xB7 menu wheel", 8, ui::RadialLabels::outside,
     ui::RadialMode::menu, ui::WizardKind::horizontal, true, true, true, ui::HighlightKind::tint},
    {"Vertical \xC2\xB7 multiple \xC2\xB7 icon wheel", 6, ui::RadialLabels::none,
     ui::RadialMode::menu, ui::WizardKind::vertical, false, false, false, ui::HighlightKind::bar},
    {"Compact \xC2\xB7 panels \xC2\xB7 hold for wheel", 4, ui::RadialLabels::inside,
     ui::RadialMode::quick, ui::WizardKind::compact, true, true, true, ui::HighlightKind::fill},
};
constexpr int kVariantCount = static_cast<int>(std::size(kVariants));

constexpr ui::Hint kHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Choose"},
    {ui::Button::triangle, "Wheel"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kWheelHints[] = {
    {ui::Button::left_stick, "Aim", ui::Button::dpad},
    {ui::Button::cross, "Choose"},
    {ui::Button::circle, "Close"},
};
constexpr ui::Hint kQuickHints[] = {
    {ui::Button::left_stick, "Aim", ui::Button::dpad},
    {ui::Button::triangle, "Release to choose"},
    {ui::Button::circle, "Cancel"},
};

constexpr std::uint32_t kConfirm = action_bit(Action::confirm);
constexpr std::uint32_t kBack = action_bit(Action::back);
constexpr std::uint32_t kWheel = action_bit(Action::north);
constexpr std::uint32_t kNext = action_bit(Action::west);
// A step's picture is taken after its wait and before its input, and the
// stick and the held buttons of a step are held while it waits.
constexpr app::TourStep kTour[] = {
    // The tree: down to Favourites, open it, into its first child. A leaf
    // has nothing to open, so right hands the focus to the list.
    {0.5f, 0, Direction::down},
    {0.25f, 0, Direction::down},
    {0.3f, 0, Direction::right},
    {0.4f, 0, Direction::right},
    {0.35f, 0, Direction::right},
    // The list, then the jump bar: two letters on, and the list follows.
    {0.3f, 0, Direction::down},
    {0.3f, 0, Direction::right},
    {0.3f, 0, Direction::down},
    {0.3f, 0, Direction::down},
    // The wizard's Next button, one step forward.
    {0.35f, 0, Direction::right},
    {0.3f, kConfirm},
    // The accordion: the second section opens and the first one closes.
    {0.5f, 0, Direction::down},
    {0.3f, 0, Direction::down},
    {0.3f, kConfirm},
    // The wheel: the stick aims, confirm chooses.
    {0.9f, kWheel, Direction::none, "structure"},
    {0.7f, 0, Direction::none, nullptr, 0.8f, -0.2f},
    {0.5f, kConfirm, Direction::none, "structure-radial", 0.8f, -0.2f},
    // Vertical steps, several sections open, a wheel of icons: the D-pad
    // turns the dial and back closes it.
    {0.5f, kNext},
    {0.7f, kWheel, Direction::none, "structure-vertical"},
    {0.5f, 0, Direction::right},
    {0.5f, kBack},
    // Compact steps, panels, and the quick wheel: Triangle goes down and
    // stays down while the stick aims; released, the pointed wedge is chosen.
    {0.5f, kNext},
    {0.7f, kWheel, Direction::none, "structure-compact"},
    {0.8f, 0, Direction::none, "structure-quick", -0.25f, 0.75f, kWheel},
    {0.5f, kNext}, // back to the first variant
};

// ---- icons: drawn from shapes, so they take any ink and any size ------------
enum Icon : int
{
    icon_play,
    icon_map,
    icon_camera,
    icon_people,
    icon_book,
    icon_disk,
    icon_gear,
    icon_door,
    icon_lantern,
    icon_rope,
    icon_compass,
    icon_flare,
    icon_folder,
    icon_page,
};

void draw_icon(gfx::DrawList &list, int icon, const Rect &box, Color ink)
{
    const float s = std::min(box.w, box.h);
    const float x = box.cx() - s * 0.5f;
    const float y = box.cy() - s * 0.5f;
    const float cx = box.cx();
    const float cy = box.cy();
    const float t = std::max(2.0f, s * 0.085f);
    const Color clear{ink.r, ink.g, ink.b, 0.0f};
    switch (icon)
    {
    case icon_play:
        list.triangle({x + s * 0.16f, y + s * 0.14f, s * 0.72f, s * 0.72f}, ink, 0.0f, kPi * 0.5f);
        break;
    case icon_map:
        list.bordered_rect({x + s * 0.06f, y + s * 0.16f, s * 0.88f, s * 0.68f}, s * 0.08f, clear,
                           t, ink);
        list.line(x + s * 0.36f, y + s * 0.24f, x + s * 0.36f, y + s * 0.76f, t * 0.7f, ink);
        list.line(x + s * 0.64f, y + s * 0.24f, x + s * 0.64f, y + s * 0.76f, t * 0.7f, ink);
        break;
    case icon_camera:
        list.rounded_rect({x + s * 0.32f, y + s * 0.14f, s * 0.36f, s * 0.16f}, s * 0.05f, ink);
        list.bordered_rect({x + s * 0.06f, y + s * 0.26f, s * 0.88f, s * 0.58f}, s * 0.1f, clear, t,
                           ink);
        list.ring(cx, y + s * 0.55f, s * 0.17f, t, ink);
        break;
    case icon_people:
        list.circle(cx, y + s * 0.3f, s * 0.19f, ink);
        list.rounded_rect({x + s * 0.14f, y + s * 0.56f, s * 0.72f, s * 0.36f}, s * 0.18f, ink);
        break;
    case icon_book:
        list.bordered_rect({x + s * 0.14f, y + s * 0.1f, s * 0.72f, s * 0.8f}, s * 0.08f, clear, t,
                           ink);
        list.line(x + s * 0.32f, y + s * 0.34f, x + s * 0.68f, y + s * 0.34f, t * 0.8f, ink);
        list.line(x + s * 0.32f, y + s * 0.52f, x + s * 0.68f, y + s * 0.52f, t * 0.8f, ink);
        break;
    case icon_disk:
        list.bordered_rect({x + s * 0.1f, y + s * 0.1f, s * 0.8f, s * 0.8f}, s * 0.1f, clear, t,
                           ink);
        list.rounded_rect({x + s * 0.3f, y + s * 0.1f, s * 0.4f, s * 0.24f}, s * 0.03f, ink);
        list.rounded_rect({x + s * 0.28f, y + s * 0.56f, s * 0.44f, s * 0.2f}, s * 0.04f, ink);
        break;
    case icon_gear:
        list.ring(cx, cy, s * 0.28f, t, ink);
        for (int i = 0; i < 6; ++i)
        {
            const float angle = static_cast<float>(i) * kPi / 3.0f;
            list.line(cx + std::sin(angle) * s * 0.3f, cy - std::cos(angle) * s * 0.3f,
                      cx + std::sin(angle) * s * 0.44f, cy - std::cos(angle) * s * 0.44f, t, ink);
        }
        break;
    case icon_door:
        list.bordered_rect({x + s * 0.1f, y + s * 0.1f, s * 0.5f, s * 0.8f}, s * 0.06f, clear, t,
                           ink);
        list.line(x + s * 0.44f, cy, x + s * 0.92f, cy, t, ink);
        list.line(x + s * 0.76f, cy - s * 0.16f, x + s * 0.92f, cy, t, ink);
        list.line(x + s * 0.76f, cy + s * 0.16f, x + s * 0.92f, cy, t, ink);
        break;
    case icon_lantern:
        list.arc(cx, y + s * 0.26f, s * 0.2f, t, -kPi * 0.5f, kPi, ink);
        list.bordered_rect({x + s * 0.3f, y + s * 0.3f, s * 0.4f, s * 0.46f}, s * 0.08f, clear, t,
                           ink);
        list.circle(cx, y + s * 0.54f, s * 0.08f, ink);
        list.rounded_rect({x + s * 0.24f, y + s * 0.8f, s * 0.52f, s * 0.1f}, s * 0.03f, ink);
        break;
    case icon_rope:
        for (int turn = 0; turn < 3; ++turn)
            list.bordered_rect({x + s * 0.14f, y + s * (0.12f + 0.22f * static_cast<float>(turn)),
                                s * 0.72f, s * 0.28f},
                               s * 0.14f, clear, t, ink);
        break;
    case icon_compass:
        list.ring(cx, cy, s * 0.42f, t, ink);
        list.line(cx, cy, cx + s * 0.16f, cy - s * 0.2f, t * 1.4f, ink);
        list.line(cx, cy, cx - s * 0.16f, cy + s * 0.2f, t * 1.4f, ink.with_alpha(0.55f));
        break;
    case icon_flare:
        list.line(x + s * 0.14f, y + s * 0.86f, x + s * 0.5f, y + s * 0.5f, t * 1.3f, ink);
        list.star(x + s * 0.66f, y + s * 0.34f, s * 0.28f, ink);
        break;
    case icon_folder:
        list.rounded_rect({x + s * 0.06f, y + s * 0.18f, s * 0.4f, s * 0.2f}, s * 0.06f, ink);
        list.rounded_rect({x + s * 0.06f, y + s * 0.28f, s * 0.88f, s * 0.54f}, s * 0.08f, ink);
        break;
    default:
        list.bordered_rect({x + s * 0.2f, y + s * 0.1f, s * 0.6f, s * 0.8f}, s * 0.08f, clear, t,
                           ink);
        list.line(x + s * 0.36f, y + s * 0.4f, x + s * 0.64f, y + s * 0.4f, t * 0.8f, ink);
        list.line(x + s * 0.36f, y + s * 0.58f, x + s * 0.64f, y + s * 0.58f, t * 0.8f, ink);
        break;
    }
}

ui::RadialItem wedge(const char *label, const char *description, int icon, const char *value = "",
                     bool disabled = false)
{
    ui::RadialItem item;
    item.label = label;
    item.description = description;
    item.value = value;
    item.disabled = disabled;
    item.tag = icon;
    return item;
}

ui::TreeNode node(const char *label, const char *value = "",
                  std::vector<ui::TreeNode> children = {})
{
    ui::TreeNode out;
    out.label = label;
    out.value = value;
    out.children = std::move(children);
    return out;
}

class StructurePage final : public Page
{
  public:
    explicit StructurePage(app::Context &context) : context_(context)
    {
        build_tree();
        build_list();
        wizard_.set_steps({{"Profile", "Name"},
                           {"Display", "4K HDR"},
                           {"Sound", "Optional"},
                           {"Network", "No signal", true},
                           {"Finish", ""}});
        wizard_.set_step(1, true);
        accordion_.set_sections({
            {"Saving", "3 slots",
             "The game saves at every lantern you light. Each journey keeps three slots, and "
             "the newest autosave is never overwritten by hand."},
            {"Party", "3 friends", "", 132.0f},
            {"Controls", "",
             "Every action can be moved to another button. Holding Options for two seconds "
             "brings back the layout the game shipped with."},
            {"Online play", "Sign in", "Sign in to see who is playing.", 0.0f, true},
            {"Accessibility", "",
             "Reduce motion, larger text and a high-contrast theme are in Options. They "
             "apply to every screen at once."},
        });
        accordion_.content =
            [this](ui::Canvas &canvas, const Rect &area, const ui::AccordionSection &, int, float)
        { draw_party(canvas, area); };
        accordion_.set_open(0, true, true);
        radial_.icon = [](ui::Canvas &canvas, const Rect &box, const ui::RadialItem &item, int,
                          float, Color ink) { draw_icon(canvas.list, item.tag, box, ink); };
        tree_.icon = [this](ui::Canvas &canvas, const Rect &box, const ui::TreeNode &, int index,
                            float, Color ink)
        {
            const float size = 26.0f;
            draw_icon(canvas.list, tree_.child_count(index) > 0 ? icon_folder : icon_page,
                      {box.x, box.cy() - size * 0.5f, size, size}, ink);
        };
        restyle(ui::default_theme(), false);
    }

    const char *title() const override
    {
        return "Structure";
    }
    const char *summary() const override
    {
        return "TreeView, JumpBar, Wizard, Accordion, RadialMenu";
    }
    const char *variant() const override
    {
        return kVariants[variant_].name;
    }

    void restyle(const ui::Theme &theme, bool reduced_motion) override
    {
        theme_ = theme;
        reduced_ = reduced_motion;
        apply_variant();
    }

    void enter() override
    {
        tree_.enter();
        list_.enter();
        accordion_.enter();
        radial_.dismiss();
        status_.clear();
    }

    void update(const InputFrame &input, float dt, ui::Feedback &feedback) override
    {
        if (radial_.is_open())
            update_wheel(input, feedback);
        else if (input.is_pressed(Action::north))
        {
            radial_.open(feedback);
            say("RadialMenu", "open()");
        }
        else if (input.is_pressed(Action::west))
        {
            variant_ = (variant_ + 1) % kVariantCount;
            apply_variant();
            ui::play_cue(feedback, style_, style_.sounds.change);
        }
        else
            update_zone(input, feedback);

        tree_.update(dt);
        list_.update(dt);
        jump_.update(dt);
        wizard_.update(dt);
        accordion_.update(dt);
        radial_.update(dt);
    }

    void draw(ui::Canvas &canvas) const override
    {
        ui::Painter paint(canvas.list, canvas.fonts, theme_, canvas.glass);
        const auto caption = [&](const char *text, float x, float y, bool active)
        { paint.label(text, x, y, 20.0f, active ? paint.page_text() : paint.page_text_muted()); };
        caption("TreeView", kTreeX, kCaptionY, zone_ == zone_tree);
        caption("ListView driven by a JumpBar", kListX, kCaptionY,
                zone_ == zone_list || zone_ == zone_jump);
        caption("Wizard", wizard_.bounds().x, kCaptionY, zone_ == zone_wizard);
        caption("Accordion", accordion_.bounds().x, accordion_.bounds().y - 24.0f,
                zone_ == zone_accordion);

        tree_.draw(canvas);
        list_.draw(canvas);
        jump_.draw(canvas); // after the list: its bubble floats over the rows
        wizard_.draw(canvas);
        accordion_.draw(canvas);
        if (!status_.empty())
            paint.body(ui::fit_body(paint, status_, 22.0f, kPageArea.w), kPageArea.x, kStatusY,
                       22.0f, paint.page_text_muted());
    }

    void draw_modal(ui::Canvas &canvas) const override
    {
        radial_.draw(canvas);
    }

    std::span<const ui::Hint> hints() const override
    {
        if (!radial_.is_open())
            return kHints;
        if (kVariants[variant_].mode == ui::RadialMode::quick)
            return kQuickHints;
        return kWheelHints;
    }
    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    // ---- content ------------------------------------------------------------
    void build_tree()
    {
        ui::TreeNode library = node(
            "Library", "",
            {node("Recently played", "12"),
             node("Favourites", "",
                  {node("Tidewater", "63 h"), node("Glass Orchard", "12 h"),
                   node("Paper Kites", "23 h")}),
             node("Collections", "",
                  {node("Cosy evenings", "8"), node("Couch co-op", "5"), node("Finished", "14")})});
        library.expanded = true;
        ui::TreeNode saves =
            node("Saves", "",
                 {node("Tidewater", "", {node("Slot 1", "63 h"), node("Slot 2", "4 h")}),
                  node("Ember and Ash", "", {node("Autosave", "96 h")})});
        ui::TreeNode captures =
            node("Captures", "", {node("Screenshots", "48"), node("Clips", "7")});
        ui::TreeNode add_ons = node("Add-ons");
        add_ons.badge = "NEW";
        ui::TreeNode network = node("Network play", "Offline");
        network.disabled = true;
        tree_.set_nodes({library, saves, captures, add_ons, network});
    }

    // The catalogue's titles, three editions of each so the list is long,
    // sorted; the jump bar's letters come from what the list holds.
    void build_list()
    {
        constexpr const char *kEditions[] = {"", " II", " Remastered"};
        std::vector<ui::ListItem> items;
        for (std::size_t i = 0; i < context_.catalog.size(); ++i)
        {
            const demo::Item &title = context_.catalog[i];
            for (const char *edition : kEditions)
            {
                ui::ListItem item;
                item.title = std::string(title.title) + edition;
                item.subtitle = title.genre;
                items.push_back(item);
            }
        }
        std::sort(items.begin(), items.end(),
                  [](const ui::ListItem &a, const ui::ListItem &b) { return a.title < b.title; });
        list_.set_items(std::move(items));

        std::vector<ui::JumpEntry> letters = ui::JumpBar::alphabet();
        for (ui::JumpEntry &entry : letters)
            entry.enabled = first_row(entry.label) >= 0;
        jump_.set_entries(std::move(letters));
        follow_list(true);
    }

    // The first row whose title starts with a letter, or -1.
    int first_row(const std::string &letter) const
    {
        const std::vector<ui::ListItem> &items = list_.items();
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            if (!letter.empty() && !items[i].title.empty() && items[i].title[0] == letter[0])
                return static_cast<int>(i);
        }
        return -1;
    }

    // The list moved by itself: the bar shows the letter it is on.
    void follow_list(bool snap)
    {
        if (list_.items().empty())
            return;
        const std::string &title = list_.items()[static_cast<std::size_t>(list_.focus())].title;
        jump_.set_current(jump_.find(title.substr(0, 1)), snap);
    }

    // ---- style --------------------------------------------------------------
    // Every variant starts from the components' defaults, so what it does not
    // name is the default. State (focus, what is open, the step) is left alone.
    void apply_variant()
    {
        const Variant &now = kVariants[variant_];
        const auto themed = [this](ui::ComponentStyle &style)
        {
            style.theme = theme_;
            style.reduced_motion = reduced_;
        };
        themed(style_);

        // ---- the tree ----
        ui::TreeStyle tree;
        themed(tree);
        tree.icon_width = 26.0f;
        tree.guides = now.guides;
        tree.highlight.kind = now.highlight;
        tree.panel = variant_ != 0;
        tree.exits.right = true;
        tree_.style = tree;
        tree_.set_bounds({kTreeX, kTop, kTreeWidth, kHeight});

        // ---- the list and its index ----
        ui::ListStyle rows;
        themed(rows);
        rows.row_height = 68.0f;
        rows.gap = 4.0f;
        rows.title_size = 24.0f;
        rows.subtitle_size = 19.0f;
        rows.padding = 18.0f;
        rows.scroll_thumb = false; // the jump bar is its scroll indicator
        rows.highlight.kind = now.highlight;
        rows.entrance_step = 0.02f;
        list_.style = rows;
        list_.set_items(list_.items());

        ui::JumpBarStyle index;
        themed(index);
        index.vertical = now.jump_vertical;
        index.wrap = variant_ == 2;
        if (now.jump_vertical)
        {
            index.thickness = kStripWidth - 8.0f;
            index.text_size = 17.0f;
            index.exits.left = true;
            index.exits.right = true;
            const float width = kListWidth - kStripWidth - kStripGap;
            list_.set_bounds({kListX, kTop, width, kHeight});
            jump_.set_bounds({kListX + width + kStripGap, kTop, kStripWidth, kHeight});
        }
        else
        {
            index.item_size = 18.0f;
            index.text_size = 16.0f;
            index.magnify = 1.2f;
            index.bubble = ui::JumpBubble::after;
            index.exits.down = true;
            index.exits.left = true;
            index.exits.right = true;
            jump_.set_bounds({kListX, kTop, kListWidth, 44.0f});
            list_.set_bounds({kListX, kTop + 60.0f, kListWidth, kHeight - 60.0f});
        }
        index.track = variant_ != 2;
        index.on_page = true;
        jump_.style = index;

        // ---- the wizard ----
        ui::WizardStyle steps;
        themed(steps);
        steps.kind = now.wizard;
        steps.buttons = true;
        steps.exits.left = true;
        // The accordion keeps room at its right for its scroll thumb, so
        // the thumb stays inside the safe area.
        const float folds_width = kRightWidth - kThumbRoom;
        Rect wizard{kRightX, kTop, kRightWidth, 196.0f};
        Rect accordion{kRightX, kTop + 256.0f, folds_width, kHeight - 256.0f};
        if (now.wizard == ui::WizardKind::vertical)
        {
            steps.button_width = 136.0f;
            steps.exits.right = true;
            wizard = {kRightX, kTop, 290.0f, kHeight};
            accordion = {kRightX + 330.0f, kTop, folds_width - 330.0f, kHeight};
        }
        else
        {
            steps.exits.down = true;
            if (now.wizard == ui::WizardKind::compact)
            {
                steps.button_width = 150.0f;
                wizard = {kRightX, kTop, kRightWidth, 72.0f};
                accordion = {kRightX, kTop + 132.0f, kRightWidth, kHeight - 132.0f};
            }
        }
        wizard_.style = steps;
        wizard_.set_bounds(wizard);

        // ---- the accordion ----
        ui::AccordionStyle folds;
        themed(folds);
        folds.single = now.single;
        folds.highlight.kind = now.highlight;
        folds.exits.left = true;
        folds.exits.up = now.wizard != ui::WizardKind::vertical;
        folds.header_height = 62.0f;
        folds.gap = 8.0f;
        folds.title_size = 24.0f;
        folds.body_size = 21.0f;
        if (variant_ == 1)
        {
            folds.cards = false;
            folds.dividers = true;
            folds.chevron = ui::AccordionChevron::leading;
            folds.padding = 16.0f;
        }
        else if (variant_ == 2)
        {
            folds.cards = false;
            folds.panel = true;
            folds.dividers = true;
        }
        accordion_.style = folds;
        accordion_.set_bounds(accordion);
        accordion_.measure(context_.fonts);

        // ---- the wheel ----
        ui::RadialStyle wheel;
        themed(wheel);
        wheel.mode = now.mode;
        wheel.labels = now.labels;
        std::vector<ui::RadialItem> items;
        if (now.mode == ui::RadialMode::quick)
        {
            // A weapon wheel: few, large wedges that hold their own labels.
            wheel.radius = 290.0f;
            wheel.thickness = 150.0f;
            wheel.hub_radius = 118.0f;
            wheel.gap = 12.0f;
            wheel.title_size = 26.0f;
            wheel.description_size = 18.0f;
            items = {wedge("Lantern", "Lights the path", icon_lantern),
                     wedge("Rope", "Down any ledge", icon_rope, "3 left"),
                     wedge("Compass", "Points to camp", icon_compass),
                     wedge("Flare", "Calls friends", icon_flare, "None left", true)};
        }
        else
        {
            items = {wedge("Resume", "Back to Lantern Pass", icon_play),
                     wedge("Map", "The charted valley", icon_map),
                     wedge("Camera", "Photo mode", icon_camera, "12 shots"),
                     wedge("Party", "Invite friends", icon_people, "Sign in to use", true),
                     wedge("Journal", "Notes and sketches", icon_book),
                     wedge("Saves", "Slot 2, chapter 8", icon_disk),
                     wedge("Options", "Sound and display", icon_gear),
                     wedge("Quit", "To the title screen", icon_door)};
            items.back().accent = theme_.danger;
            if (now.wedges < static_cast<int>(items.size()))
                items.erase(items.begin() + now.wedges, items.end());
        }
        const int pointed = radial_.focus();
        radial_.style = wheel;
        if (static_cast<int>(radial_.items().size()) != now.wedges)
        {
            radial_.set_items(std::move(items));
        }
        else
        {
            // Only the theme changed: keep what is pointed at, refresh colours.
            for (int i = 0; i < now.wedges; ++i)
                radial_.item(i).accent = items[static_cast<std::size_t>(i)].accent;
            radial_.set_focus(pointed);
        }
        radial_.set_bounds({0.0f, 0.0f, gfx::kVirtualWidth, gfx::kVirtualHeight});

        set_zone(zone_);
    }

    void set_zone(int zone)
    {
        zone_ = zone;
        tree_.set_active(zone_ == zone_tree);
        list_.set_active(zone_ == zone_list);
        jump_.set_focused(zone_ == zone_jump);
        wizard_.set_focused(zone_ == zone_wizard);
        accordion_.set_active(zone_ == zone_accordion);
    }

    float zone_x(int zone) const
    {
        switch (zone)
        {
        case zone_tree:
            return tree_.bounds().cx();
        case zone_list:
            return list_.bounds().cx();
        case zone_jump:
            return jump_.bounds().cx();
        case zone_wizard:
            return wizard_.bounds().cx();
        default:
            return accordion_.bounds().cx();
        }
    }

    void go(int zone, ui::Feedback &feedback)
    {
        set_zone(zone);
        ui::play_cue(feedback, style_, style_.sounds.move, zone_x(zone));
    }

    void say(const char *who, const char *what, const std::string &detail = {})
    {
        status_ = std::string(who) + "  \xC2\xB7  " + what;
        if (!detail.empty())
            status_ += "  \xC2\xB7  " + detail;
    }

    static const char *name(ui::Event event)
    {
        switch (event)
        {
        case ui::Event::moved:
            return "Event::moved";
        case ui::Event::changed:
            return "Event::changed";
        case ui::Event::activated:
            return "Event::activated";
        case ui::Event::cancelled:
            return "Event::cancelled";
        case ui::Event::refused:
            return "Event::refused";
        default:
            return "";
        }
    }

    // ---- input --------------------------------------------------------------
    void update_wheel(const InputFrame &input, ui::Feedback &feedback)
    {
        // The quick wheel lives as long as Triangle is down.
        if (kVariants[variant_].mode == ui::RadialMode::quick)
            radial_.set_held(input.is_held(Action::north));
        const ui::Event event = radial_.handle(input, feedback);
        if (event != ui::Event::none)
            say("RadialMenu", name(event),
                radial_.items()[static_cast<std::size_t>(radial_.focus())].label);
    }

    void update_zone(const InputFrame &input, ui::Feedback &feedback)
    {
        const Variant &now = kVariants[variant_];
        const bool upright = now.wizard == ui::WizardKind::vertical;
        switch (zone_)
        {
        case zone_tree:
        {
            const ui::Event event = tree_.handle(input, feedback);
            if (event != ui::Event::none)
                say("TreeView", name(event), tree_.node(tree_.focus()).label);
            else if (tree_.exit() == Direction::right)
                go(now.jump_vertical ? zone_list : zone_jump, feedback);
            break;
        }
        case zone_list:
        {
            if (input.nav == Direction::left)
                go(zone_tree, feedback);
            else if (input.nav == Direction::right)
                go(now.jump_vertical ? zone_jump : zone_wizard, feedback);
            else if (input.nav == Direction::up && !now.jump_vertical && list_.focus() == 0)
                go(zone_jump, feedback);
            else
            {
                const ui::Event event = list_.handle(input, feedback);
                if (event == ui::Event::moved)
                    follow_list(false);
                if (event != ui::Event::none)
                    say("ListView", name(event),
                        list_.items()[static_cast<std::size_t>(list_.focus())].title);
            }
            break;
        }
        case zone_jump:
        {
            const ui::Event event = jump_.handle(input, feedback);
            if (event == ui::Event::changed)
            {
                // The bar only reports the letter: scrolling there is ours.
                const int row = first_row(jump_.label());
                if (row >= 0)
                    list_.set_focus(row, false);
            }
            if (event != ui::Event::none)
                say("JumpBar", name(event), "label() = \"" + jump_.label() + "\"");
            else if (jump_.exit() == Direction::left)
                go(now.jump_vertical ? zone_list : zone_tree, feedback);
            else if (jump_.exit() == Direction::right)
                go(zone_wizard, feedback);
            else if (jump_.exit() == Direction::down)
                go(zone_list, feedback);
            break;
        }
        case zone_wizard:
        {
            const ui::Event event = wizard_.handle(input, feedback);
            if (event == ui::Event::changed || event == ui::Event::activated)
            {
                // Passing the step that reported a problem resolves it.
                wizard_.set_error(3, wizard_.step() <= 3);
                wizard_.step_at(3).caption = wizard_.step() <= 3 ? "No signal" : "Wi-Fi";
            }
            if (event != ui::Event::none)
            {
                char text[48];
                std::snprintf(text, sizeof(text), "step() = %d of %d", wizard_.step(),
                              wizard_.count());
                say("Wizard", name(event), text);
            }
            else if (wizard_.exit() == Direction::left)
                go(now.jump_vertical ? zone_jump : zone_list, feedback);
            else if (wizard_.exit() == Direction::down || wizard_.exit() == Direction::right)
                go(zone_accordion, feedback);
            break;
        }
        default:
        {
            const ui::Event event = accordion_.handle(input, feedback);
            if (event != ui::Event::none)
                say("Accordion", name(event),
                    accordion_.sections()[static_cast<std::size_t>(accordion_.focus())].title);
            else if (accordion_.exit() == Direction::up)
                go(zone_wizard, feedback);
            else if (accordion_.exit() == Direction::left)
                go(upright ? zone_wizard : (now.jump_vertical ? zone_jump : zone_list), feedback);
            break;
        }
        }
    }

    // ---- the accordion's slot: three friends, drawn by the page -------------
    void draw_party(ui::Canvas &canvas, const Rect &area) const
    {
        ui::Painter paint(canvas.list, canvas.fonts, theme_, canvas.glass);
        constexpr const char *kNames[] = {"Mara", "Oskar", "Lin"};
        constexpr const char *kDoing[] = {"In Tidewater", "In the store", "Away"};
        const float gap = 16.0f;
        const float width = (area.w - 2.0f * gap) / 3.0f;
        // Beside the picture when there is room, under it in a narrow column.
        const bool stacked = width < 170.0f;
        const float art = std::min(area.h - (stacked ? 30.0f : 0.0f), 64.0f);
        const bool on_surface = accordion_.style.cards || accordion_.style.panel;
        const Color strong = on_surface ? theme_.text : paint.page_text();
        const Color quiet = on_surface ? theme_.text_muted : paint.page_text_muted();
        for (int i = 0; i < 3; ++i)
        {
            const float x = area.x + static_cast<float>(i) * (width + gap);
            const Rect cover{x, area.y + 4.0f, art, art};
            const demo::Item &title = context_.catalog[static_cast<std::size_t>(i + 1)];
            const float radius = std::min(theme_.radius, art * 0.5f);
            if (title.cover != 0)
                canvas.list.image(title.cover, cover, gfx::kCanvasUv, Color::rgb(0xffffff), radius);
            else
                canvas.list.gradient_rect(cover, radius, title.mid, title.dark);
            if (stacked)
            {
                paint.label(ui::fit_label(paint, kNames[i], 20.0f, width), x, cover.y + art + 26.0f,
                            20.0f, strong);
                continue;
            }
            const float text = x + art + 14.0f;
            const float room = std::max(width - art - 14.0f, 40.0f);
            paint.label(ui::fit_label(paint, kNames[i], 22.0f, room), text, cover.y + 28.0f, 22.0f,
                        strong);
            paint.body(ui::fit_body(paint, kDoing[i], 18.0f, room), text, cover.y + 54.0f, 18.0f,
                       quiet);
        }
    }

    app::Context &context_;
    ui::Theme theme_ = ui::default_theme();
    bool reduced_ = false;
    ui::ComponentStyle style_; // the page's own cues
    ui::TreeView tree_;
    ui::ListView list_;
    ui::JumpBar jump_;
    ui::Wizard wizard_;
    ui::Accordion accordion_;
    ui::RadialMenu radial_;
    int zone_ = zone_tree;
    int variant_ = 0;
    std::string status_;
};

} // namespace

std::unique_ptr<Page> make_structure_page(app::Context &context)
{
    return std::make_unique<StructurePage>(context);
}

} // namespace hui::concepts::gallery
