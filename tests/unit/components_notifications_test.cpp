// ps5-homebrew-ui - Tests: NotificationStack, NotificationCenter and NotificationBell behaviour.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "component_fixture.hpp"
#include "concepts/components/page.hpp"
#include "ui/components/notification.hpp"
#include "ui/components/notification_bell.hpp"
#include "ui/components/notification_center.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;
using hui::gfx::Rect;
using hui::ui::CloseReason;
using hui::ui::Event;
using hui::ui::Notification;
using hui::ui::NotificationBell;
using hui::ui::NotificationCenter;
using hui::ui::NotificationLook;
using hui::ui::NotificationStack;
using hui::ui::StatusKind;
using hui::ui::ToastAnchor;

class ComponentsNotifications : public hui::testing::ComponentFixture
{
  protected:
    struct Closed
    {
        int id;
        CloseReason reason;
    };

    static Notification note(const char *title, float seconds = -1.0f,
                             std::vector<std::string> actions = {})
    {
        Notification out;
        out.source = "System";
        out.time = "now";
        out.title = title;
        out.body = "A line of text that says what happened and what can be done about it.";
        out.seconds = seconds;
        out.actions = std::move(actions);
        return out;
    }

    // Runs the stack for a while, keeping every cue it asks for.
    void run(NotificationStack &stack, float seconds)
    {
        for (int i = 0, frames = static_cast<int>(seconds / kFrame); i < frames; ++i)
            stack.update(kFrame, feedback_);
    }

    // The stack reports through its slot; the tests read it from here.
    void listen(NotificationStack &stack)
    {
        stack.on_close = [this](int id, CloseReason reason) { closed_.push_back({id, reason}); };
    }

    int count(Cue cue) const
    {
        int total = 0;
        for (const hui::audio::CueEvent &event : feedback_.cues)
            total += event.cue == cue ? 1 : 0;
        return total;
    }

    std::vector<Closed> closed_;
};

TEST_F(ComponentsNotifications, TimedOnesLeaveStickyOnesStayAndBothSayWhy)
{
    NotificationStack stack;
    listen(stack);
    const int timed = stack.push(note("Friend online", 2.0f));
    const int sticky = stack.push(note("Update available"));
    const int usual = stack.push(note("Saved", 0.0f)); // style.duration
    EXPECT_EQ(timed, 1);
    EXPECT_EQ(stack.queued_count(), 3);
    EXPECT_FLOAT_EQ(stack.remaining(timed), -1.0f); // not on screen yet

    run(stack, 1.0f);
    EXPECT_EQ(stack.visible_count(), 3);
    EXPECT_EQ(stack.newest(), usual);
    EXPECT_NEAR(stack.remaining(timed), 0.5f, 0.05f);
    EXPECT_FLOAT_EQ(stack.remaining(sticky), 1.0f);
    EXPECT_NEAR(stack.remaining(usual), 1.0f - 1.0f / stack.style.duration, 0.05f);

    run(stack, 1.5f);
    EXPECT_EQ(stack.visible_count(), 2);
    ASSERT_EQ(closed_.size(), 1u);
    EXPECT_EQ(closed_[0].id, timed);
    EXPECT_EQ(closed_[0].reason, CloseReason::timed_out);

    run(stack, 30.0f);
    EXPECT_EQ(stack.visible_count(), 1);
    EXPECT_NE(stack.find(sticky), nullptr);
    EXPECT_EQ(stack.find(timed), nullptr);
    EXPECT_FALSE(stack.empty());
}

TEST_F(ComponentsNotifications, TimersStandStillWhileTheStackHasTheFocus)
{
    NotificationStack stack;
    const int id = stack.push(note("Low battery", 2.0f, {"Dismiss"}));
    run(stack, 0.5f);
    const float before = stack.remaining(id);

    stack.set_active(true);
    EXPECT_TRUE(stack.active());
    EXPECT_EQ(stack.focus_id(), id);
    run(stack, 5.0f);
    EXPECT_FLOAT_EQ(stack.remaining(id), before);
    EXPECT_EQ(stack.visible_count(), 1);

    stack.set_active(false);
    run(stack, 2.0f);
    EXPECT_EQ(stack.visible_count(), 0);

    // The knob turns the pause off.
    stack.style.pause_when_active = false;
    stack.push(note("Low battery", 1.0f, {"Dismiss"}));
    run(stack, 0.2f);
    stack.set_active(true);
    run(stack, 1.5f);
    EXPECT_EQ(stack.visible_count(), 0);
}

TEST_F(ComponentsNotifications, MoreThanTheLimitWaitAndEachChimesWhenItAppears)
{
    NotificationStack stack;
    stack.style.max_visible = 2;
    stack.style.duration = 1.0f;
    for (int i = 0; i < 5; ++i)
        stack.push(note("Saved", 0.0f));
    EXPECT_EQ(stack.visible_count(), 0);
    EXPECT_EQ(stack.queued_count(), 5);

    feedback_.clear();
    run(stack, 0.5f);
    EXPECT_EQ(stack.visible_count(), 2);
    EXPECT_EQ(stack.queued_count(), 3);
    EXPECT_EQ(count(Cue::notify), 2);

    run(stack, 8.0f);
    EXPECT_EQ(count(Cue::notify), 5);
    EXPECT_TRUE(stack.empty());

    // The kind sets the pitch: danger lower, success higher.
    feedback_.clear();
    Notification failed = note("Save failed");
    failed.kind = StatusKind::danger;
    stack.push(failed);
    run(stack, 0.1f);
    ASSERT_EQ(feedback_.cues.size(), 1u);
    EXPECT_LT(feedback_.cues[0].pitch, 1.0f);

    stack.style.sounds.notify = Cue::count;
    feedback_.clear();
    stack.push(note("Quiet"));
    run(stack, 0.2f);
    EXPECT_TRUE(feedback_.cues.empty());
}

TEST_F(ComponentsNotifications, AStackThatIsFullKeepsTheNextOneWaiting)
{
    NotificationStack stack;
    stack.style.max_visible = 5;
    stack.set_bounds({0.0f, 0.0f, 1920.0f, 380.0f});
    hui::ui::Canvas target = canvas();
    stack.draw(target); // heights need the fonts of a draw
    const int first = stack.push(note("One", -1.0f, {"Open"}));
    stack.push(note("Two", -1.0f, {"Open"}));
    stack.push(note("Three", -1.0f, {"Open"}));
    run(stack, 1.0f);
    EXPECT_EQ(stack.visible_count(), 1);
    EXPECT_EQ(stack.queued_count(), 2);
    const Rect card = stack.card_rect(fonts_, first);
    EXPECT_GT(card.h, 150.0f);
    EXPECT_FLOAT_EQ(card.w, stack.style.width);
    EXPECT_FLOAT_EQ(card.x + card.w, 1920.0f - stack.style.margin);
    EXPECT_FLOAT_EQ(card.y, stack.style.margin);
    const Rect button = stack.control_rect(fonts_, first, 0);
    EXPECT_GT(button.y, card.y);
    EXPECT_LE(button.y + button.h, card.y + card.h);

    // Room is made: the next one comes in.
    stack.dismiss(first);
    run(stack, 2.0f);
    EXPECT_EQ(stack.visible_count(), 1);
    EXPECT_EQ(stack.queued_count(), 1);

    stack.style.fit_bounds = false;
    run(stack, 0.5f);
    EXPECT_EQ(stack.visible_count(), 2);

    stack.style.anchor = ToastAnchor::bottom_left;
    const Rect low = stack.card_rect(fonts_, stack.newest());
    EXPECT_FLOAT_EQ(low.x, stack.style.margin);
    EXPECT_NEAR(low.y + low.h, 380.0f - stack.style.margin, 0.5f);
}

TEST_F(ComponentsNotifications, ContentChangesInPlaceUnderTheSameId)
{
    NotificationStack stack;
    listen(stack);
    Notification update = note("Version 2.4 is ready", -1.0f, {"Update now", "Later", "Never"});
    update.tag = 7;
    const int id = stack.push(update);
    ASSERT_NE(stack.find(id), nullptr);
    EXPECT_EQ(stack.find(id)->actions.size(), 2u); // more than two are dropped
    // Still waiting: the change is taken as well.
    EXPECT_TRUE(stack.set_title(id, "Version 2.5 is ready"));
    run(stack, 0.5f);
    EXPECT_EQ(stack.find(id)->title, "Version 2.5 is ready");
    EXPECT_EQ(stack.controls(id), 3);

    Notification download = note("Downloading", -1.0f);
    download.closable = false;
    download.progress = 0.0f;
    EXPECT_TRUE(stack.update_notification(id, download));
    EXPECT_EQ(stack.visible_count(), 1);
    EXPECT_EQ(stack.find(id)->title, "Downloading");
    EXPECT_EQ(stack.controls(id), 0);
    EXPECT_FALSE(stack.focusable());

    EXPECT_TRUE(stack.set_progress(id, 0.5f));
    EXPECT_TRUE(stack.set_body(id, "60 of 120 MB"));
    EXPECT_FLOAT_EQ(stack.find(id)->progress, 0.5f);
    EXPECT_EQ(stack.find(id)->body, "60 of 120 MB");
    hui::ui::Canvas target = canvas();
    stack.draw(target); // half way through the cross-fade
    run(stack, 1.0f);
    stack.draw(target);
    expect_drawn("progress");

    EXPECT_TRUE(stack.set_kind(id, StatusKind::success));
    EXPECT_TRUE(stack.set_actions(id, {"Restart"}));
    EXPECT_EQ(stack.find(id)->kind, StatusKind::success);
    EXPECT_EQ(stack.controls(id), 1);
    EXPECT_TRUE(stack.focusable());

    // A timed replacement starts its own clock.
    EXPECT_TRUE(stack.update_notification(id, note("Installed", 1.0f)));
    run(stack, 2.0f);
    ASSERT_EQ(closed_.size(), 1u);
    EXPECT_EQ(closed_[0].reason, CloseReason::timed_out);
    run(stack, 2.0f);
    EXPECT_FALSE(stack.update_notification(id, note("Too late")));
    EXPECT_FALSE(stack.set_progress(id, 1.0f));
    EXPECT_FALSE(stack.set_title(99, "Nobody"));
}

TEST_F(ComponentsNotifications, TheShortcutFiresTheNewestMainActionFromAnywhere)
{
    NotificationStack stack;
    listen(stack);
    int slot_id = 0;
    int slot_action = -1;
    stack.on_action = [&](int id, int action)
    {
        slot_id = id;
        slot_action = action;
    };
    const int older = stack.push(note("Friend request", -1.0f, {"Accept"}));
    const int plain = stack.push(note("Trophy earned"));
    run(stack, 0.5f);

    // No shortcut is set: the button is not the stack's.
    EXPECT_EQ(stack.handle_shortcut(press(Action::north), feedback_), Event::none);

    stack.style.shortcut = Action::north;
    stack.style.dismiss_shortcut = Action::west;
    feedback_.clear();
    EXPECT_EQ(stack.handle_shortcut(idle(), feedback_), Event::none);
    EXPECT_EQ(stack.handle_shortcut(press(Action::north), feedback_), Event::activated);
    EXPECT_EQ(stack.event_id(), older); // the newest one that has an action
    EXPECT_EQ(stack.event_action(), 0);
    EXPECT_EQ(slot_id, older);
    EXPECT_EQ(slot_action, 0);
    EXPECT_TRUE(asked(Cue::select));
    EXPECT_FALSE(stack.active());
    // The answer can still read what it answered.
    ASSERT_NE(stack.find(older), nullptr);
    EXPECT_EQ(stack.find(older)->title, "Friend request");
    run(stack, 0.1f);
    ASSERT_EQ(closed_.size(), 1u);
    EXPECT_EQ(closed_[0].id, older);
    EXPECT_EQ(closed_[0].reason, CloseReason::action);

    // Nothing with an action is left: the press is not taken.
    EXPECT_EQ(stack.handle_shortcut(press(Action::north), feedback_), Event::none);

    feedback_.clear();
    EXPECT_EQ(stack.handle_shortcut(press(Action::west), feedback_), Event::changed);
    EXPECT_EQ(stack.event_id(), plain);
    EXPECT_TRUE(asked(Cue::modal_close));
    run(stack, 0.1f);
    ASSERT_EQ(closed_.size(), 2u);
    EXPECT_EQ(closed_[1].reason, CloseReason::closed);

    // The glyph makes the main action wider when the buttons hug their labels.
    stack.style.stretch_actions = false;
    const int one = stack.push(note("Friend request", -1.0f, {"Accept"}));
    run(stack, 1.5f);
    const float with_glyph = stack.control_rect(fonts_, one, 0).w;
    stack.style.shortcut = Action::count;
    EXPECT_LT(stack.control_rect(fonts_, one, 0).w, with_glyph);
}

TEST_F(ComponentsNotifications, TheFocusMovesBetweenControlsAndNotifications)
{
    NotificationStack stack;
    listen(stack);
    const int lower = stack.push(note("Save failed", -1.0f, {"Retry", "Details"}));
    const int upper = stack.push(note("Friend request", -1.0f, {"Accept"}));
    run(stack, 0.5f);
    // Not active: it takes nothing.
    EXPECT_EQ(stack.handle(press(Action::confirm), feedback_), Event::none);

    stack.set_active(true);
    EXPECT_EQ(stack.focus_id(), upper); // the newest, on its main control
    EXPECT_EQ(stack.focus_control(), 0);

    feedback_.clear();
    EXPECT_EQ(stack.handle(nav(Direction::right), feedback_), Event::moved);
    EXPECT_TRUE(asked(Cue::focus));
    EXPECT_EQ(stack.focus_control(), 1); // the close control
    feedback_.clear();
    EXPECT_EQ(stack.handle(nav(Direction::right), feedback_), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    hui::InputFrame held = nav(Direction::right);
    held.nav_repeat = true;
    feedback_.clear();
    EXPECT_EQ(stack.handle(held, feedback_), Event::refused);
    EXPECT_TRUE(feedback_.cues.empty());

    // The newest is at the top (top anchor, newest first).
    EXPECT_EQ(stack.handle(nav(Direction::up), feedback_), Event::refused);
    // From a close control the focus goes to the other close control.
    EXPECT_EQ(stack.handle(nav(Direction::down), feedback_), Event::moved);
    EXPECT_EQ(stack.focus_id(), lower);
    EXPECT_EQ(stack.focus_control(), 2);
    EXPECT_EQ(stack.handle(nav(Direction::down), feedback_), Event::refused);
    EXPECT_EQ(stack.handle(nav(Direction::left), feedback_), Event::moved);
    EXPECT_EQ(stack.focus_control(), 1);
    EXPECT_EQ(stack.handle(nav(Direction::up), feedback_), Event::moved);
    EXPECT_EQ(stack.focus_id(), upper);
    EXPECT_EQ(stack.focus_control(), 0);
    EXPECT_TRUE(stack.set_focus(lower, 1));
    EXPECT_FALSE(stack.set_focus(99));

    feedback_.clear();
    EXPECT_EQ(stack.handle(press(Action::confirm), feedback_), Event::activated);
    EXPECT_TRUE(asked(Cue::select));
    EXPECT_EQ(stack.event_id(), lower);
    EXPECT_EQ(stack.event_action(), 1);
    run(stack, 0.1f);
    EXPECT_EQ(stack.focus_id(), upper); // the neighbour took the focus

    EXPECT_EQ(stack.handle(nav(Direction::right), feedback_), Event::moved);
    feedback_.clear();
    EXPECT_EQ(stack.handle(press(Action::confirm), feedback_), Event::changed);
    EXPECT_TRUE(asked(Cue::modal_close));
    EXPECT_EQ(stack.event_id(), upper);
    run(stack, 0.1f);
    ASSERT_EQ(closed_.size(), 2u);
    EXPECT_EQ(closed_[0].reason, CloseReason::action);
    EXPECT_EQ(closed_[1].reason, CloseReason::closed);

    // Nothing is left: the screen gets the focus back.
    EXPECT_EQ(stack.handle(idle(), feedback_), Event::cancelled);
    EXPECT_FALSE(stack.active());

    // Back does the same and says so.
    stack.push(note("One more", -1.0f, {"Open"}));
    run(stack, 0.5f);
    stack.set_active(true);
    feedback_.clear();
    EXPECT_EQ(stack.handle(press(Action::back), feedback_), Event::cancelled);
    EXPECT_TRUE(asked(Cue::back));
    EXPECT_FALSE(stack.active());
}

TEST_F(ComponentsNotifications, TheFocusGoesToANeighbourWhenItsNotificationLeaves)
{
    NotificationStack stack;
    const int first = stack.push(note("One", -1.0f, {"Open"}));
    const int second = stack.push(note("Two", -1.0f, {"Open"}));
    const int third = stack.push(note("Three", -1.0f, {"Open"}));
    const int silent = stack.push(note("No controls"));
    run(stack, 0.5f);
    EXPECT_EQ(stack.visible_count(), 3);
    stack.set_active(true);
    EXPECT_TRUE(stack.set_focus(second));

    // The application sends the focused one away: the one that slides into
    // its place (the older one, further from the edge) takes the focus.
    stack.dismiss(second);
    run(stack, 0.1f);
    EXPECT_EQ(stack.focus_id(), first);
    stack.dismiss(first);
    run(stack, 0.1f);
    EXPECT_EQ(stack.focus_id(), third);

    // The last one with a control goes: nothing can hold the focus.
    Notification bare = note("No controls");
    bare.closable = false;
    EXPECT_TRUE(stack.update_notification(silent, bare));
    stack.dismiss(third);
    run(stack, 0.1f);
    EXPECT_EQ(stack.focus_id(), 0);
    EXPECT_EQ(stack.handle(nav(Direction::down), feedback_), Event::cancelled);
    EXPECT_FALSE(stack.active());
}

TEST_F(ComponentsNotifications, AnActionAnsweredWithNewContentKeepsItsNotification)
{
    NotificationStack stack;
    listen(stack);
    const int id = stack.push(note("Version 2.4 is ready", -1.0f, {"Update now", "Later"}));
    run(stack, 0.5f);
    stack.set_active(true);
    EXPECT_EQ(stack.handle(press(Action::confirm), feedback_), Event::activated);
    EXPECT_EQ(stack.visible_count(), 0); // on its way out...
    Notification download = note("Downloading");
    download.closable = false;
    download.progress = 0.1f;
    EXPECT_TRUE(stack.update_notification(id, download));
    EXPECT_EQ(stack.visible_count(), 1); // ... and back
    run(stack, 1.0f);
    EXPECT_TRUE(closed_.empty());
    EXPECT_EQ(stack.find(id)->title, "Downloading");
    // It has no control any more: the focus returns to the screen.
    EXPECT_EQ(stack.handle(idle(), feedback_), Event::cancelled);

    // The same from the slot, and with the knob that keeps them open.
    EXPECT_TRUE(stack.update_notification(id, note("Installed", -1.0f, {"Restart"})));
    stack.style.close_on_action = false;
    stack.set_active(true);
    EXPECT_EQ(stack.handle(press(Action::confirm), feedback_), Event::activated);
    run(stack, 1.0f);
    EXPECT_EQ(stack.visible_count(), 1);
    EXPECT_TRUE(closed_.empty());

    stack.style.close_on_action = true;
    stack.on_action = [&stack](int which, int)
    { stack.update_notification(which, note("Restarting", 1.0f)); };
    EXPECT_EQ(stack.handle(press(Action::confirm), feedback_), Event::activated);
    run(stack, 0.5f);
    EXPECT_EQ(stack.find(id)->title, "Restarting");
    EXPECT_TRUE(closed_.empty());
}

TEST_F(ComponentsNotifications, DismissAndClearReportEveryOneAsDismissed)
{
    NotificationStack stack;
    listen(stack);
    stack.style.max_visible = 2;
    const int a = stack.push(note("A"));
    stack.push(note("B"));
    const int c = stack.push(note("C"));
    run(stack, 0.5f);
    EXPECT_EQ(stack.queued_count(), 1);

    stack.dismiss(c); // still waiting: it never appears
    stack.dismiss(a);
    EXPECT_EQ(stack.visible_count(), 1);
    run(stack, 0.1f);
    ASSERT_EQ(closed_.size(), 2u);
    EXPECT_EQ(closed_[0].reason, CloseReason::dismissed);
    EXPECT_EQ(closed_[1].reason, CloseReason::dismissed);

    stack.push(note("D"));
    stack.push(note("E"));
    stack.clear();
    EXPECT_EQ(stack.visible_count(), 0);
    EXPECT_EQ(stack.queued_count(), 0);
    run(stack, 3.0f);
    EXPECT_EQ(closed_.size(), 5u);
    EXPECT_TRUE(stack.empty());

    stack.push(note("F"));
    run(stack, 0.2f);
    stack.clear(true);
    EXPECT_TRUE(stack.empty());
    run(stack, 0.1f);
    EXPECT_EQ(closed_.size(), 6u);
}

TEST_F(ComponentsNotifications, TheCenterRecordsWhatLeftAndCountsWhatWasMissed)
{
    NotificationStack stack;
    NotificationCenter center;
    center.attach(stack);
    center.set_bounds({1300.0f, 100.0f, 520.0f, 800.0f});
    EXPECT_EQ(center.count(), 0);
    EXPECT_EQ(center.focus(), -1);
    EXPECT_EQ(center.handle(press(Action::confirm), feedback_), Event::none);

    stack.push(note("Friend online", 1.0f));
    const int request = stack.push(note("Friend request", -1.0f, {"Accept"}));
    const int failed = stack.push(note("Save failed"));
    run(stack, 2.0f);
    ASSERT_EQ(center.count(), 1);
    EXPECT_EQ(center.unread(), 1); // timed out unseen
    EXPECT_EQ(center.records()[0].reason, CloseReason::timed_out);

    // What the player answered or closed has been seen.
    stack.set_active(true);
    EXPECT_TRUE(stack.set_focus(request));
    EXPECT_EQ(stack.handle(press(Action::confirm), feedback_), Event::activated);
    run(stack, 0.2f);
    EXPECT_TRUE(stack.set_focus(failed));
    EXPECT_EQ(stack.handle(press(Action::confirm), feedback_), Event::changed);
    run(stack, 0.2f);
    ASSERT_EQ(center.count(), 3);
    EXPECT_EQ(center.unread(), 1);
    EXPECT_EQ(center.records()[0].notification.title, "Save failed"); // newest first
    EXPECT_EQ(center.records()[0].reason, CloseReason::closed);
    EXPECT_EQ(center.records()[1].reason, CloseReason::action);
    EXPECT_EQ(center.records()[1].action, 0);
    EXPECT_EQ(center.records()[1].id, request);

    // The list: "New" with the missed one first, then "Earlier".
    center.enter();
    EXPECT_EQ(center.focus(), 2);
    feedback_.clear();
    EXPECT_EQ(center.handle(press(Action::confirm), feedback_), Event::refused); // no action
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_EQ(center.handle(nav(Direction::down), feedback_), Event::moved);
    EXPECT_EQ(center.focus(), 0);
    EXPECT_EQ(center.handle(nav(Direction::down), feedback_), Event::moved);
    EXPECT_EQ(center.focus(), 1);
    int again = -1;
    center.on_action = [&](const hui::ui::NotificationRecord &record, int action)
    { again = record.id * 10 + action; };
    feedback_.clear();
    EXPECT_EQ(center.handle(press(Action::confirm), feedback_), Event::activated);
    EXPECT_TRUE(asked(Cue::select));
    ASSERT_NE(center.event_record(), nullptr);
    EXPECT_EQ(center.event_record()->notification.title, "Friend request");
    EXPECT_EQ(again, request * 10);
    EXPECT_EQ(center.handle(nav(Direction::down), feedback_), Event::refused);

    center.mark_all_read();
    EXPECT_EQ(center.unread(), 0);
    center.update(kFrame);
    EXPECT_EQ(center.focus(), 1); // the focus stayed on its record

    // Above the first row is "Clear all".
    center.enter();
    EXPECT_EQ(center.focus(), 0);
    EXPECT_EQ(center.handle(nav(Direction::up), feedback_), Event::moved);
    EXPECT_EQ(center.focus(), -1);
    EXPECT_EQ(center.handle(nav(Direction::up), feedback_), Event::refused);
    EXPECT_EQ(center.handle(nav(Direction::down), feedback_), Event::moved);
    EXPECT_EQ(center.handle(nav(Direction::up), feedback_), Event::moved);
    EXPECT_EQ(center.handle(press(Action::confirm), feedback_), Event::changed);
    EXPECT_EQ(center.count(), 0);
    feedback_.clear();
    EXPECT_EQ(center.handle(press(Action::back), feedback_), Event::cancelled);
    EXPECT_TRUE(asked(Cue::back));

    // By hand, with a limit, and with everything counted as unread.
    center.style.max_records = 3;
    center.style.missed_only = false;
    for (int i = 0; i < 5; ++i)
        center.record(note("By hand"), CloseReason::closed);
    EXPECT_EQ(center.count(), 3);
    EXPECT_EQ(center.unread(), 3);
    center.clear();
    EXPECT_EQ(center.count(), 0);

    center.detach(stack);
    stack.push(note("Unrecorded", 0.5f));
    run(stack, 2.0f);
    EXPECT_EQ(center.count(), 0);
}

TEST_F(ComponentsNotifications, TheBellCountsRingsAndOpens)
{
    NotificationBell bell;
    bell.set_bounds({1700.0f, 60.0f, 80.0f, 64.0f});
    EXPECT_FLOAT_EQ(bell.rect().w, 64.0f); // a square in the middle of the bounds
    EXPECT_FLOAT_EQ(bell.rect().x, 1708.0f);
    EXPECT_EQ(bell.count(), 0);
    EXPECT_FALSE(bell.ringing());

    bell.set_count(2);
    EXPECT_EQ(bell.count(), 2);
    EXPECT_TRUE(bell.ringing()); // something arrived
    hui::ui::Canvas target = canvas();
    for (int i = 0; i < 12; ++i)
        bell.update(kFrame);
    bell.draw(target); // mid swing
    expect_drawn("bell");
    for (int i = 0; i < 120; ++i)
        bell.update(kFrame);
    EXPECT_FALSE(bell.ringing());

    bell.set_count(1); // fewer: no ring
    EXPECT_FALSE(bell.ringing());
    bell.style.ring_on_increase = false;
    bell.set_count(5);
    EXPECT_FALSE(bell.ringing());
    bell.ring();
    EXPECT_TRUE(bell.ringing());

    bell.set_active(true);
    EXPECT_TRUE(bell.active());
    feedback_.clear();
    EXPECT_EQ(bell.handle(nav(Direction::left), feedback_), Event::none);
    EXPECT_TRUE(feedback_.cues.empty());
    EXPECT_EQ(bell.handle(press(Action::confirm), feedback_), Event::activated);
    EXPECT_TRUE(asked(Cue::select));
}

TEST_F(ComponentsNotifications, DrawInEveryThemeAndLook)
{
    const NotificationLook looks[] = {NotificationLook::card, NotificationLook::compact,
                                      NotificationLook::accent};
    const ToastAnchor anchors[] = {ToastAnchor::top_left,      ToastAnchor::top_center,
                                   ToastAnchor::top_right,     ToastAnchor::bottom_left,
                                   ToastAnchor::bottom_center, ToastAnchor::bottom_right};
    const StatusKind kinds[] = {StatusKind::info,   StatusKind::success,  StatusKind::warning,
                                StatusKind::danger, StatusKind::question, StatusKind::none};
    int round = 0;
    for (const hui::ui::Theme &theme : hui::ui::themes())
    {
        for (const NotificationLook look : looks)
        {
            ++round;
            const bool odd = (round & 1) != 0;
            NotificationStack stack;
            stack.style.theme = theme;
            stack.style.look = look;
            stack.style.anchor = anchors[round % 6];
            stack.style.reduced_motion = odd;
            stack.style.frosted = !odd;
            stack.style.time_bar = odd;
            stack.style.stretch_actions = !odd;
            stack.style.newest_first = round % 3 != 0;
            stack.style.fit_bounds = false;
            stack.style.max_visible = 4;
            stack.style.shortcut = odd ? Action::north : Action::count;
            stack.style.dismiss_shortcut = odd ? Action::west : Action::count;

            Notification wordy =
                note("A title that is far too long for a notification of this width to hold", 5.0f,
                     {"A main action with a long label", "Another long label"});
            wordy.kind = kinds[round % 6];
            wordy.source = "A source with a name that does not fit beside the time either";
            wordy.time = "12 minutes ago";
            stack.push(wordy);
            Notification bare = note("Bare");
            bare.source.clear();
            bare.time.clear();
            bare.body.clear();
            bare.kind = kinds[(round + 1) % 6];
            bare.closable = odd;
            stack.push(bare);
            Notification loading = note("Downloading");
            loading.kind = kinds[(round + 2) % 6];
            loading.progress = 0.4f;
            loading.icon = [](hui::ui::Canvas &target, const Rect &box)
            { target.list.circle(box.cx(), box.cy(), box.w * 0.5f, {1.0f, 1.0f, 1.0f, 1.0f}); };
            const int id = stack.push(loading);
            for (int i = 0; i < 4; ++i)
                stack.push(note("Waiting")); // the "+N more" marker

            hui::ui::Canvas target = canvas();
            run(stack, 0.05f); // half way in
            stack.draw(target);
            stack.set_active(true);
            run(stack, 0.6f);
            stack.handle(nav(Direction::right), feedback_);
            stack.update_notification(id, note("Installed", -1.0f, {"Restart"}));
            run(stack, 0.1f); // in the cross-fade, the ring in place
            stack.draw(target);
            stack.dismiss(id);
            run(stack, 0.1f); // one leaving
            stack.draw(target);
            expect_drawn(theme.id);

            NotificationCenter center;
            center.style.theme = theme;
            center.style.reduced_motion = odd;
            center.style.on_panel = !odd;
            center.style.clear_button = round % 4 != 0;
            center.style.relative_time = odd;
            center.set_bounds({1300.0f, 120.0f, 540.0f, 700.0f});
            center.update(kFrame);
            center.draw(target); // the empty state
            center.record(wordy, CloseReason::timed_out);
            center.record(bare, CloseReason::closed);
            center.record(loading, CloseReason::dismissed);
            center.enter();
            center.handle(nav(Direction::down), feedback_);
            for (int i = 0; i < 6; ++i)
                center.update(kFrame);
            center.draw(target);

            NotificationBell bell;
            bell.style.theme = theme;
            bell.style.reduced_motion = odd;
            bell.style.shape =
                odd ? hui::ui::IconButtonShape::square : hui::ui::IconButtonShape::round;
            bell.style.role = odd ? hui::ui::ButtonRole::ghost : hui::ui::ButtonRole::secondary;
            bell.set_bounds({1700.0f, 40.0f, 64.0f, 64.0f});
            bell.set_count(round);
            bell.set_active(odd);
            for (int i = 0; i < 8; ++i)
                bell.update(kFrame);
            bell.draw(target);
            expect_drawn(theme.id);
        }
    }
}

TEST_F(ComponentsNotifications, GalleryPageTourLeavesItAsItWasFound)
{
    const std::unique_ptr<hui::concepts::gallery::Page> page =
        hui::concepts::gallery::make_notifications_page(context_);
    EXPECT_STREQ(page->title(), "Notifications");
    const std::string first = page->variant();
    const std::size_t hints = page->hints().size();
    const std::string first_hint = page->hints()[2].label;
    int pictures = 0;
    const auto frame = [&](const hui::InputFrame &input)
    {
        feedback_.clear();
        page->update(input, kFrame, feedback_);
        // The stack measures with the fonts of a draw, as it would on screen.
        hui::ui::Canvas target = canvas();
        page->draw(target);
        page->draw_modal(target);
    };
    for (const hui::app::TourStep &step : page->tour())
    {
        for (int i = 0, frames = static_cast<int>(step.wait / kFrame); i < frames; ++i)
            frame(idle());
        if (step.capture != nullptr)
        {
            EXPECT_EQ(std::strncmp(step.capture, "notifications", 13), 0) << step.capture;
            ++pictures;
            expect_drawn(step.capture);
        }
        hui::InputFrame input = idle();
        input.pressed = step.press;
        input.held = step.press;
        input.nav = step.nav;
        frame(input);
    }
    EXPECT_GE(pictures, 4);
    EXPECT_LE(pictures, 6);
    EXPECT_GE(page->tour().size(), 10u);
    EXPECT_LE(page->tour().size(), 25u);
    for (int i = 0; i < 120; ++i)
        frame(idle());
    EXPECT_EQ(first, page->variant());
    EXPECT_EQ(page->hints().size(), hints) << "nothing is open";
    EXPECT_EQ(first_hint, page->hints()[2].label) << "the page has the focus";
    // Every look restyles in every theme without losing what is on screen.
    for (const hui::ui::Theme &theme : hui::ui::themes())
    {
        page->restyle(theme, false);
        frame(press(Action::confirm));
        frame(press(Action::west));
        for (int i = 0; i < 20; ++i)
            frame(idle());
        expect_drawn(theme.id);
    }
}

} // namespace
