// ps5-homebrew-ui - Behaviour tests for the File Browser design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The browser's state is read from what it asks to hear. A focus tick is
// pitched by its row (1.12 at the top, 0.01 lower per row), a mark climbs
// with the number of entries selected, and the cue that ends an operation is
// pitched by how full the drive now is. The tree is the invented one built in
// files.cpp: "Internal" opens with five folders and three files, and the USB
// drive's "Videos" holds four clips of 1.5, 2.25, 0.75 and 3 GB.

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"

#include <cmath>

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;
using hui::audio::CueEvent;

class Files : public hui::testing::ConceptFixture
{
  protected:
    // The last send()'s request for a cue, or nullptr.
    const CueEvent *heard(Cue cue) const
    {
        for (const CueEvent &event : last_cues_)
        {
            if (event.cue == cue)
                return &event;
        }
        return nullptr;
    }

    // The row the last focus move landed on, or -1 when nothing moved.
    int row(Cue cue = Cue::focus) const
    {
        const CueEvent *event = heard(cue);
        return event != nullptr ? static_cast<int>(std::lround((1.12f - event->pitch) / 0.01f))
                                : -1;
    }

    void step(Direction direction, int times = 1)
    {
        for (int i = 0; i < times; ++i)
            send(*design_, nav(direction), 0.2f);
    }

    // Runs frame by frame until the cue is asked for.
    bool wait_for(Cue cue, float seconds)
    {
        for (int frame = 0, frames = static_cast<int>(seconds / (1.0f / 60.0f)); frame < frames;
             ++frame)
        {
            send(*design_, idle(), 0.0f);
            if (played(cue))
                return true;
        }
        return false;
    }

    // Through the rail to the USB drive, then into its "Videos" folder.
    void open_usb_videos()
    {
        step(Direction::left);
        step(Direction::down);
        send(*design_, press(Action::confirm));
        ASSERT_TRUE(played(Cue::open));
        step(Direction::down, 3);
        ASSERT_EQ(row(), 3);
        send(*design_, press(Action::confirm));
        ASSERT_TRUE(played(Cue::open));
    }

    std::unique_ptr<hui::app::Concept> design_ = hui::concepts::make_files(context_);
};

TEST_F(Files, LeavingAFolderLandsOnItAndComingBackRestoresTheFocus)
{
    step(Direction::down);
    EXPECT_EQ(row(), 1); // Captures
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::open));
    step(Direction::down, 2);
    EXPECT_EQ(row(), 2); // its third entry
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::back));
    // Back in the root the focus is on Captures, not at the top: one step
    // down is the third row.
    step(Direction::down);
    EXPECT_EQ(row(), 2);
    step(Direction::up);
    EXPECT_EQ(row(), 1);
    // Captures remembers where it was left.
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::open));
    step(Direction::up);
    EXPECT_EQ(row(), 1);
    expect_drawable(*design_);
}

TEST_F(Files, SortingKeepsFoldersFirstAndTheFocusOnItsEntry)
{
    // By name the last of the eight rows is system-report.zip (44 MB, July).
    send(*design_, press(Action::jump_next));
    EXPECT_EQ(row(Cue::slide), 7);
    // By size it is the largest file, yet all five folders stay above it.
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(Cue::tab));
    step(Direction::up);
    EXPECT_EQ(row(), 4);
    step(Direction::down);
    EXPECT_EQ(row(), 5);
    // By date (newest first) it is the oldest file: last again.
    send(*design_, press(Action::north));
    step(Direction::down);
    EXPECT_TRUE(played(Cue::error));
    // By kind an archive comes before the two text files.
    send(*design_, press(Action::north));
    step(Direction::up);
    EXPECT_EQ(row(), 4);
    // And once more around, back to names.
    send(*design_, press(Action::north));
    step(Direction::down);
    step(Direction::down);
    step(Direction::down);
    EXPECT_EQ(row(), 7);
    expect_drawable(*design_);
}

TEST_F(Files, MarksClimbAndDeletingTheSelectionFreesItsBytes)
{
    open_usb_videos();
    // Delete the focused clip (1.5 GB): the dialog starts on Cancel.
    send(*design_, press(Action::menu));
    step(Direction::down, 4);
    send(*design_, press(Action::confirm));
    step(Direction::right);
    send(*design_, press(Action::confirm));
    ASSERT_NE(heard(Cue::erase), nullptr);
    const float after_one = heard(Cue::erase)->pitch;

    // Mark the next two (2.25 GB and 0.75 GB); the mark climbs with the count.
    send(*design_, press(Action::west));
    ASSERT_NE(heard(Cue::mark), nullptr);
    const float first = heard(Cue::mark)->pitch;
    step(Direction::down);
    send(*design_, press(Action::west));
    const float second = heard(Cue::mark)->pitch;
    EXPECT_GT(second, first);
    send(*design_, press(Action::west)); // unmarked: one left
    EXPECT_FLOAT_EQ(heard(Cue::mark)->pitch, first);
    send(*design_, press(Action::west));
    EXPECT_FLOAT_EQ(heard(Cue::mark)->pitch, second);

    // With a selection the menu opens on Copy; Delete is three below.
    send(*design_, press(Action::menu));
    step(Direction::down, 3);
    send(*design_, press(Action::confirm));
    step(Direction::right);
    send(*design_, press(Action::confirm));
    ASSERT_NE(heard(Cue::erase), nullptr);
    // The cue is pitched by how full the drive is: 3 GB of 32 GB less.
    EXPECT_NEAR(after_one - heard(Cue::erase)->pitch, 0.4f * 3.0f / 32.0f, 1e-4f);

    // One clip is left: both directions are the end of the list.
    step(Direction::down);
    EXPECT_TRUE(played(Cue::error));
    step(Direction::up);
    EXPECT_TRUE(played(Cue::error));
    expect_drawable(*design_);
}

TEST_F(Files, DeleteAsksFirstAndTheSafeAnswerIsTheDefault)
{
    send(*design_, press(Action::jump_next));
    send(*design_, press(Action::menu));
    step(Direction::down, 4);
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::select));
    expect_drawable(*design_);
    EXPECT_TRUE(frame_.glass);
    // Confirming straight away cancels: nothing is erased.
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::modal_close));
    EXPECT_FALSE(played(Cue::erase));
    // All eight rows are still there and the focus is still on the last.
    step(Direction::up);
    EXPECT_EQ(row(), 6);
    step(Direction::down);
    EXPECT_EQ(row(), 7);
    step(Direction::down);
    EXPECT_TRUE(played(Cue::error));
}

TEST_F(Files, CopyingAddsTheEntryToTheDestination)
{
    open_usb_videos();
    send(*design_, press(Action::menu));
    step(Direction::down); // Copy
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::select));
    // The folder picker starts on the first place that can take the clip,
    // the internal drive. Choosing it starts the copy.
    send(*design_, press(Action::confirm), 0.2f);
    EXPECT_TRUE(played(Cue::select));
    // While the copy runs the list does not take input.
    step(Direction::down);
    EXPECT_TRUE(last_cues_.empty());
    // Nothing exists in the destination until the bar is full.
    EXPECT_TRUE(wait_for(Cue::notify, 4.0f));
    const float filled = heard(Cue::notify)->pitch;

    // The source is still here: four clips.
    send(*design_, press(Action::jump_next));
    EXPECT_EQ(row(Cue::slide), 3);
    // The internal drive now lists nine entries instead of eight.
    step(Direction::left);
    step(Direction::up);
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::open));
    send(*design_, press(Action::jump_next));
    EXPECT_EQ(row(Cue::slide), 8);

    // A second copy of something else fills the drive further.
    send(*design_, press(Action::menu));
    step(Direction::down);
    send(*design_, press(Action::confirm));
    // Now the picker starts on the last destination, which is where the
    // entry already is: a copy there is allowed and gets a new name.
    send(*design_, press(Action::confirm), 0.2f);
    EXPECT_TRUE(played(Cue::select));
    EXPECT_TRUE(wait_for(Cue::notify, 4.0f));
    EXPECT_GT(heard(Cue::notify)->pitch, filled);
    send(*design_, press(Action::jump_next));
    send(*design_, press(Action::jump_prev));
    send(*design_, press(Action::jump_next));
    EXPECT_EQ(row(Cue::slide), 9);
    expect_drawable(*design_);
}

TEST_F(Files, TheContextMenuTakesTheInput)
{
    step(Direction::down);
    EXPECT_EQ(row(), 1);
    send(*design_, press(Action::menu));
    EXPECT_TRUE(played(Cue::modal_open));
    expect_drawable(*design_);
    EXPECT_TRUE(frame_.glass); // the popover is frosted glass
    // Down now moves in the menu; Square marks nothing behind it.
    step(Direction::down, 2);
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, press(Action::west));
    EXPECT_FALSE(played(Cue::mark));
    // Its ends refuse like the list's.
    step(Direction::down, 3);
    step(Direction::down);
    EXPECT_TRUE(played(Cue::error));
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::modal_close));
    // The list did not move underneath: one step up is the top row.
    step(Direction::up);
    EXPECT_EQ(row(), 0);
    expect_drawable(*design_);
    EXPECT_FALSE(frame_.glass);
}

TEST_F(Files, EdgesRefuseSoftlyAndAHeldDirectionStaysQuiet)
{
    step(Direction::up);
    const CueEvent *refusal = heard(Cue::error);
    ASSERT_NE(refusal, nullptr);
    EXPECT_LT(refusal->gain, 1.0f); // a soft one
    EXPECT_FALSE(played(Cue::focus));
    hui::InputFrame held = nav(Direction::up);
    held.nav_repeat = true;
    send(*design_, held);
    EXPECT_TRUE(last_cues_.empty());
    // There is nothing above a drive's root, and nothing right of the list.
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::error));
    step(Direction::right);
    EXPECT_TRUE(played(Cue::error));
    // Left is the rail, whose own edges refuse too.
    step(Direction::left);
    EXPECT_TRUE(played(Cue::focus));
    step(Direction::left);
    EXPECT_TRUE(played(Cue::error));
    step(Direction::up);
    EXPECT_TRUE(played(Cue::error));
    held = nav(Direction::left);
    held.nav_repeat = true;
    send(*design_, held);
    EXPECT_TRUE(last_cues_.empty());
    // A page jump at the end of the list is refused as well.
    step(Direction::right);
    send(*design_, press(Action::jump_next));
    EXPECT_EQ(row(Cue::slide), 7);
    send(*design_, press(Action::jump_next));
    EXPECT_TRUE(played(Cue::error));
}

TEST_F(Files, SeveralMarkedEntriesDimTheActionsForOne)
{
    send(*design_, press(Action::west));
    step(Direction::down);
    send(*design_, press(Action::west));
    EXPECT_TRUE(played(Cue::mark));
    send(*design_, press(Action::menu));
    EXPECT_TRUE(played(Cue::modal_open));
    // The menu opened on Copy; Open, above it, cannot take two entries.
    step(Direction::up);
    EXPECT_TRUE(played(Cue::focus));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(Cue::invalid));
    EXPECT_FALSE(played(Cue::open));
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::modal_close));
    // Back leaves the selection first, then finds nothing above the root.
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::undo));
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(Cue::error));
    expect_drawable(*design_);
}

} // namespace
