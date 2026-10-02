// ps5-homebrew-ui - Behaviour tests for the Trophy Room design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The cabinet's rules, checked through what a design exposes: the input it
// takes and the cues it asks for. Two of its cues carry state on purpose, and
// the tests read it: the focus tick is pitched by the tier of the row it
// lands on, and the unlock fanfare climbs with the share of achievements
// earned.

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"

#include <vector>

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;

constexpr int kAchievements = 28;
constexpr int kEarned = 17;
constexpr float kEmberPitch = 0.94f;
constexpr float kGoldPitch = 1.06f;

class Trophies : public hui::testing::ConceptFixture
{
  protected:
    float pitch_of(Cue cue) const
    {
        for (const hui::audio::CueEvent &event : last_cues_)
        {
            if (event.cue == cue)
                return event.pitch;
        }
        return 0.0f;
    }

    // Walks from the focused row to the end of the list and returns the pitch
    // of every tick on the way: one per row below the starting one.
    std::vector<float> walk_down()
    {
        std::vector<float> pitches;
        for (int i = 0; i < 2 * kAchievements; ++i)
        {
            send(*design_, nav(Direction::down), 0.05f);
            if (!played(Cue::focus))
                break;
            pitches.push_back(pitch_of(Cue::focus));
        }
        return pitches;
    }

    // Rows in the list, counted from a freshly chosen filter (focus on top).
    int rows()
    {
        const int count = 1 + static_cast<int>(walk_down().size());
        EXPECT_TRUE(played(Cue::error)); // the walk ended against the list's end
        return count;
    }

    // Lets time pass and returns how often a cue was asked for meanwhile.
    int heard(Cue cue, float seconds)
    {
        int count = 0;
        for (int frame = 0, frames = static_cast<int>(seconds / (1.0f / 60.0f)); frame < frames;
             ++frame)
        {
            send(*design_, idle(), 0.0f);
            count += played(cue) ? 1 : 0;
        }
        return count;
    }

    void filter_next(int times)
    {
        for (int i = 0; i < times; ++i)
        {
            send(*design_, press(Action::jump_next), 0.3f);
            ASSERT_TRUE(played(Cue::tab));
        }
    }

    static float fanfare_pitch(int earned)
    {
        return 0.96f + 0.12f * static_cast<float>(earned) / static_cast<float>(kAchievements);
    }

    std::unique_ptr<hui::app::Concept> design_ = hui::concepts::make_trophies(context_);
};

TEST_F(Trophies, FiltersNarrowTheListAndTheLastTabRefuses)
{
    EXPECT_EQ(rows(), kAchievements);
    filter_next(1); // Unlocked
    EXPECT_EQ(rows(), kEarned);
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::error)); // nothing here is left to earn
    filter_next(1);                  // Locked
    EXPECT_EQ(rows(), kAchievements - kEarned);

    filter_next(3); // Ember, Silver, Gold
    const std::vector<float> gold = walk_down();
    EXPECT_EQ(gold.size(), 4u); // five Gold achievements
    for (float pitch : gold)
        EXPECT_FLOAT_EQ(pitch, kGoldPitch);

    filter_next(1); // Prism: a single row
    EXPECT_EQ(rows(), 1);
    send(*design_, press(Action::jump_next));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::tab));
    expect_drawable(*design_);
}

TEST_F(Trophies, AHeldDirectionAtAnEndStaysQuiet)
{
    send(*design_, nav(Direction::up));
    EXPECT_TRUE(played(Cue::error));
    hui::InputFrame held = nav(Direction::up);
    held.nav_repeat = true;
    send(*design_, held);
    EXPECT_TRUE(last_cues_.empty());
}

TEST_F(Trophies, SquareSortsAndTheFocusStaysOnItsAchievement)
{
    // Recent: the newest medal is on top, and it is already earned.
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::error));

    // Rarity: the same achievement is still focused, now 17 rows down.
    send(*design_, press(Action::west), 1.0f);
    EXPECT_TRUE(played(Cue::cascade));
    int above = 0;
    for (int i = 0; i < kAchievements; ++i)
    {
        send(*design_, nav(Direction::up), 0.05f);
        if (!played(Cue::focus))
            break;
        ++above;
    }
    EXPECT_EQ(above, 17);
    // The rarest achievement leads the list: it is the locked Prism one.
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::complete));

    // Tier: Prism stays on top, the Gold tier follows, Ember closes the list.
    send(*design_, press(Action::west), 1.0f);
    EXPECT_TRUE(played(Cue::cascade));
    const std::vector<float> pitches = walk_down();
    ASSERT_EQ(pitches.size(), static_cast<std::size_t>(kAchievements - 1));
    EXPECT_FLOAT_EQ(pitches.front(), kGoldPitch);
    EXPECT_FLOAT_EQ(pitches.back(), kEmberPitch);
    expect_drawable(*design_);
}

TEST_F(Trophies, TriangleEarnsALockedAchievementExactlyOnce)
{
    filter_next(2); // Locked; the closest one (an Ember) is on top
    send(*design_, press(Action::north), 0.1f);
    EXPECT_TRUE(played(Cue::new_record));
    EXPECT_FALSE(played(Cue::complete));
    // The fanfare's pitch is the new share: 18 of 28.
    EXPECT_FLOAT_EQ(pitch_of(Cue::new_record), fanfare_pitch(kEarned + 1));

    // The row is still there for a moment, earned: a second press refuses.
    send(*design_, press(Action::north), 0.1f);
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::new_record));

    // Then it leaves the "Locked" list and the next one moves up: the Prism.
    send(*design_, idle(), 2.0f);
    send(*design_, press(Action::north), 2.0f);
    EXPECT_TRUE(played(Cue::complete)); // the top tier gets the big fanfare
    EXPECT_FALSE(played(Cue::new_record));
    EXPECT_FLOAT_EQ(pitch_of(Cue::complete), fanfare_pitch(kEarned + 2));
    EXPECT_EQ(rows(), kAchievements - kEarned - 2);
    expect_drawable(*design_);
}

TEST_F(Trophies, AHiddenAchievementUncoversItsNameABeatLater)
{
    filter_next(2);
    // An ordinary achievement has nothing to uncover.
    send(*design_, press(Action::north), 0.0f);
    EXPECT_TRUE(played(Cue::new_record));
    EXPECT_EQ(heard(Cue::reveal, 2.5f), 0);

    // Six rows down the list (by progress) sits the first hidden one.
    for (int i = 0; i < 6; ++i)
        send(*design_, nav(Direction::down), 0.05f);
    send(*design_, press(Action::north), 0.0f);
    EXPECT_TRUE(played(Cue::new_record));
    EXPECT_FALSE(played(Cue::reveal)); // not yet: the fanfare comes first
    EXPECT_EQ(heard(Cue::reveal, 2.5f), 1);
    expect_drawable(*design_);
}

TEST_F(Trophies, TheDetailSheetOwnsTheInputUntilItCloses)
{
    filter_next(2);
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::modal_open));
    expect_drawable(*design_);
    EXPECT_TRUE(frame_.glass);
    EXPECT_FALSE(frame_.overlay.empty());

    // Under the sheet nothing moves, sorts, filters or unlocks.
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(last_cues_.empty());
    send(*design_, press(Action::jump_next));
    EXPECT_TRUE(last_cues_.empty());
    send(*design_, press(Action::west));
    EXPECT_TRUE(last_cues_.empty());
    send(*design_, press(Action::north));
    EXPECT_TRUE(last_cues_.empty());

    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::modal_close));
    // The focused achievement is still locked: the press above earned nothing.
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::new_record));
}

TEST_F(Trophies, EarningEverythingCelebratesAndLeavesAnEmptyListThatStillWorks)
{
    filter_next(2);
    int fanfares = 0;
    int grand = 0;
    float last_pitch = 0.0f;
    for (int i = 0; i < kAchievements - kEarned; ++i)
    {
        send(*design_, press(Action::north), 2.0f);
        const Cue cue = played(Cue::complete) ? Cue::complete : Cue::new_record;
        ASSERT_TRUE(played(cue)) << "unlock " << i;
        ++fanfares;
        grand += cue == Cue::complete ? 1 : 0;
        EXPECT_GT(pitch_of(cue), last_pitch); // the share only ever rises
        last_pitch = pitch_of(cue);
    }
    EXPECT_EQ(fanfares, kAchievements - kEarned);
    EXPECT_EQ(grand, 2); // the Prism, and the one that completed the cabinet
    EXPECT_TRUE(played(Cue::complete));
    EXPECT_FLOAT_EQ(last_pitch, fanfare_pitch(kAchievements));

    // Nothing is left under "Locked": every action answers with a refusal.
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::error));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::error));
    EXPECT_FALSE(played(Cue::modal_open));
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(Cue::error));
    expect_drawable(*design_);

    // The other filters carry on: "Unlocked" now holds all of them.
    send(*design_, press(Action::jump_prev));
    EXPECT_TRUE(played(Cue::tab));
    EXPECT_EQ(rows(), kAchievements);
    expect_drawable(*design_);
}

} // namespace
