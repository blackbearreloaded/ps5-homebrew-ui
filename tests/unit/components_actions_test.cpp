// ps5-homebrew-ui - Tests: the actions group (buttons, hold, quick actions, banner, tour).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "component_fixture.hpp"
#include "concepts/components/page.hpp"
#include "ui/components/banner.hpp"
#include "ui/components/button.hpp"
#include "ui/components/button_group.hpp"
#include "ui/components/coach_mark.hpp"
#include "ui/components/hold_button.hpp"
#include "ui/components/quick_action.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;
using hui::gfx::Rect;
using hui::ui::Banner;
using hui::ui::BannerLook;
using hui::ui::ButtonGroup;
using hui::ui::ButtonRole;
using hui::ui::ButtonSize;
using hui::ui::CoachMark;
using hui::ui::CoachStep;
using hui::ui::Event;
using hui::ui::GroupMode;
using hui::ui::HoldButton;
using hui::ui::HoldVariant;
using hui::ui::IconButton;
using hui::ui::PushButton;
using hui::ui::QuickAction;
using hui::ui::QuickActionBar;
using hui::ui::SplitButton;
using hui::ui::StatusKind;
using hui::ui::TooltipPlacement;

class ComponentsActions : public hui::testing::ComponentFixture
{
  protected:
    template <typename Component> void settle(Component &component, float seconds = 1.0f)
    {
        for (int i = 0, frames = static_cast<int>(seconds / kFrame); i < frames; ++i)
            component.update(kFrame);
    }

    // One input, then time for the animation to finish.
    template <typename Component> Event send(Component &component, const hui::InputFrame &input)
    {
        feedback_.clear();
        const Event event = component.handle(input, feedback_);
        settle(component);
        return event;
    }

    // Confirm kept down for a while: handle() and update() every frame, as a
    // screen does. Returns how often the button reported Event::activated.
    int hold(HoldButton &button, float seconds, Action action = Action::confirm)
    {
        int activations = 0;
        const int frames = static_cast<int>(seconds / kFrame);
        for (int i = 0; i < frames; ++i)
        {
            hui::InputFrame input = idle();
            input.held = hui::action_bit(action);
            if (i == 0)
                input.pressed = input.held;
            if (button.handle(input, feedback_) == Event::activated)
                ++activations;
            button.update(kFrame);
        }
        return activations;
    }

    static hui::InputFrame repeat(Direction direction)
    {
        hui::InputFrame input = nav(direction);
        input.nav_repeat = true;
        return input;
    }

    static std::vector<CoachStep> steps()
    {
        return {
            {{200.0f, 300.0f, 300.0f, 64.0f}, "First", "The first thing.", TooltipPlacement::below},
            {{1500.0f, 880.0f, 300.0f, 64.0f}, "Second", "In a corner.", TooltipPlacement::below},
            {{900.0f, 500.0f, 200.0f, 64.0f}, "Third", "The last one.", TooltipPlacement::right},
        };
    }
};

TEST_F(ComponentsActions, PushButtonActivatesAndRefusesWhileDisabledOrLoading)
{
    PushButton button;
    button.label = "Save";
    button.set_bounds({100.0f, 100.0f, 320.0f, 64.0f});
    button.set_active(true);
    EXPECT_EQ(send(button, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(asked(Cue::select));
    EXPECT_GT(feedback_.rumble_strength, 0.0f);
    EXPECT_EQ(send(button, nav(Direction::left)), Event::none); // moving is the screen's job

    button.set_loading(true);
    EXPECT_EQ(send(button, press(Action::confirm)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    button.set_loading(false);
    button.set_disabled(true);
    EXPECT_EQ(send(button, press(Action::confirm)), Event::refused);
    button.set_disabled(false);
    EXPECT_EQ(send(button, press(Action::confirm)), Event::activated);

    button.style.sounds.activate = Cue::count;
    button.style.sounds.rumble = 0.0f;
    EXPECT_EQ(send(button, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(feedback_.cues.empty());
    EXPECT_EQ(feedback_.rumble_strength, 0.0f);
}

TEST_F(ComponentsActions, PushButtonSizesAndHugging)
{
    PushButton button;
    button.label = "Go";
    button.set_bounds({100.0f, 100.0f, 600.0f, 100.0f});
    EXPECT_FLOAT_EQ(button.rect(fonts_).w, 600.0f);
    EXPECT_FLOAT_EQ(button.rect(fonts_).h, 64.0f);
    EXPECT_FLOAT_EQ(button.rect(fonts_).y, 118.0f); // centred in its bounds
    button.style.size = ButtonSize::small;
    EXPECT_FLOAT_EQ(button.rect(fonts_).h, 48.0f);
    button.style.size = ButtonSize::large;
    EXPECT_FLOAT_EQ(button.rect(fonts_).h, 80.0f);
    button.style.height = 70.0f;
    EXPECT_FLOAT_EQ(button.rect(fonts_).h, 70.0f);

    button.style.full_width = false;
    const float bare = button.rect(fonts_).w;
    EXPECT_LT(bare, 300.0f);
    button.glyph = hui::ui::Button::triangle;
    EXPECT_GT(button.rect(fonts_).w, bare); // the glyph takes room
    button.style.align = hui::gfx::Align::right;
    const Rect right = button.rect(fonts_);
    EXPECT_NEAR(right.x + right.w, 700.0f, 0.01f);
    button.style.min_width = 400.0f;
    EXPECT_FLOAT_EQ(button.rect(fonts_).w, 400.0f);
}

TEST_F(ComponentsActions, IconButtonTogglesCountsAndRefuses)
{
    IconButton button;
    button.tip = "Mute";
    button.set_bounds({100.0f, 100.0f, 80.0f, 64.0f});
    EXPECT_FLOAT_EQ(button.rect().w, 64.0f); // the smaller side
    button.set_active(true);
    EXPECT_EQ(send(button, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(asked(Cue::select));

    button.style.toggle = true;
    EXPECT_EQ(send(button, press(Action::confirm)), Event::changed);
    EXPECT_TRUE(button.on());
    EXPECT_TRUE(asked(Cue::toggle));
    EXPECT_EQ(send(button, press(Action::confirm)), Event::changed);
    EXPECT_FALSE(button.on());

    button.set_badge(7);
    EXPECT_EQ(button.badge(), 7);
    button.set_disabled(true);
    EXPECT_EQ(send(button, press(Action::confirm)), Event::refused);
    EXPECT_FALSE(button.on());
    EXPECT_TRUE(asked(Cue::error));
}

TEST_F(ComponentsActions, ButtonGroupMovesRefusesWrapsAndExits)
{
    ButtonGroup group;
    group.set_items({{"Cut"}, {"Copy"}, {"Paste"}});
    group.set_bounds({100.0f, 100.0f, 600.0f, 64.0f});
    group.set_active(true);
    EXPECT_FLOAT_EQ(group.item_rect(1).x, 100.0f + 192.0f + 12.0f); // shared width and a gap
    EXPECT_EQ(send(group, nav(Direction::left)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_EQ(send(group, nav(Direction::right)), Event::moved);
    EXPECT_TRUE(asked(Cue::focus));
    EXPECT_EQ(send(group, nav(Direction::right)), Event::moved);
    EXPECT_EQ(group.focus(), 2);
    EXPECT_EQ(send(group, repeat(Direction::right)), Event::refused);
    EXPECT_TRUE(feedback_.cues.empty()); // a held direction stays quiet
    EXPECT_EQ(send(group, nav(Direction::down)), Event::none);
    EXPECT_EQ(group.exit(), Direction::none);

    group.style.wrap = true;
    EXPECT_EQ(send(group, nav(Direction::right)), Event::moved);
    EXPECT_EQ(group.focus(), 0);
    group.style.wrap = false;
    group.style.exits.left = true;
    group.style.exits.down = true;
    EXPECT_EQ(send(group, nav(Direction::left)), Event::none);
    EXPECT_EQ(group.exit(), Direction::left);
    EXPECT_EQ(send(group, nav(Direction::down)), Event::none);
    EXPECT_EQ(group.exit(), Direction::down);

    group.style.layout = hui::ui::GroupLayout::column;
    group.style.joined = true;
    EXPECT_FLOAT_EQ(group.item_rect(1).y, group.item_rect(0).y + 64.0f); // joined: no gap
    EXPECT_EQ(send(group, nav(Direction::down)), Event::moved);
    EXPECT_EQ(group.focus(), 1);
}

TEST_F(ComponentsActions, ButtonGroupModes)
{
    ButtonGroup group;
    std::vector<hui::ui::GroupItem> items = {{"Grid"}, {"List"}, {"Shelf"}};
    items[2].disabled = true;
    group.set_items(items);
    group.set_bounds({100.0f, 100.0f, 600.0f, 64.0f});

    EXPECT_EQ(send(group, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(asked(Cue::select));
    EXPECT_EQ(group.selected(), -1); // actions select nothing

    group.style.mode = GroupMode::exclusive;
    EXPECT_EQ(send(group, press(Action::confirm)), Event::changed);
    EXPECT_TRUE(asked(Cue::toggle));
    EXPECT_EQ(group.selected(), 0);
    EXPECT_EQ(send(group, press(Action::confirm)), Event::none); // already the choice
    send(group, nav(Direction::right));
    EXPECT_EQ(send(group, press(Action::confirm)), Event::changed);
    EXPECT_EQ(group.selected(), 1);
    EXPECT_FALSE(group.is_selected(0));
    group.style.allow_none = true;
    EXPECT_EQ(send(group, press(Action::confirm)), Event::changed);
    EXPECT_EQ(group.selected(), -1);

    send(group, nav(Direction::right));
    EXPECT_EQ(send(group, press(Action::confirm)), Event::refused); // disabled
    EXPECT_FALSE(group.is_selected(2));

    group.style.mode = GroupMode::multiple;
    group.set_focus(0);
    EXPECT_EQ(send(group, press(Action::confirm)), Event::changed);
    send(group, nav(Direction::right));
    EXPECT_EQ(send(group, press(Action::confirm)), Event::changed);
    EXPECT_TRUE(group.is_selected(0));
    EXPECT_TRUE(group.is_selected(1));
    EXPECT_EQ(send(group, press(Action::confirm)), Event::changed);
    EXPECT_FALSE(group.is_selected(1));

    // Restyling keeps the focus and the selection.
    for (const hui::ui::Theme &theme : hui::ui::themes())
    {
        group.style.theme = theme;
        EXPECT_EQ(group.focus(), 1) << theme.id;
        EXPECT_TRUE(group.is_selected(0)) << theme.id;
    }
}

TEST_F(ComponentsActions, SplitButtonFiresItsMainActionAndItsAlternatives)
{
    SplitButton split;
    split.label = "Save";
    std::vector<hui::ui::MenuItem> items(2);
    items[0].label = "Save as copy";
    items[1].label = "Save and quit";
    split.set_alternatives(items);
    split.set_bounds({100.0f, 100.0f, 360.0f, 64.0f});
    split.set_active(true);
    EXPECT_FLOAT_EQ(split.part_rect(1).w, 64.0f); // as wide as the button is tall

    EXPECT_EQ(send(split, press(Action::confirm)), Event::activated);
    EXPECT_EQ(split.choice(), -1);
    EXPECT_EQ(send(split, nav(Direction::left)), Event::refused);
    EXPECT_EQ(send(split, nav(Direction::right)), Event::moved);
    EXPECT_EQ(split.part(), 1);
    EXPECT_EQ(send(split, nav(Direction::right)), Event::refused);

    EXPECT_EQ(send(split, press(Action::confirm)), Event::none);
    EXPECT_TRUE(split.menu_open());
    EXPECT_TRUE(asked(Cue::modal_open));
    EXPECT_EQ(send(split, nav(Direction::down)), Event::moved);
    EXPECT_EQ(send(split, press(Action::confirm)), Event::activated);
    EXPECT_EQ(split.choice(), 1);
    EXPECT_FALSE(split.menu_open());

    send(split, press(Action::confirm));
    EXPECT_TRUE(split.menu_open());
    EXPECT_EQ(send(split, press(Action::back)), Event::cancelled);
    EXPECT_FALSE(split.menu_open());

    split.style.exits.right = true;
    EXPECT_EQ(send(split, nav(Direction::right)), Event::none);
    EXPECT_EQ(split.exit(), Direction::right);

    split.style.swap_on_choose = true;
    send(split, press(Action::confirm));
    EXPECT_EQ(send(split, press(Action::confirm)), Event::activated);
    EXPECT_EQ(split.label, "Save as copy"); // the alternative became the main action
    EXPECT_EQ(split.menu().items()[0].label, "Save");
}

TEST_F(ComponentsActions, HoldButtonCompletesOnlyAfterAFullHold)
{
    HoldButton button;
    button.label = "Delete save";
    button.set_bounds({100.0f, 100.0f, 400.0f, 64.0f});
    button.set_active(true);
    button.style.hold_seconds = 1.0f;

    // Half a hold: it fills, then drains without firing.
    feedback_.clear();
    EXPECT_EQ(hold(button, 0.5f), 0);
    EXPECT_NEAR(button.progress(), 0.5f, 0.05f);
    EXPECT_TRUE(button.holding());
    EXPECT_GT(feedback_.rumble_strength, button.style.rumble_from); // the ramp
    EXPECT_TRUE(asked(Cue::slider));                                // a quarter tick
    EXPECT_EQ(button.handle(idle(), feedback_), Event::none);
    EXPECT_FALSE(button.holding());
    settle(button);
    EXPECT_FLOAT_EQ(button.progress(), 0.0f);
    EXPECT_FALSE(button.hinting()); // a long press that was let go is not a tap

    // A full hold fires exactly once, however long it goes on.
    feedback_.clear();
    EXPECT_EQ(hold(button, 2.0f), 1);
    EXPECT_TRUE(asked(Cue::launch));
    EXPECT_GE(feedback_.rumble_strength, button.style.rumble_done - 0.01f);
    settle(button);
    EXPECT_FLOAT_EQ(button.progress(), 0.0f);

    // Another action can be the one that is held.
    button.style.action = Action::west;
    EXPECT_EQ(hold(button, 1.5f, Action::confirm), 0);
    EXPECT_EQ(hold(button, 1.5f, Action::west), 1);
}

TEST_F(ComponentsActions, HoldButtonTapShowsTheHintAndRefuses)
{
    HoldButton button;
    button.label = "Sign out";
    button.hint = "Hold to sign out";
    button.set_bounds({100.0f, 100.0f, 400.0f, 64.0f});
    button.set_active(true);

    feedback_.clear();
    EXPECT_EQ(button.handle(press(Action::confirm), feedback_), Event::none);
    button.update(kFrame);
    EXPECT_EQ(button.handle(idle(), feedback_), Event::refused); // let go at once
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_TRUE(button.hinting());
    settle(button, button.style.hint_seconds + 0.5f);
    EXPECT_FALSE(button.hinting());
    EXPECT_FLOAT_EQ(button.progress(), 0.0f);

    button.set_disabled(true);
    EXPECT_EQ(button.handle(press(Action::confirm), feedback_), Event::refused);
    EXPECT_FALSE(button.holding());

    // Losing the focus mid-hold lets go: no handle(), no hold.
    button.set_disabled(false);
    hold(button, 0.4f);
    EXPECT_GT(button.progress(), 0.2f);
    settle(button);
    EXPECT_FLOAT_EQ(button.progress(), 0.0f);
    EXPECT_FALSE(button.holding());
}

TEST_F(ComponentsActions, QuickActionAnswersItsOwnButtonOnly)
{
    QuickAction action;
    action.label = "Search";
    action.glyph = hui::ui::Button::triangle;
    action.action = Action::north;
    action.set_bounds({96.0f, 236.0f, 1728.0f, 716.0f});
    settle(action);

    EXPECT_EQ(send(action, press(Action::confirm)), Event::none);
    EXPECT_EQ(send(action, nav(Direction::left)), Event::none);
    EXPECT_EQ(send(action, press(Action::north)), Event::activated);
    EXPECT_TRUE(asked(Cue::select));

    // It sits in its corner, inside its bounds.
    const Rect r = action.rect(fonts_);
    EXPECT_NEAR(r.x + r.w, 96.0f + 1728.0f, 0.01f);
    EXPECT_NEAR(r.y + r.h, 236.0f + 716.0f, 0.01f);
    action.style.anchor = hui::ui::QuickAnchor::top_left;
    action.style.margin = 10.0f;
    EXPECT_NEAR(action.rect(fonts_).x, 106.0f, 0.01f);
    EXPECT_NEAR(action.rect(fonts_).y, 246.0f, 0.01f);

    // The label folds away after a while and comes back on a press.
    const float open = action.width(fonts_);
    action.style.collapse_after = 1.0f;
    settle(action, 3.0f);
    EXPECT_TRUE(action.collapsed());
    EXPECT_LT(action.width(fonts_), open - 20.0f);
    EXPECT_EQ(action.handle(press(Action::north), feedback_), Event::activated);
    EXPECT_FALSE(action.collapsed());
    action.style.collapse_after = 0.0f;

    action.set_enabled(false);
    EXPECT_EQ(send(action, press(Action::north)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    action.set_enabled(true);
    action.set_visible(false);
    EXPECT_EQ(send(action, press(Action::north)), Event::none); // hidden: not listening
    action.set_badge(5);
    EXPECT_EQ(action.badge(), 5);
}

TEST_F(ComponentsActions, QuickActionBarKeepsItsActionsApart)
{
    QuickActionBar bar;
    bar.set_actions({{"Search", hui::ui::Button::triangle, Action::north},
                     {"Sort", hui::ui::Button::square, Action::west}});
    bar.set_bounds({96.0f, 236.0f, 1728.0f, 716.0f});
    settle(bar);
    EXPECT_EQ(bar.count(), 2);

    const Rect first = bar.rect(fonts_, 0);
    const Rect second = bar.rect(fonts_, 1);
    EXPECT_NEAR(second.x - (first.x + first.w), bar.style.spacing, 0.01f);
    EXPECT_NEAR(second.x + second.w, 96.0f + 1728.0f, 0.01f); // the row ends in the corner

    EXPECT_EQ(send(bar, press(Action::west)), Event::activated);
    EXPECT_EQ(bar.fired(), 1);
    EXPECT_EQ(send(bar, press(Action::north)), Event::activated);
    EXPECT_EQ(bar.fired(), 0);
    EXPECT_EQ(send(bar, press(Action::confirm)), Event::none);
    bar.at(1).set_enabled(false);
    EXPECT_EQ(send(bar, press(Action::west)), Event::refused);
    EXPECT_EQ(bar.fired(), 1);

    bar.style.layout = hui::ui::GroupLayout::column;
    settle(bar);
    const Rect top = bar.rect(fonts_, 0);
    const Rect bottom = bar.rect(fonts_, 1);
    EXPECT_NEAR(bottom.y - (top.y + top.h), bar.style.spacing, 0.01f);
    EXPECT_NEAR(bottom.y + bottom.h, 236.0f + 716.0f, 0.01f);
}

TEST_F(ComponentsActions, BannerPushesContentAndIsAnswered)
{
    Banner banner;
    banner.style.kind = StatusKind::warning;
    banner.title = "Storage almost full";
    banner.body = "2.1 GB left.";
    banner.set_actions({"Manage", "Later", "Dropped"});
    EXPECT_EQ(banner.actions().size(), 2u); // at most two
    banner.set_bounds({96.0f, 240.0f, 1728.0f, 0.0f});
    banner.set_active(true);
    EXPECT_FALSE(banner.visible());
    EXPECT_FLOAT_EQ(banner.pushed(fonts_), 0.0f);
    EXPECT_EQ(banner.handle(press(Action::confirm), feedback_), Event::none); // hidden

    feedback_.clear();
    banner.show(feedback_);
    EXPECT_TRUE(asked(Cue::notify));
    settle(banner);
    EXPECT_GE(banner.height(fonts_), banner.style.min_height);
    EXPECT_NEAR(banner.pushed(fonts_), banner.height(fonts_), 0.5f);
    EXPECT_EQ(banner.controls(), 3); // two actions and the dismiss control

    // The controls sit inside the panel, left to right.
    const Rect panel = banner.rect(fonts_);
    const Rect last = banner.control_rect(fonts_, 2);
    EXPECT_GT(banner.control_rect(fonts_, 1).x, banner.control_rect(fonts_, 0).x);
    EXPECT_GT(last.x, banner.control_rect(fonts_, 1).x);
    EXPECT_LE(last.x + last.w, panel.x + panel.w);

    EXPECT_EQ(send(banner, nav(Direction::left)), Event::refused);
    EXPECT_EQ(send(banner, nav(Direction::right)), Event::moved);
    EXPECT_EQ(send(banner, press(Action::confirm)), Event::activated);
    EXPECT_EQ(banner.choice(), 1);
    EXPECT_TRUE(banner.is_shown()); // an action does not close it by itself
    EXPECT_EQ(send(banner, nav(Direction::right)), Event::moved);
    EXPECT_EQ(send(banner, nav(Direction::right)), Event::refused);
    banner.style.exits.down = true;
    EXPECT_EQ(send(banner, nav(Direction::down)), Event::none);
    EXPECT_EQ(banner.exit(), Direction::down);

    EXPECT_EQ(send(banner, press(Action::confirm)), Event::cancelled); // the dismiss control
    EXPECT_TRUE(asked(Cue::modal_close));
    EXPECT_FALSE(banner.is_shown());
    EXPECT_FLOAT_EQ(banner.pushed(fonts_), 0.0f);

    banner.show(feedback_);
    settle(banner);
    EXPECT_EQ(banner.focus(), 0); // it opens on its first action again
    EXPECT_EQ(send(banner, press(Action::back)), Event::cancelled);
    banner.show(feedback_);
    banner.style.dismissable = false;
    EXPECT_EQ(banner.controls(), 2);
    EXPECT_EQ(send(banner, press(Action::back)), Event::none);
    EXPECT_TRUE(banner.is_shown());
}

TEST_F(ComponentsActions, CoachMarkStepsGlidesAndEnds)
{
    CoachMark coach;
    EXPECT_FALSE(coach.is_open());
    coach.start({}, feedback_);
    EXPECT_FALSE(coach.is_open()); // nothing to show
    EXPECT_EQ(coach.handle(press(Action::confirm), feedback_), Event::none);

    feedback_.clear();
    coach.start(steps(), feedback_);
    EXPECT_TRUE(coach.is_open());
    EXPECT_TRUE(asked(Cue::modal_open));
    EXPECT_EQ(coach.count(), 3);
    settle(coach, 2.0f);
    // The spotlight came to rest around the first target, with its padding.
    EXPECT_NEAR(coach.spotlight().x, 200.0f - coach.style.spot_padding, 0.5f);
    EXPECT_NEAR(coach.spotlight().w, 300.0f + 2.0f * coach.style.spot_padding, 0.5f);

    EXPECT_EQ(send(coach, nav(Direction::left)), Event::refused); // a direction does not skip
    EXPECT_EQ(send(coach, press(Action::confirm)), Event::changed);
    EXPECT_TRUE(asked(Cue::tab));
    EXPECT_EQ(coach.step(), 1);
    // No room below a target in the bottom corner: the bubble flips and stays
    // inside the bounds.
    const hui::ui::Tooltip::Placed placed = coach.bubble(fonts_);
    EXPECT_EQ(placed.placement, TooltipPlacement::above);
    EXPECT_GE(placed.bubble.x, coach.style.screen_margin - 0.01f);
    EXPECT_LE(placed.bubble.x + placed.bubble.w, 1920.0f - coach.style.screen_margin + 0.01f);
    EXPECT_GE(placed.bubble.y, coach.style.screen_margin - 0.01f);

    EXPECT_EQ(send(coach, press(Action::back)), Event::changed);
    EXPECT_EQ(coach.step(), 0);
    EXPECT_EQ(send(coach, nav(Direction::right)), Event::changed);
    EXPECT_EQ(send(coach, nav(Direction::right)), Event::changed);
    EXPECT_EQ(coach.step(), 2);
    EXPECT_EQ(send(coach, nav(Direction::right)), Event::refused); // the end needs confirm
    EXPECT_TRUE(coach.is_open());
    coach.set_target(2, {100.0f, 100.0f, 80.0f, 80.0f});
    settle(coach, 2.0f);
    EXPECT_NEAR(coach.spotlight().x, 100.0f - coach.style.spot_padding, 0.5f);
    EXPECT_EQ(send(coach, press(Action::confirm)), Event::activated);
    EXPECT_FALSE(coach.is_open());

    coach.start(steps(), feedback_);
    feedback_.clear();
    EXPECT_EQ(send(coach, press(Action::back)), Event::cancelled); // skipped on the first step
    EXPECT_TRUE(asked(Cue::back));
    EXPECT_FALSE(coach.is_open());
    settle(coach, 2.0f);
    EXPECT_FALSE(coach.visible());
}

TEST_F(ComponentsActions, DrawsInEveryThemeAndVariant)
{
    PushButton push;
    push.label = "Continue";
    push.glyph = hui::ui::Button::cross;
    push.icon = [](hui::ui::Canvas &canvas, const Rect &box, hui::gfx::Color ink, float)
    { canvas.list.circle(box.cx(), box.cy(), box.w * 0.4f, ink); };
    push.set_bounds({100.0f, 100.0f, 320.0f, 80.0f});
    push.set_active(true);

    IconButton icon;
    icon.tip = "Favourite";
    icon.style.toggle = true;
    icon.set_bounds({100.0f, 200.0f, 64.0f, 64.0f});
    icon.set_badge(120);
    icon.set_active(true);

    ButtonGroup group;
    group.set_items({{"Grid"}, {"List"}, {"Shelf"}});
    group.set_bounds({100.0f, 300.0f, 540.0f, 64.0f});
    group.style.mode = GroupMode::exclusive;
    group.set_selected(1);
    group.set_active(true);

    SplitButton split;
    split.label = "Save";
    std::vector<hui::ui::MenuItem> items(2);
    items[0].label = "Save as copy";
    items[1].label = "Export";
    split.set_alternatives(items);
    split.set_bounds({100.0f, 400.0f, 360.0f, 64.0f});
    split.set_active(true);
    split.set_part(1);
    split.handle(press(Action::confirm), feedback_); // the menu is open

    HoldButton held;
    held.label = "Delete save";
    held.set_bounds({100.0f, 500.0f, 400.0f, 64.0f});
    held.set_active(true);
    hold(held, 0.6f);

    QuickActionBar bar;
    bar.set_actions({{"Search", hui::ui::Button::triangle, Action::north},
                     {"Sort", hui::ui::Button::l1, Action::west}});
    bar.at(0).set_badge(3);

    Banner banner;
    banner.title = "Update ready";
    banner.body = "Version 2.4 adds a photo mode. It installs the next time you close the game, "
                  "and the notes are in the Extras menu if you want to read them first.";
    banner.set_actions({"Install", "Notes"});
    banner.set_active(true);
    banner.set_shown(true, true);

    CoachMark coach;
    coach.start(steps(), feedback_);

    const ButtonRole roles[] = {ButtonRole::primary, ButtonRole::secondary, ButtonRole::ghost,
                                ButtonRole::danger};
    const ButtonSize sizes[] = {ButtonSize::small, ButtonSize::medium, ButtonSize::large};
    const HoldVariant holds[] = {HoldVariant::fill, HoldVariant::ring, HoldVariant::underline};
    const BannerLook looks[] = {BannerLook::filled, BannerLook::tinted, BannerLook::outlined,
                                BannerLook::accent};
    const StatusKind kinds[] = {StatusKind::info, StatusKind::success, StatusKind::warning,
                                StatusKind::danger};
    for (const hui::ui::Theme &theme : hui::ui::themes())
    {
        for (int variant = 0; variant < 4; ++variant)
        {
            const bool odd = variant % 2 == 1;
            push.style.theme = theme;
            push.style.role = roles[variant];
            push.style.size = sizes[variant % 3];
            push.style.full_width = !odd;
            push.style.glyph_tinted = !odd;
            push.style.loading_hides_label = odd;
            push.set_loading(variant >= 2);
            push.set_disabled(variant == 3);

            icon.style.theme = theme;
            icon.style.role = roles[variant];
            icon.style.shape =
                odd ? hui::ui::IconButtonShape::round : hui::ui::IconButtonShape::square;
            icon.set_on(odd);

            group.style.theme = theme;
            group.style.role = roles[variant];
            group.style.selected_role = roles[(variant + 1) % 4];
            group.style.joined = odd;
            group.style.size = sizes[variant % 3];
            group.style.layout =
                variant == 3 ? hui::ui::GroupLayout::column : hui::ui::GroupLayout::row;
            group.style.icon_width = odd ? 24.0f : 0.0f;

            split.style.theme = theme;
            split.style.role = roles[variant];

            held.style.theme = theme;
            held.style.role = roles[3 - variant];
            held.style.variant = holds[variant % 3];
            held.style.glyph = odd ? hui::ui::Button::none : hui::ui::Button::cross;

            bar.style.theme = theme;
            bar.style.role = roles[variant];
            bar.style.pill = !odd;
            bar.style.collapse_after = odd ? 0.1f : 0.0f;
            bar.style.anchor =
                odd ? hui::ui::QuickAnchor::top_center : hui::ui::QuickAnchor::bottom_right;

            banner.style.theme = theme;
            banner.style.look = looks[variant];
            banner.style.kind = kinds[variant];
            banner.style.dismissable = variant != 1;

            coach.style.theme = theme;
            coach.style.counter = !odd;
            coach.style.hints = variant != 3;

            // Everything animates a little between draws: mid-flight is drawn too.
            push.update(kFrame);
            icon.update(0.5f);
            group.update(kFrame);
            split.update(kFrame);
            held.update(kFrame);
            bar.update(0.3f);
            banner.update(kFrame);
            coach.update(kFrame);

            hui::ui::Canvas target = canvas();
            push.draw(target);
            icon.draw(target);
            icon.draw_tooltip(target);
            group.draw(target);
            split.draw(target);
            split.draw_menu(target);
            held.draw(target);
            bar.draw(target);
            banner.draw(target);
            coach.draw(target);
            expect_drawn(theme.id);
            for (const hui::gfx::MeshVertex &vertex : list_.mesh_vertices())
                ASSERT_TRUE(std::isfinite(vertex.x) && std::isfinite(vertex.y)) << theme.id;
        }
    }
    // Restyling thirty times lost nothing.
    EXPECT_EQ(group.selected(), 1);
    EXPECT_TRUE(split.menu_open());
    EXPECT_TRUE(banner.is_shown());
    EXPECT_TRUE(coach.is_open());
    EXPECT_EQ(bar.at(0).badge(), 3);
}

TEST_F(ComponentsActions, PageTourEndsWhereItBegan)
{
    const std::unique_ptr<hui::concepts::gallery::Page> page =
        hui::concepts::gallery::make_actions_page(context_);
    EXPECT_STREQ(page->title(), "Actions");
    const std::string first = page->variant();
    const std::size_t board_hints = page->hints().size();
    int pictures = 0;
    for (const hui::app::TourStep &step : page->tour())
    {
        // The wait, with whatever the step keeps held, then its input.
        hui::InputFrame waiting = idle();
        waiting.held = step.hold;
        for (int i = 0, frames = static_cast<int>(step.wait / kFrame); i < frames; ++i)
        {
            feedback_.clear();
            page->update(waiting, kFrame, feedback_);
        }
        if (step.capture != nullptr)
        {
            ++pictures;
            EXPECT_EQ(std::string(step.capture).rfind("actions", 0), 0u) << step.capture;
        }
        hui::InputFrame input = idle();
        input.pressed = step.press;
        input.held = step.press | step.hold;
        input.nav = step.nav;
        feedback_.clear();
        page->update(input, kFrame, feedback_);
        hui::ui::Canvas target = canvas();
        page->draw(target);
        page->draw_modal(target);
        expect_drawn("actions page");
    }
    EXPECT_GE(page->tour().size(), 10u);
    EXPECT_LE(page->tour().size(), 25u);
    EXPECT_GE(pictures, 3);
    EXPECT_LE(pictures, 5);
    EXPECT_EQ(first, page->variant());            // the first variant again
    EXPECT_EQ(page->hints().size(), board_hints); // nothing modal left open
}

} // namespace
