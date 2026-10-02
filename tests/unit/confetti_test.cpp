// ps5-homebrew-ui - Confetti particle tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/confetti.hpp"

#include <gtest/gtest.h>

using hui::ui::Confetti;

TEST(Confetti, BurstThenSettlesToNothing)
{
    Confetti confetti;
    EXPECT_FALSE(confetti.active());
    confetti.burst(hui::gfx::Color::rgb(0x336699), 42);
    EXPECT_EQ(confetti.size(), 220u);
    for (int frame = 0; frame < 60 * 5; ++frame)
        confetti.update(1.0f / 60.0f);
    EXPECT_FALSE(confetti.active());
}

TEST(Confetti, ReducedMotionIsSparse)
{
    Confetti confetti;
    confetti.burst(hui::gfx::Color::rgb(0x336699), 42, true);
    EXPECT_EQ(confetti.size(), 60u);
}

TEST(Confetti, DrawsOneShapePerPieceInFlight)
{
    Confetti confetti;
    confetti.burst(hui::gfx::Color::rgb(0x336699), 9);
    for (int frame = 0; frame < 30; ++frame)
        confetti.update(1.0f / 60.0f);
    hui::gfx::DrawList list;
    confetti.draw(list);
    EXPECT_EQ(list.instances().size(), confetti.size());
}
