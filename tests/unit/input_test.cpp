// ps5-homebrew-ui - Controller input model tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/input.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace
{

using hui::Action;
using hui::Direction;
using hui::InputTracker;
using hui::PadSample;
namespace bits = hui::pad_bits;

PadSample sample(std::uint32_t buttons, std::uint8_t lx = 128, std::uint8_t ly = 128)
{
    PadSample s;
    s.buttons = buttons;
    s.left_x = lx;
    s.left_y = ly;
    s.connected = true;
    return s;
}

TEST(Input, KeepsATapThatStartsAndEndsBetweenFrames)
{
    InputTracker tracker;
    const std::vector<PadSample> samples = {sample(0), sample(bits::kCross), sample(0)};
    const auto frame = tracker.update(samples, 0);
    EXPECT_TRUE(frame.is_pressed(Action::confirm));
    EXPECT_FALSE(frame.is_held(Action::confirm));
    EXPECT_NE(frame.released & hui::action_bit(Action::confirm), 0u);
}

TEST(Input, SwapsConfirmAndBack)
{
    InputTracker tracker;
    hui::InputSettings settings;
    settings.swap_confirm = true;
    tracker.set_settings(settings);
    const std::vector<PadSample> samples = {sample(bits::kCircle)};
    const auto frame = tracker.update(samples, 0);
    EXPECT_TRUE(frame.is_pressed(Action::confirm));
    EXPECT_FALSE(frame.is_pressed(Action::back));
}

TEST(Input, NavigationFiresThenRepeatsAfterDelay)
{
    InputTracker tracker;
    const std::vector<PadSample> down = {sample(bits::kDown)};
    auto frame = tracker.update(down, 0);
    EXPECT_EQ(frame.nav, Direction::down);
    EXPECT_FALSE(frame.nav_repeat);
    frame = tracker.update(down, 200000);
    EXPECT_EQ(frame.nav, Direction::none);
    frame = tracker.update(down, 350000);
    EXPECT_EQ(frame.nav, Direction::down);
    EXPECT_TRUE(frame.nav_repeat);
    frame = tracker.update(down, 400000);
    EXPECT_EQ(frame.nav, Direction::none);
    frame = tracker.update(down, 460000);
    EXPECT_EQ(frame.nav, Direction::down);
    const std::vector<PadSample> none = {sample(0)};
    frame = tracker.update(none, 470000);
    EXPECT_EQ(frame.nav, Direction::none);
}

TEST(Input, QuickDirectionTapNavigatesOnce)
{
    InputTracker tracker;
    const std::vector<PadSample> tap = {sample(bits::kRight), sample(0)};
    const auto frame = tracker.update(tap, 0);
    EXPECT_EQ(frame.nav, Direction::right);
}

TEST(Input, StickActsAsDpadBeyondThreshold)
{
    EXPECT_EQ(InputTracker::stick_direction(sample(0, 128, 128)), Direction::none);
    EXPECT_EQ(InputTracker::stick_direction(sample(0, 100, 150)), Direction::none);
    EXPECT_EQ(InputTracker::stick_direction(sample(0, 10, 100)), Direction::left);
    EXPECT_EQ(InputTracker::stick_direction(sample(0, 250, 128)), Direction::right);
    EXPECT_EQ(InputTracker::stick_direction(sample(0, 140, 5)), Direction::up);
    EXPECT_EQ(InputTracker::stick_direction(sample(0, 128, 255)), Direction::down);
}

TEST(Input, StickDeadzoneAndScaling)
{
    InputTracker tracker;
    std::vector<PadSample> centre = {sample(0, 135, 124)};
    auto frame = tracker.update(centre, 0);
    EXPECT_FLOAT_EQ(frame.stick_x, 0.0f);
    EXPECT_FLOAT_EQ(frame.stick_y, 0.0f);
    std::vector<PadSample> full = {sample(0, 255, 128)};
    frame = tracker.update(full, 0);
    EXPECT_NEAR(frame.stick_x, 1.0f, 0.01f);
    EXPECT_NEAR(frame.stick_y, 0.0f, 0.01f);
}

TEST(Input, InterceptOrDisconnectReleasesEverythingAndLosesFocus)
{
    InputTracker tracker;
    const std::vector<PadSample> hold = {sample(bits::kCross | bits::kL1)};
    tracker.update(hold, 0);
    PadSample intercepted = sample(bits::kIntercepted | bits::kCross);
    const std::vector<PadSample> overlay = {intercepted};
    auto frame = tracker.update(overlay, 1000);
    EXPECT_TRUE(frame.focus_lost);
    EXPECT_EQ(frame.held, 0u);
    EXPECT_TRUE(frame.released & hui::action_bit(Action::confirm));

    PadSample unplugged;
    unplugged.connected = false;
    const std::vector<PadSample> gone = {unplugged};
    frame = tracker.update(gone, 2000);
    EXPECT_FALSE(frame.connected);
}

TEST(Input, FocusLostFiresOnceWhileOverlayOwnsInput)
{
    InputTracker tracker;
    const std::vector<PadSample> idle = {sample(0)};
    tracker.update(idle, 0);
    const std::vector<PadSample> overlay = {sample(bits::kIntercepted)};
    EXPECT_TRUE(tracker.update(overlay, 1000).focus_lost);
    EXPECT_FALSE(tracker.update(overlay, 2000).focus_lost);
    tracker.update(idle, 3000);
    EXPECT_TRUE(tracker.update(overlay, 4000).focus_lost);
}

TEST(Input, AnalogTriggersMapToJumps)
{
    PadSample s = sample(0);
    s.r2 = 200;
    const auto actions = InputTracker::map_buttons(s, false);
    EXPECT_NE(actions & hui::action_bit(Action::jump_next), 0u);
    EXPECT_EQ(actions & hui::action_bit(Action::jump_prev), 0u);
}

} // namespace
