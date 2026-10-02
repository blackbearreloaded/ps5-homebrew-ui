// ps5-homebrew-ui - Factories of every UI design in the app.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/concept.hpp"

#include <memory>

namespace hui::concepts
{

// One function per design, each defined in its own file in this directory.
// To add a design: write the file, declare its factory here and list it in
// registry.cpp. Nothing else in the app needs to change.
std::unique_ptr<app::Concept> make_aurora(app::Context &context);
std::unique_ptr<app::Concept> make_paper(app::Context &context);
std::unique_ptr<app::Concept> make_neon(app::Context &context);
std::unique_ptr<app::Concept> make_editorial(app::Context &context);
std::unique_ptr<app::Concept> make_carousel(app::Context &context);
std::unique_ptr<app::Concept> make_radial(app::Context &context);
std::unique_ptr<app::Concept> make_hud(app::Context &context);
std::unique_ptr<app::Concept> make_dashboard(app::Context &context);
std::unique_ptr<app::Concept> make_player(app::Context &context);
std::unique_ptr<app::Concept> make_keyboard(app::Context &context);
std::unique_ptr<app::Concept> make_constellation(app::Context &context);
std::unique_ptr<app::Concept> make_terminal(app::Context &context);
std::unique_ptr<app::Concept> make_settings(app::Context &context);
std::unique_ptr<app::Concept> make_themes(app::Context &context);
std::unique_ptr<app::Concept> make_toolbox(app::Context &context);

} // namespace hui::concepts
