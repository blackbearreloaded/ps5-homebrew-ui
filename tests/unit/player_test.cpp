// ps5-homebrew-ui - Behaviour tests for the Now Playing design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The player keeps its own playback clock, so its rules can be tested without
// audio: send input, let time pass, check the cues it asks for. The seek tick
// is pitched by the position in the track, which makes the clock observable.

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;

class Player : public hui::testing::ConceptFixture
{
  protected:
    // Pitch of the first cue of the last send(), or 0 when there was none.
    float pitch() const
    {
        return last_cues_.empty() ? 0.0f : last_cues_.front().pitch;
    }

    // Opens the queue from the transport row and plays its first track, which
    // leaves the focus in the queue and the clock at 0:00 of track 1.
    void play_first_track()
    {
        send(*design_, nav(Direction::down));
        for (int i = 0; i < 4; ++i)
            send(*design_, nav(Direction::up), 0.05f);
        send(*design_, press(Action::confirm), 0.05f);
        EXPECT_TRUE(played(Cue::select));
        send(*design_, press(Action::back), 0.05f);
        EXPECT_TRUE(played(Cue::back));
    }

    std::unique_ptr<hui::app::Concept> design_ = hui::concepts::make_player(context_);
};

TEST_F(Player, TheFocusGlidesAlongTheTransportRowAndItsLeftEndRefuses)
{
    // The focus starts on play / pause, the middle of five buttons.
    send(*design_, nav(Direction::left));
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, nav(Direction::left));
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, nav(Direction::left));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::focus));
    // A held direction against the edge stays quiet.
    hui::InputFrame held = nav(Direction::left);
    held.nav_repeat = true;
    send(*design_, held);
    EXPECT_TRUE(last_cues_.empty());
}

TEST_F(Player, ShuffleAndRepeatAreTogglesAndTheStarFollowsTriangle)
{
    send(*design_, nav(Direction::left));
    send(*design_, nav(Direction::left));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::toggle));
    const float on = pitch();
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::toggle));
    EXPECT_LT(pitch(), on); // switching off sounds lower than switching on

    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::favorite_on));
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::favorite_off));
    expect_drawable(*design_);
}

TEST_F(Player, TheScrubberSeeksWithRisingPitchAndRefusesAtTheStart)
{
    play_first_track();
    send(*design_, press(Action::confirm), 0.05f); // pause, so the clock holds still
    send(*design_, nav(Direction::up), 0.05f);
    EXPECT_TRUE(played(Cue::focus));

    send(*design_, nav(Direction::left), 0.05f);
    EXPECT_TRUE(played(Cue::error)); // already at 0:00
    hui::InputFrame held = nav(Direction::left);
    held.nav_repeat = true;
    send(*design_, held, 0.05f);
    EXPECT_TRUE(last_cues_.empty());

    send(*design_, nav(Direction::right), 0.05f);
    EXPECT_TRUE(played(Cue::slider));
    const float first = pitch();
    send(*design_, nav(Direction::right), 0.05f);
    EXPECT_TRUE(played(Cue::slider));
    EXPECT_GT(pitch(), first);

    // L2 and R2 seek from anywhere, in bigger steps.
    send(*design_, nav(Direction::down), 0.05f);
    send(*design_, press(Action::jump_next), 0.05f);
    EXPECT_TRUE(played(Cue::slider));
    EXPECT_GT(pitch(), first);
    send(*design_, press(Action::jump_prev), 0.05f);
    EXPECT_TRUE(played(Cue::slider));
}

TEST_F(Player, TimeRunsWhilePlayingAndStandsStillWhilePaused)
{
    play_first_track();
    send(*design_, press(Action::confirm), 0.05f); // pause
    EXPECT_TRUE(played(Cue::select));
    send(*design_, press(Action::jump_next), 5.0f);
    const float paused = pitch();
    send(*design_, press(Action::jump_prev), 0.05f);
    send(*design_, press(Action::jump_next), 0.05f);
    EXPECT_FLOAT_EQ(pitch(), paused); // five seconds passed, the track did not move

    send(*design_, press(Action::jump_prev), 0.05f);
    send(*design_, press(Action::confirm), 5.0f); // play, and let five seconds run
    EXPECT_TRUE(played(Cue::select));
    send(*design_, press(Action::jump_next), 0.05f);
    EXPECT_GT(pitch(), paused);
}

TEST_F(Player, PreviousRestartsATrackThatHasBeenPlayingAndRefusesAtTheFirst)
{
    play_first_track();
    send(*design_, nav(Direction::left), 0.05f); // previous
    // At 0:00 of the first track, without repeat, there is nothing before it.
    send(*design_, press(Action::confirm), 0.05f);
    EXPECT_TRUE(played(Cue::error));
    // A few seconds in, the same button means "from the top".
    send(*design_, idle(), 4.0f);
    send(*design_, press(Action::confirm), 0.05f);
    EXPECT_TRUE(played(Cue::tab));
    send(*design_, press(Action::confirm), 0.05f);
    EXPECT_TRUE(played(Cue::error));
}

TEST_F(Player, NextStopsAtTheEndOfTheListUnlessRepeatIsOn)
{
    // Walk the queue's highlight to the last track and play it.
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::focus));
    for (int i = 0; i < 12; ++i)
        send(*design_, nav(Direction::down), 0.05f);
    EXPECT_TRUE(played(Cue::error)); // the bottom of the list
    send(*design_, press(Action::confirm), 0.05f);
    EXPECT_TRUE(played(Cue::select));
    send(*design_, nav(Direction::left), 0.05f); // back to the transport row
    EXPECT_TRUE(played(Cue::focus));

    send(*design_, nav(Direction::right), 0.05f); // next
    send(*design_, press(Action::confirm), 0.05f);
    EXPECT_TRUE(played(Cue::error));
    send(*design_, nav(Direction::right), 0.05f); // repeat
    send(*design_, press(Action::confirm), 0.05f);
    EXPECT_TRUE(played(Cue::toggle));
    send(*design_, nav(Direction::left), 0.05f);
    send(*design_, press(Action::confirm), 0.05f);
    EXPECT_TRUE(played(Cue::tab)); // wraps to the first track
    expect_drawable(*design_);
}

TEST_F(Player, SquareHidesTheQueueAndTheFocusNeverStaysOnIt)
{
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, press(Action::west));
    EXPECT_TRUE(played(Cue::modal_close));
    // The focus fell back to the transport row: left moves along it again.
    send(*design_, nav(Direction::left));
    EXPECT_TRUE(played(Cue::focus));
    // With the queue hidden there is nothing below the row.
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::error));
    send(*design_, press(Action::west));
    EXPECT_TRUE(played(Cue::open));
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::focus));
    expect_drawable(*design_);
}

TEST_F(Player, ATrackThatEndsHandsOverToTheNextWithoutASound)
{
    play_first_track();
    // Seek to the last seconds of the track, then let it run out.
    bool refused = false;
    for (int i = 0; i < 20 && !refused; ++i)
    {
        send(*design_, press(Action::jump_next), 0.02f);
        refused = played(Cue::error); // the end of the track refuses further seeks
    }
    EXPECT_TRUE(refused);
    // Nobody pressed anything when the track ran out: no cue for two seconds.
    hui::app::Feedback quiet;
    for (int frame = 0; frame < 120; ++frame)
        design_->update(idle(), 1.0f / 60.0f, quiet);
    EXPECT_TRUE(quiet.cues.empty());
    // The clock is back near the start: seeking forward works again, low in pitch.
    send(*design_, press(Action::jump_next), 0.05f);
    EXPECT_TRUE(played(Cue::slider));
    EXPECT_LT(pitch(), 0.95f);
    expect_drawable(*design_);
}

} // namespace
