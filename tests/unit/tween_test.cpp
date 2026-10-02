// ps5-homebrew-ui - Easing and spring tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/tween.hpp"

#include <gtest/gtest.h>

namespace
{

using namespace hui::tween;

TEST(Tween, EasingsHitEndpointsAndStayMonotonicWhereExpected)
{
    for (auto ease : {cubic_out, cubic_in_out, expo_out, back_out})
    {
        EXPECT_NEAR(ease(0.0f), 0.0f, 1e-5f);
        EXPECT_NEAR(ease(1.0f), 1.0f, 1e-5f);
        EXPECT_NEAR(ease(-3.0f), 0.0f, 1e-5f);
        EXPECT_NEAR(ease(9.0f), 1.0f, 1e-5f);
    }
    float previous = 0.0f;
    for (int i = 1; i <= 100; ++i)
    {
        const float v = cubic_out(static_cast<float>(i) / 100.0f);
        EXPECT_GE(v, previous);
        previous = v;
    }
    EXPECT_GT(back_out(0.7f), 1.0f); // overshoot
}

TEST(Tween, SpringConvergesWithoutOvershootIndependentOfFrameRate)
{
    Spring fast;
    Spring slow;
    fast.snap(0.0f);
    slow.snap(0.0f);
    fast.target = slow.target = 1.0f;
    for (int i = 0; i < 120; ++i)
        fast.update(1.0f / 120.0f);
    for (int i = 0; i < 60; ++i)
        slow.update(1.0f / 60.0f);
    EXPECT_NEAR(fast.value, slow.value, 0.01f);
    Spring spring;
    spring.snap(0.0f);
    spring.target = 1.0f;
    float peak = 0.0f;
    for (int i = 0; i < 600; ++i)
    {
        spring.update(1.0f / 60.0f);
        peak = std::max(peak, spring.value);
    }
    EXPECT_LE(peak, 1.0f + 1e-4f);
    EXPECT_TRUE(spring.settled());
}

TEST(Tween, TimerRunsOnce)
{
    Timer timer;
    timer.start(0.5f);
    timer.update(0.25f);
    EXPECT_NEAR(timer.progress(), 0.5f, 1e-5f);
    EXPECT_TRUE(timer.running);
    timer.update(1.0f);
    EXPECT_FLOAT_EQ(timer.progress(), 1.0f);
    EXPECT_FALSE(timer.running);
}

} // namespace
