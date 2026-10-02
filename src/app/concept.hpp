// ps5-homebrew-ui - The contract between the shell and one UI design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "audio/cues.hpp"
#include "core/input.hpp"
#include "core/settings.hpp"
#include "demo/catalog.hpp"
#include "gfx/backdrop_spec.hpp"
#include "gfx/draw_list.hpp"
#include "ui/feedback.hpp"
#include "ui/fonts.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace hui::app
{

// What a design asks the platform to do this frame besides drawing: sounds
// and controller rumble. The shell plays the cues with the design's sound set.
// It lives in the kit (ui/feedback.hpp) so components can ask for both too.
using Feedback = ui::Feedback;

// Live numbers about the app itself, for designs that show real data.
struct Telemetry
{
    static constexpr std::size_t kHistory = 240;
    std::array<float, kHistory> frame_ms{}; // ring buffer, newest at head - 1
    std::size_t head = 0;
    float fps = 0.0f;
    float average_ms = 0.0f;
    std::size_t draw_calls = 0;
    std::size_t instances = 0;
    int voices = 0;      // sounds playing now
    double uptime = 0.0; // seconds since the app opened

    void push(float ms)
    {
        frame_ms[head] = ms;
        head = (head + 1) % kHistory;
    }
    // age 0 is the newest frame.
    float sample(std::size_t age) const
    {
        return frame_ms[(head + kHistory - 1 - age % kHistory) % kHistory];
    }
};

// Long-lived services a design receives when it is created.
struct Context
{
    const ui::Fonts &fonts;
    const demo::Catalog &catalog;
    const Telemetry &telemetry;
    // Live settings. A design that edits them sets settings_changed; the app
    // applies the change (volumes, button swap, ...) and saves it.
    Settings &settings;
    bool settings_changed = false;
};

// One frame of a design, back to front:
//   backdrop  -> scene -> [glass capture] -> overlay -> post
struct Frame
{
    gfx::BackdropSpec backdrop; // procedural shader behind everything
    gfx::DrawList scene;        // the screen itself
    gfx::DrawList overlay;      // dialogs and drawers, above the glass capture
    gfx::BackdropSpec post;     // optional overlay shader (scanlines, vignette)
    // Set glass to have the backdrop and scene blurred into glass_texture
    // before the overlay is drawn; overlay.glass(glass_texture, ...) then
    // paints frosted panels.
    bool glass = false;
    std::uint32_t glass_texture = 0;

    void reset()
    {
        backdrop = {};
        post = {};
        scene.clear();
        overlay.clear();
        glass = false;
    }
};

// One scripted input for the automated tour (host snapshots and the console
// validation run): wait, optionally save a picture, then send the input.
struct TourStep
{
    float wait = 0.5f;       // seconds before the step acts
    std::uint32_t press = 0; // action bits pressed for one frame
    Direction nav = Direction::none;
    const char *capture = nullptr; // picture name suffix, taken after the wait
    float stick_x = 0.0f;          // left stick held during the wait
    float stick_y = 0.0f;
    std::uint32_t hold = 0; // action bits held during the wait
    float trigger_l = 0.0f; // analog triggers held during the wait
    float trigger_r = 0.0f;
};

struct ConceptInfo
{
    const char *id;      // short, lowercase: file names and logs
    const char *name;    // shown in the switcher
    const char *tagline; // one line: what the design is for
    const char *source;  // repository path of its implementation
    audio::SoundSet sounds = audio::SoundSet::glass;
    gfx::Color accent;                       // light bar and shell chrome
    std::span<const char *const> techniques; // listed in the info panel
};

// A complete, self-contained UI design. The shell owns the L1/R1 switch and
// the touchpad info panel; every other input reaches the active design.
class Concept
{
  public:
    virtual ~Concept() = default;
    virtual const ConceptInfo &info() const = 0;
    // The design became the active one: restart its entrance animation.
    virtual void enter()
    {
    }
    virtual void update(const InputFrame &input, float dt, Feedback &feedback) = 0;
    // Must not change state: the shell may draw a design more than once per
    // frame (transitions, the glass copy).
    virtual void draw(Frame &frame) const = 0;
    // Inputs that show the design off; empty means "just look at it".
    virtual std::span<const TourStep> tour() const
    {
        return {};
    }
};

using ConceptFactory = std::unique_ptr<Concept> (*)(Context &context);

// Every design, in switcher order (src/concepts/registry.cpp).
std::span<const ConceptFactory> concept_registry();

} // namespace hui::app
