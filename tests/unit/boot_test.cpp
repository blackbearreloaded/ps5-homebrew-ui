// ps5-homebrew-ui - Behaviour tests for the Launch Sequence design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The sequence is a small state machine, and every screen answers the same
// button with a different cue. That makes the flow testable without looking
// at a single pixel: press, and listen to which screen replied.

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;

class Boot : public hui::testing::ConceptFixture
{
  protected:
    // Lets time pass with no input; true if the design asked for the cue.
    bool heard_within(Cue cue, float seconds)
    {
        bool heard = false;
        for (int frame = 0, frames = static_cast<int>(seconds / (1.0f / 60.0f)); frame < frames;
             ++frame)
        {
            feedback_.clear();
            design_->update(idle(), 1.0f / 60.0f, feedback_);
            for (const hui::audio::CueEvent &event : feedback_.cues)
                heard = heard || event.cue == cue;
        }
        return heard;
    }

    void to_title()
    {
        send(*design_, press(Action::west), 1.0f);
    }
    void to_profiles()
    {
        to_title();
        send(*design_, press(Action::confirm), 1.0f);
    }
    void to_loading()
    {
        to_profiles();
        send(*design_, press(Action::confirm), 0.2f);
    }

    std::unique_ptr<hui::app::Concept> design_ = hui::concepts::make_boot(context_);
};

TEST_F(Boot, TheSplashChimesAndLeavesByItselfOrOnAnyButton)
{
    EXPECT_TRUE(heard_within(Cue::welcome, 1.0f));
    // Left alone, the splash ends: the next press already belongs to the title.
    EXPECT_FALSE(heard_within(Cue::welcome, 3.0f));
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::select));

    // enter() starts over, and any button (even a direction) skips.
    design_->enter();
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::tab));
    EXPECT_FALSE(played(Cue::select));
    expect_drawable(*design_);
}

TEST_F(Boot, AnyButtonLeavesTheTitle)
{
    to_title();
    send(*design_, press(Action::menu));
    EXPECT_TRUE(played(Cue::select));
    // Now on the profiles: Circle goes back, and the title answers again.
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::back));
    send(*design_, press(Action::l3));
    EXPECT_TRUE(played(Cue::select));
}

TEST_F(Boot, ProfileFocusMovesAndTheEndsRefuse)
{
    to_profiles();
    send(*design_, nav(Direction::left));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::focus));
    // Four profiles and the "Add profile" card: four steps to the right.
    for (int i = 0; i < 4; ++i)
    {
        send(*design_, nav(Direction::right));
        EXPECT_TRUE(played(Cue::focus));
    }
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::error));
    // Leaning on the D-pad at the end stays quiet.
    hui::InputFrame held = nav(Direction::right);
    held.nav_repeat = true;
    send(*design_, held);
    EXPECT_TRUE(last_cues_.empty());
    expect_drawable(*design_);
}

TEST_F(Boot, FocusSoundsFollowTheCardAcrossTheScreen)
{
    to_profiles();
    float previous = -1.0f;
    for (int i = 0; i < 4; ++i)
    {
        send(*design_, nav(Direction::right));
        ASSERT_FALSE(last_cues_.empty());
        EXPECT_GT(last_cues_.front().pan, previous);
        previous = last_cues_.front().pan;
    }
}

TEST_F(Boot, ChoosingAProfileStartsLoading)
{
    to_profiles();
    send(*design_, press(Action::confirm), 0.2f);
    EXPECT_TRUE(played(Cue::open));
    // Left and right now step through the tips instead of the cards.
    send(*design_, nav(Direction::right), 0.2f);
    EXPECT_TRUE(played(Cue::tab));
    EXPECT_FALSE(played(Cue::focus));
    // Circle cancels back to the profiles.
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::back));
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::focus));
}

TEST_F(Boot, TheLoaderNeedsItsTimeAndThenAcceptsContinue)
{
    to_loading();
    // Too early: the press is refused, softly.
    send(*design_, press(Action::confirm), 0.2f);
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::launch));
    // Half-way through it is still not ready...
    EXPECT_FALSE(heard_within(Cue::resume, 3.0f));
    send(*design_, press(Action::confirm), 0.2f);
    EXPECT_FALSE(played(Cue::launch));
    // ... then it announces itself once, and the same press continues.
    EXPECT_TRUE(heard_within(Cue::resume, 6.0f));
    EXPECT_FALSE(heard_within(Cue::resume, 2.0f));
    send(*design_, press(Action::confirm), 0.2f);
    EXPECT_TRUE(played(Cue::launch));
    expect_drawable(*design_);
}

TEST_F(Boot, TipsStepByHandInBothDirections)
{
    to_loading();
    send(*design_, nav(Direction::right), 0.2f);
    ASSERT_TRUE(played(Cue::tab));
    const hui::audio::CueEvent forward = last_cues_.front();
    EXPECT_GT(forward.pan, 0.0f);
    send(*design_, nav(Direction::right), 0.2f);
    ASSERT_TRUE(played(Cue::tab));
    EXPECT_GT(last_cues_.front().pitch, forward.pitch); // the pitch names the tip
    send(*design_, nav(Direction::left), 0.2f);
    ASSERT_TRUE(played(Cue::tab));
    EXPECT_LT(last_cues_.front().pan, 0.0f);
    EXPECT_FLOAT_EQ(last_cues_.front().pitch, forward.pitch);
    // The tips are a ring: stepping left from the first one wraps, no refusal.
    send(*design_, nav(Direction::left), 0.2f);
    send(*design_, nav(Direction::left), 0.2f);
    EXPECT_TRUE(played(Cue::tab));
    EXPECT_FALSE(played(Cue::error));
}

TEST_F(Boot, OptionsReturnsFromTheGameToTheTitle)
{
    to_loading();
    ASSERT_TRUE(heard_within(Cue::resume, 9.0f));
    send(*design_, press(Action::confirm), 0.1f);
    ASSERT_TRUE(played(Cue::launch));
    // In the game the autosave toast reports in, and Cross does nothing.
    EXPECT_TRUE(heard_within(Cue::saved, 3.0f));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(last_cues_.empty());
    send(*design_, press(Action::menu), 1.2f);
    EXPECT_TRUE(played(Cue::back));
    // Back on the title: any button moves on to the profiles.
    send(*design_, press(Action::west));
    EXPECT_TRUE(played(Cue::select));
    expect_drawable(*design_);
}

} // namespace
