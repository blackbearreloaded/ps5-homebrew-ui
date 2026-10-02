// ps5-homebrew-ui - Behaviour tests for the Satchel design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The bag's rules, checked through what it asks to hear. Every rule has its
// own cue (pickup, drop, rotate for a swap, merge, place for a split, connect
// for an equip, reveal for a use, flip for the last use, invalid for a
// refusal), and two of them carry numbers: `merge` is pitched by the size of
// the resulting stack and `connect` by the loadout's attack plus defence.
//
// The bag opens on its first cell. The cells the tests rely on:
//
//    0 Tidewater Sabre   1 Redcap Tonic x5   2 helm   3 Iron Ore x14
//    4 Emberfall Greatsword   5 empty   6 Waybread x8
//    9 Redcap Tonic x3   10 Scalemail   13 Firebloom Flask x1
//
// The character starts with attack 15 and defence 32.

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;
using hui::audio::CueEvent;

class Inventory : public hui::testing::ConceptFixture
{
  protected:
    // The last send()'s request for a cue, or nullptr.
    const CueEvent *heard(Cue cue) const
    {
        for (const CueEvent &event : last_cues_)
        {
            if (event.cue == cue)
                return &event;
        }
        return nullptr;
    }

    // Moves the focus; every step must succeed, with the sound of looking
    // (`focus`) or of carrying (`slide`).
    void go(Direction direction, int steps = 1)
    {
        for (int i = 0; i < steps; ++i)
        {
            send(*design_, nav(direction), 0.1f);
            ASSERT_TRUE(played(Cue::focus) || played(Cue::slide)) << "step " << i;
        }
    }

    // Triangle on a stack until it is gone; returns how many it held.
    int use_up()
    {
        for (int uses = 1; uses <= 30; ++uses)
        {
            send(*design_, press(Action::north), 0.1f);
            if (played(Cue::flip))
                return uses;
            if (!played(Cue::reveal))
                return -1;
        }
        return -1;
    }

    // What `connect` sounds like for a loadout of this attack plus defence.
    static float power_pitch(int power)
    {
        return 0.8f + static_cast<float>(power) / 250.0f;
    }

    std::unique_ptr<hui::app::Concept> design_ = hui::concepts::make_inventory(context_);
};

TEST_F(Inventory, PickUpAndPlaceMovesAnItemAndCancelReturnsIt)
{
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::pickup));
    go(Direction::right, 5);
    EXPECT_TRUE(played(Cue::slide)); // carrying sounds different from looking
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::drop));

    // The sabre now lives in the sixth cell: it can be lifted from there, and
    // cancelling puts it back there, not in the cell it started in.
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::pickup));
    go(Direction::down);
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::back));
    go(Direction::up);
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::pickup));
    send(*design_, press(Action::back));

    // Its first cell is empty: nothing to lift.
    go(Direction::left, 5);
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::invalid));
    EXPECT_FALSE(played(Cue::pickup));
    expect_drawable(*design_);
}

TEST_F(Inventory, PlacingOnAnotherItemSwapsTheTwo)
{
    send(*design_, press(Action::confirm));
    go(Direction::right);
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::rotate));
    EXPECT_FALSE(played(Cue::drop));

    // The sabre is under the focus now: Triangle equips it (attack 22).
    send(*design_, press(Action::north));
    const CueEvent *equip = heard(Cue::connect);
    ASSERT_NE(equip, nullptr);
    EXPECT_NEAR(equip->pitch, power_pitch(22 + 32), 1e-4f);
    // ... and the tonics travelled to the first cell: Triangle drinks one.
    go(Direction::left);
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::reveal));
}

TEST_F(Inventory, MergingStacksSumsTheCounts)
{
    go(Direction::right);
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::pickup));
    go(Direction::down);
    send(*design_, press(Action::confirm));
    const CueEvent *merge = heard(Cue::merge);
    ASSERT_NE(merge, nullptr);
    // Five and three: the pitch says eight, and eight uses empty the stack.
    EXPECT_NEAR(merge->pitch, 0.8f + 0.02f * 8.0f, 1e-4f);
    EXPECT_EQ(use_up(), 8);
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::invalid));

    // The cell the five came from is empty.
    go(Direction::up);
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::invalid));
    expect_drawable(*design_);
}

TEST_F(Inventory, SplittingHalvesAStackIntoTheNearestEmptySlot)
{
    go(Direction::right, 6);
    send(*design_, press(Action::west));
    EXPECT_TRUE(played(Cue::place));
    // Eight rations became four here and four in the empty cell to the left.
    EXPECT_EQ(use_up(), 4);
    go(Direction::left);
    EXPECT_EQ(use_up(), 4);

    // A single item and a stack of one cannot be split.
    go(Direction::left);
    send(*design_, press(Action::west));
    EXPECT_TRUE(played(Cue::invalid));
    EXPECT_FALSE(played(Cue::place));
    go(Direction::down);
    go(Direction::right);
    send(*design_, press(Action::west));
    EXPECT_TRUE(played(Cue::invalid));
}

TEST_F(Inventory, EquippingChangesTheTotals)
{
    // Carry the sabre to the main hand: attack 15 becomes 22.
    send(*design_, press(Action::confirm));
    go(Direction::left);
    send(*design_, press(Action::confirm));
    const CueEvent *sabre = heard(Cue::connect);
    ASSERT_NE(sabre, nullptr);
    EXPECT_NEAR(sabre->pitch, power_pitch(22 + 32), 1e-4f);

    // The blade that was worn went to the sabre's cell; Triangle swaps back.
    go(Direction::right);
    send(*design_, press(Action::north));
    const CueEvent *blade = heard(Cue::connect);
    ASSERT_NE(blade, nullptr);
    EXPECT_NEAR(blade->pitch, power_pitch(15 + 32), 1e-4f);

    // The scalemail replaces the gambeson: defence 32 - 8 + 36.
    go(Direction::right, 2);
    go(Direction::down);
    send(*design_, press(Action::north));
    const CueEvent *mail = heard(Cue::connect);
    ASSERT_NE(mail, nullptr);
    EXPECT_NEAR(mail->pitch, power_pitch(15 + 60), 1e-4f);
    expect_drawable(*design_);
}

TEST_F(Inventory, AWeaponOnTheHeadSlotIsRefusedAndNothingMoves)
{
    send(*design_, press(Action::confirm));
    go(Direction::left, 2); // main hand, then across the figure to the head
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::invalid));
    EXPECT_FALSE(played(Cue::connect));
    expect_drawable(*design_);

    // The sabre is still in the hand and the old blade still worn: equipping
    // it now gives exactly the totals of a first equip.
    go(Direction::right);
    send(*design_, press(Action::confirm));
    const CueEvent *equip = heard(Cue::connect);
    ASSERT_NE(equip, nullptr);
    EXPECT_NEAR(equip->pitch, power_pitch(22 + 32), 1e-4f);
    // The hood never left the head: Triangle takes it off into the bag.
    go(Direction::left);
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::drop));
}

TEST_F(Inventory, UsingAConsumableCountsDownAndTheLastOneRemovesIt)
{
    go(Direction::right);
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::reveal));
    EXPECT_EQ(use_up(), 4); // five tonics, one already gone

    // A stack of one goes at once.
    go(Direction::down);
    go(Direction::right, 4);
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::flip));
    EXPECT_FALSE(played(Cue::reveal));
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::invalid));

    // Materials are not for the road.
    go(Direction::up);
    go(Direction::left, 2);
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::invalid));
    expect_drawable(*design_);
}

TEST_F(Inventory, CategoryTabsFilterAndTheSortMenuReordersTheBag)
{
    // In "All" the second cell holds tonics; in "Weapons" the greatsword.
    send(*design_, press(Action::jump_next));
    EXPECT_TRUE(played(Cue::tab));
    go(Direction::right);
    send(*design_, press(Action::north));
    const CueEvent *equip = heard(Cue::connect);
    ASSERT_NE(equip, nullptr);
    EXPECT_NEAR(equip->pitch, power_pitch(34 + 32), 1e-4f);
    // Six weapons are shown, packed: the seventh cell is empty.
    go(Direction::right, 5);
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::invalid));

    send(*design_, press(Action::jump_prev));
    EXPECT_TRUE(played(Cue::tab));
    send(*design_, press(Action::jump_prev));
    EXPECT_TRUE(played(Cue::error)); // no tab before "All"
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::reveal)); // back in "All", that cell holds rations

    // Sort by rarity: the menu takes the input, then the bag is reordered.
    send(*design_, press(Action::menu));
    EXPECT_TRUE(played(Cue::modal_open));
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, press(Action::confirm), 1.5f);
    EXPECT_TRUE(played(Cue::cascade));
    // The scalemail, the only legendary left in the bag, leads it now.
    go(Direction::left, 6);
    send(*design_, press(Action::north));
    const CueEvent *mail = heard(Cue::connect);
    ASSERT_NE(mail, nullptr);
    EXPECT_NEAR(mail->pitch, power_pitch(34 + 60), 1e-4f);
    expect_drawable(*design_);
}

TEST_F(Inventory, EdgesRefuseSoftlyAndAHeldDirectionStaysQuiet)
{
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::error));
    hui::InputFrame held = nav(Direction::up);
    held.nav_repeat = true;
    send(*design_, held);
    EXPECT_TRUE(last_cues_.empty());

    // Left from the first column is not an edge: it leads to the equipment.
    send(*design_, nav(Direction::left));
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, nav(Direction::left));
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, nav(Direction::left));
    EXPECT_TRUE(played(Cue::error));

    // Sorting with an item in the hand is refused.
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::pickup));
    send(*design_, press(Action::menu));
    EXPECT_TRUE(played(Cue::invalid));
    EXPECT_FALSE(played(Cue::modal_open));
}

} // namespace
