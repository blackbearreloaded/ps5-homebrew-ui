// ps5-homebrew-ui - Behaviour tests for the Field HUD design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The HUD's rules are checked through the cues it asks for: what a hit, a
// heal, an ability or a menu move sounds like tells which rule fired.

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;

class Hud : public hui::testing::ConceptFixture
{
  protected:
    // Takes hits until the player goes down; returns how many it took.
    int hit_until_down()
    {
        for (int hits = 1; hits <= 20; ++hits)
        {
            send(*design_, press(Action::confirm), 0.05f);
            if (played(Cue::game_over))
                return hits;
        }
        return 0;
    }

    void open_pause()
    {
        send(*design_, press(Action::menu));
        ASSERT_TRUE(played(Cue::modal_open));
    }

    std::unique_ptr<hui::app::Concept> design_ = hui::concepts::make_hud(context_);
};

TEST_F(Hud, AHitLandsAHealRestoresAndFullHealthRefusesTheHeal)
{
    // Nothing to heal yet: the press is refused, softly.
    send(*design_, press(Action::west));
    EXPECT_TRUE(played(Cue::invalid));
    EXPECT_FALSE(played(Cue::reveal));

    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::explode));
    send(*design_, press(Action::west));
    EXPECT_TRUE(played(Cue::reveal));
    // One heal covers one hit (at most 20 against 25), so the bar is full again.
    send(*design_, press(Action::west));
    EXPECT_TRUE(played(Cue::invalid));
    expect_drawable(*design_);
}

TEST_F(Hud, EnoughHitsTakeThePlayerDownAndTheReviveIsAutomatic)
{
    // 8 to 20 a hit: never down before the fifth, always by the thirteenth.
    const int hits = hit_until_down();
    EXPECT_GE(hits, 5);
    EXPECT_LE(hits, 13);
    expect_drawable(*design_);

    // While down, the game buttons answer but do nothing.
    send(*design_, press(Action::confirm), 0.05f);
    EXPECT_TRUE(played(Cue::invalid));
    EXPECT_FALSE(played(Cue::explode));
    send(*design_, press(Action::north), 0.05f);
    EXPECT_TRUE(played(Cue::invalid));
    EXPECT_FALSE(played(Cue::mark));

    // Two seconds later the player is back at full health.
    send(*design_, idle(), 2.5f);
    send(*design_, press(Action::west));
    EXPECT_TRUE(played(Cue::invalid)); // nothing to heal
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::explode));
}

TEST_F(Hud, ObjectiveLinesTickAndTheLastOneCompletesTheCard)
{
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::mark));
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::mark));
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::complete));
    EXPECT_FALSE(played(Cue::mark));
    // The finished card is waiting to hand over: a press skips the wait...
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::slide));
    // ... and the next objective starts from its first line.
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::mark));
    expect_drawable(*design_);
}

TEST_F(Hud, AnAbilityOnCooldownRefusesAndAHeldDirectionStaysQuiet)
{
    send(*design_, nav(Direction::up)); // Dash: three seconds
    EXPECT_TRUE(played(Cue::slide));
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::invalid));
    EXPECT_FALSE(played(Cue::slide));

    hui::InputFrame held = nav(Direction::up);
    held.nav_repeat = true;
    send(*design_, held);
    EXPECT_FALSE(played(Cue::invalid));
    EXPECT_FALSE(played(Cue::slide));

    // Circle is the second tile; it has its own cooldown.
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::spawn));

    send(*design_, idle(), 2.0f);
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::slide));
}

TEST_F(Hud, TheStickSteersAndHoldingL2PutsTheAbilitiesOnTheFaceButtons)
{
    // A direction that came from the left stick is not a D-pad press.
    hui::InputFrame stick = nav(Direction::right);
    stick.stick_x = 1.0f;
    send(*design_, stick);
    EXPECT_FALSE(played(Cue::spawn));

    // With L2 held, Cross is the third tile instead of "take a hit".
    hui::InputFrame palette = press(Action::confirm);
    palette.held |= hui::action_bit(Action::jump_prev);
    send(*design_, palette);
    EXPECT_TRUE(played(Cue::connect));
    EXPECT_FALSE(played(Cue::explode));
}

TEST_F(Hud, PausingFreezesCooldownsAndALostControllerPauses)
{
    // The system took the controller away: the game stops for the player.
    hui::InputFrame lost = idle();
    lost.focus_lost = true;
    send(*design_, lost);
    EXPECT_TRUE(played(Cue::modal_open));
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::modal_close));

    send(*design_, nav(Direction::up)); // Dash: three seconds
    EXPECT_TRUE(played(Cue::slide));
    open_pause();
    send(*design_, idle(), 6.0f);
    send(*design_, press(Action::menu));
    EXPECT_TRUE(played(Cue::modal_close));
    // Six seconds on the wall clock, none in the game: still cooling down.
    send(*design_, nav(Direction::up), 0.1f);
    EXPECT_TRUE(played(Cue::invalid));
    // Unpaused, the rest of the cooldown runs out.
    send(*design_, idle(), 2.5f);
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::slide));
}

TEST_F(Hud, ThePauseMenuTakesTheInputAndItsEndsRefuse)
{
    open_pause();
    // The game does not see the buttons while the menu is up.
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::slide));
    hui::InputFrame held = nav(Direction::up);
    held.nav_repeat = true;
    send(*design_, held);
    EXPECT_TRUE(last_cues_.empty());

    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::focus));
    // Inventory is not part of the demo: refused, with the reason on screen.
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::error));
    expect_drawable(*design_);

    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::modal_close));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::explode));
}

TEST_F(Hud, QuitAsksFirstFocusesCancelAndOnlyQuitStartsAFreshSession)
{
    send(*design_, press(Action::confirm)); // take a hit: health is no longer full
    open_pause();
    for (int i = 0; i < 4; ++i)
        send(*design_, nav(Direction::down), 0.1f);
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::modal_open));
    expect_drawable(*design_);

    // The dialog opens on Cancel: confirming leaves the menu up, nothing reset.
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::modal_close));
    EXPECT_FALSE(played(Cue::restart));
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::error)); // still in the menu, on its last row

    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::modal_open));
    send(*design_, nav(Direction::left));
    EXPECT_TRUE(played(Cue::error)); // Cancel is the left end
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::restart));

    // Back in the game with a fresh session: health is full again.
    send(*design_, press(Action::west));
    EXPECT_TRUE(played(Cue::invalid));
    expect_drawable(*design_);
}

} // namespace
