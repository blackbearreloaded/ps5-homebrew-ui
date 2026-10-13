// ps5-homebrew-ui - measure_paragraph agrees with what paragraph draws.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "font_loader.hpp"
#include "gfx/draw_list.hpp"
#include "gfx/font.hpp"
#include "ui/fonts.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <string_view>

namespace
{

using hui::gfx::DrawList;
using hui::gfx::Font;
using hui::ui::FontRef;
using hui::ui::measure_paragraph;
using hui::ui::paragraph;
using hui::ui::paragraph_lines;

constexpr std::string_view kEllipsis = "\xE2\x80\xA6";

const Font &regular()
{
    static Font font = hui::testing::load_font("inter-regular");
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

// The metrics describe the very lines that are drawn: their count, the widest
// of them with its ellipsis, and whether the ellipsis is there at all.
TEST(Paragraph, MetricsDescribeTheLinesDrawn)
{
    const FontRef ref{&regular(), 0};
    const std::string_view texts[] = {kLong, "Resume", "", "One\nTwo\nThree"};
    for (std::string_view text : texts)
        for (int max_lines : {-1, 0, 1, 2, 3, 99})
        {
            SCOPED_TRACE(std::string(text) + " / max_lines " + std::to_string(max_lines));
            const auto lines = paragraph_lines(ref, text, 22, 380, max_lines);
            const auto m = measure_paragraph(ref, text, 22, 380, 28, max_lines);
            ASSERT_FALSE(lines.lines.empty());
            EXPECT_LE(static_cast<int>(lines.lines.size()), std::max(max_lines, 1));
            EXPECT_EQ(m.lines, static_cast<int>(lines.lines.size()));
            EXPECT_EQ(m.truncated, lines.truncated);
            EXPECT_EQ(lines.truncated, regular().wrap(text, 22, 380).size() > lines.lines.size());
            EXPECT_EQ(lines.truncated, lines.lines.back().ends_with(kEllipsis));
            float widest = 0.0f;
            for (const std::string &line : lines.lines)
            {
                EXPECT_LE(ref.measure(line, 22), 380.0f + 1e-3f) << line;
                widest = std::max(widest, ref.measure(line, 22));
            }
            EXPECT_FLOAT_EQ(m.width, widest);

            // Drawing the text and drawing its lines put the same glyphs down.
            DrawList from_text;
            DrawList from_lines;
            const auto white = hui::gfx::Color::rgb(0xffffff);
            const float end =
                paragraph(from_text, ref, text, 0, 100, 22, 380, 28, white, max_lines);
            EXPECT_FLOAT_EQ(paragraph(from_lines, ref, lines, 0, 100, 22, 28, white), end);
            EXPECT_EQ(from_text.instances().size(), from_lines.instances().size());
            EXPECT_FLOAT_EQ(measure_paragraph(ref, lines, 22, 28).width, m.width);
        }
}

// A cap below one line still shows one, and says that text was cut.
TEST(Paragraph, ACapBelowOneLineStillMarksTheCut)
{
    const FontRef ref{&regular(), 0};
    const auto lines = paragraph_lines(ref, kLong, 24, 420, 0);
    ASSERT_EQ(lines.lines.size(), 1u);
    EXPECT_TRUE(lines.truncated);
    EXPECT_TRUE(lines.lines.back().ends_with(kEllipsis));
}

TEST(Paragraph, EmptyTextIsOneEmptyLine)
{
    const FontRef ref{&regular(), 0};
    const auto m = measure_paragraph(ref, "", 24, 420, 30);
    EXPECT_EQ(m.lines, 1);
    EXPECT_FLOAT_EQ(m.height, 30.0f);
    EXPECT_FLOAT_EQ(m.width, 0.0f);
    EXPECT_FALSE(m.truncated);
}

} // namespace
