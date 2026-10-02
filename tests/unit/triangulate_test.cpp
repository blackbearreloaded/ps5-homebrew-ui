// ps5-homebrew-ui - Polygon triangulation tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gfx/triangulate.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace
{

double triangle_area_sum(const std::vector<float> &xy, const std::vector<std::uint32_t> &tris)
{
    double total = 0.0;
    for (std::size_t t = 0; t + 2 < tris.size(); t += 3)
    {
        const float *a = &xy[2 * tris[t]];
        const float *b = &xy[2 * tris[t + 1]];
        const float *c = &xy[2 * tris[t + 2]];
        total += std::fabs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) * 0.5;
    }
    return total;
}

double polygon_area(const std::vector<float> &xy)
{
    double area = 0.0;
    const std::size_t n = xy.size() / 2;
    for (std::size_t i = 0, j = n - 1; i < n; j = i++)
        area += static_cast<double>(xy[2 * j]) * xy[2 * i + 1] -
                static_cast<double>(xy[2 * i]) * xy[2 * j + 1];
    return std::fabs(area) * 0.5;
}

TEST(Triangulate, ConvexSquareEitherWinding)
{
    const std::vector<float> ccw = {0, 0, 10, 0, 10, 10, 0, 10};
    std::vector<std::uint32_t> tris;
    ASSERT_TRUE(hui::gfx::triangulate(ccw.data(), 4, tris));
    EXPECT_EQ(tris.size(), 6u);
    EXPECT_NEAR(triangle_area_sum(ccw, tris), 100.0, 1e-6);
    const std::vector<float> cw = {0, 0, 0, 10, 10, 10, 10, 0};
    tris.clear();
    ASSERT_TRUE(hui::gfx::triangulate(cw.data(), 4, tris));
    EXPECT_NEAR(triangle_area_sum(cw, tris), 100.0, 1e-6);
}

TEST(Triangulate, ConcaveShapesCoverExactArea)
{
    // An L shape and a star-like arrow (concave vertices).
    const std::vector<std::vector<float>> shapes = {
        {0, 0, 20, 0, 20, 5, 5, 5, 5, 20, 0, 20},
        {0, 0, 10, 4, 20, 0, 16, 10, 20, 20, 10, 16, 0, 20, 4, 10},
    };
    for (const auto &shape : shapes)
    {
        std::vector<std::uint32_t> tris;
        ASSERT_TRUE(hui::gfx::triangulate(shape.data(), static_cast<int>(shape.size() / 2), tris));
        EXPECT_EQ(tris.size(), (shape.size() / 2 - 2) * 3);
        EXPECT_NEAR(triangle_area_sum(shape, tris), polygon_area(shape), 1e-6);
    }
}

TEST(Triangulate, HandlesCollinearAndDegenerateInput)
{
    const std::vector<float> collinear_edge = {0, 0, 5, 0, 10, 0, 10, 10, 0, 10};
    std::vector<std::uint32_t> tris;
    ASSERT_TRUE(hui::gfx::triangulate(collinear_edge.data(), 5, tris));
    EXPECT_NEAR(triangle_area_sum(collinear_edge, tris), 100.0, 1e-6);

    const std::vector<float> line = {0, 0, 5, 5, 10, 10};
    tris.clear();
    EXPECT_FALSE(hui::gfx::triangulate(line.data(), 3, tris));
    EXPECT_FALSE(hui::gfx::triangulate(line.data(), 2, tris));
}

} // namespace
