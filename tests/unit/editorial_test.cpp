// ps5-homebrew-ui - Behaviour tests for the Editorial design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The design has two views (the list and the article) that share one focus.
// The tests drive it with input and check the cues it asks for, which is
// enough to pin down which view took the input and where the focus is.

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;

constexpr int kFeatures = 8;

class Editorial : public hui::testing::ConceptFixture
{
  protected:
    std::unique_ptr<hui::app::Concept> design_ = hui::concepts::make_editorial(context_);
};

TEST_F(Editorial, TheListTicksDownAndBothEndsRefuse)
{
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::focus));
    // Seven steps reach the last feature; the eighth is refused.
    for (int i = 1; i < kFeatures; ++i)
    {
        send(*design_, nav(Direction::down), 0.1f);
        EXPECT_TRUE(played(Cue::focus)) << "step " << i;
    }
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::focus));
    expect_drawable(*design_);
}

TEST_F(Editorial, AHeldDirectionAtTheEdgeStaysQuiet)
{
    hui::InputFrame held = nav(Direction::up);
    held.nav_repeat = true;
    send(*design_, held);
    EXPECT_TRUE(last_cues_.empty());
}

TEST_F(Editorial, RowsFurtherDownSoundLower)
{
    send(*design_, nav(Direction::down));
    ASSERT_EQ(last_cues_.size(), 1u);
    const float first = last_cues_[0].pitch;
    send(*design_, nav(Direction::down));
    ASSERT_EQ(last_cues_.size(), 1u);
    EXPECT_LT(last_cues_[0].pitch, first);
    // The list sits left of centre, and so does its sound.
    EXPECT_LT(last_cues_[0].pan, 0.0f);
}

TEST_F(Editorial, TheArticleTakesTheInputAndBackReturnsToTheList)
{
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::select));
    // In the article, down scrolls the text; it does not move the focus.
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::slide));
    EXPECT_FALSE(played(Cue::focus));
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::slide));
    // At the top of the text, up is refused; held, it is silent.
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::error));
    hui::InputFrame held = nav(Direction::up);
    held.nav_repeat = true;
    send(*design_, held);
    EXPECT_TRUE(last_cues_.empty());
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::back));
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::focus));
    expect_drawable(*design_);
}

TEST_F(Editorial, TheTextEndsWithARefusal)
{
    send(*design_, press(Action::confirm));
    // However long the article is, scrolling down reaches an end that answers.
    bool refused = false;
    for (int i = 0; i < 12 && !refused; ++i)
    {
        send(*design_, nav(Direction::down), 0.2f);
        refused = played(Cue::error);
        EXPECT_TRUE(refused || played(Cue::slide));
    }
    EXPECT_TRUE(refused);
    expect_drawable(*design_);
}

TEST_F(Editorial, TriggersTurnThePagesAndTheIssueHasTwoEnds)
{
    // The triggers belong to the article: in the list they do nothing.
    send(*design_, press(Action::jump_next));
    EXPECT_TRUE(last_cues_.empty());

    send(*design_, press(Action::confirm));
    send(*design_, press(Action::jump_prev));
    EXPECT_TRUE(played(Cue::error));
    for (int i = 1; i < kFeatures; ++i)
    {
        send(*design_, press(Action::jump_next), 0.2f);
        ASSERT_TRUE(played(Cue::flip)) << "page " << i;
        // The page is heard on the side it turns to.
        EXPECT_GT(last_cues_[0].pan, 0.0f);
    }
    send(*design_, press(Action::jump_next));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::flip));
    send(*design_, press(Action::jump_prev));
    EXPECT_TRUE(played(Cue::flip));
    EXPECT_LT(last_cues_[0].pan, 0.0f);
    expect_drawable(*design_);
}

TEST_F(Editorial, TheFocusFollowsTheArticleBeingRead)
{
    // Open the first feature, turn one page, go back: the list is on the
    // second row, so up moves once and is then refused.
    send(*design_, press(Action::confirm));
    send(*design_, press(Action::jump_next));
    send(*design_, press(Action::back));
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::error));
    // Without a page turn the focus is exactly where it was.
    send(*design_, press(Action::confirm));
    send(*design_, press(Action::back));
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::error));
}

TEST_F(Editorial, TriangleBookmarksInBothViews)
{
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::mark));
    send(*design_, press(Action::confirm));
    // The same feature, now in the article: the bookmark comes off.
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::erase));
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::mark));
    expect_drawable(*design_);
}

} // namespace
