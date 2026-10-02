// ps5-homebrew-ui - Behaviour tests for the Constellation design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The star map's rules, checked through the only things a design exposes:
// the input it takes and the cues it asks for. (That every star can be
// reached at all is proven at compile time, by a static_assert next to the
// layout in constellation.cpp.)

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"

#include <initializer_list>

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;

class Constellation : public hui::testing::ConceptFixture
{
  protected:
    // Moves the focus along a path; every step must land on a star.
    void walk(std::initializer_list<Direction> path)
    {
        for (Direction direction : path)
        {
            send(*design_, nav(direction), 0.1f);
            ASSERT_TRUE(played(Cue::focus));
        }
    }

    // Walks a path and unlocks the star at its end.
    void unlock(std::initializer_list<Direction> path)
    {
        walk(path);
        send(*design_, press(Action::confirm), 0.1f);
    }

    float pitch_of(Cue cue) const
    {
        for (const hui::audio::CueEvent &event : last_cues_)
        {
            if (event.cue == cue)
                return event.pitch;
        }
        return 0.0f;
    }

    // Lights the whole Sentinel branch, which costs exactly the nine points
    // a new tree starts with. The last unlock is left in last_cues_.
    void complete_sentinel()
    {
        unlock({Direction::down});                    // Watchfire
        unlock({Direction::left});                    // Bulwark
        unlock({Direction::down});                    // Stone Skin
        unlock({Direction::right, Direction::right}); // Iron Vigil
        unlock({Direction::right});                   // Second Wind
        unlock({Direction::down});                    // Riposte
        ASSERT_TRUE(played(Cue::connect));
        unlock({Direction::left, Direction::left, Direction::down}); // Last Stand
    }

    std::unique_ptr<hui::app::Concept> design_ = hui::concepts::make_constellation(context_);
};

TEST_F(Constellation, TheFocusFollowsTheMapAndRefusesWhereNothingLies)
{
    // Nothing lies above the origin.
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::focus));
    // Left is the first star of a branch; right from there is the way back.
    send(*design_, nav(Direction::left));
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::error));
}

TEST_F(Constellation, AHeldDirectionAtAnEdgeStaysQuiet)
{
    hui::InputFrame held = nav(Direction::up);
    held.nav_repeat = true;
    send(*design_, held);
    EXPECT_TRUE(last_cues_.empty());
}

TEST_F(Constellation, UnlockingAlongABranchClimbsAScale)
{
    unlock({Direction::left});
    EXPECT_TRUE(played(Cue::connect));
    const float first = pitch_of(Cue::connect);
    unlock({Direction::up});
    EXPECT_TRUE(played(Cue::connect));
    const float second = pitch_of(Cue::connect);
    unlock({Direction::left});
    const float third = pitch_of(Cue::connect);
    EXPECT_GT(second, first);
    EXPECT_GT(third, second);
    // Pressing again on a lit star is acknowledged, but buys nothing.
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::tick));
    EXPECT_FALSE(played(Cue::connect));
    expect_drawable(*design_);
}

TEST_F(Constellation, ALockedStarRefusesUntilItsPrerequisiteIsLit)
{
    // Two steps left of the origin: its prerequisite is still dark.
    walk({Direction::left, Direction::left});
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::invalid));
    EXPECT_FALSE(played(Cue::connect));
    // Light the prerequisite, come back: now it opens.
    unlock({Direction::right});
    EXPECT_TRUE(played(Cue::connect));
    unlock({Direction::left});
    EXPECT_TRUE(played(Cue::connect));
}

TEST_F(Constellation, TheLastStarOfABranchPlaysTheFanfareAndThePointsRunOut)
{
    complete_sentinel();
    EXPECT_TRUE(played(Cue::complete));
    EXPECT_FALSE(played(Cue::connect)); // one sound per action
    // All nine points are spent: an open star of another branch now refuses.
    walk({Direction::up, Direction::up, Direction::up, Direction::right});
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::invalid));
    EXPECT_FALSE(played(Cue::connect));
    expect_drawable(*design_);
}

TEST_F(Constellation, RefundAsksFirstAndFocusesTheSafeChoice)
{
    // Nothing spent: nothing to ask about.
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::invalid));

    unlock({Direction::left});
    const float first = pitch_of(Cue::connect);
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::modal_open));
    // The prompt owns the input: the map's focus does not move under it.
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(last_cues_.empty());
    // Confirming straight away keeps everything.
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::modal_close));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::tick)); // still lit

    // Again, this time choosing the refund.
    send(*design_, press(Action::north));
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, press(Action::confirm), 1.0f);
    EXPECT_TRUE(played(Cue::undo));
    // The star is dark again and the scale starts over.
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::connect));
    EXPECT_FLOAT_EQ(pitch_of(Cue::connect), first);
}

TEST_F(Constellation, APressDuringTheRefundCascadeIsNotLost)
{
    complete_sentinel();
    send(*design_, press(Action::north));
    send(*design_, nav(Direction::down));
    // No time for the cascade to run: the very next frame unlocks again.
    send(*design_, press(Action::confirm), 0.0f);
    EXPECT_TRUE(played(Cue::undo));
    walk({Direction::up, Direction::up, Direction::up, Direction::right});
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::connect));
    expect_drawable(*design_);
}

TEST_F(Constellation, ZoomStepsBetweenThreeLevelsAndStopsAtTheEnds)
{
    send(*design_, press(Action::jump_prev));
    EXPECT_TRUE(played(Cue::slide));
    send(*design_, press(Action::jump_prev));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::slide));
    send(*design_, press(Action::jump_next));
    EXPECT_TRUE(played(Cue::slide));
    send(*design_, press(Action::jump_next));
    EXPECT_TRUE(played(Cue::slide));
    send(*design_, press(Action::jump_next));
    EXPECT_TRUE(played(Cue::error));
    // The focus still moves at the far zoom, where the camera holds still.
    send(*design_, press(Action::jump_prev));
    send(*design_, press(Action::jump_prev));
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::focus));
    expect_drawable(*design_);
}

} // namespace
