// ps5-homebrew-ui - 2D draw list tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gfx/draw_list.hpp"

#include <gtest/gtest.h>

namespace
{

using hui::gfx::Color;
using hui::gfx::DrawList;
using hui::gfx::Rect;
using hui::gfx::Shape;

TEST(DrawList, BatchesUntexturedShapesIntoOneRun)
{
    DrawList list;
    list.rounded_rect({0, 0, 10, 10}, 2, Color{});
    list.circle(50, 50, 5, Color{});
    list.line(0, 0, 10, 10, 2, Color{});
    ASSERT_EQ(list.runs().size(), 1u);
    EXPECT_EQ(list.runs()[0].count, 3u);
    EXPECT_EQ(list.instances()[2].params[3], static_cast<float>(Shape::capsule));
}

TEST(DrawList, SplitsRunsOnTextureAndClipChanges)
{
    DrawList list;
    list.image(7, {0, 0, 10, 10}, {0, 0, 1, 1}, Color{});
    list.rounded_rect({0, 0, 10, 10}, 0, Color{}); // joins the textured run
    list.image(9, {0, 0, 10, 10}, {0, 0, 1, 1}, Color{});
    list.push_clip({0, 0, 100, 100});
    list.rounded_rect({0, 0, 10, 10}, 0, Color{});
    list.pop_clip();
    list.rounded_rect({0, 0, 10, 10}, 0, Color{});
    ASSERT_EQ(list.runs().size(), 4u);
    EXPECT_EQ(list.runs()[0].texture, 7u);
    EXPECT_EQ(list.runs()[0].count, 2u);
    EXPECT_EQ(list.runs()[1].texture, 9u);
    EXPECT_TRUE(list.runs()[2].clipped);
    EXPECT_FALSE(list.runs()[3].clipped);
}

TEST(DrawList, NestedClipsIntersect)
{
    DrawList list;
    list.push_clip({0, 0, 100, 100});
    list.push_clip({50, 50, 100, 100});
    list.rounded_rect({0, 0, 1, 1}, 0, Color{});
    const Rect clip = list.runs().back().clip;
    EXPECT_FLOAT_EQ(clip.x, 50);
    EXPECT_FLOAT_EQ(clip.w, 50);
    EXPECT_FLOAT_EQ(clip.h, 50);
}

TEST(DrawList, TransformsScaleAboutOriginAndCompose)
{
    DrawList list;
    list.push_transform(2.0f, 100.0f, 100.0f, 10.0f, 0.0f);
    list.rounded_rect({100, 100, 10, 10}, 4, Color{});
    list.push_transform(0.5f, 0.0f, 0.0f, 0.0f, 0.0f);
    list.rounded_rect({100, 100, 10, 10}, 4, Color{});
    list.pop_transform();
    list.pop_transform();
    list.rounded_rect({100, 100, 10, 10}, 4, Color{});
    const auto &i = list.instances();
    EXPECT_FLOAT_EQ(i[0].rect[0], 110.0f);
    EXPECT_FLOAT_EQ(i[0].rect[2], 20.0f);
    EXPECT_FLOAT_EQ(i[0].params[0], 8.0f); // radius scales too
    EXPECT_FLOAT_EQ(i[1].rect[2], 10.0f);  // 2 * 0.5
    EXPECT_FLOAT_EQ(i[1].rect[0], 10.0f);  // inner 100*0.5 = 50; outer 100 + (50-100)*2 + 10
    EXPECT_FLOAT_EQ(i[2].rect[0], 100.0f);
}

TEST(DrawList, OpacityMultipliesAlpha)
{
    DrawList list;
    list.push_opacity(0.5f);
    list.push_opacity(0.5f);
    list.rounded_rect({0, 0, 1, 1}, 0, Color{1, 1, 1, 0.8f});
    list.pop_opacity();
    list.pop_opacity();
    list.rounded_rect({0, 0, 1, 1}, 0, Color{1, 1, 1, 0.8f});
    EXPECT_FLOAT_EQ(list.instances()[0].color_top[3], 0.2f);
    EXPECT_FLOAT_EQ(list.instances()[1].color_top[3], 0.8f);
}

TEST(DrawList, ClearResetsEverything)
{
    DrawList list;
    list.push_transform(2.0f, 0, 0, 0, 0);
    list.push_clip({0, 0, 1, 1});
    list.rounded_rect({0, 0, 1, 1}, 0, Color{});
    list.clear();
    list.rounded_rect({1, 1, 1, 1}, 0, Color{});
    EXPECT_EQ(list.instances().size(), 1u);
    EXPECT_FALSE(list.runs()[0].clipped);
    EXPECT_FLOAT_EQ(list.instances()[0].rect[0], 1.0f);
}

TEST(Viewport, LetterboxesToAspect)
{
    const auto exact = hui::gfx::fit_viewport(3840, 2160);
    EXPECT_FLOAT_EQ(exact.scale, 2.0f);
    EXPECT_FLOAT_EQ(exact.offset_x, 0.0f);
    const auto wide = hui::gfx::fit_viewport(2560, 1080);
    EXPECT_FLOAT_EQ(wide.scale, 1.0f);
    EXPECT_FLOAT_EQ(wide.offset_x, 320.0f);
    EXPECT_FLOAT_EQ(wide.offset_y, 0.0f);
}

TEST(Color, ParsesHex)
{
    const Color c = Color::rgb(0xff8000, 0.5f);
    EXPECT_FLOAT_EQ(c.r, 1.0f);
    EXPECT_NEAR(c.g, 0.502f, 0.001f);
    EXPECT_FLOAT_EQ(c.b, 0.0f);
    EXPECT_FLOAT_EQ(c.a, 0.5f);
}

} // namespace
