// ps5-homebrew-ui - Test fixture for components: fonts, a canvas, every theme.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "concept_fixture.hpp"
#include "ui/components/component.hpp"

#include <cmath>

namespace hui::testing
{

// A component needs fonts to measure and draw text; the design fixture loads
// them. Drawing records shapes only, so no OpenGL is involved.
class ComponentFixture : public ConceptFixture
{
  protected:
    static constexpr float kFrame = 1.0f / 60.0f;

    // A fresh canvas over the fixture's draw list.
    ui::Canvas canvas()
    {
        list_.clear();
        return {list_, fonts_, 0, 1.25f};
    }

    // True if `feedback_` holds this cue (clear it before the input you test).
    bool asked(audio::Cue cue) const
    {
        for (const audio::CueEvent &event : feedback_.cues)
        {
            if (event.cue == cue)
                return true;
        }
        return false;
    }

    // Every shape the last draw recorded is finite, and there is at least one.
    void expect_drawn(const char *what)
    {
        EXPECT_FALSE(list_.empty()) << what;
        for (const gfx::Instance &instance : list_.instances())
        {
            for (float value : instance.rect)
                ASSERT_TRUE(std::isfinite(value)) << what;
            for (float value : instance.color_top)
                ASSERT_TRUE(std::isfinite(value)) << what;
        }
    }

    gfx::DrawList list_;
};

} // namespace hui::testing
