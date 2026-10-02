// ps5-homebrew-ui - Baked font loading and layout tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/save_file.hpp"
#include "gfx/font.hpp"

#include <gtest/gtest.h>

#include <string>

#ifndef HUI_SOURCE_DIR
#define HUI_SOURCE_DIR "."
#endif

namespace
{

using hui::gfx::Align;
using hui::gfx::Font;
using hui::gfx::GlyphQuad;

const Font &inter()
{
    static Font font = []
    {
        Font loaded;
        std::string data;
        EXPECT_TRUE(
            hui::save::read_file(HUI_SOURCE_DIR "/assets/fonts/inter-semibold.huifont", &data));
        EXPECT_TRUE(loaded.load(data)) << loaded.error();
        return loaded;
    }();
    return font;
}

TEST(Font, LoadsAsciiAndSymbols)
{
    const Font &font = inter();
    for (char c = ' '; c < 127; ++c)
        EXPECT_TRUE(font.has_glyph(static_cast<std::uint32_t>(c))) << c;
    EXPECT_TRUE(font.has_glyph(0x2022));
    EXPECT_GT(font.ascent(40), 30.0f);
    EXPECT_GT(font.descent(40), 5.0f);
    EXPECT_GT(font.line_height(40), font.ascent(40));
}

TEST(Font, RejectsCorruptData)
{
    Font font;
    EXPECT_FALSE(font.load("short"));
    std::string data;
    ASSERT_TRUE(hui::save::read_file(HUI_SOURCE_DIR "/assets/fonts/inter-regular.huifont", &data));
    EXPECT_FALSE(font.load(data.substr(0, data.size() - 1)));
    data[0] = 'X';
    EXPECT_FALSE(font.load(data));
}

TEST(Font, MeasureScalesLinearlyAndKerns)
{
    const Font &font = inter();
    const float at20 = font.measure("Light Up", 20);
    const float at40 = font.measure("Light Up", 40);
    EXPECT_GT(at20, 0.0f);
    EXPECT_NEAR(at40, at20 * 2.0f, 0.01f);
    // stb_truetype only reads legacy/simple kerning; pairs never widen text.
    EXPECT_LE(font.measure("AV", 40), font.measure("A", 40) + font.measure("V", 40));
    EXPECT_FLOAT_EQ(font.measure("", 40), 0.0f);
}

TEST(Font, LayoutAlignsAndSkipsSpaces)
{
    const Font &font = inter();
    std::vector<GlyphQuad> quads;
    const float width = font.layout("A B", 100.0f, 50.0f, 32.0f, Align::left, quads);
    ASSERT_EQ(quads.size(), 2u);
    // Quads include the distance-field margin around each glyph.
    EXPECT_GE(quads[0].x0, 100.0f - font.sdf_range(32.0f) - 1.0f);
    EXPECT_LT(quads[0].x0, 100.0f);
    EXPECT_LT(quads[0].y0, 50.0f); // glyphs rise above the baseline
    EXPECT_GT(quads[1].x0, quads[0].x1 - 20.0f);
    for (const GlyphQuad &q : quads)
    {
        EXPECT_GE(q.u0, 0.0f);
        EXPECT_LE(q.u1, 1.0f);
        EXPECT_LT(q.u0, q.u1);
        EXPECT_LT(q.v0, q.v1);
    }
    std::vector<GlyphQuad> centred;
    font.layout("A B", 100.0f, 50.0f, 32.0f, Align::center, centred);
    EXPECT_NEAR(centred[0].x0, quads[0].x0 - width * 0.5f, 0.01f);
    std::vector<GlyphQuad> right;
    font.layout("A B", 100.0f, 50.0f, 32.0f, Align::right, right);
    EXPECT_NEAR(right[0].x0, quads[0].x0 - width, 0.01f);
}

TEST(Font, UnknownCharactersFallBackToQuestionMark)
{
    const Font &font = inter();
    EXPECT_FLOAT_EQ(font.measure("\xE2\x98\x83", 30), font.measure("?", 30)); // U+2603 snowman
    EXPECT_FLOAT_EQ(font.measure("\xFF", 30), font.measure("?", 30));         // invalid byte
}

TEST(Font, DecodesUtf8)
{
    std::size_t index = 0;
    const std::string text = "a\xC3\x97\xE2\x80\xA2";
    EXPECT_EQ(hui::gfx::next_codepoint(text, &index), 'a');
    EXPECT_EQ(hui::gfx::next_codepoint(text, &index), 0xD7u);
    EXPECT_EQ(hui::gfx::next_codepoint(text, &index), 0x2022u);
    EXPECT_EQ(index, text.size());
}

TEST(Font, WrapsAtWordsAndNewlines)
{
    const Font &font = inter();
    const auto lines =
        font.wrap("Connect all the islands with a network of bridges.\nNext", 30, 300);
    ASSERT_GE(lines.size(), 3u);
    for (const std::string &line : lines)
        EXPECT_LE(font.measure(line, 30), 300.0f + 1e-3f) << line;
    EXPECT_EQ(lines.back(), "Next");
}

} // namespace
