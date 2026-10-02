// ps5-homebrew-ui - Behaviour tests for the Aurora Shelf design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A design's logic is plain C++ with no OpenGL, so its behaviour can be
// pinned down in ordinary unit tests: send input, check the sounds it asked
// for. Use this file as the pattern for testing a new design.

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;

class Aurora : public hui::testing::ConceptFixture
{
  protected:
    std::unique_ptr<hui::app::Concept> design_ = hui::concepts::make_aurora(context_);
};

TEST_F(Aurora, MovingAlongAShelfTicksAndTheEndRefuses)
{
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::focus));
    // Left twice: back to the first card, then against the edge.
    send(*design_, nav(Direction::left));
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, nav(Direction::left));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::focus));
}

TEST_F(Aurora, AHeldDirectionAtTheEdgeStaysQuiet)
{
    hui::InputFrame held = nav(Direction::left);
    held.nav_repeat = true;
    send(*design_, held);
    EXPECT_TRUE(last_cues_.empty());
}

TEST_F(Aurora, ChangingShelfPlaysTheTabCue)
{
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::tab));
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::tab));
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::error));
}

TEST_F(Aurora, TheSheetOpensTakesTheInputAndCloses)
{
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::open));
    // While the sheet is open the shelves do not move: down picks an action.
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::focus));
    EXPECT_FALSE(played(Cue::tab));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::favorite_on));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::favorite_off));
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::back));
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::tab));
    expect_drawable(*design_);
}

} // namespace
