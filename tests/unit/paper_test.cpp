// ps5-homebrew-ui - Behaviour tests for the Paper Library design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The library's rules, pinned down through the sounds it asks for: the grid
// has edges, filters can be empty, the dialog owns the input while it is up,
// and a title can be finished only once.

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;

class Paper : public hui::testing::ConceptFixture
{
  protected:
    // The pan of the first cue of the last send(), to tell left from right.
    float first_pan() const
    {
        return last_cues_.empty() ? 0.0f : last_cues_.front().pan;
    }

    std::unique_ptr<hui::app::Concept> design_ = hui::concepts::make_paper(context_);
};

TEST_F(Paper, TheGridHasFourEdgesThatRefuseSoftly)
{
    // The focus starts on the first card: left and up are the edge.
    send(*design_, nav(Direction::left));
    EXPECT_TRUE(played(Cue::error));
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::error));
    // Six cards across: five steps right, then the edge.
    for (int i = 0; i < 5; ++i)
    {
        send(*design_, nav(Direction::right));
        EXPECT_TRUE(played(Cue::focus)) << i;
        EXPECT_FALSE(played(Cue::error)) << i;
    }
    EXPECT_GT(first_pan(), 0.0f); // the tick followed the card to the right
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::error));
    // Twenty-four titles are four rows: three steps down, then the edge.
    for (int i = 0; i < 3; ++i)
    {
        send(*design_, nav(Direction::down));
        EXPECT_TRUE(played(Cue::focus)) << i;
    }
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::error));
    expect_drawable(*design_);
}

TEST_F(Paper, AHeldDirectionAtAnEdgeStaysQuiet)
{
    hui::InputFrame held = nav(Direction::left);
    held.nav_repeat = true;
    send(*design_, held);
    EXPECT_TRUE(last_cues_.empty());
}

TEST_F(Paper, AnEmptyFilterIsAPlaceNotABrokenScreen)
{
    // Nothing is starred at the start, so Favorites is empty.
    send(*design_, press(Action::jump_next));
    EXPECT_TRUE(played(Cue::tab));
    expect_drawable(*design_);
    // There is nothing to open or star; the screen says no, softly.
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::invalid));
    EXPECT_FALSE(played(Cue::modal_open));
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::invalid));
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::error));
    // Back on the full shelf everything works again.
    send(*design_, press(Action::jump_prev));
    EXPECT_TRUE(played(Cue::tab));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::modal_open));
}

TEST_F(Paper, TheFiltersWrapAroundInBothDirections)
{
    // L2 from "All" lands on the last chip, and R2 comes back.
    send(*design_, press(Action::jump_prev));
    EXPECT_TRUE(played(Cue::tab));
    const float last_chip = first_pan();
    send(*design_, press(Action::jump_next));
    EXPECT_TRUE(played(Cue::tab));
    EXPECT_LT(first_pan(), last_chip); // "All" is the leftmost chip
    expect_drawable(*design_);
}

TEST_F(Paper, UnstarringOnTheFavoritesShelfRemovesTheCard)
{
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::favorite_on));
    send(*design_, press(Action::jump_next));
    // One favourite: it is alone on the shelf, so every direction is an edge.
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::error));
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::favorite_off));
    // The shelf is empty again: there is no card left to open.
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::invalid));
    expect_drawable(*design_);
}

TEST_F(Paper, SortingAnswersAndKeepsTheGridUsable)
{
    for (int i = 0; i < 3; ++i)
    {
        send(*design_, press(Action::west), 0.1f); // interrupt the cards mid-flight
        EXPECT_TRUE(played(Cue::flip)) << i;
        expect_drawable(*design_);
    }
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::focus));
}

TEST_F(Paper, TheDialogTakesTheInputAndATitleFinishesOnce)
{
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::modal_open));
    // The grid does not react while the dialog is up.
    send(*design_, press(Action::west));
    EXPECT_TRUE(last_cues_.empty());
    send(*design_, press(Action::jump_next));
    EXPECT_TRUE(last_cues_.empty());
    // Play, Favorite, Mark as finished, Close: left of Play is the edge.
    send(*design_, nav(Direction::left));
    EXPECT_TRUE(played(Cue::error));
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::favorite_on));
    send(*design_, nav(Direction::right));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::complete));
    expect_drawable(*design_); // confetti in the air
    // Finished is finished: a second press is refused.
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::invalid));
    EXPECT_FALSE(played(Cue::complete));
    send(*design_, nav(Direction::right));
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::error));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::modal_close));
    // The grid has the input back.
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::focus));
    expect_drawable(*design_);
}

TEST_F(Paper, PlayLaunchesAndBackClosesTheDialog)
{
    send(*design_, press(Action::confirm));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::launch));
    send(*design_, press(Action::west)); // closed: Square sorts again
    EXPECT_TRUE(played(Cue::flip));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::modal_open));
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::back));
    // enter() also closes a dialog left open.
    send(*design_, press(Action::confirm));
    design_->enter();
    send(*design_, press(Action::west));
    EXPECT_TRUE(played(Cue::flip));
}

} // namespace
