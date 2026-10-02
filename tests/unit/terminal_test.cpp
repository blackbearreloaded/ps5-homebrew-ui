// ps5-homebrew-ui - Behaviour tests for the Phosphor terminal design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The terminal has more modes than most designs (boot, menu, an opened page,
// a prompt, power-off), so the tests follow the player through them and
// check the sounds each step asks for and where it places them.

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;

class Terminal : public hui::testing::ConceptFixture
{
  protected:
    // Starts the design and skips its boot sequence: the menu has the input.
    void boot()
    {
        design_->enter();
        send(*design_, press(Action::confirm));
    }

    // Lets the design run with no input and counts how often it asks for a cue.
    int count_cues(Cue cue, float seconds)
    {
        int count = 0;
        for (int frame = 0, frames = static_cast<int>(seconds / (1.0f / 60.0f)); frame < frames;
             ++frame)
        {
            feedback_.clear();
            design_->update(idle(), 1.0f / 60.0f, feedback_);
            for (const hui::audio::CueEvent &event : feedback_.cues)
                count += event.cue == cue ? 1 : 0;
        }
        return count;
    }

    // Stereo position of a cue the last send() asked for (0 if it did not).
    float pan_of(Cue cue) const
    {
        for (const hui::audio::CueEvent &event : last_cues_)
        {
            if (event.cue == cue)
                return event.pan;
        }
        return 0.0f;
    }

    void move_to_shutdown()
    {
        for (int i = 0; i < 6; ++i)
            send(*design_, nav(Direction::down), 0.1f);
    }

    std::unique_ptr<hui::app::Concept> design_ = hui::concepts::make_terminal(context_);
};

TEST_F(Terminal, TheBootTypesWithFewKeySoundsAndAnyButtonSkipsIt)
{
    design_->enter();
    // Text appears at up to ninety characters a second; the key sound must
    // not: twelve a second at most.
    const int typed = count_cues(Cue::type, 1.0f);
    EXPECT_GE(typed, 3);
    EXPECT_LE(typed, 12);
    expect_drawable(*design_);
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::select));
    // The menu has the input now.
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::tick));
}

TEST_F(Terminal, ReducedMotionShowsTheBootTextAtOnceAndSilently)
{
    settings_.reduced_motion = true;
    design_->enter();
    EXPECT_EQ(count_cues(Cue::type, 2.0f), 0);
    // The boot has ended by itself.
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::tick));
    expect_drawable(*design_);
}

TEST_F(Terminal, TheMenuTicksOnTheLeftAndItsEndsRefuse)
{
    boot();
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::tick));
    EXPECT_LT(pan_of(Cue::tick), 0.0f);
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::tick));
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::tick));
    // There is nothing above the menu to go back to.
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::error));
}

TEST_F(Terminal, AHeldDirectionAtTheEdgeStaysQuiet)
{
    boot();
    hui::InputFrame held = nav(Direction::up);
    held.nav_repeat = true;
    send(*design_, held);
    EXPECT_TRUE(last_cues_.empty());
}

TEST_F(Terminal, CrossOpensThePageWhichTakesTheInputUntilCircle)
{
    boot();
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::select));
    // Down now moves the row cursor of the page, on the right of the screen.
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::tick));
    EXPECT_GT(pan_of(Cue::tick), 0.0f);
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::mark));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::erase));
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::back));
    send(*design_, nav(Direction::down));
    EXPECT_LT(pan_of(Cue::tick), 0.0f);
    expect_drawable(*design_);
}

TEST_F(Terminal, SquareCyclesThePhosphorColour)
{
    boot();
    send(*design_, press(Action::west));
    EXPECT_TRUE(played(Cue::toggle));
    send(*design_, press(Action::confirm));
    send(*design_, press(Action::west));
    EXPECT_TRUE(played(Cue::toggle));
    expect_drawable(*design_);
}

TEST_F(Terminal, ShutdownAsksFirstAndNoIsTheDefault)
{
    boot();
    move_to_shutdown();
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::modal_open));
    // Confirming without choosing answers "no".
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::modal_close));
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::tick));
}

TEST_F(Terminal, ShuttingDownPowersOffAndBootsAgain)
{
    boot();
    move_to_shutdown();
    send(*design_, press(Action::confirm));
    send(*design_, nav(Direction::left));
    EXPECT_TRUE(played(Cue::tick));
    send(*design_, press(Action::confirm), 0.2f);
    EXPECT_TRUE(played(Cue::drop));
    expect_drawable(*design_); // the picture is collapsing
    // The tube ignores the controller while it is off.
    send(*design_, nav(Direction::up), 0.6f);
    EXPECT_TRUE(last_cues_.empty());
    expect_drawable(*design_); // dark
    // Then the boot sequence runs again, and a button skips it as before.
    EXPECT_GE(count_cues(Cue::type, 1.5f), 1);
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::select));
    // The selection was kept: SHUTDOWN is the last command.
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::error));
}

} // namespace
