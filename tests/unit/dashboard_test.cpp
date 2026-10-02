// ps5-homebrew-ui - Behaviour tests for the Pulse Dashboard design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dashboard's focus moves spatially between tiles of different sizes.
// A focus tick is panned by the tile it lands on, so the tests can tell where
// the focus went from the sound alone, without reaching into the design.

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;

// The stereo position of each tile's centre (ui::pan_for_x of its middle).
constexpr float kPanPlay = 0.125f;     // columns 7 to 9
constexpr float kPanRenderer = 0.376f; // columns 10 to 12
constexpr float kPanLibrary = -0.376f; // columns 1 to 3
constexpr float kPanStorage = 0.0f;    // columns 4 to 9: centred
constexpr float kPanSession = -0.125f; // columns 4 to 6
constexpr float kPanRecent = 0.376f;   // columns 10 to 12

class Dashboard : public hui::testing::ConceptFixture
{
  protected:
    // The first cue of that kind the last send() asked for.
    const hui::audio::CueEvent *event(Cue cue) const
    {
        for (const hui::audio::CueEvent &candidate : last_cues_)
        {
            if (candidate.cue == cue)
                return &candidate;
        }
        return nullptr;
    }

    // Sends a direction and checks the focus landed on the tile with this pan.
    void expect_move(Direction direction, float pan)
    {
        send(*design_, nav(direction));
        const hui::audio::CueEvent *tick = event(Cue::focus);
        ASSERT_NE(tick, nullptr);
        EXPECT_NEAR(tick->pan, pan, 0.01f);
    }

    std::unique_ptr<hui::app::Concept> design_ = hui::concepts::make_dashboard(context_);
};

TEST_F(Dashboard, FocusMovesAlongTheTopRowAndTheEdgeRefuses)
{
    expect_move(Direction::right, kPanPlay);
    expect_move(Direction::right, kPanRenderer);
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::focus));
}

TEST_F(Dashboard, AHeldDirectionAtTheEdgeStaysQuiet)
{
    hui::InputFrame held = nav(Direction::left);
    held.nav_repeat = true;
    send(*design_, held);
    EXPECT_TRUE(last_cues_.empty());
}

TEST_F(Dashboard, TilesOfDifferentSizesAreReachedSpatially)
{
    // Down from the wide hero tile: the tile squarely below its left half.
    expect_move(Direction::down, kPanLibrary);
    // Right: the wide storage tile and the session tile are equally near;
    // the tie goes to the earlier one in reading order.
    expect_move(Direction::right, kPanStorage);
    // Up from the wide tile: the tile above its centre, not the hero.
    expect_move(Direction::up, kPanPlay);
    // Down again does not skip the wide tile for the one in line below it.
    expect_move(Direction::down, kPanStorage);
    expect_move(Direction::down, kPanSession);
}

TEST_F(Dashboard, TheOppositeDirectionReturnsToWhereTheFocusCameFrom)
{
    expect_move(Direction::right, kPanPlay);
    expect_move(Direction::down, kPanStorage);
    expect_move(Direction::down, kPanSession);
    expect_move(Direction::right, kPanPlay); // achievements, below the play tile
    expect_move(Direction::right, kPanRecent);
    // Spatially the nearest tile to the left is the storage tile (pan 0), but
    // the focus came from the achievements tile and goes back there.
    expect_move(Direction::left, kPanPlay);
}

TEST_F(Dashboard, LowerTilesTickAtALowerPitch)
{
    send(*design_, nav(Direction::right));
    const hui::audio::CueEvent *top = event(Cue::focus);
    ASSERT_NE(top, nullptr);
    const float top_pitch = top->pitch;
    send(*design_, nav(Direction::down));
    const hui::audio::CueEvent *lower = event(Cue::focus);
    ASSERT_NE(lower, nullptr);
    EXPECT_LT(lower->pitch, top_pitch);
}

TEST_F(Dashboard, ExpandingTakesTheInputAndCollapsingGivesItBack)
{
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::open));
    expect_drawable(*design_);
    EXPECT_TRUE(frame_.glass); // the detail view is a frosted overlay
    // The frame graph's detail has a three-step window selector.
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::slider));
    EXPECT_FALSE(played(Cue::focus));
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::slider));
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::error));
    send(*design_, press(Action::back), 1.5f);
    EXPECT_TRUE(played(Cue::back));
    expect_drawable(*design_);
    EXPECT_FALSE(frame_.glass);
    expect_move(Direction::right, kPanPlay);

    // Coming back to the design finds the grid, not a detail view left open.
    send(*design_, press(Action::confirm));
    design_->enter();
    expect_move(Direction::right, kPanRenderer);
    expect_drawable(*design_);
    EXPECT_FALSE(frame_.glass);
}

TEST_F(Dashboard, TheWeekChartStartsOnTodayAndStepsThroughTheDays)
{
    send(*design_, nav(Direction::right));
    send(*design_, press(Action::confirm));
    // Today is the last bar: nothing lies to its right.
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::error));
    send(*design_, nav(Direction::left));
    const hui::audio::CueEvent *first = event(Cue::slider);
    ASSERT_NE(first, nullptr);
    const float first_pitch = first->pitch;
    send(*design_, nav(Direction::left));
    const hui::audio::CueEvent *second = event(Cue::slider);
    ASSERT_NE(second, nullptr);
    EXPECT_LT(second->pitch, first_pitch); // earlier days sound lower
    // The days are one row: up and down have nowhere to go.
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::error));
}

// The host snapshots have no telemetry, so this is where the graph with real
// samples is exercised: the frame times arrive, the line appears, pausing
// (Square) stops it from growing.
TEST_F(Dashboard, TheGraphDrawsFrameTimesAsTheyArriveAndPauseHoldsIt)
{
    expect_drawable(*design_);
    const std::size_t empty = frame_.scene.instances().size();

    const auto present = [this](int frames)
    {
        for (int i = 0; i < frames; ++i)
        {
            telemetry_.push(i % 50 == 0 ? 31.0f : 16.6f);
            telemetry_.fps = 60.0f;
            send(*design_, idle(), 0.0f);
        }
    };
    present(120);
    expect_drawable(*design_);
    const std::size_t live = frame_.scene.instances().size();
    EXPECT_GT(live, empty + 150); // a fill and a line segment per sample

    send(*design_, press(Action::west));
    EXPECT_TRUE(played(Cue::toggle));
    expect_drawable(*design_);
    const std::size_t paused = frame_.scene.instances().size();
    present(60);
    expect_drawable(*design_);
    EXPECT_EQ(frame_.scene.instances().size(), paused);

    send(*design_, press(Action::west));
    EXPECT_TRUE(played(Cue::toggle));
    present(60);
    expect_drawable(*design_);
    EXPECT_GT(frame_.scene.instances().size(), paused + 50);
    // The detail view shows a longer window of the same history.
    send(*design_, press(Action::confirm), 1.0f);
    expect_drawable(*design_);
}

} // namespace
