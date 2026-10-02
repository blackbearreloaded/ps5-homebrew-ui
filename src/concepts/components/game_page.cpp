// ps5-homebrew-ui - Component Library page: the pieces only a game needs.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A small "game" runs in the page's main area: a calm procedural landscape
// with a live HUD over it (ui::HealthBar, ui::AmmoCounter,
// ui::ObjectiveTracker, ui::MinimapFrame), driven by a timeline that takes
// damage, heals, fires, reloads and completes objectives. Above it sits a row
// of triggers: the ui::PauseMenu, a ui::UnlockPopup of each tier, the
// ui::ProfilePicker, and two panels that swap in over the game, a
// ui::InventoryGrid and a ui::NodeMap. Pausing stops the game's clock, as it
// would in a game. Square changes the knobs of all of them together.

#include "concepts/components/page.hpp"

#include "ui/components/hud.hpp"
#include "ui/components/inventory_grid.hpp"
#include "ui/components/node_map.hpp"
#include "ui/components/overlay.hpp"
#include "ui/components/pause_menu.hpp"
#include "ui/components/profile_picker.hpp"
#include "ui/components/unlock_popup.hpp"

#include <algorithm>
#include <array>
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

constexpr int kTriggers = 8;
constexpr float kTriggerY = 246.0f;
constexpr float kTriggerHeight = 58.0f;
constexpr float kTriggerGap = 14.0f;
constexpr float kCaptionY = 342.0f;
// The game's picture, and the room the two panels swap into.
constexpr Rect kMain{96.0f, 360.0f, 1728.0f, 592.0f};
constexpr float kHudInset = 36.0f;

enum Trigger : int
{
    trigger_bronze,
    trigger_silver,
    trigger_gold,
    trigger_special,
    trigger_pause,
    trigger_profiles,
    trigger_inventory,
    trigger_nodes,
};

constexpr const char *kTriggerLabels[kTriggers] = {
    "Bronze", "Silver", "Gold", "Special", "Pause", "Profiles", "Inventory", "Skill tree",
};

enum class Focus : std::uint8_t
{
    triggers,
    inventory,
    nodes,
};

constexpr const char *kVariants[] = {
    "Segmented, round map, left",
    "Plates, square map, centre",
    "Bare, compact, right",
};
constexpr int kVariantCount = static_cast<int>(std::size(kVariants));

constexpr ui::Hint kHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Open"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kPauseHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Choose"},
    {ui::Button::circle, "Resume"},
};
constexpr ui::Hint kProfileHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Play"},
    {ui::Button::circle, "Close"},
};
constexpr ui::Hint kBagHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Lift"},
    {ui::Button::triangle, "Filter"},
    {ui::Button::circle, "Back"},
};
constexpr ui::Hint kCarryHints[] = {
    {ui::Button::dpad, "Carry"},
    {ui::Button::cross, "Set down"},
    {ui::Button::circle, "Put back"},
};
constexpr ui::Hint kTreeHints[] = {
    {ui::Button::dpad, "Move"},
    {ui::Button::cross, "Learn"},
    {ui::Button::circle, "Back"},
};

constexpr std::uint32_t kConfirm = action_bit(Action::confirm);
constexpr std::uint32_t kBack = action_bit(Action::back);
constexpr std::uint32_t kWest = action_bit(Action::west);

constexpr app::TourStep kTour[] = {
    {0.45f, 0, Direction::right},
    {0.2f, 0, Direction::right},
    {0.25f, kConfirm}, // a gold unlock, while the HUD takes its first hit
    {0.8f, 0, Direction::right, "game"},
    {0.3f, 0, Direction::right},
    {0.3f, kConfirm}, // pause: the game's clock stops
    {1.5f, kBack, Direction::none, "game-pause"},
    {0.5f, 0, Direction::right},
    {0.3f, kConfirm}, // who is playing?
    {0.7f, 0, Direction::right},
    {0.9f, kBack, Direction::none, "game-profiles"},
    {0.5f, 0, Direction::right},
    {0.3f, kConfirm}, // into the bag
    {0.8f, kConfirm}, // lift the first item
    {0.3f, 0, Direction::right},
    {0.5f, kConfirm, Direction::none, "game-inventory"}, // carried; then merged
    {0.5f, kBack},
    {0.4f, 0, Direction::right},
    {0.3f, kConfirm}, // into the skill tree
    {0.7f, 0, Direction::right},
    {0.5f, kConfirm}, // learn it
    {0.9f, kBack, Direction::none, "game-nodes"},
    {0.5f, kWest},
    {1.2f, kWest},
    {0.6f, kWest},
};

// ---- content ----------------------------------------------------------------

struct Goal
{
    const char *text;
    const char *distance;
};
constexpr int kGoalSets = 3;
constexpr int kGoalLines = 3;
constexpr const char *kGoalTitles[kGoalSets] = {
    "The Lantern Pass",
    "Signal in the Fog",
    "Hollow Reach",
};
constexpr Goal kGoals[kGoalSets][kGoalLines] = {
    {{"Reach the signal tower", "240 m"}, {"Light the beacon", ""}, {"Hold until dawn", "2:00"}},
    {{"Find the ferryman", "80 m"},
     {"Cross the black water", ""},
     {"Open the sluice gate", "410 m"}},
    {{"Follow the ridge road", "1.2 km"},
     {"Disarm three snares", "0 / 3"},
     {"Meet the warden", ""}},
};

constexpr float kHits[] = {24.0f, 31.0f, 22.0f, 17.0f};

struct UnlockSample
{
    const char *title;
    const char *subtitle;
    int points;
};
constexpr UnlockSample kUnlocks[ui::kUnlockTiers] = {
    {"First steps", "Leave the valley", 10},
    {"Pathfinder", "Find five hidden shrines", 25},
    {"First light", "Reach the summit before dawn", 50},
    {"Nothing left unseen", "Earn every other medal", 100},
};

struct Player
{
    const char *name;
    const char *detail;
    const char *extra;
    std::uint32_t accent;
    int controller;
};
constexpr Player kPlayers[] = {
    {"Mara Voss", "Played today", "Level 12, 63 hours", 0x5aa9ff, 1},
    {"Tomas Reyk", "Played yesterday", "Level 7, 21 hours", 0xff8a5a, 2},
    {"Ines Calder", "Played last week", "Level 19, 140 hours", 0x7bd88f, 0},
    {"Guest", "Nothing is saved", "Starts a new journey", 0xc79bff, 0},
};

enum Category : int
{
    gear = 1,
    supplies = 2,
    keys = 3,
};
constexpr const char *kFilterNames[] = {"All", "Gear", "Supplies", "Keys"};
constexpr int kFilters = 4;

// Rarity colours are content, like the metals of a medal: a rare item is
// blue in every theme.
constexpr std::uint32_t kUncommon = 0x4fbf6b;
constexpr std::uint32_t kRare = 0x4a90e2;
constexpr std::uint32_t kEpic = 0xa55eea;
constexpr std::uint32_t kLegend = 0xf0a030;

enum Glyph : int
{
    bottle,
    blade,
    lantern,
    drop,
    key,
    chart,
    cloak,
    bolt,
    horn,
};

struct Loot
{
    const char *name;
    const char *about;
    int count;
    int max_stack;
    int kind;
    int category;
    std::uint32_t rarity; // 0: common
    int glyph;            // which picture stands for it (see draw_loot)
};
constexpr Loot kLoot[] = {
    {"Tonic", "Restores a little health. Stacks to nine.", 3, 9, 1, supplies, 0, bottle},
    {"Tonic", "Restores a little health. Stacks to nine.", 4, 9, 1, supplies, 0, bottle},
    {"Warden blade", "A patient weapon, heavier than it looks.", 1, 1, 0, gear, kRare, blade},
    {"Lantern", "Burns for an hour on one measure of oil.", 1, 1, 0, gear, kUncommon, lantern},
    {"Oil", "One measure. Stacks to five.", 2, 5, 2, supplies, 0, drop},
    {"Oil", "One measure. Stacks to five.", 4, 5, 2, supplies, 0, drop},
    {"Sluice key", "Opens the gate under the mill.", 1, 1, 0, keys, kLegend, key},
    {"Ridge map", "The road north, and what watches it.", 1, 1, 0, keys, kUncommon, chart},
    {"Storm cloak", "Sheds rain and, some say, arrows.", 1, 1, 0, gear, kEpic, cloak},
    {"Bolt", "Crossbow bolts. Stacks to forty.", 26, 40, 3, supplies, 0, bolt},
    {"Bolt", "Crossbow bolts. Stacks to forty.", 9, 40, 3, supplies, 0, bolt},
    {"Signal horn", "Carries across the whole pass.", 1, 1, 0, gear, kRare, horn},
};
constexpr int kLootCount = static_cast<int>(std::size(kLoot));

struct Skill
{
    float x, y;
    const char *label;
};
// Wider than the panel on purpose: the camera has to follow the focus.
constexpr Skill kSkills[] = {
    {0, 220, "Core"},         {230, 100, "Dash"},      {230, 340, "Guard"},
    {470, 40, "Leap"},        {470, 190, "Focus"},     {470, 400, "Parry"},
    {720, 100, "Vault"},      {720, 300, "Riposte"},   {960, 40, "Glide"},
    {960, 220, "Surge"},      {960, 400, "Bulwark"},   {1210, 130, "Tempest"},
    {1210, 330, "Aegis"},     {1460, 60, "Stormcall"}, {1460, 240, "Overdrive"},
    {1460, 410, "Bastion"},   {1710, 150, "Zenith"},   {1710, 340, "Citadel"},
    {1950, 240, "Ascendant"},
};
constexpr ui::MapLink kSkillLinks[] = {
    {0, 1},   {0, 2},   {1, 3},   {1, 4},   {2, 4},   {2, 5},   {3, 6},
    {4, 6},   {4, 7},   {5, 7},   {6, 8},   {6, 9},   {7, 9},   {7, 10},
    {8, 11},  {9, 11},  {9, 12},  {10, 12}, {11, 13}, {11, 14}, {12, 14},
    {12, 15}, {13, 16}, {14, 16}, {14, 17}, {15, 17}, {16, 18}, {17, 18},
};

Color opaque(Color color)
{
    return {color.r, color.g, color.b, 1.0f};
}

Rect trigger_rect(int index)
{
    const float width = (kPageArea.w - kTriggerGap * static_cast<float>(kTriggers - 1)) /
                        static_cast<float>(kTriggers);
    return {kPageArea.x + static_cast<float>(index) * (width + kTriggerGap), kTriggerY, width,
            kTriggerHeight};
}

class GamePage final : public Page
{
  public:
    explicit GamePage(app::Context &context) : context_(context)
    {
        std::vector<ui::ListItem> rows(5);
        rows[0].title = "Resume";
        rows[1].title = "Inventory";
        rows[1].value = "12 items";
        rows[2].title = "Skills";
        rows[2].value = "3 points";
        rows[3].title = "Options";
        rows[3].chevron = true;
        rows[4].title = "Quit to title";
        pause_.title = "Hollow Reach";
        pause_.subtitle = "Chapter 3, saved four minutes ago";
        pause_.set_items(std::move(rows));
        pause_.side = [this](ui::Canvas &canvas, const Rect &area, float shown)
        { draw_pause_side(canvas, area, shown); };

        std::vector<ui::Profile> people;
        for (const Player &player : kPlayers)
        {
            ui::Profile who;
            who.name = player.name;
            who.detail = player.detail;
            who.extra = player.extra;
            who.accent = Color::rgb(player.accent);
            who.controller = player.controller;
            people.push_back(std::move(who));
        }
        who_.set_profiles(std::move(people));

        std::vector<ui::MapNode> nodes;
        for (const Skill &skill : kSkills)
        {
            ui::MapNode node;
            node.x = skill.x;
            node.y = skill.y;
            node.label = skill.label;
            nodes.push_back(std::move(node));
        }
        nodes[0].state = ui::NodeState::done;
        nodes[1].state = ui::NodeState::done;
        nodes[2].state = ui::NodeState::available;
        nodes[3].state = ui::NodeState::available;
        nodes[4].state = ui::NodeState::available;
        tree_.set_nodes(std::move(nodes),
                        std::vector<ui::MapLink>(std::begin(kSkillLinks), std::end(kSkillLinks)));
        tree_.set_focus(1);

        const Color themed{0.0f, 0.0f, 0.0f, 0.0f}; // alpha 0: the theme's accent
        map_.set_pips({{0.7f, 0.45f, themed},
                       {2.4f, 0.8f, themed},
                       {4.4f, 0.6f, Color::rgb(0xff6b5e)},
                       {5.5f, 1.4f, themed, true}});
        side_map_.set_pips(map_.pips());

        bag_.icon = [this](ui::Canvas &canvas, const Rect &tile, const ui::InventoryItem &item,
                           float) { draw_loot(canvas, tile, item); };

        // The icon slot: a cover from the catalogue instead of a medal.
        unlocks_.icon =
            [this](ui::Canvas &canvas, const Rect &area, const ui::Unlock &unlock, float)
        {
            if (context_.catalog.size() == 0)
                return;
            const demo::Item &art = context_.catalog[static_cast<std::size_t>(unlock.tag) + 2];
            const float radius = std::min(theme_.radius, 12.0f);
            if (art.cover != 0)
                canvas.list.image(art.cover, area, gfx::kCanvasUv, Color::rgb(0xffffff), radius);
            else
                canvas.list.gradient_rect(area, radius, art.mid, art.dark);
        };

        restyle(ui::default_theme(), false);
        reset_game();
        highlight_.snap(trigger_rect(trigger_));
    }

    const char *title() const override
    {
        return "Game";
    }
    const char *summary() const override
    {
        return "ui::PauseMenu, UnlockPopup, ProfilePicker, InventoryGrid, NodeMap, HUD";
    }
    const char *variant() const override
    {
        return kVariants[variant_];
    }

    void restyle(const ui::Theme &theme, bool reduced_motion) override
    {
        theme_ = theme;
        reduced_ = reduced_motion;
        apply_variant();
    }

    void enter() override
    {
        age_ = 0.0f;
        pause_.dismiss();
        unlocks_.clear(true);
        who_open_ = false;
        who_fade_.snap(0.0f);
        focus_ = Focus::triggers;
        panel_.snap(0.0f);
        status_.clear();
        reset_game();
    }

    void update(const InputFrame &input, float dt, ui::Feedback &feedback) override
    {
        clock_ += dt;
        age_ += dt;

        if (pause_.is_open())
            update_pause(input, feedback);
        else if (who_open_)
            update_profiles(input, feedback);
        else if (focus_ == Focus::inventory)
            update_bag(input, feedback);
        else if (focus_ == Focus::nodes)
            update_tree(input, feedback);
        else
            update_triggers(input, feedback);

        // The game holds its breath while a menu covers it.
        if (!pause_.is_open() && !who_open_)
            advance_game(dt);

        highlight_.target(trigger_rect(trigger_));
        highlight_.update(dt, style_);
        press_.update(dt, 8.0f);
        panel_.target = focus_ == Focus::triggers ? 0.0f : 1.0f;
        panel_.update(dt, std::max(style_.omega(), 12.0f));
        who_fade_.target = who_open_ ? 1.0f : 0.0f;
        who_fade_.update(dt, std::max(style_.omega(), 12.0f) * (who_open_ ? 1.2f : 1.8f));

        health_.update(dt);
        ammo_.update(dt);
        goals_.update(dt);
        map_.update(dt);
        side_map_.update(dt);
        pause_.update(dt);
        unlocks_.update(dt, feedback);
        who_.update(dt);
        bag_.set_active(focus_ == Focus::inventory);
        bag_.update(dt);
        tree_.set_active(focus_ == Focus::nodes);
        tree_.update(dt);
    }

    void draw(ui::Canvas &canvas) const override
    {
        gfx::DrawList &list = canvas.list;
        ui::Painter paint(list, canvas.fonts, theme_, canvas.glass);

        // ---- the triggers ----
        const bool on_triggers = focus_ == Focus::triggers;
        for (int i = 0; i < kTriggers; ++i)
        {
            const float arrive =
                reduced_ ? tween::cubic_out(age_ / 0.2f) : tween::stagger(age_, i, 0.03f, 0.32f);
            list.push_opacity(arrive);
            list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, reduced_ ? 0.0f : 16.0f * (1.0f - arrive));
            ui::Look look;
            if (i == trigger_)
                look.press = tween::clamp01(press_.value);
            paint.button(trigger_rect(i), kTriggerLabels[i], ui::ButtonKind::secondary, look);
            list.pop_transform();
            list.pop_opacity();
        }
        // One ring for the whole row; it rests, faint, while a panel has the focus.
        ui::HighlightStyle ring;
        ring.kind = ui::HighlightKind::ring;
        highlight_.draw(canvas, style_, ring,
                        tween::cubic_out(age_ / 0.3f) * (on_triggers ? 1.0f : 0.35f));

        const float swap = tween::clamp01(panel_.value);
        const char *caption =
            "HealthBar, ObjectiveTracker, MinimapFrame and AmmoCounter over a game";
        if (shown_ == Focus::inventory && swap > 0.5f)
            caption = "InventoryGrid";
        else if (shown_ == Focus::nodes && swap > 0.5f)
            caption = "NodeMap";
        paint.label(caption, kPageArea.x, kCaptionY, 20.0f, paint.page_text_muted());
        if (!status_.empty())
        {
            const float room = kPageArea.w - paint.label_width(caption, 20.0f) - 60.0f;
            paint.body(ui::fit_body(paint, status_, 20.0f, std::max(room, 80.0f)),
                       kPageArea.x + kPageArea.w, kCaptionY, 20.0f, paint.page_text_muted(),
                       gfx::Align::right);
        }

        // ---- the game, and the panel that covers it ----
        const float arrive = tween::cubic_out((age_ - 0.12f) / 0.4f);
        list.push_opacity(arrive);
        if (swap < 0.995f)
        {
            list.push_opacity(1.0f - swap);
            draw_scene(canvas);
            // In a game the HUD owns the whole screen; here its soft glows
            // must not spill out of the picture onto the page.
            list.push_clip(kMain);
            health_.draw(canvas);
            goals_.draw(canvas);
            map_.draw(canvas);
            ammo_.draw(canvas);
            list.pop_clip();
            list.pop_opacity();
        }
        if (swap > 0.005f)
        {
            list.push_opacity(swap);
            list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, reduced_ ? 0.0f : 18.0f * (1.0f - swap));
            paint.panel(kMain);
            if (shown_ == Focus::inventory)
                draw_bag(canvas);
            else
                tree_.draw(canvas);
            list.pop_transform();
            list.pop_opacity();
        }
        list.pop_opacity();
    }

    void draw_modal(ui::Canvas &canvas) const override
    {
        const float fade = tween::clamp01(who_fade_.value);
        if (fade > 0.004f)
        {
            gfx::DrawList &list = canvas.list;
            list.rounded_rect({0.0f, 0.0f, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0.0f,
                              Color::rgb(0x000000, 0.58f * fade));
            const Rect panel = who_panel();
            list.push_opacity(tween::clamp01(fade * 1.4f));
            list.push_transform(reduced_ ? 1.0f : tween::lerp(0.95f, 1.0f, fade), panel.cx(),
                                panel.cy(), 0.0f, reduced_ ? 0.0f : 20.0f * (1.0f - fade));
            ui::draw_overlay_panel(canvas, theme_, panel, true);
            who_.draw(canvas);
            list.pop_transform();
            list.pop_opacity();
        }
        pause_.draw(canvas);
        // Above everything: a reward interrupts nothing and hides behind nothing.
        unlocks_.draw(canvas);
    }

    std::span<const ui::Hint> hints() const override
    {
        if (pause_.is_open())
            return kPauseHints;
        if (who_open_)
            return kProfileHints;
        if (focus_ == Focus::inventory)
            return bag_.holding() != 0 ? std::span<const ui::Hint>(kCarryHints)
                                       : std::span<const ui::Hint>(kBagHints);
        if (focus_ == Focus::nodes)
            return kTreeHints;
        return kHints;
    }
    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    // ---- layout that depends on the variant ----

    Rect who_panel() const
    {
        const float height = who_.height() + 96.0f;
        const float width = variant_ == 1 ? 980.0f : 1500.0f;
        return {960.0f - width * 0.5f, std::max(540.0f - height * 0.5f, 60.0f), width, height};
    }

    // The grid hugs the panel's left edge: its bounds are exactly its slots.
    Rect bag_bounds(int columns, int rows, float gap) const
    {
        const float across = static_cast<float>(columns);
        const float down = static_cast<float>(rows);
        const float room_w = 900.0f;
        const float room_h = kMain.h - 72.0f;
        const float cell = std::min((room_w - gap * (across - 1.0f)) / across,
                                    (room_h - gap * (down - 1.0f)) / down);
        const float width = across * (cell + gap) - gap;
        const float height = down * (cell + gap) - gap;
        return {kMain.x + 40.0f, kMain.y + 36.0f + (room_h - height) * 0.5f, width, height};
    }

    void say(const char *who, const char *what, const std::string &detail = {})
    {
        status_ = std::string(who) + "  \xC2\xB7  " + what;
        if (!detail.empty())
            status_ += "  \xC2\xB7  " + detail;
    }

    void fill_bag()
    {
        bag_.clear();
        // Spread over the grid with gaps, so there is room to move things.
        const int slots = bag_.slot_count();
        for (int i = 0; i < kLootCount; ++i)
        {
            const Loot &loot = kLoot[i];
            ui::InventoryItem item;
            item.id = i + 1;
            item.name = loot.name;
            item.count = loot.count;
            item.max_stack = loot.max_stack;
            item.kind = loot.kind;
            item.category = loot.category;
            if (loot.rarity != 0)
                item.rarity = Color::rgb(loot.rarity);
            item.tag = i;
            int slot = (i * 3) / 2;
            while (slot < slots && bag_.at(slot) != nullptr)
                ++slot;
            if (slot < slots)
                bag_.put(slot, std::move(item));
        }
        bag_.set_focus(0);
    }

    // Every variant starts from the components' defaults, so what it does not
    // name is the default. State (what is open, the focus, the game) stays.
    void apply_variant()
    {
        style_.theme = theme_;
        style_.reduced_motion = reduced_;

        ui::HealthBarStyle health;
        ui::AmmoStyle ammo;
        ui::ObjectiveStyle goals;
        ui::MinimapStyle map;
        ui::PauseMenuStyle pause;
        ui::UnlockPopupStyle unlock;
        ui::ProfilePickerStyle who;
        ui::InventoryStyle bag;
        ui::NodeMapStyle tree;

        Rect health_at{kMain.x + kHudInset, kMain.y + 30.0f, 400.0f, 58.0f};
        Rect goals_at{kMain.x + kHudInset, kMain.y + 132.0f, 440.0f, 210.0f};
        Rect map_at{kMain.x + kHudInset, kMain.y + kMain.h - 30.0f - 190.0f, 190.0f, 190.0f};
        Rect ammo_at{kMain.x + kMain.w - kHudInset - 360.0f, kMain.y + kMain.h - 30.0f - 76.0f,
                     360.0f, 76.0f};

        health.kind = ui::HealthKind::segmented;
        pause.side_height = 400.0f;
        // The gallery draws this page above the blurred layer: blurring the
        // backdrop would hide the game instead of softening it.
        pause.backdrop_blur = false;
        unlock.anchor = ui::PopupAnchor::top_right;
        unlock.hold = 2.6f;
        unlock.margin = 28.0f;
        who.on_surface = true;
        tree.on_surface = true;
        tree.margin = 260.0f;

        if (variant_ == 1)
        {
            const ui::HudBacking plate = ui::HudBacking::plate;
            health.kind = ui::HealthKind::continuous;
            health.backing = plate;
            ammo.backing = plate;
            ammo.pips = true;
            goals.backing = plate;
            map.backing = plate;
            map.shape = ui::MinimapShape::square;
            map.rotate = false;
            health_at.x += 8.0f;
            health_at.y += 10.0f;
            goals_at.x += 8.0f;
            goals_at.y += 22.0f;
            goals_at.h = 190.0f;
            map_at.x += 8.0f;
            map_at.y += 14.0f;
            map_at.w = map_at.h = 170.0f;
            ammo_at.x -= 8.0f;
            ammo_at.y -= 8.0f;
            ammo_at.h = 92.0f;
            pause.layout = ui::PauseLayout::center;
            pause.highlight.kind = ui::HighlightKind::bar;
            pause.side_width = 480.0f;
            unlock.anchor = ui::PopupAnchor::bottom_center;
            who.columns = 3;
            who.avatar_shape = ui::AvatarShape::rounded;
            who.card_width = 250.0f;
            bag.columns = 8;
            bag.rows = 3;
            tree.zoom = 0.8f;
            tree.node_size = 72.0f;
        }
        else if (variant_ == 2)
        {
            const ui::HudBacking bare = ui::HudBacking::none;
            health.backing = bare;
            health.kind = ui::HealthKind::continuous;
            health.bar_height = 14.0f;
            health.show_value = false;
            health_at.w = 320.0f;
            health_at.h = 44.0f;
            ammo.backing = bare;
            ammo.ring = false;
            ammo.digits_size = 48.0f;
            ammo.label = "Crossbow";
            goals.backing = bare;
            goals.text_size = 21.0f;
            goals.row_height = 34.0f;
            goals.linger = 0.8f;
            goals_at.y -= 14.0f;
            map.backing = bare;
            map.rotate = false;
            map_at.y += 40.0f;
            map_at.w = map_at.h = 150.0f;
            pause.layout = ui::PauseLayout::right;
            pause.highlight.kind = ui::HighlightKind::tint;
            pause.dividers = true;
            pause.width = 480.0f;
            pause.row_height = 58.0f;
            pause.title_size = 38.0f;
            pause.side_width = 620.0f;
            unlock.anchor = ui::PopupAnchor::top_center;
            unlock.width = 470.0f;
            unlock.height = 96.0f;
            unlock.medal_size = 60.0f;
            unlock.title_size = 23.0f;
            unlock.subtitle_size = 0.0f;
            unlock.sparks = false;
            who.add_card = false;
            who.expand = 0.0f;
            who.dim = 0.4f;
            who.card_width = 210.0f;
            who.avatar_size = 96.0f;
            who.card_height = 226.0f;
            bag.columns = 5;
            bag.rows = 4;
            bag.wrap = true;
            bag.tilt = 0.0f;
            tree.zoom = 1.12f;
            tree.flow = false;
        }

        const auto themed = [this](ui::ComponentStyle &style)
        {
            style.theme = theme_;
            style.reduced_motion = reduced_;
        };
        themed(health);
        themed(ammo);
        themed(goals);
        themed(map);
        themed(pause);
        themed(unlock);
        themed(who);
        themed(bag);
        themed(tree);

        health_.style = health;
        health_.title = "Warden";
        health_.set_bounds(health_at);
        ammo_.style = ammo;
        ammo_.set_capacity(12);
        ammo_.set_bounds(ammo_at);
        goals_.style = goals;
        goals_.set_bounds(goals_at);
        map_.style = map;
        map_.set_bounds(map_at);

        // The pause menu's own map: north up, on the menu's panel.
        ui::MinimapStyle chart = map;
        chart.backing = ui::HudBacking::plate;
        chart.frosted = false;
        chart.shape = ui::MinimapShape::square;
        chart.rotate = false;
        side_map_.style = chart;

        pause_.style = pause;
        pause_.set_bounds({0.0f, 0.0f, gfx::kVirtualWidth, gfx::kVirtualHeight});
        unlocks_.style = unlock;
        // Inside the game's picture, like a console's own notification.
        unlocks_.set_bounds(kMain);

        who_.style = who;
        const Rect panel = who_panel();
        who_.set_bounds({panel.x + 40.0f, panel.y + 48.0f, panel.w - 80.0f, panel.h - 96.0f});

        const bool resized = bag.columns != bag_.style.columns || bag.rows != bag_.style.rows;
        bag_.style = bag;
        bag_.set_bounds(bag_bounds(bag.columns, bag.rows, bag.gap));
        if (resized || bag_.item_count() == 0)
            fill_bag();

        tree_.style = tree;
        tree_.set_bounds(kMain.inset(18.0f));
    }

    // ---- the game ----

    void reset_game()
    {
        game_ = 0.0f;
        hit_in_ = 1.25f;
        hit_index_ = 0;
        health_.set_max(100.0f);
        health_.set_value(100.0f, true);
        health_.set_shield(0.0f, true);
        rounds_ = 12;
        reserve_ = 48;
        reload_ = -1.0f;
        fire_in_ = 0.5f;
        burst_ = 0;
        ammo_.set_ammo(rounds_, reserve_);
        ammo_.set_reload(-1.0f);
        goal_set_ = 0;
        goal_in_ = 2.4f;
        goals_.clear();
        goal_ids_.clear();
        add_goals();
        heading_ = 0.4f;
        map_.set_heading(heading_, true);
        side_map_.set_heading(0.0f, true);
    }

    void add_goals()
    {
        goals_.title = kGoalTitles[goal_set_ % kGoalSets];
        goal_ids_.clear();
        for (const Goal &goal : kGoals[goal_set_ % kGoalSets])
            goal_ids_.push_back(goals_.add(goal.text, goal.distance));
        ++goal_set_;
    }

    void advance_game(float dt)
    {
        game_ += dt;

        hit_in_ -= dt;
        if (hit_in_ <= 0.0f)
        {
            if (health_.shield() > 0.0f)
            {
                // The shield takes the first blow.
                health_.set_shield(0.0f);
                hit_in_ = 1.6f;
            }
            else if (health_.value() > 34.0f)
            {
                health_.set_value(health_.value() - kHits[hit_index_++ % 4]);
                hit_in_ = 2.2f;
            }
            else
            {
                health_.set_value(100.0f);
                health_.set_shield(30.0f);
                hit_in_ = 2.6f;
            }
        }

        if (reload_ >= 0.0f)
        {
            reload_ += dt / 1.3f;
            if (reload_ >= 1.0f)
            {
                reload_ = -1.0f;
                rounds_ = ammo_.capacity();
                reserve_ = reserve_ >= ammo_.capacity() ? reserve_ - ammo_.capacity() : 48;
            }
        }
        else
        {
            fire_in_ -= dt;
            if (fire_in_ <= 0.0f)
            {
                // Bursts of four, then a breath.
                --rounds_;
                burst_ = (burst_ + 1) % 4;
                fire_in_ = burst_ == 0 ? 1.1f : 0.16f;
                if (rounds_ <= 0)
                    reload_ = 0.0f;
            }
        }
        ammo_.set_ammo(rounds_, reserve_);
        ammo_.set_reload(reload_);

        goal_in_ -= dt;
        if (goal_in_ <= 0.0f)
        {
            goal_in_ = 2.4f;
            bool ticked = false;
            for (const int id : goal_ids_)
            {
                if (!ticked && goals_.complete(id))
                    ticked = true;
            }
            if (!ticked && goals_.count() == 0)
                add_goals();
        }

        heading_ += dt * 0.22f;
        map_.set_heading(heading_);
    }

    // ---- input ----

    void update_triggers(const InputFrame &input, ui::Feedback &feedback)
    {
        const float x = trigger_rect(trigger_).cx();
        if (input.is_pressed(Action::west))
        {
            variant_ = (variant_ + 1) % kVariantCount;
            apply_variant();
            ui::play_cue(feedback, style_, style_.sounds.change);
            return;
        }
        if (input.nav == Direction::left || input.nav == Direction::right)
        {
            const int next = trigger_ + (input.nav == Direction::right ? 1 : -1);
            if (next < 0 || next >= kTriggers)
            {
                ui::refuse(feedback, style_, input, highlight_.refusal(), x);
                return;
            }
            trigger_ = next;
            ui::play_cue(feedback, style_, style_.sounds.move, trigger_rect(trigger_).cx());
            return;
        }
        if (input.nav != Direction::none)
        {
            ui::refuse(feedback, style_, input, highlight_.refusal(), x);
            return;
        }
        if (!input.is_pressed(Action::confirm))
            return;
        press_.trigger();
        switch (trigger_)
        {
        case trigger_pause:
            pause_.open(feedback);
            say("PauseMenu", "open()");
            break;
        case trigger_profiles:
            who_open_ = true;
            who_.set_focus(0);
            who_.enter();
            ui::play_cue(feedback, style_, style_.sounds.open);
            say("ProfilePicker", "shown on a panel of the page");
            break;
        case trigger_inventory:
            focus_ = shown_ = Focus::inventory;
            bag_.enter();
            ui::play_cue(feedback, style_, style_.sounds.activate, x);
            say("InventoryGrid", "confirm lifts, carries and sets down");
            break;
        case trigger_nodes:
            focus_ = shown_ = Focus::nodes;
            tree_.enter();
            ui::play_cue(feedback, style_, style_.sounds.activate, x);
            say("NodeMap", "a direction picks the best node that way");
            break;
        default:
        {
            const int tier = trigger_ - trigger_bronze;
            const UnlockSample &sample = kUnlocks[tier];
            ui::Unlock unlock;
            unlock.title = sample.title;
            unlock.subtitle = sample.subtitle;
            unlock.points = sample.points;
            unlock.tier = static_cast<ui::UnlockTier>(tier);
            unlock.icon = variant_ == 1; // the second variant shows the icon slot
            unlock.tag = tier;
            unlocks_.push(std::move(unlock));
            say("UnlockPopup", "push()", kTriggerLabels[trigger_]);
            break;
        }
        }
    }

    void update_pause(const InputFrame &input, ui::Feedback &feedback)
    {
        const ui::Event event = pause_.handle(input, feedback);
        if (event == ui::Event::activated)
        {
            char text[64];
            std::snprintf(text, sizeof(text), "focus() = %d", pause_.focus());
            say("PauseMenu", "Event::activated", text);
            if (pause_.focus() == 0)
                pause_.close(feedback);
        }
        else if (event == ui::Event::cancelled)
        {
            say("PauseMenu", "Event::cancelled");
        }
    }

    void update_profiles(const InputFrame &input, ui::Feedback &feedback)
    {
        const ui::Event event = who_.handle(input, feedback);
        if (event == ui::Event::activated)
        {
            if (who_.add_focused())
                say("ProfilePicker", "Event::activated", "add_focused()");
            else
                say("ProfilePicker", "Event::activated",
                    who_.profiles()[static_cast<std::size_t>(who_.focus())].name);
            who_open_ = false;
        }
        else if (event == ui::Event::cancelled)
        {
            say("ProfilePicker", "Event::cancelled");
            who_open_ = false;
        }
    }

    void update_bag(const InputFrame &input, ui::Feedback &feedback)
    {
        if (input.is_pressed(Action::north))
        {
            filter_ = (filter_ + 1) % kFilters;
            bag_.set_filter(filter_ == 0 ? -1 : filter_);
            ui::play_cue(feedback, style_, style_.sounds.page);
            say("InventoryGrid", "set_filter()", kFilterNames[filter_]);
            return;
        }
        const ui::Event event = bag_.handle(input, feedback);
        if (event == ui::Event::cancelled)
        {
            focus_ = Focus::triggers;
            say("InventoryGrid", "Event::cancelled");
        }
        else if (event == ui::Event::changed)
        {
            constexpr const char *kActions[] = {
                "none", "picked", "placed", "swapped", "merged", "returned",
            };
            const ui::InventoryMove &move = bag_.last_move();
            char text[64];
            std::snprintf(text, sizeof(text), "%s, slot %d to %d",
                          kActions[static_cast<int>(move.action)], move.from, move.to);
            say("InventoryGrid", "Event::changed", text);
        }
    }

    void update_tree(const InputFrame &input, ui::Feedback &feedback)
    {
        const ui::Event event = tree_.handle(input, feedback);
        if (event == ui::Event::cancelled)
        {
            focus_ = Focus::triggers;
            say("NodeMap", "Event::cancelled");
        }
        else if (event == ui::Event::activated)
        {
            const int at = tree_.focus();
            const std::string &name = tree_.nodes()[static_cast<std::size_t>(at)].label;
            if (tree_.nodes()[static_cast<std::size_t>(at)].state != ui::NodeState::available)
            {
                say("NodeMap", "Event::activated", name + " is already learned");
                return;
            }
            // The screen decides what a confirm means: here it learns the
            // skill and opens whatever now has a learned parent.
            tree_.set_state(at, ui::NodeState::done);
            for (const ui::MapLink &link : tree_.links())
            {
                if (link.from == at &&
                    tree_.nodes()[static_cast<std::size_t>(link.to)].state == ui::NodeState::locked)
                    tree_.set_state(link.to, ui::NodeState::available);
            }
            ui::play_cue(feedback, style_, audio::Cue::connect, tree_.node_rect(at).cx());
            say("NodeMap", "Event::activated", name + " learned");
        }
    }

    // ---- drawing ----

    // A dusk over layered ridges, tinted by the theme's main colour. It is a
    // game's picture, not interface: bright at the horizon and dark above, so
    // the HUD is tested against both.
    void draw_scene(ui::Canvas &canvas) const
    {
        gfx::DrawList &list = canvas.list;
        ui::Painter paint(list, canvas.fonts, theme_, 0);
        const Color tint = opaque(theme_.primary);
        const Color night = gfx::mix(Color::rgb(0x0b1430), tint, 0.22f);
        const Color glow = gfx::mix(Color::rgb(0xffb070), tint, 0.22f);
        const Color far = gfx::mix(night, glow, 0.42f);
        const Color mid = gfx::mix(night, glow, 0.2f);
        const Color near = gfx::mix(night, Color::rgb(0x000000), 0.35f);
        const float drift = reduced_ ? 0.0f : game_;

        list.push_clip(kMain);
        const float horizon = kMain.y + kMain.h * 0.7f;
        list.gradient_rect({kMain.x, kMain.y, kMain.w, horizon - kMain.y}, 0.0f, night, glow);
        list.gradient_rect({kMain.x, horizon, kMain.w, kMain.y + kMain.h - horizon}, 0.0f, mid,
                           near);
        // The sun sits low and right of centre, behind the far ridge.
        const float sun_x = kMain.x + kMain.w * 0.64f;
        const float sun_y = horizon - 40.0f;
        list.shadow({sun_x - 90.0f, sun_y - 90.0f, 180.0f, 180.0f}, 90.0f, 150.0f,
                    Color::rgb(0xffe9c4, 0.42f));
        list.circle(sun_x, sun_y, 54.0f, Color::rgb(0xfff1d6));

        // Three ridges; the nearer ones are larger, darker and drift faster.
        struct Ridge
        {
            float width, height, speed, lift;
            Color color;
        };
        const Ridge ridges[] = {
            {420.0f, 190.0f, 5.0f, 0.0f, far},
            {560.0f, 250.0f, 11.0f, 70.0f, mid},
            {760.0f, 260.0f, 22.0f, 170.0f, near},
        };
        for (const Ridge &ridge : ridges)
        {
            const float step = ridge.width * 0.56f;
            const float shift = std::fmod(drift * ridge.speed, step * 2.0f);
            const int count = static_cast<int>(kMain.w / step) + 5;
            for (int i = 0; i < count; ++i)
            {
                // Alternate two heights so the skyline is not a saw blade.
                const float tall = ridge.height * (i % 2 == 0 ? 1.0f : 0.72f);
                const float x = kMain.x - 2.0f * step + static_cast<float>(i) * step - shift;
                const float base = horizon + ridge.lift + 40.0f;
                list.triangle({x, base - tall, ridge.width, tall}, ridge.color);
            }
            list.rounded_rect({kMain.x, horizon + ridge.lift + 39.0f, kMain.w, kMain.h}, 0.0f,
                              ridge.color);
        }
        // A cross-hair dot: where the player is looking.
        const float cx = kMain.cx();
        const float cy = kMain.y + kMain.h * 0.52f;
        list.circle(cx, cy, 5.0f, Color::rgb(0x05080d, 0.5f));
        list.circle(cx, cy, 3.0f, Color::rgb(0xf5f7fb));
        list.pop_clip();
        paint.stroke(kMain, 0.0f, std::max(theme_.border, 2.0f),
                     opaque(gfx::mix(theme_.outline, paint.page_text_muted(), 0.4f)));
    }

    // The bag's icon slot: a plate in the item's rarity and a picture made of
    // a few shapes, so every item is recognisable without artwork.
    void draw_loot(ui::Canvas &canvas, const Rect &tile, const ui::InventoryItem &item) const
    {
        gfx::DrawList &list = canvas.list;
        ui::Painter paint(list, canvas.fonts, theme_, 0);
        const Color surface = ui::solid_surface(theme_);
        const Color plate = item.rarity.a > 0.0f
                                ? gfx::mix(item.rarity, surface, 0.22f)
                                : gfx::mix(surface, opaque(theme_.text_muted), 0.3f);
        paint.fill(tile, std::min(theme_.radius, tile.w * 0.22f), plate);
        const Color ink = ui::Painter::on(plate);
        const float s = tile.w;
        const auto px = [&](float u) { return tile.x + s * u; };
        const auto py = [&](float v) { return tile.y + s * v; };
        const float pen = s * 0.07f;
        switch (kLoot[static_cast<std::size_t>(item.tag) % std::size(kLoot)].glyph)
        {
        case bottle:
            list.rounded_rect({px(0.3f), py(0.42f), s * 0.4f, s * 0.42f}, s * 0.08f, ink);
            list.rounded_rect({px(0.42f), py(0.22f), s * 0.16f, s * 0.24f}, 0.0f, ink);
            list.rounded_rect({px(0.38f), py(0.14f), s * 0.24f, s * 0.09f}, s * 0.03f, ink);
            break;
        case blade:
            list.line(px(0.3f), py(0.72f), px(0.74f), py(0.24f), s * 0.09f, ink);
            list.line(px(0.3f), py(0.54f), px(0.48f), py(0.72f), pen, ink);
            break;
        case lantern:
            list.arc(px(0.5f), py(0.3f), s * 0.14f, s * 0.05f, -1.5707963f, 3.1415927f, ink, false);
            list.rounded_rect({px(0.3f), py(0.3f), s * 0.4f, s * 0.08f}, 0.0f, ink);
            list.rounded_rect({px(0.34f), py(0.4f), s * 0.32f, s * 0.34f}, s * 0.05f, ink);
            list.circle(px(0.5f), py(0.57f), s * 0.07f, plate);
            list.rounded_rect({px(0.3f), py(0.76f), s * 0.4f, s * 0.07f}, 0.0f, ink);
            break;
        case drop:
            list.triangle({px(0.33f), py(0.16f), s * 0.34f, s * 0.42f}, ink);
            list.circle(px(0.5f), py(0.6f), s * 0.2f, ink);
            break;
        case key:
            list.ring(px(0.36f), py(0.36f), s * 0.16f, pen, ink);
            list.line(px(0.47f), py(0.47f), px(0.78f), py(0.78f), pen, ink);
            list.line(px(0.66f), py(0.66f), px(0.76f), py(0.56f), pen, ink);
            break;
        case chart:
            list.bordered_rect({px(0.2f), py(0.28f), s * 0.6f, s * 0.44f}, s * 0.03f,
                               Color{0.0f, 0.0f, 0.0f, 0.0f}, s * 0.05f, ink);
            list.line(px(0.4f), py(0.32f), px(0.4f), py(0.68f), s * 0.035f, ink);
            list.line(px(0.6f), py(0.32f), px(0.6f), py(0.68f), s * 0.035f, ink);
            list.circle(px(0.7f), py(0.42f), s * 0.045f, ink);
            break;
        case cloak:
            list.triangle({px(0.22f), py(0.3f), s * 0.56f, s * 0.5f}, ink);
            list.circle(px(0.5f), py(0.28f), s * 0.1f, ink);
            break;
        case bolt:
            list.line(px(0.26f), py(0.74f), px(0.66f), py(0.34f), s * 0.06f, ink);
            list.triangle({px(0.58f), py(0.18f), s * 0.24f, s * 0.24f}, ink, 0.0f, 0.7853982f);
            list.line(px(0.24f), py(0.62f), px(0.38f), py(0.76f), s * 0.06f, ink);
            break;
        default:
            // The horn: a cone lying on its side, and the rim of its bell.
            list.triangle({px(0.24f), py(0.26f), s * 0.48f, s * 0.48f}, ink, 0.0f, 4.712389f);
            list.rounded_rect({px(0.7f), py(0.24f), s * 0.07f, s * 0.52f}, s * 0.03f, ink);
            break;
        }
    }

    // The inventory panel: the grid, and what the focused slot holds.
    void draw_bag(ui::Canvas &canvas) const
    {
        gfx::DrawList &list = canvas.list;
        ui::Painter paint(list, canvas.fonts, theme_, canvas.glass);
        bag_.draw(canvas);

        const Rect &grid = bag_.bounds();
        const float x = std::max(grid.x + grid.w + 48.0f, kMain.x + 880.0f);
        const float right = kMain.x + kMain.w - 40.0f;
        const float room = right - x;
        float y = kMain.y + 44.0f;

        // The filter: the category helper, one chip per choice.
        float chip_x = x;
        for (int i = 0; i < kFilters; ++i)
        {
            const float width = paint.label_width(kFilterNames[i], 20.0f) + 40.0f;
            ui::Look look;
            paint.chip({chip_x, y, width, 42.0f}, kFilterNames[i], i == filter_ ? 1.0f : 0.0f,
                       look);
            chip_x += width + 10.0f;
        }
        y += 42.0f + 56.0f;

        const int held = bag_.holding();
        const ui::InventoryItem *item = held != 0 ? bag_.find(held) : bag_.at(bag_.focus());
        if (item == nullptr)
        {
            paint.heading("Empty slot", x, y, 36.0f, theme_.text_muted);
            paint.body(held != 0 ? "Set it down here." : "Nothing to lift.", x, y + 44.0f, 23.0f,
                       theme_.text_muted);
        }
        else
        {
            const Loot &loot = kLoot[static_cast<std::size_t>(item->tag) % std::size(kLoot)];
            const std::vector<std::string> name =
                ui::wrap_heading(canvas, theme_, item->name, 36.0f, room, 1);
            if (!name.empty())
                paint.heading(name[0], x, y, 36.0f, theme_.text);
            const char *rarity = "Common";
            if (loot.rarity == kUncommon)
                rarity = "Uncommon";
            else if (loot.rarity == kRare)
                rarity = "Rare";
            else if (loot.rarity == kEpic)
                rarity = "Epic";
            else if (loot.rarity == kLegend)
                rarity = "Legendary";
            y += 40.0f;
            if (item->rarity.a > 0.0f)
            {
                list.circle(x + 8.0f, y - 7.0f, 8.0f, item->rarity);
                paint.label(rarity, x + 26.0f, y, 21.0f, theme_.text_muted);
            }
            else
            {
                paint.label(rarity, x, y, 21.0f, theme_.text_muted);
            }
            if (item->max_stack > 1)
            {
                char text[32];
                std::snprintf(text, sizeof(text), "%d / %d", item->count, item->max_stack);
                paint.label(text, right, y, 21.0f, theme_.text_muted, gfx::Align::right);
            }
            y += 44.0f;
            for (const std::string &line : ui::wrap_body(paint, loot.about, 23.0f, room, 3))
            {
                paint.body(line, x, y, 23.0f, theme_.text);
                y += 33.0f;
            }
        }

        const float foot = kMain.y + kMain.h - 44.0f;
        list.rounded_rect({x, foot - 40.0f, room, 1.5f}, 0.0f, theme_.text_muted.with_alpha(0.25f));
        paint.body(ui::fit_body(paint,
                                held != 0 ? "Carrying. Like kinds merge, others trade places."
                                          : "Lift an item to carry it.",
                                21.0f, room),
                   x, foot, 21.0f, theme_.text_muted);
    }

    // The pause menu's side panel: where the player is and what they were doing.
    void draw_pause_side(ui::Canvas &canvas, const Rect &area, float shown) const
    {
        ui::Painter paint(canvas.list, canvas.fonts, theme_, canvas.glass);
        (void)shown;
        const float chart = std::min(area.h - 150.0f, std::min(area.w * 0.46f, 220.0f));
        paint.label(ui::fit_label(paint, "CURRENT OBJECTIVE", 17.0f, area.w), area.x,
                    area.y + 16.0f, 17.0f, theme_.text_muted);
        const std::vector<std::string> title =
            ui::wrap_heading(canvas, theme_, goals_.title, 30.0f, area.w, 1);
        if (!title.empty())
            paint.heading(title[0], area.x, area.y + 58.0f, 30.0f, theme_.text);
        char text[48];
        std::snprintf(text, sizeof(text), "%d of %d steps left", goals_.remaining(), kGoalLines);
        paint.body(ui::fit_body(paint, text, 22.0f, area.w), area.x, area.y + 92.0f, 22.0f,
                   theme_.text_muted);

        const float top = area.y + 124.0f;
        if (chart >= 90.0f)
        {
            // The map is a member with its own heading spring: it is only
            // placed here, never mutated, so draw() stays pure.
            canvas.list.push_transform(1.0f, 0.0f, 0.0f, area.x - side_map_.bounds().x,
                                       top - side_map_.bounds().y);
            canvas.list.push_transform(chart / std::max(side_map_.bounds().w, 1.0f),
                                       side_map_.bounds().x, side_map_.bounds().y, 0.0f, 0.0f);
            side_map_.draw(canvas);
            canvas.list.pop_transform();
            canvas.list.pop_transform();
        }
        const float x = area.x + (chart >= 90.0f ? chart + 28.0f : 0.0f);
        const float room = area.x + area.w - x;
        constexpr const char *kNames[3] = {"Health", "Played", "Deaths"};
        char values[3][24];
        std::snprintf(values[0], sizeof(values[0]), "%d / 100",
                      static_cast<int>(health_.value() + 0.5f));
        std::snprintf(values[1], sizeof(values[1]), "12:40");
        std::snprintf(values[2], sizeof(values[2]), "3");
        float y = top + 30.0f;
        for (int i = 0; i < 3 && room > 120.0f; ++i)
        {
            paint.body(kNames[i], x, y, 21.0f, theme_.text_muted);
            paint.label(values[i], x + room, y, 21.0f, theme_.text, gfx::Align::right);
            canvas.list.rounded_rect({x, y + 14.0f, room, 1.5f}, 0.0f,
                                     theme_.text_muted.with_alpha(0.2f));
            y += 46.0f;
        }
    }

    app::Context &context_;
    ui::Theme theme_ = ui::default_theme();
    bool reduced_ = false;
    ui::ComponentStyle style_; // the page's own cues and the triggers' ring

    ui::HealthBar health_;
    ui::AmmoCounter ammo_;
    ui::ObjectiveTracker goals_;
    ui::MinimapFrame map_;
    ui::MinimapFrame side_map_;
    ui::PauseMenu pause_;
    ui::UnlockPopup unlocks_;
    ui::ProfilePicker who_;
    ui::InventoryGrid bag_;
    ui::NodeMap tree_;

    ui::Highlight highlight_;
    ui::Pulse press_;
    tween::Spring panel_;    // 0 the game .. 1 a panel over it
    tween::Spring who_fade_; // the profile picker's own modal
    Focus focus_ = Focus::triggers;
    Focus shown_ = Focus::inventory; // the panel last opened: it fades out as itself
    bool who_open_ = false;
    int trigger_ = 0;
    int variant_ = 0;
    int filter_ = 0;
    float clock_ = 0.0f;
    float age_ = 0.0f;
    std::string status_;

    // The game's own state.
    float game_ = 0.0f;
    float hit_in_ = 0.0f;
    int hit_index_ = 0;
    int rounds_ = 12;
    int reserve_ = 48;
    float reload_ = -1.0f;
    float fire_in_ = 0.0f;
    int burst_ = 0;
    int goal_set_ = 0;
    float goal_in_ = 0.0f;
    std::vector<int> goal_ids_;
    float heading_ = 0.0f;
};

} // namespace

std::unique_ptr<Page> make_game_page(app::Context &context)
{
    return std::make_unique<GamePage>(context);
}

} // namespace hui::concepts::gallery
