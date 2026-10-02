// ps5-homebrew-ui - Component Library: the contract of one gallery page.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The gallery (src/concepts/components.cpp) owns the header, the theme, the
// page switch and the hint row. A page owns a group of components: it lays
// them out inside the area it is given, forwards input to the one that has
// the focus, and shows what their style knobs do.

#pragma once

#include "app/concept.hpp"
#include "ui/components/component.hpp"
#include "ui/glyphs.hpp"

#include <memory>
#include <span>

namespace hui::concepts::gallery
{

// Where a page draws, in virtual pixels. The header is above, the hints below.
constexpr gfx::Rect kPageArea{96.0f, 236.0f, 1728.0f, 716.0f};

class Page
{
  public:
    virtual ~Page() = default;

    virtual const char *title() const = 0;   // "Lists"
    virtual const char *summary() const = 0; // one line under the title
    // What Square currently shows ("Highlight: bar"); empty when the page has
    // no variants.
    virtual const char *variant() const
    {
        return "";
    }

    // Give every component of the page this theme. Called once at start and
    // again whenever the gallery's theme or the reduced-motion setting changes.
    virtual void restyle(const ui::Theme &theme, bool reduced_motion) = 0;
    // The page became visible: replay entrances, close anything modal.
    virtual void enter()
    {
    }
    // Every input except the gallery's own: L1/R1 and the touchpad (shell),
    // L2/R2 (page) and Options (theme).
    virtual void update(const InputFrame &input, float dt, ui::Feedback &feedback) = 0;
    virtual void draw(ui::Canvas &canvas) const = 0;
    // Drawn after the page and the gallery's header, under the hint row:
    // scrims, dialogs, sheets and toasts go here.
    virtual void draw_modal(ui::Canvas &canvas) const
    {
        (void)canvas;
    }
    virtual std::span<const ui::Hint> hints() const = 0;
    // Inputs that show the page off; the gallery appends "next page" itself.
    // Leave the page as it was found (nothing open, first variant).
    virtual std::span<const app::TourStep> tour() const
    {
        return {};
    }
};

using PageFactory = std::unique_ptr<Page> (*)(app::Context &context);

std::unique_ptr<Page> make_lists_page(app::Context &context);
std::unique_ptr<Page> make_collections_page(app::Context &context);
std::unique_ptr<Page> make_navigation_page(app::Context &context);
std::unique_ptr<Page> make_overlays_page(app::Context &context);
std::unique_ptr<Page> make_forms_page(app::Context &context);
std::unique_ptr<Page> make_indicators_page(app::Context &context);
std::unique_ptr<Page> make_structure_page(app::Context &context);
std::unique_ptr<Page> make_actions_page(app::Context &context);
std::unique_ptr<Page> make_pickers_page(app::Context &context);
std::unique_ptr<Page> make_entry_page(app::Context &context);
std::unique_ptr<Page> make_data_page(app::Context &context);
std::unique_ptr<Page> make_media_page(app::Context &context);
std::unique_ptr<Page> make_game_page(app::Context &context);
std::unique_ptr<Page> make_layout_page(app::Context &context);
std::unique_ptr<Page> make_notifications_page(app::Context &context);

} // namespace hui::concepts::gallery
