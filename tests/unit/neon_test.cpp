// ps5-homebrew-ui - Behaviour tests for the Neon Arcade design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The rules of the main menu, its track selector and its quit dialog, pinned
// down through the sounds each input asks for.

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;

class Neon : public hui::testing::ConceptFixture
{
  protected:
    // The first event of the last send() that asked for this cue.
    const hui::audio::CueEvent *event(Cue cue) const
    {
        for (const hui::audio::CueEvent &e : last_cues_)
        {
            if (e.cue == cue)
                return &e;
        }
        return nullptr;
    }

    std::unique_ptr<hui::app::Concept> design_ = hui::concepts::make_neon(context_);
};

TEST_F(Neon, MovingDownTheMenuDescendsAScaleAndTheEndsRefuse)
{
    // Six entries: five steps down, each lower than the one before, each
    // panned to the left where the menu is.
    float last = 2.0f;
    for (int i = 0; i < 5; ++i)
    {
        send(*design_, nav(Direction::down));
        const hui::audio::CueEvent *focus = event(Cue::focus);
        ASSERT_NE(focus, nullptr) << "step " << i;
        EXPECT_LT(focus->pitch, last) << "step " << i;
        EXPECT_GE(focus->pitch, 0.7f);
        EXPECT_LT(focus->pan, 0.0f);
        last = focus->pitch;
    }
    // No wrap-around: the last entry refuses, softly.
    send(*design_, nav(Direction::down));
    EXPECT_FALSE(played(Cue::focus));
    const hui::audio::CueEvent *error = event(Cue::error);
    ASSERT_NE(error, nullptr);
    EXPECT_LT(error->gain, 1.0f);
    // Going back up climbs again.
    send(*design_, nav(Direction::up));
    ASSERT_NE(event(Cue::focus), nullptr);
    EXPECT_GT(event(Cue::focus)->pitch, last);
}

TEST_F(Neon, AHeldDirectionAtTheEndStaysQuiet)
{
    hui::InputFrame held = nav(Direction::up);
    held.nav_repeat = true;
    send(*design_, held);
    EXPECT_TRUE(last_cues_.empty());
    // A fresh press at the same end is answered.
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::error));
}

TEST_F(Neon, OrdinaryEntriesOnlyConfirm)
{
    send(*design_, nav(Direction::down)); // CAREER
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::select));
    EXPECT_FALSE(played(Cue::open));
    EXPECT_FALSE(played(Cue::modal_open));
    // Nothing opened: the menu still moves.
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::focus));
    // Back has nowhere to go at the top level and says nothing.
    send(*design_, press(Action::back));
    EXPECT_TRUE(last_cues_.empty());
}

TEST_F(Neon, PlayOpensTheTrackSelectorAndBackReturns)
{
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::open));
    // The menu is gone: up and down do nothing, left is the selector's edge.
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(last_cues_.empty());
    send(*design_, nav(Direction::left));
    EXPECT_TRUE(played(Cue::error));
    // Five tracks: four steps right, each tick further right in the stereo field.
    float last = -1.0f;
    for (int i = 0; i < 4; ++i)
    {
        send(*design_, nav(Direction::right));
        ASSERT_NE(event(Cue::focus), nullptr) << "step " << i;
        EXPECT_GT(event(Cue::focus)->pan, last);
        last = event(Cue::focus)->pan;
    }
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::error));
    expect_drawable(*design_);
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::back));
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::focus));
}

TEST_F(Neon, TheLaunchMomentOwnsTheScreenForItsSecond)
{
    send(*design_, press(Action::confirm));
    send(*design_, press(Action::confirm), 0.1f);
    EXPECT_TRUE(played(Cue::launch));
    expect_drawable(*design_);
    EXPECT_FALSE(frame_.overlay.empty()); // "GO" is drawn over everything
    // Presses during the flash are not acted on: no second launch, no move.
    send(*design_, press(Action::confirm), 0.1f);
    EXPECT_TRUE(last_cues_.empty());
    send(*design_, nav(Direction::right), 0.1f);
    EXPECT_TRUE(last_cues_.empty());
    send(*design_, press(Action::back), 1.2f);
    EXPECT_TRUE(last_cues_.empty());
    // Afterwards the selector is back, where it was.
    expect_drawable(*design_);
    EXPECT_TRUE(frame_.overlay.empty());
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::focus));
}

TEST_F(Neon, QuitAsksFirstAndNoIsTheDefault)
{
    for (int i = 0; i < 5; ++i)
        send(*design_, nav(Direction::down), 0.1f);
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::modal_open));
    EXPECT_FALSE(played(Cue::select)); // one sound per action
    expect_drawable(*design_);
    EXPECT_TRUE(frame_.glass); // the dialog is frosted glass
    // The dialog takes the input: the menu does not move under it.
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(last_cues_.empty());
    // "No" is focused: left is its edge, confirm closes without quitting.
    send(*design_, nav(Direction::left));
    EXPECT_TRUE(played(Cue::error));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::modal_close));
    // Still on QUIT: one step up is a menu move again.
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::focus));
}

TEST_F(Neon, TheDialogCanBeCancelledOrAnswered)
{
    for (int i = 0; i < 5; ++i)
        send(*design_, nav(Direction::down), 0.1f);
    send(*design_, press(Action::confirm));
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::back));
    // Open it again: it starts on "No" again, whatever was focused before.
    send(*design_, press(Action::confirm));
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, press(Action::back));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::modal_open));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::modal_close));
    // "Yes" restarts the title screen with PLAY focused: confirm enters the selector.
    send(*design_, press(Action::confirm));
    send(*design_, nav(Direction::right));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::select));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::open));
    expect_drawable(*design_);
}

TEST_F(Neon, ReducedMotionDrawsEveryState)
{
    settings_.reduced_motion = true;
    design_->enter();
    send(*design_, idle(), 0.05f);
    expect_drawable(*design_);
    for (int i = 0; i < 5; ++i)
    {
        send(*design_, nav(Direction::down), 0.05f); // mid cross-fade of each preview
        expect_drawable(*design_);
    }
    send(*design_, press(Action::confirm), 0.05f);
    expect_drawable(*design_);
    EXPECT_TRUE(frame_.glass);
    send(*design_, press(Action::back));
    for (int i = 0; i < 5; ++i)
        send(*design_, nav(Direction::up), 0.05f);
    send(*design_, press(Action::confirm), 0.05f);
    send(*design_, press(Action::confirm), 0.3f);
    EXPECT_TRUE(played(Cue::launch));
    expect_drawable(*design_);
}

} // namespace
