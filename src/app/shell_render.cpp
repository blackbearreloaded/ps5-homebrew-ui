// ps5-homebrew-ui - The shell's frame composition (the part that needs the renderer).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/shell.hpp"

#include "gfx/renderer.hpp"

namespace hui::app
{

namespace
{
// How far a design slides while it is switched in or out.
constexpr float kSlide = 90.0f;
} // namespace

void Shell::compose(gfx::Renderer &renderer)
{
    renderer.begin();
    if (concepts_.empty())
        return;
    Frame &now = frames_[0];
    if (transition_.running)
    {
        // The old design fades and slides away while the new one arrives
        // from the other side; their backdrops cross-fade underneath.
        const float t = transition_.progress();
        const float out = 1.0f - tween::smoothstep(t * 1.7f);
        const float in = tween::smoothstep((t - 0.2f) / 0.8f);
        const float slide = settings_.reduced_motion ? 0.0f : kSlide;
        const float move = tween::quint_out(t);
        const float direction = static_cast<float>(direction_);
        Frame &old = frames_[1];
        record(old, *concepts_[previous_], out, -direction * slide * move,
               settings_.reduced_motion ? 1.0f : 1.0f - 0.03f * move);
        record(now, *concepts_[current_], in, direction * slide * (1.0f - move),
               settings_.reduced_motion ? 1.0f : 0.97f + 0.03f * move);
        renderer.backdrop(old.backdrop);
        renderer.backdrop(now.backdrop, tween::smoothstep(t));
        renderer.draw(old.scene);
        renderer.draw(old.overlay);
        renderer.draw(now.scene);
        if (now.glass)
            renderer.glass();
        renderer.draw(now.overlay);
        renderer.backdrop(old.post, 1.0f - t);
        renderer.backdrop(now.post, t);
    }
    else
    {
        record(now, *concepts_[current_], 1.0f, 0.0f, 1.0f);
        renderer.backdrop(now.backdrop);
        renderer.draw(now.scene);
        if (now.glass)
            renderer.glass();
        renderer.draw(now.overlay);
        renderer.backdrop(now.post);
    }

    chrome_.clear();
    draw_chrome();
    if (info_show_.value > 0.01f)
        renderer.glass(); // the info panel frosts whatever the design drew
    renderer.draw(chrome_);
}

} // namespace hui::app
