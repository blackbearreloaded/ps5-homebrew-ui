// ps5-homebrew-ui - Tests: a held pair of buttons that steps and repeats.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/held_step.hpp"

#include <gtest/gtest.h>

namespace
{

using hui::Action;
using hui::HeldStep;
using hui::InputFrame;

constexpr float kFrame = 1.0f / 60.0f;

InputFrame pressed(Action action)
{
    InputFrame input;
    input.pressed = hui::action_bit(action);
    input.held = input.pressed;
    return input;
}

InputFrame holding(Action action)
{
    InputFrame input;
    input.held = hui::action_bit(action);
    return input;
}

// Steps taken while `action` stays down for `seconds` after the press.
int steps_while_held(HeldStep &steps, Action action, float seconds)
{
    int total = 0;
    const int frames = static_cast<int>(seconds / kFrame + 0.5f);
    for (int i = 0; i < frames; ++i)
        total += steps.step(holding(action), kFrame);
    return total;
}

TEST(HeldStep, APressIsOneStepInItsDirection)
{
    HeldStep steps;
    EXPECT_EQ(steps.step(pressed(Action::jump_next), kFrame), 1);
    EXPECT_FALSE(steps.repeating());
    EXPECT_EQ(steps.step(InputFrame{}, kFrame), 0);
    EXPECT_EQ(steps.step(pressed(Action::jump_prev), kFrame), -1);
}

TEST(HeldStep, HoldingRepeatsAfterAPauseAndThenFaster)
{
    HeldStep steps;
    ASSERT_EQ(steps.step(pressed(Action::jump_next), kFrame), 1);
    // Nothing during the pause.
    EXPECT_EQ(steps_while_held(steps, Action::jump_next, 0.30f), 0);
    EXPECT_FALSE(steps.repeating());
    // Then the slow pace: about nine a second.
    const int slow = steps_while_held(steps, Action::jump_next, 0.5f);
    EXPECT_GE(slow, 3);
    EXPECT_LE(slow, 6);
    EXPECT_TRUE(steps.repeating());
    // And the fast one once it has repeated enough.
    steps_while_held(steps, Action::jump_next, 0.5f);
    const int fast = steps_while_held(steps, Action::jump_next, 1.0f);
    EXPECT_GE(fast, 14);
}

TEST(HeldStep, LettingGoStopsAndTheOtherButtonDoesNotCarryOn)
{
    HeldStep steps;
    ASSERT_EQ(steps.step(pressed(Action::jump_next), kFrame), 1);
    steps_while_held(steps, Action::jump_next, 1.0f);
    EXPECT_EQ(steps.step(InputFrame{}, kFrame), 0);
    EXPECT_FALSE(steps.repeating());
    // Held without a press of its own (it was down before the screen opened).
    EXPECT_EQ(steps_while_held(steps, Action::jump_prev, 1.0f), 0);
}

TEST(HeldStep, WorksOnAnyPairOfActions)
{
    HeldStep steps(Action::page_prev, Action::page_next);
    EXPECT_EQ(steps.step(pressed(Action::jump_next), kFrame), 0);
    EXPECT_EQ(steps.step(pressed(Action::page_prev), kFrame), -1);
    EXPECT_LT(steps_while_held(steps, Action::page_prev, 1.0f), 0);
}

} // namespace
