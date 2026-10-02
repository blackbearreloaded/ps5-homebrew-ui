// ps5-homebrew-ui - Test fixture: a design with real fonts and no OpenGL.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/concept.hpp"
#include "core/save_file.hpp"
#include "demo/catalog.hpp"
#include "gfx/font.hpp"
#include "ui/fonts.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <string>

namespace hui::testing
{

// Loads the baked fonts and builds the Context a design needs. Covers are
// not rendered (texture 0): drawing only records shapes, so that is enough to
// exercise every code path of a design.
class ConceptFixture : public ::testing::Test
{
  protected:
    ConceptFixture() : context_{fonts_, catalog_, telemetry_, settings_}
    {
        load("inter-regular", &regular_, &fonts_.regular);
        load("inter-semibold", &semibold_, &fonts_.semibold);
        load("montserrat-medium", &display_, &fonts_.display);
        load("dejavu-sans-mono", &mono_, &fonts_.mono);
        load("press-start-2p", &pixel_, &fonts_.pixel);
        load("patrick-hand", &hand_, &fonts_.hand);
    }

    static InputFrame idle()
    {
        InputFrame input;
        input.connected = true;
        return input;
    }
    static InputFrame nav(Direction direction)
    {
        InputFrame input = idle();
        input.nav = direction;
        return input;
    }
    static InputFrame press(Action action)
    {
        InputFrame input = idle();
        input.pressed = action_bit(action);
        input.held = input.pressed;
        return input;
    }

    // Sends one input, then lets the design animate for `seconds`.
    void send(app::Concept &design, const InputFrame &input, float seconds = 0.5f)
    {
        feedback_.clear();
        design.update(input, 1.0f / 60.0f, feedback_);
        last_cues_ = feedback_.cues;
        for (int frame = 0, frames = static_cast<int>(seconds / (1.0f / 60.0f)); frame < frames;
             ++frame)
        {
            feedback_.clear();
            design.update(idle(), 1.0f / 60.0f, feedback_);
        }
    }

    // True if the last send() asked for this cue.
    bool played(audio::Cue cue) const
    {
        for (const audio::CueEvent &event : last_cues_)
        {
            if (event.cue == cue)
                return true;
        }
        return false;
    }

    // Draws the design and checks every recorded shape is finite.
    void expect_drawable(const app::Concept &design)
    {
        frame_.reset();
        design.draw(frame_);
        for (const gfx::DrawList *list : {&frame_.scene, &frame_.overlay})
        {
            for (const gfx::Instance &instance : list->instances())
            {
                for (float value : instance.rect)
                    ASSERT_TRUE(std::isfinite(value)) << design.info().id;
                for (float value : instance.color_top)
                    ASSERT_TRUE(std::isfinite(value)) << design.info().id;
            }
            for (const gfx::MeshVertex &vertex : list->mesh_vertices())
                ASSERT_TRUE(std::isfinite(vertex.x) && std::isfinite(vertex.y)) << design.info().id;
        }
        EXPECT_FALSE(frame_.scene.empty() && frame_.overlay.empty()) << design.info().id;
    }

    gfx::Font regular_, semibold_, display_, mono_, pixel_, hand_;
    ui::Fonts fonts_;
    demo::Catalog catalog_;
    app::Telemetry telemetry_;
    Settings settings_;
    app::Context context_;
    app::Feedback feedback_;
    std::vector<audio::CueEvent> last_cues_;
    app::Frame frame_;

  private:
    static void load(const char *name, gfx::Font *font, ui::FontRef *ref)
    {
        std::string data;
        const std::string path = std::string(HUI_SOURCE_DIR) + "/assets/fonts/" + name + ".huifont";
        ASSERT_TRUE(save::read_file(path, &data)) << path;
        ASSERT_TRUE(font->load(data)) << font->error();
        ref->font = font;
        ref->texture = 1;
    }
};

} // namespace hui::testing
