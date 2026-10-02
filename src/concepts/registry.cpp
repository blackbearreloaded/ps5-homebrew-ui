// ps5-homebrew-ui - The list of UI designs, in switcher order.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "concepts/concepts.hpp"

namespace hui::app
{

std::span<const ConceptFactory> concept_registry()
{
    static constexpr ConceptFactory kFactories[] = {
        concepts::make_aurora,    concepts::make_paper,         concepts::make_neon,
        concepts::make_editorial, concepts::make_carousel,      concepts::make_radial,
        concepts::make_hud,       concepts::make_dashboard,     concepts::make_player,
        concepts::make_keyboard,  concepts::make_constellation, concepts::make_terminal,
        concepts::make_settings,  concepts::make_themes,        concepts::make_toolbox,
    };
    return kFactories;
}

} // namespace hui::app
