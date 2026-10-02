// ps5-homebrew-ui - Tests: PauseMenu, UnlockPopup, ProfilePicker, InventoryGrid, NodeMap, HUD.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "component_fixture.hpp"
#include "ui/components/hud.hpp"
#include "ui/components/inventory_grid.hpp"
#include "ui/components/node_map.hpp"
#include "ui/components/pause_menu.hpp"
#include "ui/components/profile_picker.hpp"
#include "ui/components/unlock_popup.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;
using hui::gfx::Rect;
using hui::ui::AmmoCounter;
using hui::ui::Event;
using hui::ui::HealthBar;
using hui::ui::InventoryAction;
using hui::ui::InventoryGrid;
using hui::ui::InventoryItem;
using hui::ui::ListItem;
using hui::ui::MapLink;
using hui::ui::MapNode;
using hui::ui::MinimapFrame;
using hui::ui::NodeMap;
using hui::ui::NodeState;
using hui::ui::ObjectiveTracker;
using hui::ui::PauseLayout;
using hui::ui::PauseMenu;
using hui::ui::PopupAnchor;
using hui::ui::Profile;
using hui::ui::ProfilePicker;
using hui::ui::Unlock;
using hui::ui::UnlockPopup;
using hui::ui::UnlockTier;

constexpr int kTonic = 7;

class ComponentsGame : public hui::testing::ComponentFixture
{
  protected:
    // One input, then half a second of animation.
    template <typename Component> Event send(Component &component, const hui::InputFrame &input)
    {
        feedback_.clear();
        const Event event = component.handle(input, feedback_);
        settle(component, 0.5f);
        return event;
    }

    template <typename Component> void settle(Component &component, float seconds = 1.5f)
    {
        for (int i = 0, frames = static_cast<int>(seconds / kFrame); i < frames; ++i)
            component.update(kFrame);
    }

    // The pop-up sounds from update(): the cues of the whole run are kept.
    void run(UnlockPopup &popup, float seconds)
    {
        for (int i = 0, frames = static_cast<int>(seconds / kFrame); i < frames; ++i)
            popup.update(kFrame, feedback_);
    }

    static std::vector<ListItem> menu()
    {
        std::vector<ListItem> rows(4);
        rows[0].title = "Resume";
        rows[1].title = "Options";
        rows[2].title = "Online";
        rows[2].disabled = true;
        rows[3].title = "Quit to title";
        return rows;
    }

    static std::vector<Profile> people()
    {
        std::vector<Profile> list(3);
        list[0].name = "Mara Voss";
        list[0].detail = "Played today";
        list[0].extra = "Level 12, 63 hours";
        list[0].controller = 1;
        list[1].name = "Tomas Reyk";
        list[1].accent = hui::gfx::Color::rgb(0xff8a5a);
        list[2].name = "A player whose name is far too long for one card";
        list[2].controller = 4;
        return list;
    }

    static InventoryItem item(int id, const char *name, int count = 1, int max_stack = 1,
                              int kind = 0, int category = 0)
    {
        InventoryItem made;
        made.id = id;
        made.name = name;
        made.count = count;
        made.max_stack = max_stack;
        made.kind = kind;
        made.category = category;
        return made;
    }

    // Four columns by two rows: tonics in slots 0 and 1, a blade in slot 2.
    static void fill(InventoryGrid &bag)
    {
        bag.style.columns = 4;
        bag.style.rows = 2;
        bag.set_bounds({100.0f, 100.0f, 460.0f, 224.0f});
        bag.put(0, item(1, "Tonic", 3, 9, kTonic, 2));
        bag.put(1, item(2, "Tonic", 4, 9, kTonic, 2));
        bag.put(2, item(3, "Blade", 1, 1, 0, 1));
    }

    // A root, two children, a grandchild and one node far to the right.
    static void plant(NodeMap &tree)
    {
        std::vector<MapNode> nodes(5);
        nodes[0] = {0.0f, 0.0f, "Root", NodeState::done};
        nodes[1] = {200.0f, -60.0f, "Dash", NodeState::available};
        nodes[2] = {200.0f, 80.0f, "Guard", NodeState::locked};
        nodes[3] = {420.0f, 0.0f, "Vault", NodeState::locked};
        nodes[4] = {2000.0f, 0.0f, "Far", NodeState::locked};
        tree.set_nodes(nodes, {MapLink{0, 1}, MapLink{0, 2}, MapLink{1, 3}});
        tree.set_bounds({100.0f, 100.0f, 600.0f, 300.0f});
        tree.set_focus(0);
    }
};

TEST_F(ComponentsGame, PauseMenuOpensChoosesAndResumes)
{
    PauseMenu pause;
    pause.title = "Hollow Reach";
    pause.set_items(menu());
    EXPECT_FALSE(pause.visible());
    EXPECT_EQ(pause.handle(press(Action::confirm), feedback_), Event::none);

    feedback_.clear();
    pause.open(feedback_);
    EXPECT_TRUE(asked(Cue::modal_open));
    EXPECT_TRUE(pause.is_open());
    EXPECT_EQ(pause.focus(), 0);
    settle(pause, 0.5f);

    EXPECT_EQ(send(pause, nav(Direction::down)), Event::moved);
    EXPECT_EQ(pause.focus(), 1);
    EXPECT_TRUE(asked(Cue::focus));
    EXPECT_EQ(send(pause, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(asked(Cue::select));
    EXPECT_TRUE(pause.is_open()); // the screen decides what a row does

    // A disabled row takes the focus but refuses confirm.
    EXPECT_EQ(send(pause, nav(Direction::down)), Event::moved);
    EXPECT_EQ(send(pause, press(Action::confirm)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));

    // Back resumes: it closes with the close cue and reports `cancelled`.
    feedback_.clear();
    EXPECT_EQ(pause.handle(press(Action::back), feedback_), Event::cancelled);
    EXPECT_TRUE(asked(Cue::modal_close));
    EXPECT_FALSE(pause.is_open());
    EXPECT_TRUE(pause.visible()); // still leaving
    settle(pause);
    EXPECT_FALSE(pause.visible());
}

TEST_F(ComponentsGame, PauseMenuWrapsAndPlacesItsPanels)
{
    PauseMenu pause;
    pause.set_items(menu());
    pause.open(feedback_);
    EXPECT_EQ(send(pause, nav(Direction::up)), Event::moved); // wraps by default
    EXPECT_EQ(pause.focus(), 3);
    pause.style.wrap = false;
    EXPECT_EQ(send(pause, nav(Direction::down)), Event::refused);

    // With close_on_back off the screen keeps the menu and hears `cancelled`.
    pause.style.close_on_back = false;
    EXPECT_EQ(send(pause, press(Action::back)), Event::cancelled);
    EXPECT_TRUE(asked(Cue::back));
    EXPECT_TRUE(pause.is_open());

    EXPECT_FLOAT_EQ(pause.side_rect().w, 0.0f); // no slot, no side panel
    pause.side = [](hui::ui::Canvas &, const Rect &, float) {};
    pause.style.layout = PauseLayout::left;
    const Rect left = pause.panel_rect();
    EXPECT_GE(pause.side_rect().x, left.x + left.w);
    pause.style.layout = PauseLayout::center;
    const Rect centre = pause.panel_rect();
    EXPECT_GE(pause.side_rect().x, centre.x + centre.w);
    pause.style.layout = PauseLayout::right;
    const Rect right = pause.panel_rect();
    EXPECT_LE(pause.side_rect().x + pause.side_rect().w, right.x);
    EXPECT_LT(left.x, centre.x);
    EXPECT_LT(centre.x, right.x);

    // dismiss() is immediate and silent.
    feedback_.clear();
    pause.dismiss();
    EXPECT_FALSE(pause.visible());
    EXPECT_TRUE(feedback_.cues.empty());
}

TEST_F(ComponentsGame, UnlockPopupQueuesAndSoundsByTier)
{
    UnlockPopup popup;
    popup.style.hold = 0.4f;
    popup.style.gap = 0.1f;
    EXPECT_TRUE(popup.empty());
    popup.push("First steps", "Leave the valley", UnlockTier::bronze, 10);
    popup.push("First light", "Reach the summit", UnlockTier::gold, 50);
    EXPECT_FALSE(popup.showing()); // nothing appears before update()

    feedback_.clear();
    popup.update(kFrame, feedback_);
    ASSERT_TRUE(popup.showing());
    EXPECT_EQ(popup.current()->title, "First steps");
    EXPECT_EQ(popup.queued(), 1);
    EXPECT_TRUE(asked(Cue::notify));
    EXPECT_FALSE(asked(Cue::complete));

    // One at a time: the gold one waits for the bronze one to leave.
    feedback_.clear();
    run(popup, 0.3f);
    EXPECT_EQ(popup.current()->title, "First steps");
    EXPECT_TRUE(feedback_.cues.empty());
    run(popup, 0.7f);
    ASSERT_TRUE(popup.showing());
    EXPECT_EQ(popup.current()->title, "First light");
    EXPECT_TRUE(asked(Cue::complete)); // the celebratory cue of its tier
    EXPECT_GT(feedback_.rumble_strength, 0.0f);
    run(popup, 3.0f);
    EXPECT_TRUE(popup.empty());
    EXPECT_EQ(popup.current(), nullptr);

    // The anchor names the corner it rests in.
    popup.set_bounds({0.0f, 0.0f, 1920.0f, 1080.0f});
    popup.style.anchor = PopupAnchor::top_right;
    Rect at = popup.rect();
    EXPECT_FLOAT_EQ(at.x + at.w, 1920.0f - popup.style.margin);
    EXPECT_FLOAT_EQ(at.y, popup.style.margin);
    popup.style.anchor = PopupAnchor::bottom_left;
    at = popup.rect();
    EXPECT_FLOAT_EQ(at.x, popup.style.margin);
    EXPECT_FLOAT_EQ(at.y + at.h, 1080.0f - popup.style.margin);
    popup.style.anchor = PopupAnchor::top_center;
    EXPECT_FLOAT_EQ(popup.rect().cx(), 960.0f);

    // A silenced tier cue falls back to the notify cue; clear(true) is instant.
    popup.style.special_cue = Cue::count;
    popup.push("Everything", "", UnlockTier::special);
    feedback_.clear();
    run(popup, 0.2f);
    EXPECT_TRUE(asked(Cue::notify));
    popup.clear(true);
    EXPECT_TRUE(popup.empty());
}

TEST_F(ComponentsGame, ProfilePickerMovesChoosesAndAdds)
{
    ProfilePicker who;
    who.set_profiles(people());
    who.set_bounds({96.0f, 300.0f, 1728.0f, 480.0f});
    EXPECT_EQ(who.count(), 4); // three people and the "add" card
    EXPECT_EQ(who.focus(), 0);
    EXPECT_EQ(send(who, nav(Direction::left)), Event::refused);

    EXPECT_EQ(send(who, nav(Direction::right)), Event::moved);
    EXPECT_TRUE(asked(Cue::focus));
    EXPECT_EQ(send(who, press(Action::confirm)), Event::activated);
    EXPECT_EQ(who.focus(), 1);
    EXPECT_FALSE(who.add_focused());

    send(who, nav(Direction::right));
    send(who, nav(Direction::right));
    EXPECT_TRUE(who.add_focused());
    EXPECT_EQ(send(who, press(Action::confirm)), Event::activated);
    EXPECT_EQ(send(who, nav(Direction::right)), Event::refused);
    hui::InputFrame held = nav(Direction::right);
    held.nav_repeat = true;
    EXPECT_EQ(send(who, held), Event::refused);
    EXPECT_TRUE(feedback_.cues.empty()); // a held direction stays quiet

    // An exit hands the focus to the screen instead of refusing.
    who.style.exits.right = true;
    EXPECT_EQ(send(who, nav(Direction::right)), Event::none);
    EXPECT_EQ(who.exit(), Direction::right);
    who.style.exits.right = false;
    who.style.wrap = true;
    EXPECT_EQ(send(who, nav(Direction::right)), Event::moved);
    EXPECT_EQ(who.focus(), 0);
    EXPECT_EQ(send(who, press(Action::back)), Event::cancelled);
    EXPECT_TRUE(asked(Cue::back));

    // Without the "add" card there are only people.
    who.style.add_card = false;
    who.set_profiles(people());
    EXPECT_EQ(who.count(), 3);
    EXPECT_FALSE(who.add_focused());

    // A grid: down goes to the nearest card of the next row.
    who.style.add_card = true;
    who.style.columns = 2;
    who.set_profiles(people());
    who.set_focus(1);
    EXPECT_EQ(send(who, nav(Direction::down)), Event::moved);
    EXPECT_EQ(who.focus(), 3);
    EXPECT_EQ(send(who, nav(Direction::down)), Event::refused);
    EXPECT_EQ(send(who, nav(Direction::up)), Event::moved);
    EXPECT_EQ(who.focus(), 1);
    EXPECT_GT(who.card_rect(2).y, who.card_rect(0).y);
}

TEST_F(ComponentsGame, InventoryLiftsCarriesPlacesSwapsAndMerges)
{
    InventoryGrid bag;
    fill(bag);
    EXPECT_EQ(bag.item_count(), 3);
    EXPECT_FALSE(bag.put(0, item(9, "Taken slot")));
    EXPECT_FALSE(bag.put(5, item(1, "Known id")));
    EXPECT_FALSE(bag.put(99, item(10, "No such slot")));
    EXPECT_FALSE(bag.put(5, item(0, "No id")));

    // Lift the first tonics and merge them into the stack beside them.
    EXPECT_EQ(send(bag, press(Action::confirm)), Event::changed);
    EXPECT_TRUE(asked(Cue::pickup));
    EXPECT_EQ(bag.last_move().action, InventoryAction::picked);
    EXPECT_EQ(bag.holding(), 1);
    EXPECT_EQ(bag.slot_of(1), -1);
    EXPECT_EQ(bag.at(0), nullptr);
    EXPECT_EQ(send(bag, nav(Direction::right)), Event::moved);
    EXPECT_EQ(send(bag, press(Action::confirm)), Event::changed);
    EXPECT_TRUE(asked(Cue::merge));
    EXPECT_EQ(bag.last_move().action, InventoryAction::merged);
    EXPECT_EQ(bag.last_move().amount, 3);
    EXPECT_EQ(bag.last_move().other, 2);
    EXPECT_EQ(bag.holding(), 0);
    ASSERT_NE(bag.find(2), nullptr);
    EXPECT_EQ(bag.find(2)->count, 7);
    EXPECT_EQ(bag.item_count(), 2);
    EXPECT_EQ(bag.find(1), nullptr);

    // Carry the blade to an empty slot.
    bag.set_focus(2);
    EXPECT_EQ(send(bag, press(Action::confirm)), Event::changed);
    EXPECT_EQ(send(bag, nav(Direction::down)), Event::moved);
    EXPECT_EQ(send(bag, press(Action::confirm)), Event::changed);
    EXPECT_TRUE(asked(Cue::drop));
    EXPECT_EQ(bag.last_move().action, InventoryAction::placed);
    EXPECT_EQ(bag.last_move().from, 2);
    EXPECT_EQ(bag.last_move().to, 6);
    EXPECT_EQ(bag.slot_of(3), 6);

    // Set down on a different kind: the two trade places.
    EXPECT_EQ(send(bag, press(Action::confirm)), Event::changed);
    bag.set_focus(1);
    EXPECT_EQ(send(bag, press(Action::confirm)), Event::changed);
    EXPECT_TRUE(asked(Cue::rotate));
    EXPECT_EQ(bag.last_move().action, InventoryAction::swapped);
    EXPECT_EQ(bag.slot_of(3), 1);
    EXPECT_EQ(bag.slot_of(2), 6);

    // Back with a full hand puts the item where it came from.
    EXPECT_EQ(send(bag, press(Action::confirm)), Event::changed);
    send(bag, nav(Direction::right));
    EXPECT_EQ(send(bag, press(Action::back)), Event::changed);
    EXPECT_EQ(bag.last_move().action, InventoryAction::returned);
    EXPECT_EQ(bag.slot_of(3), 1);
    EXPECT_EQ(bag.holding(), 0);
    EXPECT_EQ(send(bag, press(Action::back)), Event::cancelled);

    // A stack that cannot take everything leaves the rest in the hand.
    ASSERT_TRUE(bag.put(0, item(4, "Tonic", 4, 9, kTonic)));
    bag.set_focus(0);
    send(bag, press(Action::confirm));
    bag.set_focus(6);
    EXPECT_EQ(send(bag, press(Action::confirm)), Event::changed);
    EXPECT_EQ(bag.last_move().amount, 2);
    EXPECT_EQ(bag.find(2)->count, 9);
    EXPECT_EQ(bag.holding(), 4);
    EXPECT_EQ(bag.find(4)->count, 2);

    // The screen's own rule: nothing stacks, so the full hand swaps.
    bag.merge = [](const InventoryItem &, const InventoryItem &) { return 0; };
    EXPECT_EQ(send(bag, press(Action::confirm)), Event::changed);
    EXPECT_EQ(bag.last_move().action, InventoryAction::swapped);
    EXPECT_EQ(bag.slot_of(4), 6);
    EXPECT_EQ(bag.slot_of(2), 0);
    EXPECT_TRUE(bag.remove(4));
    EXPECT_FALSE(bag.remove(4));
}

TEST_F(ComponentsGame, InventoryEdgesFilterAndExits)
{
    InventoryGrid bag;
    fill(bag);
    EXPECT_EQ(send(bag, nav(Direction::left)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    bag.set_focus(3);
    EXPECT_EQ(send(bag, press(Action::confirm)), Event::refused); // an empty slot

    bag.style.wrap = true;
    EXPECT_EQ(send(bag, nav(Direction::right)), Event::moved);
    EXPECT_EQ(bag.focus(), 0);
    bag.style.wrap = false;

    // The filter fades other categories and they cannot be lifted.
    bag.set_filter(1);
    EXPECT_EQ(send(bag, press(Action::confirm)), Event::refused);
    bag.set_focus(2);
    EXPECT_EQ(send(bag, press(Action::confirm)), Event::changed);
    EXPECT_EQ(bag.holding(), 3);

    // The hand never leaves the bag with something in it.
    bag.style.exits.up = true;
    EXPECT_EQ(send(bag, nav(Direction::up)), Event::refused);
    send(bag, press(Action::back));
    EXPECT_EQ(send(bag, nav(Direction::up)), Event::none);
    EXPECT_EQ(bag.exit(), Direction::up);

    const Rect first = bag.slot_rect(0);
    const Rect last = bag.slot_rect(7);
    EXPECT_GE(first.x, 100.0f - 0.5f);
    EXPECT_LE(last.x + last.w, 560.0f + 0.5f);
    EXPECT_LE(last.y + last.h, 324.0f + 0.5f);
    EXPECT_FLOAT_EQ(first.w, first.h);

    bag.clear();
    EXPECT_EQ(bag.item_count(), 0);
}

TEST_F(ComponentsGame, HealthBarGhostLagsBehindAHit)
{
    HealthBar bar;
    bar.set_value(100.0f, true);
    EXPECT_FLOAT_EQ(bar.shown(), 1.0f);
    bar.set_value(60.0f);
    settle(bar, 0.2f);
    // The fill is already down; the ghost still shows what was lost.
    EXPECT_LT(bar.shown(), 0.65f);
    EXPECT_GT(bar.ghost(), 0.95f);
    settle(bar, 4.0f);
    EXPECT_NEAR(bar.ghost(), 0.6f, 0.01f);

    // Healing grows the fill; the ghost never trails below it.
    bar.set_value(90.0f);
    for (int i = 0; i < 120; ++i)
    {
        bar.update(kFrame);
        EXPECT_GE(bar.ghost(), bar.shown() - 1e-4f);
    }
    EXPECT_NEAR(bar.shown(), 0.9f, 0.01f);

    EXPECT_FALSE(bar.low());
    bar.set_value(20.0f);
    EXPECT_TRUE(bar.low());
    bar.set_value(-5.0f);
    EXPECT_FLOAT_EQ(bar.value(), 0.0f);
    bar.set_max(200.0f);
    bar.set_value(500.0f, true);
    EXPECT_FLOAT_EQ(bar.value(), 200.0f);
    EXPECT_FLOAT_EQ(bar.ghost(), 1.0f);
    bar.set_shield(50.0f);
    EXPECT_FLOAT_EQ(bar.shield(), 50.0f);

    // Without the ghost the two are the same bar.
    bar.style.ghost = false;
    bar.set_value(100.0f);
    settle(bar, 0.2f);
    EXPECT_FLOAT_EQ(bar.ghost(), bar.shown());
}

TEST_F(ComponentsGame, AmmoCounterWarnsWhenLowAndEmpty)
{
    AmmoCounter ammo;
    ammo.set_capacity(10);
    ammo.set_ammo(10, 30);
    EXPECT_EQ(ammo.level(), hui::ui::Status::neutral);
    ammo.set_ammo(2, 30);
    EXPECT_EQ(ammo.level(), hui::ui::Status::warning);
    ammo.set_ammo(0, 30);
    EXPECT_EQ(ammo.level(), hui::ui::Status::danger);
    EXPECT_FALSE(ammo.reloading());
    ammo.set_reload(0.5f);
    EXPECT_TRUE(ammo.reloading());
    ammo.set_reload(-1.0f);
    EXPECT_FALSE(ammo.reloading());
    ammo.set_ammo(-3, -1);
    EXPECT_EQ(ammo.current(), 0);
    EXPECT_EQ(ammo.reserve(), 0);
    ammo.set_capacity(0);
    EXPECT_EQ(ammo.capacity(), 1);
}

TEST_F(ComponentsGame, ObjectivesTickAndLeave)
{
    ObjectiveTracker goals;
    goals.style.linger = 0.3f;
    goals.style.tick_time = 0.1f;
    goals.title = "The Lantern Pass";
    const int tower = goals.add("Reach the signal tower", "240 m");
    const int beacon = goals.add("Light the beacon");
    goals.add("Hold until dawn", "2:00", &feedback_);
    EXPECT_TRUE(feedback_.cues.empty()); // adding is silent unless a cue is named
    EXPECT_EQ(goals.count(), 3);
    EXPECT_EQ(goals.remaining(), 3);

    feedback_.clear();
    EXPECT_TRUE(goals.complete(beacon, &feedback_));
    EXPECT_TRUE(asked(Cue::mark));
    EXPECT_FALSE(goals.complete(beacon, &feedback_));
    EXPECT_FALSE(goals.complete(999));
    EXPECT_TRUE(goals.done(beacon));
    EXPECT_FALSE(goals.done(tower));
    EXPECT_EQ(goals.remaining(), 2);
    EXPECT_EQ(goals.count(), 3); // it lingers, ticked
    settle(goals, 2.0f);
    EXPECT_EQ(goals.count(), 2); // then leaves

    goals.set_distance(tower, "20 m");
    goals.style.linger = -1.0f; // a ticked objective now stays
    EXPECT_TRUE(goals.complete(tower));
    settle(goals, 2.0f);
    EXPECT_EQ(goals.count(), 2);
    goals.clear(false);
    EXPECT_EQ(goals.count(), 0);
}

TEST_F(ComponentsGame, MinimapTurnsTheShortWayAndKeepsPipsInside)
{
    MinimapFrame map;
    map.set_bounds({0.0f, 0.0f, 200.0f, 200.0f});
    map.set_pips({{0.0f, 1.0f}, {1.0f, 3.0f}});
    map.set_heading(0.2f, true);
    // 6.2 radians is just left of north: the pip due north must not swing
    // through the bottom of the frame to get there.
    map.set_heading(6.2f);
    EXPECT_NEAR(map.heading(), 6.2f, 1e-3f);
    for (int i = 0; i < 90; ++i)
    {
        map.update(kFrame);
        float x = 0.0f, y = 0.0f;
        map.pip_position(map.pips()[0], &x, &y);
        EXPECT_LT(y, 100.0f);
    }
    float x = 0.0f, y = 0.0f;
    map.pip_position(map.pips()[0], &x, &y);
    EXPECT_GT(x, 100.0f); // the map turned left, so north is now to the right

    // A pip beyond the range sits on the rim, not outside it.
    const Rect inner = map.inner();
    map.pip_position(map.pips()[1], &x, &y);
    EXPECT_LE(std::hypot(x - 100.0f, y - 100.0f), inner.w * 0.5f + 0.5f);
    map.style.shape = hui::ui::MinimapShape::square;
    map.pip_position(map.pips()[1], &x, &y);
    EXPECT_GE(x, inner.x - 0.5f);
    EXPECT_LE(x, inner.x + inner.w + 0.5f);
    EXPECT_LE(y, inner.y + inner.h + 0.5f);

    // North up: the heading turns the arrow, not the map.
    map.style.rotate = false;
    map.pip_position(map.pips()[0], &x, &y);
    EXPECT_NEAR(x, 100.0f, 0.01f);
    EXPECT_LT(y, 100.0f);
}

TEST_F(ComponentsGame, NodeMapFollowsDirectionsAndLines)
{
    NodeMap tree;
    plant(tree);
    // Right of the root: both children are linked; the one nearer the axis wins.
    EXPECT_EQ(tree.neighbour(0, Direction::right), 1);
    EXPECT_EQ(tree.neighbour(0, Direction::left), -1);
    EXPECT_EQ(tree.neighbour(1, Direction::down), 2);
    EXPECT_TRUE(tree.linked(1, 0));
    EXPECT_FALSE(tree.linked(2, 3));

    EXPECT_EQ(send(tree, nav(Direction::left)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    tree.style.exits.left = true;
    EXPECT_EQ(send(tree, nav(Direction::left)), Event::none);
    EXPECT_EQ(tree.exit(), Direction::left);

    EXPECT_EQ(send(tree, nav(Direction::right)), Event::moved);
    EXPECT_TRUE(asked(Cue::focus));
    EXPECT_EQ(tree.focus(), 1);
    // An available node reports `activated`: the screen decides to unlock.
    EXPECT_EQ(send(tree, press(Action::confirm)), Event::activated);
    EXPECT_EQ(tree.nodes()[1].state, NodeState::available);
    tree.set_state(1, NodeState::done);
    EXPECT_EQ(tree.nodes()[1].state, NodeState::done);

    // A locked node refuses, unless the screen wants to hear about it.
    tree.set_focus(2);
    EXPECT_EQ(send(tree, press(Action::confirm)), Event::refused);
    tree.style.activate_locked = true;
    EXPECT_EQ(send(tree, press(Action::confirm)), Event::activated);
    EXPECT_EQ(send(tree, press(Action::back)), Event::cancelled);

    // The camera pans until a far node is inside the bounds.
    tree.set_focus(3);
    EXPECT_EQ(send(tree, nav(Direction::right)), Event::moved);
    EXPECT_EQ(tree.focus(), 4);
    settle(tree, 4.0f);
    const Rect far = tree.node_rect(4);
    EXPECT_GE(far.x, 100.0f);
    EXPECT_LE(far.x + far.w, 700.0f);
    // ... and back again.
    tree.set_focus(0, false);
    settle(tree, 4.0f);
    EXPECT_GE(tree.node_rect(0).x, 100.0f);

    // Links to nodes that do not exist are dropped.
    tree.set_nodes(tree.nodes(), {MapLink{0, 1}, MapLink{0, 42}, MapLink{2, 2}});
    EXPECT_EQ(tree.links().size(), 1u);
}

TEST_F(ComponentsGame, RestylingKeepsState)
{
    PauseMenu pause;
    pause.set_items(menu());
    pause.open(feedback_);
    pause.set_focus(3);
    ProfilePicker who;
    who.set_profiles(people());
    who.set_focus(2);
    InventoryGrid bag;
    fill(bag);
    bag.set_focus(1);
    bag.handle(press(Action::confirm), feedback_);
    NodeMap tree;
    plant(tree);
    tree.set_focus(3);
    HealthBar bar;
    bar.set_value(40.0f);
    for (const hui::ui::Theme &theme : hui::ui::themes())
    {
        pause.style.theme = theme;
        who.style.theme = theme;
        bag.style.theme = theme;
        tree.style.theme = theme;
        bar.style.theme = theme;
        pause.update(kFrame);
        who.update(kFrame);
        bag.update(kFrame);
        tree.update(kFrame);
        bar.update(kFrame);
        EXPECT_TRUE(pause.is_open()) << theme.id;
        EXPECT_EQ(pause.focus(), 3) << theme.id;
        EXPECT_EQ(who.focus(), 2) << theme.id;
        EXPECT_EQ(bag.holding(), 2) << theme.id;
        EXPECT_EQ(tree.focus(), 3) << theme.id;
        EXPECT_FLOAT_EQ(bar.value(), 40.0f) << theme.id;
    }
}

TEST_F(ComponentsGame, DrawInEveryThemeAndVariant)
{
    const PopupAnchor anchors[] = {PopupAnchor::top_left,      PopupAnchor::top_center,
                                   PopupAnchor::top_right,     PopupAnchor::bottom_left,
                                   PopupAnchor::bottom_center, PopupAnchor::bottom_right};
    const UnlockTier tiers[] = {UnlockTier::bronze, UnlockTier::silver, UnlockTier::gold,
                                UnlockTier::special};
    const PauseLayout layouts[] = {PauseLayout::left, PauseLayout::center, PauseLayout::right};
    const hui::ui::HudBacking backings[] = {hui::ui::HudBacking::glow, hui::ui::HudBacking::plate,
                                            hui::ui::HudBacking::none};
    int round = 0;
    for (const hui::ui::Theme &theme : hui::ui::themes())
    {
        for (int variant = 0; variant < 3; ++variant, ++round)
        {
            const bool odd = (round & 1) != 0;
            hui::ui::Canvas target = canvas();

            PauseMenu pause;
            pause.style.theme = theme;
            pause.style.layout = layouts[variant];
            pause.style.reduced_motion = odd;
            pause.style.dividers = odd;
            pause.title = "A title far too long for a narrow pause menu panel to hold";
            pause.subtitle = "Chapter 3, saved four minutes ago";
            pause.set_items(menu());
            if (variant != 1)
                pause.side = [](hui::ui::Canvas &, const Rect &, float) {};
            pause.open(feedback_);
            settle(pause, 0.05f); // half way in, then settled: both must draw
            pause.draw(target);
            settle(pause);
            pause.draw(target);

            UnlockPopup popup;
            popup.style.theme = theme;
            popup.style.anchor = anchors[round % 6];
            popup.style.reduced_motion = odd;
            popup.style.sparks = !odd;
            Unlock unlock;
            unlock.title = "A very long achievement title that cannot fit the plate";
            unlock.subtitle = "And a subtitle that is longer still than the title above it";
            unlock.tier = tiers[round % 4];
            unlock.points = variant == 0 ? 0 : 150;
            unlock.icon = variant == 2;
            if (variant == 2)
                popup.icon = [](hui::ui::Canvas &canvas, const Rect &area, const Unlock &, float)
                { canvas.list.rounded_rect(area, 8.0f, hui::gfx::Color::rgb(0x336699)); };
            popup.push(unlock);
            run(popup, 0.25f);
            popup.draw(target);
            run(popup, 0.5f);
            popup.draw(target);

            ProfilePicker who;
            who.style.theme = theme;
            who.style.columns = variant == 1 ? 2 : 0;
            who.style.add_card = variant != 2;
            who.style.avatar_shape =
                odd ? hui::ui::AvatarShape::rounded : hui::ui::AvatarShape::circle;
            who.style.reduced_motion = odd;
            who.set_profiles(people());
            who.set_bounds({96.0f, 300.0f, variant == 2 ? 500.0f : 1728.0f, 620.0f});
            who.set_focus(round % who.count());
            settle(who, 0.3f);
            who.draw(target);

            InventoryGrid bag;
            bag.style.theme = theme;
            fill(bag);
            bag.style.reduced_motion = odd;
            bag.style.rarity_frame = variant != 2;
            if (variant == 1)
                bag.set_filter(1);
            bag.handle(press(Action::confirm), feedback_);
            bag.handle(nav(Direction::right), feedback_);
            settle(bag, 0.05f); // caught while the item travels and leans
            bag.draw(target);
            bag.handle(press(Action::confirm), feedback_);
            settle(bag, 0.1f);
            bag.draw(target);

            NodeMap tree;
            tree.style.theme = theme;
            tree.style.zoom = variant == 1 ? 0.7f : 1.0f;
            tree.style.labels = variant != 2;
            tree.style.reduced_motion = odd;
            plant(tree);
            tree.set_focus(round % 5);
            tree.set_state(2, NodeState::done);
            settle(tree, 0.2f);
            tree.draw(target);

            HealthBar bar;
            bar.style.theme = theme;
            bar.style.backing = backings[variant];
            bar.style.kind = odd ? hui::ui::HealthKind::segmented : hui::ui::HealthKind::continuous;
            bar.style.reduced_motion = variant == 1;
            bar.title = "Warden";
            bar.set_shield(30.0f, true);
            bar.set_value(variant == 2 ? 12.0f : 55.0f);
            settle(bar, 0.1f);
            bar.draw(target);

            AmmoCounter ammo;
            ammo.style.theme = theme;
            ammo.style.backing = backings[variant];
            ammo.style.pips = odd;
            ammo.style.ring = variant != 2;
            ammo.style.label = "Crossbow";
            ammo.set_capacity(12);
            ammo.set_ammo(variant * 5, 48);
            ammo.set_reload(variant == 1 ? 0.4f : -1.0f);
            settle(ammo, 0.1f);
            ammo.draw(target);

            ObjectiveTracker goals;
            goals.style.theme = theme;
            goals.style.backing = backings[variant];
            goals.style.max_rows = 2 + variant;
            goals.style.reduced_motion = odd;
            goals.title = "The Lantern Pass";
            const int first = goals.add("An objective with a text too long for its row", "240 m");
            goals.add("Light the beacon");
            goals.add("Hold until dawn", "2:00");
            goals.add("One more than fits");
            goals.complete(first);
            settle(goals, 0.25f);
            goals.draw(target);

            MinimapFrame map;
            map.style.theme = theme;
            map.style.backing = backings[variant];
            map.style.shape = odd ? hui::ui::MinimapShape::square : hui::ui::MinimapShape::round;
            map.style.rotate = variant != 1;
            map.set_pips({{0.4f, 0.5f}, {2.0f, 1.6f, hui::gfx::Color::rgb(0xff0000), true}});
            map.set_heading(1.2f);
            settle(map, 0.1f);
            map.draw(target);
            if (variant == 2)
            {
                map.content =
                    [](hui::ui::Canvas &canvas, const Rect &inner, float, hui::ui::MinimapShape)
                { canvas.list.rounded_rect(inner, 0.0f, hui::gfx::Color::rgb(0x224422)); };
                map.draw(target);
            }

            expect_drawn(theme.id);
        }
    }
}

} // namespace
