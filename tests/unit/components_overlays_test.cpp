// ps5-homebrew-ui - Tests: Dialog, Sheet, ToastStack and Tooltip behaviour.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "component_fixture.hpp"
#include "ui/components/dialog.hpp"
#include "ui/components/sheet.hpp"
#include "ui/components/toast.hpp"
#include "ui/components/tooltip.hpp"

#include <gtest/gtest.h>

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;
using hui::gfx::Rect;
using hui::ui::ButtonKind;
using hui::ui::Dialog;
using hui::ui::DialogContent;
using hui::ui::Event;
using hui::ui::Sheet;
using hui::ui::SheetEdge;
using hui::ui::StatusKind;
using hui::ui::ToastAnchor;
using hui::ui::ToastStack;
using hui::ui::Tooltip;
using hui::ui::TooltipPlacement;

class ComponentsOverlays : public hui::testing::ComponentFixture
{
  protected:
    static DialogContent destructive()
    {
        DialogContent content;
        content.icon = StatusKind::danger;
        content.title = "Delete this save?";
        content.body = "Slot 2, Lantern Pass, 63 hours. This cannot be undone.";
        content.buttons = {{"Cancel", ButtonKind::secondary, false},
                           {"Delete", ButtonKind::primary, true}};
        content.default_button = 1; // asks for the destructive one
        return content;
    }

    Event send(Dialog &dialog, const hui::InputFrame &input)
    {
        feedback_.clear();
        const Event event = dialog.handle(input, feedback_);
        settle(dialog);
        return event;
    }

    template <typename Component> void settle(Component &component, float seconds = 1.0f)
    {
        for (int i = 0, frames = static_cast<int>(seconds / kFrame); i < frames; ++i)
            component.update(kFrame);
    }

    int count(Cue cue) const
    {
        int total = 0;
        for (const hui::audio::CueEvent &event : feedback_.cues)
            total += event.cue == cue ? 1 : 0;
        return total;
    }

    // Runs the toast stack for a while, keeping every cue it asks for.
    void run(ToastStack &toasts, float seconds)
    {
        for (int i = 0, frames = static_cast<int>(seconds / kFrame); i < frames; ++i)
            toasts.update(kFrame, feedback_);
    }
};

TEST_F(ComponentsOverlays, DialogOpensOnTheSafeButton)
{
    Dialog dialog;
    EXPECT_FALSE(dialog.is_open());
    EXPECT_FALSE(dialog.visible());
    dialog.open(destructive(), feedback_);
    EXPECT_TRUE(dialog.is_open());
    EXPECT_TRUE(asked(Cue::modal_open));
    EXPECT_EQ(dialog.focus(), 0);
    EXPECT_EQ(dialog.choice(), -1);

    // Without a destructive button the default is honoured.
    DialogContent plain = destructive();
    plain.buttons[1].destructive = false;
    dialog.open(plain, feedback_);
    EXPECT_EQ(dialog.focus(), 1);

    // No buttons at all: the dialog still has an answer.
    dialog.open(DialogContent{}, feedback_);
    ASSERT_EQ(dialog.content().buttons.size(), 1u);
    EXPECT_EQ(dialog.focus(), 0);
}

TEST_F(ComponentsOverlays, DialogMovesBetweenButtonsAndRefusesAtTheEnds)
{
    Dialog dialog;
    dialog.open(destructive(), feedback_);
    EXPECT_EQ(send(dialog, nav(Direction::left)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_EQ(send(dialog, nav(Direction::right)), Event::moved);
    EXPECT_TRUE(asked(Cue::focus));
    EXPECT_EQ(dialog.focus(), 1);
    hui::InputFrame held = nav(Direction::right);
    held.nav_repeat = true;
    EXPECT_EQ(send(dialog, held), Event::refused);
    EXPECT_TRUE(feedback_.cues.empty());
    // A row ignores the other axis; a stack uses it.
    EXPECT_EQ(send(dialog, nav(Direction::up)), Event::none);
    dialog.style.buttons = hui::ui::DialogButtons::stacked;
    EXPECT_EQ(send(dialog, nav(Direction::up)), Event::moved);
    EXPECT_EQ(dialog.focus(), 0);
    EXPECT_EQ(send(dialog, nav(Direction::left)), Event::none);
}

TEST_F(ComponentsOverlays, DialogConfirmGivesTheChoiceAndCloses)
{
    Dialog dialog;
    dialog.open(destructive(), feedback_);
    settle(dialog);
    feedback_.clear();
    dialog.handle(nav(Direction::right), feedback_);
    feedback_.clear();
    EXPECT_EQ(dialog.handle(press(Action::confirm), feedback_), Event::activated);
    EXPECT_TRUE(asked(Cue::select));
    EXPECT_EQ(dialog.choice(), 1);
    EXPECT_FALSE(dialog.is_open());
    EXPECT_TRUE(dialog.visible()); // still animating out
    settle(dialog, 2.0f);
    EXPECT_FALSE(dialog.visible());
    // Closed, it takes no input.
    EXPECT_EQ(send(dialog, press(Action::confirm)), Event::none);

    dialog.style.close_on_activate = false;
    dialog.open(feedback_);
    EXPECT_EQ(send(dialog, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(dialog.is_open());
}

TEST_F(ComponentsOverlays, DialogBackCancelsUnlessItMustBeAnswered)
{
    Dialog dialog;
    dialog.open(destructive(), feedback_);
    EXPECT_EQ(send(dialog, press(Action::back)), Event::cancelled);
    EXPECT_TRUE(asked(Cue::modal_close));
    EXPECT_FALSE(dialog.is_open());
    EXPECT_EQ(dialog.choice(), -1);

    dialog.style.dismissable = false;
    dialog.open(feedback_);
    EXPECT_EQ(send(dialog, press(Action::back)), Event::refused);
    EXPECT_TRUE(dialog.is_open());
}

TEST_F(ComponentsOverlays, DialogPanelFollowsItsTextAndAlignment)
{
    Dialog dialog;
    dialog.set_content(destructive());
    hui::ui::Canvas target = canvas();
    const Rect small = dialog.panel_rect(target);
    EXPECT_FLOAT_EQ(small.w, dialog.style.width);
    EXPECT_NEAR(small.cy(), 540.0f, 1.0f);

    DialogContent wordy = destructive();
    wordy.body = "One sentence is not enough here. The panel has to grow with the text it is "
                 "given, line by line, until the limit of lines is reached and the rest is cut.";
    dialog.set_content(wordy);
    const Rect tall = dialog.panel_rect(target);
    EXPECT_GT(tall.h, small.h + dialog.style.body_size);

    dialog.style.align = hui::ui::DialogAlign::bottom;
    const Rect low = dialog.panel_rect(target);
    EXPECT_NEAR(low.y + low.h, 1080.0f - dialog.style.bottom_margin, 0.5f);

    // Restyling keeps what is open and where the focus is.
    dialog.open(feedback_);
    dialog.set_focus(1);
    dialog.style.theme = hui::ui::themes()[5];
    settle(dialog);
    EXPECT_TRUE(dialog.is_open());
    EXPECT_EQ(dialog.focus(), 1);
}

TEST_F(ComponentsOverlays, SheetSlidesFromItsEdgeAndOnlyWantsBack)
{
    Sheet sheet;
    sheet.set_title("Queue");
    int drawn = 0;
    Rect seen;
    sheet.content = [&](hui::ui::Canvas &, const Rect &area, float opacity)
    {
        ++drawn;
        seen = area;
        EXPECT_GT(opacity, 0.0f);
    };
    hui::ui::Canvas target = canvas();
    sheet.draw(target);
    EXPECT_EQ(drawn, 0); // closed: nothing to draw

    sheet.open(feedback_);
    EXPECT_TRUE(asked(Cue::modal_open));
    settle(sheet);
    sheet.draw(target);
    EXPECT_EQ(drawn, 1);
    const Rect area = sheet.content_rect();
    EXPECT_FLOAT_EQ(seen.x, area.x);
    EXPECT_FLOAT_EQ(seen.h, area.h);

    const Rect right = sheet.panel_rect();
    EXPECT_FLOAT_EQ(right.x + right.w, 1920.0f);
    EXPECT_FLOAT_EQ(right.w, sheet.style.size);
    sheet.style.edge = SheetEdge::left;
    EXPECT_FLOAT_EQ(sheet.panel_rect().x, 0.0f);
    sheet.style.edge = SheetEdge::bottom;
    sheet.style.footer = 100.0f;
    const Rect low = sheet.panel_rect();
    EXPECT_FLOAT_EQ(low.y + low.h, 980.0f);
    EXPECT_FLOAT_EQ(low.h, sheet.style.size);
    sheet.style.margin = 30.0f;
    EXPECT_FLOAT_EQ(sheet.panel_rect().x, 30.0f);

    // Everything but back belongs to what is inside.
    feedback_.clear();
    EXPECT_EQ(sheet.handle(nav(Direction::down), feedback_), Event::none);
    EXPECT_EQ(sheet.handle(press(Action::confirm), feedback_), Event::none);
    EXPECT_TRUE(feedback_.cues.empty());
    EXPECT_EQ(sheet.handle(press(Action::back), feedback_), Event::cancelled);
    EXPECT_TRUE(asked(Cue::modal_close));
    EXPECT_FALSE(sheet.is_open());
    EXPECT_TRUE(sheet.visible());
    settle(sheet, 2.0f);
    EXPECT_FALSE(sheet.visible());
}

TEST_F(ComponentsOverlays, ToastsQueuePastTheLimitAndChimeOnceEach)
{
    ToastStack toasts;
    toasts.style.max_visible = 2;
    toasts.style.duration = 1.0f;
    for (int i = 0; i < 5; ++i)
        toasts.push(StatusKind::info, "Saved", "Slot 2");
    EXPECT_EQ(toasts.visible_count(), 0);
    EXPECT_EQ(toasts.queued_count(), 5);

    feedback_.clear();
    run(toasts, 0.5f);
    EXPECT_EQ(toasts.visible_count(), 2);
    EXPECT_EQ(toasts.queued_count(), 3);
    EXPECT_EQ(count(Cue::notify), 2);

    // Each one sounds when it appears, however long it waited.
    run(toasts, 8.0f);
    EXPECT_EQ(count(Cue::notify), 5);
    EXPECT_TRUE(toasts.empty());

    toasts.style.sounds.notify = Cue::count;
    feedback_.clear();
    toasts.push(StatusKind::success, "Quiet");
    run(toasts, 0.2f);
    EXPECT_TRUE(feedback_.cues.empty());
}

TEST_F(ComponentsOverlays, ToastsCountDownStayOrAreDismissed)
{
    ToastStack toasts;
    const int timed = toasts.push(StatusKind::warning, "Battery low", "", 2.0f);
    const int sticky = toasts.push(StatusKind::danger, "Connection lost", "", -1.0f);
    EXPECT_FLOAT_EQ(toasts.remaining(timed), -1.0f); // not on screen yet
    run(toasts, 1.0f);
    EXPECT_NEAR(toasts.remaining(timed), 0.5f, 0.05f);
    EXPECT_FLOAT_EQ(toasts.remaining(sticky), 1.0f);
    run(toasts, 6.0f);
    EXPECT_FLOAT_EQ(toasts.remaining(timed), -1.0f);
    EXPECT_EQ(toasts.visible_count(), 1);

    toasts.dismiss(sticky);
    EXPECT_EQ(toasts.visible_count(), 0);
    run(toasts, 3.0f);
    EXPECT_TRUE(toasts.empty());

    toasts.push(StatusKind::info, "One");
    toasts.push(StatusKind::info, "Two");
    run(toasts, 0.1f);
    toasts.clear(true);
    EXPECT_TRUE(toasts.empty());
}

TEST_F(ComponentsOverlays, TooltipFlipsAndSlidesToStayOnScreen)
{
    Tooltip tip;
    hui::ui::Canvas target = canvas();
    const Rect middle{900.0f, 500.0f, 200.0f, 60.0f};
    tip.show(middle, "Opens the save menu");
    Tooltip::Placed placed = tip.place(target);
    EXPECT_EQ(placed.placement, TooltipPlacement::above);
    EXPECT_LE(placed.bubble.y + placed.bubble.h, middle.y);
    EXPECT_NEAR(placed.tip_x, middle.cx(), 0.5f);

    // No room above: it goes below.
    tip.show({900.0f, 30.0f, 200.0f, 60.0f}, "Opens the save menu");
    placed = tip.place(target);
    EXPECT_EQ(placed.placement, TooltipPlacement::below);
    EXPECT_GE(placed.bubble.y, 90.0f);

    // No room on the right: it goes left; in a corner it slides along the edge.
    tip.style.placement = TooltipPlacement::right;
    tip.show({1700.0f, 1000.0f, 200.0f, 60.0f}, "Opens the save menu");
    placed = tip.place(target);
    EXPECT_EQ(placed.placement, TooltipPlacement::left);
    EXPECT_LE(placed.bubble.x + placed.bubble.w, 1700.0f);
    EXPECT_LE(placed.bubble.y + placed.bubble.h, 1080.0f - tip.style.screen_margin + 0.5f);

    // Long text wraps instead of growing past the limit.
    tip.style.placement = TooltipPlacement::above;
    tip.show(middle, "A long explanation that cannot fit on one line of a small label bubble "
                     "and has to wrap to a second one");
    placed = tip.place(target);
    EXPECT_LE(placed.bubble.w, tip.style.max_width + 0.5f);
    EXPECT_GT(placed.bubble.h, 2.0f * tip.style.text_size);
}

TEST_F(ComponentsOverlays, TooltipWaitsForItsDelayAndFollowsTheAnchor)
{
    Tooltip tip;
    tip.style.delay = 0.5f;
    const Rect first{300.0f, 500.0f, 200.0f, 60.0f};
    tip.show(first, "First");
    settle(tip, 0.3f);
    EXPECT_TRUE(tip.is_shown());
    EXPECT_FALSE(tip.visible());
    settle(tip, 0.6f);
    EXPECT_TRUE(tip.visible());

    // The same text again only moves it: no new wait.
    hui::ui::Canvas target = canvas();
    tip.show({400.0f, 500.0f, 200.0f, 60.0f}, "First");
    tip.update(kFrame);
    EXPECT_TRUE(tip.visible());
    EXPECT_NEAR(tip.place(target).tip_x, 500.0f, 0.5f);

    // Other text: the old bubble fades while the new one waits its turn.
    tip.show(first, "Second");
    EXPECT_EQ(tip.text(), "Second");
    settle(tip, 1.5f);
    EXPECT_TRUE(tip.visible());
    tip.hide();
    settle(tip, 1.5f);
    EXPECT_FALSE(tip.visible());
}

TEST_F(ComponentsOverlays, DrawInEveryThemeAndVariant)
{
    const StatusKind kinds[] = {StatusKind::info,   StatusKind::success,  StatusKind::warning,
                                StatusKind::danger, StatusKind::question, StatusKind::none};
    const SheetEdge edges[] = {SheetEdge::left, SheetEdge::right, SheetEdge::bottom};
    const ToastAnchor anchors[] = {ToastAnchor::top_left,      ToastAnchor::top_center,
                                   ToastAnchor::top_right,     ToastAnchor::bottom_left,
                                   ToastAnchor::bottom_center, ToastAnchor::bottom_right};
    const TooltipPlacement sides[] = {TooltipPlacement::above, TooltipPlacement::below,
                                      TooltipPlacement::left, TooltipPlacement::right};
    int round = 0;
    for (const hui::ui::Theme &theme : hui::ui::themes())
    {
        for (int variant = 0; variant < 4; ++variant, ++round)
        {
            const bool odd = (variant & 1) != 0;
            const bool high = (variant & 2) != 0;

            Dialog dialog;
            dialog.style.theme = theme;
            dialog.style.reduced_motion = high;
            dialog.style.frosted = odd;
            dialog.style.centered = !high;
            dialog.style.buttons =
                odd ? hui::ui::DialogButtons::stacked : hui::ui::DialogButtons::row;
            dialog.style.align = high ? hui::ui::DialogAlign::bottom : hui::ui::DialogAlign::center;
            DialogContent content = destructive();
            content.icon = kinds[round % 6];
            content.buttons.push_back(
                {"A third answer with a long label", ButtonKind::ghost, false});
            dialog.open(content, feedback_);
            // Half way in, then settled: both must draw.
            settle(dialog, 0.05f);
            hui::ui::Canvas target = canvas();
            dialog.draw(target);
            settle(dialog);
            dialog.draw(target);

            Sheet sheet;
            sheet.style.theme = theme;
            sheet.style.edge = edges[round % 3];
            sheet.style.margin = odd ? 24.0f : 0.0f;
            sheet.style.frosted = high;
            sheet.style.handle = !odd;
            sheet.set_title("A sheet title that is far too long for a narrow drawer to hold");
            sheet.open(feedback_);
            settle(sheet, 0.1f);
            sheet.draw(target);

            ToastStack toasts;
            toasts.style.theme = theme;
            toasts.style.anchor = anchors[round % 6];
            toasts.style.progress = odd;
            toasts.style.frosted = high;
            toasts.style.reduced_motion = odd;
            for (const StatusKind kind : kinds)
                toasts.push(kind, "Controller battery low", "About twenty minutes are left");
            run(toasts, 0.3f);
            toasts.draw(target);

            Tooltip tip;
            tip.style.theme = theme;
            tip.style.placement = sides[round % 4];
            tip.style.inverted = odd;
            tip.style.pointer = high ? 0.0f : 10.0f;
            tip.show({800.0f, 500.0f, 240.0f, 64.0f}, "Explains the focused control");
            settle(tip, 0.2f);
            tip.draw(target);

            expect_drawn(theme.id);
        }
    }
}

} // namespace
