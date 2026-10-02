// ps5-homebrew-ui - Behaviour tests for the First Run design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The wizard's rules, checked through what it asks to hear: a `type` cue is a
// character entered, its pan says which key made it, its pitch whether it was
// a capital. The keyboard opens on "g", in the middle of the board.

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;
using hui::audio::CueEvent;

class Keyboard : public hui::testing::ConceptFixture
{
  protected:
    // The last send()'s request for a cue, or nullptr.
    const CueEvent *heard(Cue cue) const
    {
        for (const CueEvent &event : last_cues_)
        {
            if (event.cue == cue)
                return &event;
        }
        return nullptr;
    }

    // Step 1 to step 2, where the keyboard is.
    void open_keyboard()
    {
        send(*design_, press(Action::confirm));
        ASSERT_TRUE(played(Cue::select));
    }

    void type(int characters)
    {
        for (int i = 0; i < characters; ++i)
        {
            send(*design_, press(Action::confirm), 0.1f);
            ASSERT_TRUE(played(Cue::type)) << "character " << i;
        }
    }

    // Holds a button without a fresh press and counts a cue while it is down.
    int hold(Action action, float seconds, Cue cue)
    {
        hui::InputFrame held = idle();
        held.held = hui::action_bit(action);
        int count = 0;
        for (int frame = 0, frames = static_cast<int>(seconds / (1.0f / 60.0f)); frame < frames;
             ++frame)
        {
            send(*design_, held, 0.0f);
            if (played(Cue::error))
                ADD_FAILURE() << "a held button must not be refused aloud";
            if (played(cue))
                ++count;
        }
        return count;
    }

    std::unique_ptr<hui::app::Concept> design_ = hui::concepts::make_keyboard(context_);
};

TEST_F(Keyboard, TypingSoundsFromWhereTheKeyIs)
{
    open_keyboard();
    send(*design_, press(Action::confirm));
    const CueEvent *middle = heard(Cue::type);
    ASSERT_NE(middle, nullptr);
    const float middle_pan = middle->pan;
    // Four keys to the right, the sound has moved to the right as well.
    for (int i = 0; i < 4; ++i)
    {
        send(*design_, nav(Direction::right), 0.1f);
        EXPECT_TRUE(played(Cue::focus));
    }
    send(*design_, press(Action::confirm));
    const CueEvent *right = heard(Cue::type);
    ASSERT_NE(right, nullptr);
    EXPECT_GT(right->pan, middle_pan + 0.1f);
    // Triangle types a space without moving the focus.
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::type));
    EXPECT_FALSE(played(Cue::focus));
    expect_drawable(*design_);
}

TEST_F(Keyboard, TheFifteenthCharacterIsRefused)
{
    open_keyboard();
    type(14);
    send(*design_, press(Action::confirm));
    EXPECT_FALSE(played(Cue::type));
    const CueEvent *refusal = heard(Cue::error);
    ASSERT_NE(refusal, nullptr);
    EXPECT_LT(refusal->gain, 1.0f); // a soft one
    // One character less and there is room again.
    send(*design_, press(Action::west));
    EXPECT_TRUE(played(Cue::erase));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::type));
}

TEST_F(Keyboard, SquareDeletesAndRepeatsWhileHeld)
{
    open_keyboard();
    type(2);
    send(*design_, press(Action::west));
    EXPECT_TRUE(played(Cue::erase));
    send(*design_, press(Action::west));
    EXPECT_TRUE(played(Cue::erase));
    // Nothing left: a press is refused.
    send(*design_, press(Action::west));
    EXPECT_FALSE(played(Cue::erase));
    EXPECT_TRUE(played(Cue::error));

    type(14);
    // Holding does nothing before the repeat delay; after it the field empties
    // one character at a time and then stays quiet instead of complaining
    // about every further repeat.
    EXPECT_EQ(hold(Action::west, 0.2f, Cue::erase), 0);
    EXPECT_EQ(hold(Action::west, 3.0f, Cue::erase), 14);
    send(*design_, press(Action::jump_next));
    EXPECT_TRUE(played(Cue::error)) << "the field should be empty";
}

TEST_F(Keyboard, ShiftCapitalisesOneLetterAndLocksOnTheSecondPress)
{
    open_keyboard();
    // An empty field arms shift by itself: the first letter is a capital
    // (a higher type sound), the second is not.
    send(*design_, press(Action::confirm));
    ASSERT_NE(heard(Cue::type), nullptr);
    EXPECT_GT(heard(Cue::type)->pitch, 1.0f);
    send(*design_, press(Action::confirm));
    ASSERT_NE(heard(Cue::type), nullptr);
    EXPECT_FLOAT_EQ(heard(Cue::type)->pitch, 1.0f);

    // L2 once: the next letter only.
    send(*design_, press(Action::jump_prev));
    EXPECT_TRUE(played(Cue::toggle));
    send(*design_, press(Action::confirm));
    EXPECT_GT(heard(Cue::type)->pitch, 1.0f);
    send(*design_, press(Action::confirm));
    EXPECT_FLOAT_EQ(heard(Cue::type)->pitch, 1.0f);

    // L2 twice: locked until L2 again.
    send(*design_, press(Action::jump_prev));
    send(*design_, press(Action::jump_prev));
    EXPECT_TRUE(played(Cue::toggle));
    for (int i = 0; i < 3; ++i)
    {
        send(*design_, press(Action::confirm));
        EXPECT_GT(heard(Cue::type)->pitch, 1.0f);
    }
    send(*design_, press(Action::jump_prev));
    EXPECT_TRUE(played(Cue::toggle));
    send(*design_, press(Action::confirm));
    EXPECT_FLOAT_EQ(heard(Cue::type)->pitch, 1.0f);
}

TEST_F(Keyboard, AnEmptyNameCannotContinue)
{
    open_keyboard();
    send(*design_, press(Action::jump_next));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::select));
    expect_drawable(*design_); // the warning is on screen
    // A space is not a name either, and a name cannot start with one.
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::type));
    // Still on the keyboard: Cross types. Then R2 goes on to the summary,
    // where Cross starts and the fanfare plays.
    type(1);
    send(*design_, press(Action::jump_next));
    EXPECT_TRUE(played(Cue::select));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::complete));
    expect_drawable(*design_);
}

TEST_F(Keyboard, RowsWrapAroundButTopAndBottomAreEdges)
{
    open_keyboard();
    // From "g" (column 5) four steps left reach "a", the start of the row.
    float pan = 0.0f;
    for (int i = 0; i < 4; ++i)
    {
        send(*design_, nav(Direction::left), 0.1f);
        ASSERT_NE(heard(Cue::focus), nullptr);
        pan = heard(Cue::focus)->pan;
    }
    EXPECT_LT(pan, 0.0f);
    // One more is not refused: the focus comes back in on the far right.
    send(*design_, nav(Direction::left));
    EXPECT_FALSE(played(Cue::error));
    ASSERT_NE(heard(Cue::focus), nullptr);
    EXPECT_GT(heard(Cue::focus)->pan, 0.0f);
    // ... and right from there returns to the left end.
    send(*design_, nav(Direction::right));
    ASSERT_NE(heard(Cue::focus), nullptr);
    EXPECT_FLOAT_EQ(heard(Cue::focus)->pan, pan);

    // Up: the letters row, the digits row, then the edge.
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::error));
    // A held direction against the edge stays quiet.
    hui::InputFrame held = nav(Direction::up);
    held.nav_repeat = true;
    send(*design_, held);
    EXPECT_TRUE(last_cues_.empty());
}

TEST_F(Keyboard, AWideKeyRemembersTheColumn)
{
    open_keyboard();
    // "g" -> "h" -> "n" -> the space bar, which covers four columns.
    send(*design_, nav(Direction::right));
    send(*design_, nav(Direction::down));
    ASSERT_NE(heard(Cue::focus), nullptr);
    const float pan_n = heard(Cue::focus)->pan;
    send(*design_, nav(Direction::down));
    ASSERT_NE(heard(Cue::focus), nullptr);
    EXPECT_NE(heard(Cue::focus)->pan, pan_n);
    // Up again is "n", not the key above the middle of the bar.
    send(*design_, nav(Direction::up));
    ASSERT_NE(heard(Cue::focus), nullptr);
    EXPECT_FLOAT_EQ(heard(Cue::focus)->pan, pan_n);
    // Cross on the bar types a space only after a first character.
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::type));
    send(*design_, nav(Direction::down));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::type));
}

TEST_F(Keyboard, CircleGoesBackOneStepAndTheNameIsKept)
{
    // Nothing is before step 1.
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::error));

    open_keyboard();
    type(14);
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::back));
    // Back on the avatars, left and right choose again ...
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::focus));
    // ... and the keyboard still holds the 14 characters: one more is refused.
    open_keyboard();
    send(*design_, press(Action::confirm));
    EXPECT_FALSE(played(Cue::type));
    EXPECT_TRUE(played(Cue::error));

    // Summary, welcome, and around to step 1 with the name still full.
    send(*design_, press(Action::jump_next));
    EXPECT_TRUE(played(Cue::select));
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::back));
    send(*design_, press(Action::jump_next));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::complete));
    send(*design_, idle(), 3.0f); // the welcome leaves by itself
    open_keyboard();
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::error));
    expect_drawable(*design_);
}

} // namespace
