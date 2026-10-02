// ps5-homebrew-ui - Component Library: placeholders for pages still being built.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "concepts/components/page.hpp"

namespace hui::concepts::gallery
{

namespace
{

class StubPage final : public Page
{
  public:
    explicit StubPage(const char *title) : title_(title)
    {
    }
    const char *title() const override
    {
        return title_;
    }
    const char *summary() const override
    {
        return "This page is being built";
    }
    void restyle(const ui::Theme &theme, bool) override
    {
        theme_ = theme;
    }
    void update(const InputFrame &, float, ui::Feedback &) override
    {
    }
    void draw(ui::Canvas &canvas) const override
    {
        ui::Painter paint(canvas.list, canvas.fonts, theme_, canvas.glass);
        paint.panel(kPageArea);
        paint.heading(title_, kPageArea.cx(), kPageArea.cy(), 48.0f, gfx::Align::center);
    }
    std::span<const ui::Hint> hints() const override
    {
        return {};
    }

  private:
    const char *title_;
    ui::Theme theme_ = ui::default_theme();
};

} // namespace

// Each factory below disappears when its real page lands.
std::unique_ptr<Page> make_actions_page(app::Context &)
{
    return std::make_unique<StubPage>("Actions");
}
std::unique_ptr<Page> make_game_page(app::Context &)
{
    return std::make_unique<StubPage>("Game");
}

} // namespace hui::concepts::gallery
