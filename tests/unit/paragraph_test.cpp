// ps5-homebrew-ui - measure_paragraph agrees with what paragraph draws.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/save_file.hpp"
#include "gfx/draw_list.hpp"
#include "gfx/font.hpp"
#include "ui/fonts.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

#ifndef HUI_SOURCE_DIR
#define HUI_SOURCE_DIR "."
#endif

namespace
{

using hui::gfx::DrawList;
using hui::gfx::Font;
using hui::ui::FontRef;
using hui::ui::measure_paragraph;
using hui::ui::paragraph;

const Font &regular()
{
    static Font font = []
    {
        Font loaded;
        std::string data;
        EXPECT_TRUE(
            hui::save::read_file(HUI_SOURCE_DIR "/assets/fonts/inter-regular.huifont", &data));
        EXPECT_TRUE(loaded.load(data)) << loaded.error();
        return loaded;
    }();
    return font;
}

constexpr std::string_view kLong =
    "Rows are sized by how many lines their text wraps to, so a long release note no longer "
    "runs past the card it sits in, and the list stops where the space ends.";

// How far paragraph() moves the baseline when it draws the same text.
float drawn_height(const FontRef &ref, std::string_view text, float size, float width,
                   float line_height, int max_lines)
{
    DrawList list;
    const float start = 100.0f;
    return paragraph(list, ref, text, 0.0f, start, size, width, line_height,
                     hui::gfx::Color::rgb(0xffffff), max_lines) -
           start;
}

TEST(Paragraph, ShortTextIsOneLine)
{
    const FontRef ref{&regular(), 0};
    const auto m = measure_paragraph(ref, "Resume", 24, 600, 32);
    EXPECT_EQ(m.lines, 1);
    EXPECT_FLOAT_EQ(m.height, 32.0f);
    EXPECT_FLOAT_EQ(m.width, ref.measure("Resume", 24));
    EXPECT_FALSE(m.truncated);
}

TEST(Paragraph, CountsTheWrappedLines)
{
    const FontRef ref{&regular(), 0};
    const auto lines = regular().wrap(kLong, 24, 420);
    ASSERT_GE(lines.size(), 3u);
    const auto m = measure_paragraph(ref, kLong, 24, 420, 30);
    EXPECT_EQ(m.lines, static_cast<int>(lines.size()));
    EXPECT_FLOAT_EQ(m.height, 30.0f * static_cast<float>(lines.size()));
    EXPECT_LE(m.width, 420.0f + 1e-3f);
    EXPECT_GT(m.width, 0.0f);
    EXPECT_FALSE(m.truncated);
}

TEST(Paragraph, CapsAtMaxLinesAndReportsTruncation)
{
    const FontRef ref{&regular(), 0};
    const auto m = measure_paragraph(ref, kLong, 24, 420, 30, 2);
    EXPECT_EQ(m.lines, 2);
    EXPECT_FLOAT_EQ(m.height, 60.0f);
    EXPECT_LE(m.width, 420.0f + 1e-3f);
    EXPECT_TRUE(m.truncated);
}

TEST(Paragraph, AgreesWithWhatParagraphDraws)
{
    const FontRef ref{&regular(), 0};
    const std::string_view texts[] = {kLong, "Resume", "", "One\nTwo\nThree"};
    for (std::string_view text : texts)
        for (int max_lines : {0, 1, 2, 3, 99})
        {
            const auto m = measure_paragraph(ref, text, 22, 380, 28, max_lines);
            EXPECT_FLOAT_EQ(m.height, drawn_height(ref, text, 22, 380, 28, max_lines))
                << "text \"" << text << "\" max_lines " << max_lines;
        }
}

} // namespace
