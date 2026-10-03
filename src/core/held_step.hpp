// ps5-homebrew-ui - A pair of buttons held down: one step, then more, faster.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/input.hpp"

namespace hui
{

// Buttons other than the directions do not repeat by themselves. This gives a
// pair of them (L2 / R2 by default: turn a page, jump a letter) the feel of a
// held direction: a press is one step; held, the steps go on after a pause
// and then come faster, so a long list is crossed without hammering.
//
//   hui::HeldStep pages;
//   ...
//   if (const int step = pages.step(input, dt))
//       turn_page(step, /*quiet=*/pages.repeating());
class HeldStep
{
  public:
    explicit HeldStep(Action back = Action::jump_prev, Action forward = Action::jump_next)
        : back_(back), forward_(forward)
    {
    }

    float delay = 0.35f;         // seconds held before the first repeat
    float interval = 0.11f;      // seconds between the first repeats
    float fast_interval = 0.05f; // and between the later ones
    int fast_after = 6;          // repeats before the pace picks up

    // Call once a frame. -1 (back), 1 (forward) or 0.
    int step(const InputFrame &input, float dt)
    {
        const int pressed = input.is_pressed(forward_) ? 1 : input.is_pressed(back_) ? -1 : 0;
        if (pressed != 0)
        {
            direction_ = pressed;
            waited_ = 0.0f;
            repeats_ = 0;
            return pressed;
        }
        const bool held =
            (direction_ > 0 && input.is_held(forward_)) || (direction_ < 0 && input.is_held(back_));
        if (!held)
        {
            direction_ = 0;
            repeats_ = 0;
            return 0;
        }
        waited_ += dt;
        const float wait =
            repeats_ == 0 ? delay : (repeats_ < fast_after ? interval : fast_interval);
        if (waited_ < wait)
            return 0;
        waited_ = 0.0f;
        ++repeats_;
        return direction_;
    }

    // The last step came from holding, not from a press: play a quieter cue
    // and skip the glide.
    bool repeating() const
    {
        return repeats_ > 0;
    }

  private:
    Action back_;
    Action forward_;
    int direction_ = 0;
    float waited_ = 0.0f;
    int repeats_ = 0;
};

} // namespace hui
