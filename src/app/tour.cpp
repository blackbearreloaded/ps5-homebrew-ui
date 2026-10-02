// ps5-homebrew-ui - Automated tour: drives every design with scripted input.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/tour.hpp"

#include <cstdio>

namespace hui::app
{

namespace
{

// Time for a design's entrance animation to finish before its first picture.
constexpr float kIntroSeconds = 1.6f;
// Time the last state of a design stays up before moving on.
constexpr float kOutroSeconds = 0.6f;

} // namespace

Tour::Tour(Shell &shell, std::string only) : shell_(shell), only_(std::move(only))
{
    if (!only_.empty())
    {
        bool found = false;
        for (std::size_t i = 0; i < shell_.concept_count(); ++i)
        {
            if (only_ == shell_.concept_at(i).info().id)
            {
                index_ = i;
                found = true;
            }
        }
        finished_ = !found;
    }
    else
    {
        finished_ = shell_.concept_count() == 0;
    }
}

std::string Tour::picture_name(const char *suffix) const
{
    char name[96];
    const char *id = shell_.concept_at(index_).info().id;
    if (suffix == nullptr || suffix[0] == '\0')
        std::snprintf(name, sizeof(name), "%02zu-%s", index_ + 1, id);
    else
        std::snprintf(name, sizeof(name), "%02zu-%s-%s", index_ + 1, id, suffix);
    return name;
}

void Tour::begin_concept()
{
    // show() does nothing for the design already on screen, so leave first.
    if (shell_.current() == index_ && shell_.concept_count() > 1)
        shell_.show((index_ + 1) % shell_.concept_count(), false);
    shell_.show(index_, false);
    shell_.hide_banner();
    step_ = 0;
    clock_ = 0.0f;
    intro_done_ = false;
    stats_ = Stats{};
}

void Tour::end_concept()
{
    char line[200];
    const ConceptInfo &info = shell_.concept_at(index_).info();
    std::snprintf(line, sizeof(line),
                  "tour %02zu %-12s frames=%d avg_ms=%.2f worst_ms=%.2f draws=%zu shapes=%zu",
                  index_ + 1, info.id, stats_.frames,
                  stats_.frames > 0 ? stats_.total_ms / stats_.frames : 0.0,
                  static_cast<double>(stats_.worst_ms), stats_.draw_calls, stats_.instances);
    report_.push_back(line);
}

void Tour::record_frame(float frame_ms, std::size_t draw_calls, std::size_t instances)
{
    if (finished_ || !started_)
        return;
    ++stats_.frames;
    stats_.total_ms += static_cast<double>(frame_ms);
    if (frame_ms > stats_.worst_ms)
        stats_.worst_ms = frame_ms;
    if (draw_calls > stats_.draw_calls)
        stats_.draw_calls = draw_calls;
    if (instances > stats_.instances)
        stats_.instances = instances;
}

InputFrame Tour::step(float dt)
{
    InputFrame input;
    input.connected = true;
    if (finished_)
        return input;
    if (!started_)
    {
        started_ = true;
        begin_concept();
        return input;
    }
    if (!capture_.empty())
        return input; // the caller has not saved the last picture yet

    clock_ += dt;
    const std::span<const TourStep> script = shell_.concept_at(index_).tour();
    if (!intro_done_)
    {
        if (clock_ < kIntroSeconds)
            return input;
        intro_done_ = true;
        clock_ = 0.0f;
        capture_ = picture_name(nullptr);
        return input;
    }
    if (step_ < script.size())
    {
        const TourStep &now = script[step_];
        input.stick_x = now.stick_x;
        input.stick_y = now.stick_y;
        input.held = now.hold;
        input.trigger_l = now.trigger_l;
        input.trigger_r = now.trigger_r;
        if (clock_ < now.wait)
            return input;
        if (now.capture != nullptr && !pictured_)
        {
            // The picture shows the state before this step's input acts.
            pictured_ = true;
            capture_ = picture_name(now.capture);
            return input;
        }
        pictured_ = false;
        input.pressed = now.press;
        input.held = now.press | now.hold;
        input.nav = now.nav;
        ++step_;
        clock_ = 0.0f;
        return input;
    }
    if (clock_ < kOutroSeconds)
        return input;

    // After the first design, show the shell's info panel once.
    if (!info_shown_ && only_.empty())
    {
        shell_.set_info_open(true);
        if (clock_ < kOutroSeconds + 1.0f)
            return input;
        info_shown_ = true;
        pictured_ = true;
        capture_ = picture_name("info");
        return input;
    }
    if (pictured_)
    {
        pictured_ = false;
        shell_.set_info_open(false);
        clock_ = 0.0f;
        return input;
    }

    end_concept();
    if (!only_.empty() || index_ + 1 >= shell_.concept_count())
    {
        finished_ = true;
        return input;
    }
    ++index_;
    begin_concept();
    return input;
}

} // namespace hui::app
