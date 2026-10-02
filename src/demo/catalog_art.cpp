// ps5-homebrew-ui - Cover art for the sample catalogue, rendered at start-up.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The covers show how to render into a texture with the UI renderer itself:
// bind a framebuffer, queue a backdrop and a draw list, present. An app with
// real artwork would upload its images with GlBatch::create_texture instead.

#include "demo/catalog.hpp"

#include "gfx/backdrop.hpp"
#include "gfx/renderer.hpp"

#include <GL/glcorearb.h>

#include <iterator>
#include <string>

namespace hui::demo
{

namespace
{

using gfx::Color;

constexpr gfx::BackdropMode kModes[] = {
    gfx::BackdropMode::aurora, gfx::BackdropMode::waves,  gfx::BackdropMode::bokeh,
    gfx::BackdropMode::grid,   gfx::BackdropMode::vista,  gfx::BackdropMode::stars,
    gfx::BackdropMode::dots,   gfx::BackdropMode::aurora,
};

// A few shapes over the backdrop give each cover its own emblem.
void draw_motif(gfx::DrawList &list, int index, const Item &item)
{
    const Color light = Color{1.0f, 1.0f, 1.0f, 1.0f};
    switch (index % 6)
    {
    case 0: // ringed planet
        list.glow({156, 96, 200, 200}, 100, 60, item.accent.with_alpha(0.35f));
        list.circle(256, 196, 92, item.accent);
        list.arc(256, 196, 150, 6, 1.9f, 2.5f, light.with_alpha(0.75f));
        list.arc(256, 196, 176, 3, 2.1f, 2.1f, light.with_alpha(0.4f));
        break;
    case 1: // stacked tilted cards
        list.rotated_rect({146, 76, 220, 220}, 36, 0.36f, item.accent.with_alpha(0.25f));
        list.rotated_rect({146, 76, 220, 220}, 36, 0.18f, item.accent.with_alpha(0.5f));
        list.rotated_rect({146, 76, 220, 220}, 36, 0.0f, item.accent);
        list.circle(256, 186, 44, item.dark.with_alpha(0.85f));
        break;
    case 2: // rays
        for (int i = 0; i < 7; ++i)
        {
            const float x = 80.0f + static_cast<float>(i) * 58.0f;
            list.line(x, 300.0f - static_cast<float>(i % 3) * 40.0f, x + 90.0f, 70.0f,
                      10.0f + static_cast<float>(i % 2) * 8.0f,
                      (i % 2 == 0 ? item.accent : light).with_alpha(0.75f));
        }
        break;
    case 3: // sun on a horizon
        list.glow({146, 76, 220, 220}, 110, 70, item.accent.with_alpha(0.4f));
        list.arc(256, 250, 120, 120, -1.5708f, 3.14159f, item.accent, false);
        list.line(60, 252, 452, 252, 6, light.with_alpha(0.9f));
        list.line(110, 280, 402, 280, 4, light.with_alpha(0.5f));
        list.line(160, 304, 352, 304, 3, light.with_alpha(0.3f));
        break;
    case 4: // dot lattice
        for (int row = 0; row < 5; ++row)
            for (int column = 0; column < 7; ++column)
            {
                const float radius = 6.0f + static_cast<float>((row * 3 + column * 5) % 7) * 2.6f;
                list.circle(88.0f + static_cast<float>(column) * 56.0f,
                            84.0f + static_cast<float>(row) * 52.0f, radius,
                            ((row + column) % 3 == 0 ? light : item.accent).with_alpha(0.85f));
            }
        break;
    default: // sonar arcs
        for (int i = 0; i < 5; ++i)
            list.arc(256, 300, 70.0f + static_cast<float>(i) * 44.0f, 10, -1.0f, 2.0f,
                     item.accent.with_alpha(1.0f - static_cast<float>(i) * 0.17f));
        list.circle(256, 300, 18, light);
        break;
    }
}

} // namespace

void Catalog::release_covers()
{
    for (Item &item : items_)
    {
        if (item.cover != 0)
            glDeleteTextures(1, &item.cover);
        item.cover = 0;
    }
}

bool Catalog::build_covers(gfx::Renderer &renderer, const ui::Fonts &fonts)
{
    release_covers();
    constexpr float kSize = static_cast<float>(kCoverSize);
    GLuint framebuffer = 0;
    glGenFramebuffers(1, &framebuffer);
    gfx::DrawList list;
    bool ok = true;
    int index = 0;
    for (Item &item : items_)
    {
        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kCoverSize, kCoverSize, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
        // One level only: mip chains leave ps5-opengl's batched fast path.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            glDeleteTextures(1, &texture);
            ok = false;
            break;
        }

        gfx::BackdropSpec spec;
        spec.mode = kModes[static_cast<std::size_t>(index) % std::size(kModes)];
        spec.colors[0] = item.dark;
        spec.colors[1] = item.mid;
        spec.colors[2] = gfx::mix(item.mid, item.accent, 0.6f);
        spec.colors[3] = item.accent;
        spec.params[0] = 0.4f;
        spec.params[1] = 0.3f;
        spec.params[2] = 0.5f;
        spec.time = 11.0f + static_cast<float>(index) * 7.3f; // a different moment per cover

        list.clear();
        draw_motif(list, index, item);
        // A dark foot keeps the title readable on any artwork.
        list.gradient_rect({0, 250, kSize, 262}, 0, item.dark.with_alpha(0.0f),
                           item.dark.with_alpha(0.92f));
        ui::text(list, fonts.semibold, item.studio, 36, 54, 17, Color{1.0f, 1.0f, 1.0f, 0.7f},
                 gfx::Align::left, 2.5f);
        const std::vector<std::string> lines = fonts.display.font->wrap(item.title, 54, kSize - 72);
        float baseline = kSize - 44.0f - static_cast<float>(lines.size() - 1) * 58.0f;
        for (const std::string &line : lines)
        {
            ui::text(list, fonts.display, line, 36, baseline, 54, Color{1.0f, 1.0f, 1.0f, 1.0f});
            baseline += 58.0f;
        }

        renderer.begin();
        renderer.backdrop(spec);
        renderer.draw(list);
        renderer.present(framebuffer, kCoverSize, kCoverSize, kSize, kSize);
        item.cover = texture;
        ++index;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &framebuffer);
    return ok;
}

} // namespace hui::demo
