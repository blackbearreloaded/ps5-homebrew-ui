// ps5-homebrew-ui - Behaviour tests for the Radial Dial design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The wheel's rules are about analog input: which sector a stick direction
// selects, when it lets go of one, and that the D-pad reaches the same
// sectors. Each detent asks for a `tick` whose pitch and pan name the sector,
// so the tests can read the focus from the sounds alone.

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"

#include <cmath>

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;

constexpr float kDegree = 3.14159265f / 180.0f;

class Radial : public hui::testing::ConceptFixture
{
  protected:
    // The left stick pushed to `amount` in a direction given in degrees,
    // clockwise from up (the wheel's own convention).
    static hui::InputFrame stick(float degrees, float amount = 1.0f)
    {
        hui::InputFrame input = idle();
        input.stick_x = std::sin(degrees * kDegree) * amount;
        input.stick_y = -std::cos(degrees * kDegree) * amount;
        return input;
    }

    // One frame of input with no idle frames after it: the stick stays put.
    void hold(const hui::InputFrame &input)
    {
        send(*design_, input, 0.0f);
    }

    int count(Cue cue) const
    {
        int total = 0;
        for (const hui::audio::CueEvent &event : last_cues_)
            total += event.cue == cue ? 1 : 0;
        return total;
    }

    const hui::audio::CueEvent *find(Cue cue) const
    {
        for (const hui::audio::CueEvent &event : last_cues_)
        {
            if (event.cue == cue)
                return &event;
        }
        return nullptr;
    }

    std::unique_ptr<hui::app::Concept> design_ = hui::concepts::make_radial(context_);
};

TEST_F(Radial, TheStickSelectsTheSectorItPointsAt)
{
    // East is the third sector: its tick is pitched up and comes from the right.
    hold(stick(90.0f));
    const hui::audio::CueEvent *east = find(Cue::tick);
    ASSERT_NE(east, nullptr);
    EXPECT_FLOAT_EQ(east->pitch, 0.9f + 0.04f * 2.0f);
    EXPECT_GT(east->pan, 0.0f);

    // West is the seventh: higher still, from the left.
    hold(stick(270.0f));
    const hui::audio::CueEvent *west = find(Cue::tick);
    ASSERT_NE(west, nullptr);
    EXPECT_FLOAT_EQ(west->pitch, 0.9f + 0.04f * 6.0f);
    EXPECT_LT(west->pan, 0.0f);

    // Holding the direction does not tick again.
    hold(stick(270.0f));
    EXPECT_TRUE(last_cues_.empty());
    expect_drawable(*design_);
}

TEST_F(Radial, AStickRestingOnABoundaryDoesNotFlicker)
{
    // The first two sectors meet at 22.5 degrees. A little past the boundary
    // the focus stays where it is ...
    hold(stick(25.0f));
    EXPECT_TRUE(last_cues_.empty());
    // ... clearly inside the neighbour it moves ...
    hold(stick(32.0f));
    EXPECT_EQ(count(Cue::tick), 1);
    // ... and it does not come back until the stick is clearly back as well.
    hold(stick(20.0f));
    EXPECT_TRUE(last_cues_.empty());
    hold(stick(25.0f));
    EXPECT_TRUE(last_cues_.empty());
    hold(stick(13.0f));
    EXPECT_EQ(count(Cue::tick), 1);
}

TEST_F(Radial, ALightTouchOfTheStickSelectsNothing)
{
    hold(stick(90.0f, 0.3f));
    EXPECT_TRUE(last_cues_.empty());
    // Once engaged, the stick may relax below the engage threshold and keep aiming.
    hold(stick(90.0f, 0.5f));
    EXPECT_TRUE(played(Cue::tick));
    hold(stick(180.0f, 0.3f));
    EXPECT_TRUE(played(Cue::tick));
}

TEST_F(Radial, AStickPushIsNotAlsoADpadStep)
{
    // The shell reports a pushed stick as `nav` too. The wheel must move one
    // sector for it, not two.
    hui::InputFrame push = stick(45.0f);
    push.nav = Direction::right;
    hold(push);
    EXPECT_EQ(count(Cue::tick), 1);
    EXPECT_FLOAT_EQ(find(Cue::tick)->pitch, 0.9f + 0.04f * 1.0f);
}

TEST_F(Radial, TheDpadTurnsTheDialAndWrapsRound)
{
    // Anticlockwise from the top sector wraps to the eighth.
    send(*design_, nav(Direction::left));
    ASSERT_TRUE(played(Cue::tick));
    EXPECT_FLOAT_EQ(find(Cue::tick)->pitch, 0.9f + 0.04f * 7.0f);
    send(*design_, nav(Direction::right));
    ASSERT_TRUE(played(Cue::tick));
    EXPECT_FLOAT_EQ(find(Cue::tick)->pitch, 0.9f);

    // Up heads for the top sector: already there, it refuses softly, and a
    // held direction says nothing at all.
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::tick));
    hui::InputFrame held = nav(Direction::up);
    held.nav_repeat = true;
    send(*design_, held);
    EXPECT_TRUE(last_cues_.empty());

    // Down walks to the bottom sector in four steps, then refuses.
    for (int i = 0; i < 4; ++i)
    {
        send(*design_, nav(Direction::down));
        EXPECT_TRUE(played(Cue::tick));
    }
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::error));
}

TEST_F(Radial, TheTriggersTurnTheWheelAndTheEndsRefuse)
{
    send(*design_, press(Action::jump_prev));
    EXPECT_TRUE(played(Cue::error));
    send(*design_, press(Action::jump_next));
    EXPECT_TRUE(played(Cue::tab));
    expect_drawable(*design_);
    send(*design_, press(Action::jump_next));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::tab));
    send(*design_, press(Action::jump_prev), 0.1f); // mid-turn: sectors still rotating
    EXPECT_TRUE(played(Cue::tab));
    expect_drawable(*design_);
}

TEST_F(Radial, ConfirmEquipsAndAnEmptyItemRefuses)
{
    send(*design_, press(Action::confirm), 0.1f);
    EXPECT_TRUE(played(Cue::select));
    expect_drawable(*design_); // the ripple is in flight

    // South-west is the tonic, and there is none left.
    hold(stick(225.0f));
    ASSERT_TRUE(played(Cue::tick));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::select));

    // The same sector on the other wheel is a signal: always available.
    send(*design_, press(Action::jump_next));
    send(*design_, press(Action::confirm), 0.2f);
    EXPECT_TRUE(played(Cue::select));
    expect_drawable(*design_); // the icon is flying to the card
}

TEST_F(Radial, ClosingFoldsTheWheelAndTheNextInputOnlyOpensIt)
{
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::back));
    expect_drawable(*design_);

    // Folded, a direction opens the wheel without also moving the focus ...
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::modal_open));
    EXPECT_FALSE(played(Cue::tick));

    // ... and Cross opens it without also equipping.
    send(*design_, press(Action::back));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::modal_open));
    EXPECT_FALSE(played(Cue::select));

    // Tilting the stick opens it on the sector the stick points at: the next
    // frame of the same tilt has nothing left to change.
    send(*design_, press(Action::back));
    hold(stick(180.0f));
    EXPECT_TRUE(played(Cue::modal_open));
    hold(stick(180.0f));
    EXPECT_TRUE(last_cues_.empty());
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::select));
}

} // namespace
