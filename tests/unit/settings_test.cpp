// ps5-homebrew-ui - Behaviour tests for the Control Room (settings) design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The controls of this design are real, so the tests check the settings
// themselves and the flag that tells the app to apply and save them, not
// only the sounds.

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;

class ControlRoom : public hui::testing::ConceptFixture
{
  protected:
    // L2/R2 work from anywhere; the focus stays in the column it was in.
    void go_to_category(int index)
    {
        for (int i = 0; i < 4; ++i)
            send(*design_, press(Action::jump_prev), 0.1f);
        for (int i = 0; i < index; ++i)
            send(*design_, press(Action::jump_next), 0.1f);
    }

    // Lets time pass and counts how often a cue was asked for.
    int count_cue(Cue cue, float seconds)
    {
        int count = 0;
        for (int frame = 0, frames = static_cast<int>(seconds / (1.0f / 60.0f)); frame < frames;
             ++frame)
        {
            feedback_.clear();
            design_->update(idle(), 1.0f / 60.0f, feedback_);
            for (const hui::audio::CueEvent &event : feedback_.cues)
                count += event.cue == cue ? 1 : 0;
        }
        return count;
    }

    std::unique_ptr<hui::app::Concept> design_ = hui::concepts::make_settings(context_);
};

TEST_F(ControlRoom, RightOnMusicVolumeRaisesItAndTellsTheApp)
{
    send(*design_, nav(Direction::right)); // from the rail into the rows
    EXPECT_TRUE(played(Cue::focus));
    EXPECT_FALSE(context_.settings_changed);

    const int before = settings_.music_volume;
    send(*design_, nav(Direction::right));
    EXPECT_EQ(settings_.music_volume, before + 1);
    EXPECT_TRUE(context_.settings_changed);
    ASSERT_TRUE(played(Cue::slider));
    const float low = last_cues_.front().pitch;

    // The cue rises with the value.
    send(*design_, nav(Direction::right));
    EXPECT_EQ(settings_.music_volume, before + 2);
    EXPECT_GT(last_cues_.front().pitch, low);

    send(*design_, nav(Direction::left));
    EXPECT_EQ(settings_.music_volume, before + 1);
    expect_drawable(*design_);
}

TEST_F(ControlRoom, ASliderRefusesSoftlyAtItsEndsAndAHeldDirectionStaysQuiet)
{
    settings_.music_volume = 10;
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::select));

    send(*design_, nav(Direction::right));
    EXPECT_EQ(settings_.music_volume, 10);
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::slider));
    EXPECT_FALSE(context_.settings_changed);

    hui::InputFrame held = nav(Direction::right);
    held.nav_repeat = true;
    send(*design_, held);
    EXPECT_TRUE(last_cues_.empty());
    EXPECT_EQ(settings_.music_volume, 10);
}

TEST_F(ControlRoom, SwitchesFlipTheRealSettingsAndVibrationConfirmsItself)
{
    go_to_category(2); // Controller
    send(*design_, press(Action::confirm));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(settings_.swap_confirm);
    EXPECT_TRUE(played(Cue::toggle));
    EXPECT_TRUE(context_.settings_changed);
    send(*design_, press(Action::confirm));
    EXPECT_FALSE(settings_.swap_confirm);

    // Vibration: off is silent in the hand, on answers with a rumble.
    send(*design_, nav(Direction::down));
    feedback_.clear();
    design_->update(press(Action::confirm), 1.0f / 60.0f, feedback_);
    EXPECT_FALSE(settings_.haptics);
    EXPECT_EQ(feedback_.rumble_strength, 0.0f);
    feedback_.clear();
    design_->update(press(Action::confirm), 1.0f / 60.0f, feedback_);
    EXPECT_TRUE(settings_.haptics);
    EXPECT_GT(feedback_.rumble_strength, 0.0f);

    send(*design_, nav(Direction::down));
    send(*design_, press(Action::confirm));
    EXPECT_FALSE(settings_.light_bar);
}

TEST_F(ControlRoom, BackReturnsToTheRailAndEachCategoryRemembersItsRow)
{
    send(*design_, press(Action::confirm));
    send(*design_, nav(Direction::down)); // Effects volume
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::back));

    // On the rail, up and down change the category.
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::tab));
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::tab));
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::error));

    // Back in the rows the focus is where it was left: on Effects volume.
    const int music = settings_.music_volume;
    const int effects = settings_.sfx_volume;
    send(*design_, nav(Direction::right));
    send(*design_, nav(Direction::left));
    EXPECT_EQ(settings_.sfx_volume, effects - 1);
    EXPECT_EQ(settings_.music_volume, music);
}

TEST_F(ControlRoom, TheStepperWalksItsChoicesAndStopsAtTheLast)
{
    go_to_category(1); // Display
    send(*design_, press(Action::confirm));
    send(*design_, nav(Direction::down));
    send(*design_, nav(Direction::down)); // Hint size
    ASSERT_EQ(settings_.hint_size, 1);
    send(*design_, nav(Direction::right));
    EXPECT_EQ(settings_.hint_size, 2);
    EXPECT_TRUE(played(Cue::slider));
    send(*design_, nav(Direction::right));
    EXPECT_EQ(settings_.hint_size, 2);
    EXPECT_TRUE(played(Cue::error));

    // A switch has nothing to adjust: left leads back to the rail instead.
    send(*design_, nav(Direction::up));
    send(*design_, nav(Direction::left));
    EXPECT_TRUE(played(Cue::focus));
    EXPECT_FALSE(settings_.show_fps);
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::tab));
}

TEST_F(ControlRoom, ResetAsksFirstFocusesCancelAndKeepsTheStartDesign)
{
    settings_.music_volume = 9;
    settings_.reduced_motion = true;
    settings_.concept_index = 2;
    go_to_category(4); // About
    send(*design_, press(Action::confirm));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::modal_open));
    expect_drawable(*design_);
    EXPECT_TRUE(frame_.glass);

    // Confirming straight away takes the safe choice.
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::modal_close));
    EXPECT_EQ(settings_.music_volume, 9);
    EXPECT_FALSE(context_.settings_changed);

    send(*design_, press(Action::confirm));
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, press(Action::confirm));
    EXPECT_EQ(settings_.music_volume, hui::Settings{}.music_volume);
    EXPECT_FALSE(settings_.reduced_motion);
    EXPECT_EQ(settings_.concept_index, 2);
    EXPECT_TRUE(context_.settings_changed);
    expect_drawable(*design_);
    EXPECT_FALSE(frame_.glass);
}

TEST_F(ControlRoom, ABurstOfChangesGivesOneSavedToast)
{
    send(*design_, nav(Direction::right));
    for (int i = 0; i < 4; ++i)
    {
        send(*design_, nav(Direction::right), 0.2f);
        EXPECT_FALSE(played(Cue::saved));
    }
    EXPECT_EQ(count_cue(Cue::saved, 0.3f), 0); // still inside the wait
    EXPECT_EQ(count_cue(Cue::saved, 4.0f), 1);

    // A change while the toast is still up keeps it up without a second chime.
    send(*design_, nav(Direction::left), 0.0f);
    EXPECT_EQ(count_cue(Cue::saved, 1.2f), 1);
    send(*design_, nav(Direction::left), 0.0f);
    EXPECT_EQ(count_cue(Cue::saved, 4.0f), 0);
}

TEST_F(ControlRoom, ReduceMotionStopsTheBackdropAtOnce)
{
    expect_drawable(*design_);
    const float before = frame_.backdrop.time;
    send(*design_, idle(), 0.5f);
    expect_drawable(*design_);
    EXPECT_GT(frame_.backdrop.time, before);

    go_to_category(3); // Accessibility
    send(*design_, press(Action::confirm));
    send(*design_, press(Action::confirm), 0.0f);
    EXPECT_TRUE(settings_.reduced_motion);
    expect_drawable(*design_);
    const float frozen = frame_.backdrop.time;
    send(*design_, idle(), 0.5f);
    expect_drawable(*design_);
    EXPECT_EQ(frame_.backdrop.time, frozen);
}

} // namespace
