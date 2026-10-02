// ps5-homebrew-ui - The shell: switches between designs and draws shared chrome.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/concept.hpp"
#include "core/tween.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace hui::gfx
{
class Renderer;
}

namespace hui::app
{

// Owns every design and shows one at a time.
//
//   L1 / R1    previous / next design (cross-fade and slide)
//   touchpad   info panel: what the design demonstrates and where its code is
//
// Everything else goes to the active design. The shell also keeps the
// settings on disk and feeds the telemetry the designs may display.
class Shell
{
  public:
    Shell(const ui::Fonts &fonts, const demo::Catalog &catalog, std::string data_root,
          std::uint32_t glass_texture);

    static Settings load_settings(const std::string &data_root);

    void update(const InputFrame &input, float dt);
    // Queues this frame's layers; the caller presents them.
    void compose(gfx::Renderer &renderer);
    // Records the active design's frame without rendering it. A console draws
    // every frame; a tour on a PC only renders its pictures, so it calls this
    // on the other frames to keep draw-time work (lazy text layout) honest.
    void rehearse();

    // Sounds and rumble requested during the last update.
    const Feedback &feedback() const
    {
        return feedback_;
    }
    audio::SoundSet sound_set() const;
    gfx::Color accent() const;

    const Settings &settings() const
    {
        return settings_;
    }
    // True once after the settings changed: apply volumes, button swap, ...
    bool take_settings_changed();

    Telemetry &telemetry()
    {
        return telemetry_;
    }
    void set_version(std::string version)
    {
        version_ = std::move(version);
    }

    // ---- for the tour and tests ----
    std::size_t concept_count() const
    {
        return concepts_.size();
    }
    std::size_t current() const
    {
        return current_;
    }
    const Concept &concept_at(std::size_t index) const
    {
        return *concepts_[index];
    }
    // animate = false switches at once (and still restarts the entrance).
    void show(std::size_t index, bool animate);
    void set_info_open(bool open);
    // Drops the switcher banner at once (the tour keeps it out of pictures).
    void hide_banner()
    {
        banner_ = 0.0f;
        banner_show_.snap(0.0f);
    }
    bool transitioning() const
    {
        return transition_.running;
    }

  private:
    void record(Frame &frame, const Concept &design, float opacity, float dx, float scale);
    void draw_chrome();
    void save_settings();

    ui::Fonts fonts_;
    std::string data_root_;
    Settings settings_;
    Telemetry telemetry_;
    Context context_;
    std::vector<std::unique_ptr<Concept>> concepts_;
    std::size_t current_ = 0;
    std::size_t previous_ = 0;
    int direction_ = 1;
    tween::Timer transition_;
    Frame frames_[2];
    gfx::DrawList chrome_;
    Feedback feedback_;
    std::uint32_t glass_texture_ = 0;
    std::string version_;
    float banner_ = 0.0f; // seconds the switcher banner stays up
    tween::Spring banner_show_;
    bool info_open_ = false;
    tween::Spring info_show_;
    bool settings_changed_ = true; // applied once at start-up
    float save_delay_ = 0.0f;      // > 0: a save is pending
    bool welcomed_ = false;
};

} // namespace hui::app
