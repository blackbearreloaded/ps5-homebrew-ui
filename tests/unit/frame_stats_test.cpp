// ps5-homebrew-ui - Frame-time histogram tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/frame_stats.hpp"

#include <gtest/gtest.h>

#include <string>

namespace
{

TEST(FrameStats, BucketsMatchPacingEdges)
{
    EXPECT_EQ(hui::FrameStats::bucket_for(0.0), 0u);
    EXPECT_EQ(hui::FrameStats::bucket_for(8.3), 0u);
    EXPECT_EQ(hui::FrameStats::bucket_for(16.67), 1u);
    EXPECT_EQ(hui::FrameStats::bucket_for(17.5), 2u);
    EXPECT_EQ(hui::FrameStats::bucket_for(33.3), 3u);
    EXPECT_EQ(hui::FrameStats::bucket_for(49.9), 4u);
    EXPECT_EQ(hui::FrameStats::bucket_for(99.0), 5u);
    EXPECT_EQ(hui::FrameStats::bucket_for(250.0), 6u);
}

TEST(FrameStats, AccumulatesMeanMaxAndHistogram)
{
    hui::FrameStats stats;
    for (int frame = 0; frame < 598; ++frame)
        stats.add(16.67);
    stats.add(33.3);
    stats.add(-1.0);
    EXPECT_EQ(stats.count(), 600u);
    EXPECT_DOUBLE_EQ(stats.max_ms(), 33.3);
    EXPECT_EQ(stats.buckets()[0], 1u);
    EXPECT_EQ(stats.buckets()[1], 598u);
    EXPECT_EQ(stats.buckets()[3], 1u);
    EXPECT_NEAR(stats.mean_ms(), (598 * 16.67 + 33.3) / 600.0, 1e-9);
}

TEST(FrameStats, FormatsOneLineSummaryAndResets)
{
    hui::FrameStats stats;
    stats.add(16.0);
    stats.add(18.0);
    char line[160];
    ASSERT_GT(stats.format(line, sizeof(line)), 0);
    EXPECT_EQ(std::string(line), "frames=2 mean=17.00ms max=18.00ms hist=0,1,1,0,0,0,0");
    stats.reset();
    EXPECT_EQ(stats.count(), 0u);
    EXPECT_DOUBLE_EQ(stats.mean_ms(), 0.0);
}

} // namespace
