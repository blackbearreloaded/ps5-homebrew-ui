// ps5-homebrew-ui - Automated tour: drives every design with scripted input.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/shell.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace hui::app
{

// Plays the app without a person: shows each design in turn, replays the
// inputs its tour() describes and marks the frames worth saving as pictures.
// The same tour renders the documentation images on a PC (make
// host-snapshots) and validates a build on the console (a tour.txt trigger
// file), where it also collects frame-time statistics per design.
class Tour
{
  public:
    // only: when not empty, tour just the design with that id.
    explicit Tour(Shell &shell, std::string only = {});

    // Advances the script by dt and returns the input for this frame.
    InputFrame step(float dt);
    bool finished() const
    {
        return finished_;
    }
    // Name of the picture to save from the frame just drawn, or "".
    const std::string &capture() const
    {
        return capture_;
    }
    void capture_done()
    {
        capture_.clear();
    }
    // Feed every presented frame's time here (console statistics).
    void record_frame(float frame_ms, std::size_t draw_calls, std::size_t instances);
    // One line per design: frames, average and worst frame time, draw calls.
    const std::vector<std::string> &report() const
    {
        return report_;
    }

  private:
    struct Stats
    {
        int frames = 0;
        double total_ms = 0.0;
        float worst_ms = 0.0f;
        std::size_t draw_calls = 0;
        std::size_t instances = 0;
    };

    void begin_concept();
    void end_concept();
    std::string picture_name(const char *suffix) const;

    Shell &shell_;
    std::string only_;
    std::size_t index_ = 0; // design being toured
    std::size_t step_ = 0;  // next step of its script
    float clock_ = 0.0f;    // seconds into the current wait
    bool started_ = false;
    bool intro_done_ = false;
    bool finished_ = false;
    bool info_shown_ = false;
    bool pictured_ = false; // the current step's picture has been requested
    std::string capture_;
    Stats stats_;
    std::vector<std::string> report_;
};

} // namespace hui::app
