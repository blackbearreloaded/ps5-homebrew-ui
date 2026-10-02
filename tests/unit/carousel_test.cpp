// ps5-homebrew-ui - Behaviour tests for the Cover Flow design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The carousel is a wheel with a position and a velocity, so besides the
// sounds these tests look at where the centred cover is drawn: at rest it is
// the largest image on screen and sits in the middle; stretched past an end
// it is pulled off-centre.

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"

#include <vector>

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;
using hui::audio::CueEvent;

constexpr float kFrame = 1.0f / 60.0f;
constexpr float kMiddle = 960.0f;  // where a cover at rest is centred
constexpr float kCentred = 440.0f; // ... and how large it is
constexpr int kStart = 3;          // the cover the design opens on
constexpr int kCovers = 24;

class CoverFlow : public hui::testing::ConceptFixture
{
  protected:
    // Holds the same input for `seconds` and returns every cue asked for.
    std::vector<CueEvent> hold(const hui::InputFrame &input, float seconds)
    {
        std::vector<CueEvent> cues;
        for (int frame = 0, frames = static_cast<int>(seconds / (kFrame)); frame < frames; ++frame)
        {
            feedback_.clear();
            design_->update(input, kFrame, feedback_);
            cues.insert(cues.end(), feedback_.cues.begin(), feedback_.cues.end());
        }
        return cues;
    }

    static hui::InputFrame stick(float x)
    {
        hui::InputFrame input = idle();
        input.stick_x = x;
        return input;
    }

    static int count(const std::vector<CueEvent> &cues, Cue cue)
    {
        int found = 0;
        for (const CueEvent &event : cues)
            found += event.cue == cue ? 1 : 0;
        return found;
    }

    // The pan of the cue the last send() asked for (0 if it did not).
    float pan_of(Cue cue) const
    {
        for (const CueEvent &event : last_cues_)
        {
            if (event.cue == cue)
                return event.pan;
        }
        return 0.0f;
    }

    // The largest image drawn: the cover nearest the centre of the row.
    hui::gfx::Rect front_cover()
    {
        frame_.reset();
        design_->draw(frame_);
        hui::gfx::Rect best;
        for (const hui::gfx::Instance &instance : frame_.scene.instances())
        {
            const bool image =
                static_cast<int>(instance.params[3]) == static_cast<int>(hui::gfx::Shape::image);
            if (image && instance.rect[2] * instance.rect[3] > best.w * best.h)
                best = {instance.rect[0], instance.rect[1], instance.rect[2], instance.rect[3]};
        }
        return best;
    }

    std::unique_ptr<hui::app::Concept> design_ = hui::concepts::make_carousel(context_);
};

TEST_F(CoverFlow, AStepSoundsOnItsSideAndTheFirstCoverRefuses)
{
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::focus));
    EXPECT_GT(pan_of(Cue::focus), 0.0f);
    for (int i = 0; i < kStart + 1; ++i)
    {
        send(*design_, nav(Direction::left));
        EXPECT_TRUE(played(Cue::focus));
        EXPECT_LT(pan_of(Cue::focus), 0.0f);
    }
    send(*design_, nav(Direction::left));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::focus));
    // The refusal pushed the wheel against its stop; it comes back to rest.
    hold(idle(), 1.5f);
    EXPECT_NEAR(front_cover().cx(), kMiddle, 0.5f);
}

TEST_F(CoverFlow, AHeldDirectionAtTheEndStaysQuiet)
{
    for (int i = 0; i < kStart; ++i)
        send(*design_, nav(Direction::left));
    hui::InputFrame held = nav(Direction::left);
    held.nav_repeat = true;
    send(*design_, held);
    EXPECT_TRUE(last_cues_.empty());
}

TEST_F(CoverFlow, TheTriggersJumpFiveCoversAndStopAtTheLast)
{
    // 3 -> 8 -> 13 -> 18 -> 23: the last jump is shorter but still a jump.
    for (int at = kStart; at < kCovers - 1; at += 5)
    {
        send(*design_, press(Action::jump_next));
        EXPECT_TRUE(played(Cue::tab));
    }
    send(*design_, press(Action::jump_next));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::tab));
    send(*design_, press(Action::jump_prev));
    EXPECT_TRUE(played(Cue::tab));
}

TEST_F(CoverFlow, TheStickScrubsThenTheWheelCoastsAndSettlesOnACover)
{
    const std::vector<CueEvent> scrubbed = hold(stick(1.0f), 0.8f);
    EXPECT_GT(count(scrubbed, Cue::tick), 2);
    EXPECT_EQ(count(scrubbed, Cue::focus), 0); // the stick's nav repeats are not steps

    // Let go: the wheel keeps turning on its own and covers keep passing.
    const std::vector<CueEvent> coasted = hold(idle(), 0.4f);
    EXPECT_GT(count(coasted, Cue::tick), 0);

    // ... and it comes to rest exactly on one of them.
    hold(idle(), 3.0f);
    const hui::gfx::Rect cover = front_cover();
    EXPECT_NEAR(cover.cx(), kMiddle, 0.5f);
    EXPECT_NEAR(cover.w, kCentred, 0.5f);
    EXPECT_TRUE(hold(idle(), 0.5f).empty());
}

TEST_F(CoverFlow, TicksAreLimitedToARateTheEarCanSeparate)
{
    // A full second at full deflection passes about ten covers.
    const std::vector<CueEvent> cues = hold(stick(1.0f), 1.0f);
    EXPECT_GT(count(cues, Cue::tick), 4);
    EXPECT_LE(count(cues, Cue::tick), 12);
}

TEST_F(CoverFlow, ScrubbingIntoAnEndStretchesRefusesOnceAndSpringsBack)
{
    const std::vector<CueEvent> cues = hold(stick(-1.0f), 1.5f);
    EXPECT_EQ(count(cues, Cue::error), 1);
    // Held against the stop, the first cover is pulled off-centre.
    EXPECT_GT(front_cover().cx(), kMiddle + 40.0f);
    EXPECT_LT(front_cover().cx(), kMiddle + 260.0f);

    EXPECT_EQ(count(hold(idle(), 1.5f), Cue::error), 0);
    EXPECT_NEAR(front_cover().cx(), kMiddle, 0.5f);
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::focus));
}

TEST_F(CoverFlow, AnOpenCoverLocksTheWheelUntilBack)
{
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::open));
    // The details own the input now: the row does not move.
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(last_cues_.empty());
    EXPECT_TRUE(hold(stick(1.0f), 0.3f).empty());
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::launch));
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::favorite_on));
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::favorite_off));
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::back));
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::focus));
    expect_drawable(*design_);
}

TEST_F(CoverFlow, ReducedMotionKeepsTheRefusalButNotTheStretch)
{
    settings_.reduced_motion = true;
    const std::vector<CueEvent> cues = hold(stick(-1.0f), 1.5f);
    EXPECT_EQ(count(cues, Cue::error), 1);
    EXPECT_NEAR(front_cover().cx(), kMiddle, 0.5f);
    // Letting go mid-spin is caught at once instead of coasting on: the wheel
    // is at rest well before a coasting one would even have slowed down.
    hold(stick(1.0f), 0.5f);
    hold(idle(), 0.8f);
    EXPECT_NEAR(front_cover().cx(), kMiddle, 0.5f);
    EXPECT_NEAR(front_cover().w, kCentred, 0.5f);
}

} // namespace
