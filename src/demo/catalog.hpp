// ps5-homebrew-ui - Invented sample content shared by every design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "gfx/draw_list.hpp"
#include "ui/fonts.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace hui::gfx
{
class Renderer;
}

namespace hui::demo
{

// One made-up title. Nothing here refers to a real product: the names,
// studios and numbers exist only to give the designs something to show.
struct Item
{
    const char *title;
    const char *studio;
    const char *genre;
    const char *blurb;
    int year;
    float rating;      // 0..5
    float progress;    // 0..1 of the story
    int hours;         // time played
    int players;       // local players supported
    gfx::Color dark;   // palette: the deepest tone of the cover
    gfx::Color mid;    // its body colour
    gfx::Color accent; // the colour a design may pick up as highlight
    // Square cover art, rendered at start-up. Draw it with
    // list.image(item.cover, rect, gfx::kCanvasUv, tint, radius).
    std::uint32_t cover = 0;
};

class Catalog
{
  public:
    Catalog();
    Catalog(const Catalog &) = delete;
    Catalog &operator=(const Catalog &) = delete;

    // Renders every cover into its own texture (needs a GL context). The
    // covers are drawn with the same renderer as the UI: a procedural
    // backdrop, a few shapes and the title set in the display font.
    bool build_covers(gfx::Renderer &renderer, const ui::Fonts &fonts);
    void release_covers();

    std::span<const Item> items() const
    {
        return items_;
    }
    std::size_t size() const
    {
        return items_.size();
    }
    const Item &operator[](std::size_t index) const
    {
        return items_[index % items_.size()];
    }

    static constexpr int kCoverSize = 512;

  private:
    std::vector<Item> items_;
};

} // namespace hui::demo
