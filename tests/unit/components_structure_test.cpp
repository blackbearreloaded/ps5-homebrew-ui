// ps5-homebrew-ui - Tests: RadialMenu, Wizard, Accordion, TreeView and JumpBar behaviour.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "component_fixture.hpp"
#include "concepts/components/page.hpp"
#include "ui/components/accordion.hpp"
#include "ui/components/jump_bar.hpp"
#include "ui/components/radial.hpp"
#include "ui/components/tree.hpp"
#include "ui/components/wizard.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;
using hui::gfx::Rect;
using hui::ui::Accordion;
using hui::ui::Event;
using hui::ui::JumpBar;
using hui::ui::RadialItem;
using hui::ui::RadialMenu;
using hui::ui::TreeNode;
using hui::ui::TreeView;
using hui::ui::Wizard;

class ComponentsStructure : public hui::testing::ComponentFixture
{
  protected:
    ComponentsStructure()
    {
        // Eight wedges: the first at 12 o'clock, 45 degrees apart, clockwise.
        std::vector<RadialItem> wedges(8);
        const char *names[] = {"North", "North-east", "East", "South-east",
                               "South", "South-west", "West", "North-west"};
        for (std::size_t i = 0; i < wedges.size(); ++i)
            wedges[i].label = names[i];
        wedges[3].disabled = true;
        wheel_.set_items(wedges);

        wizard_.set_steps({{"Profile"}, {"Display"}, {"Sound"}, {"Finish"}});
        wizard_.set_bounds({400.0f, 300.0f, 900.0f, 200.0f});

        folds_.set_sections({{"Saving", "3 slots", "The game saves at every lantern."},
                             {"Controls", "", "Remap any button in Options."},
                             {"Online", "Sign in", "Sign in first.", 0.0f, true},
                             {"Party", "", "", 120.0f},
                             {"About", "", "Version 2.4."}});
        folds_.set_bounds({100.0f, 100.0f, 600.0f, 400.0f});

        //  0 Library            (open)
        //  1   Recent
        //  2   Favourites
        //  3     Tidewater
        //  4     Kiln
        //  5 Saves
        //  6   Slot 1
        //  7 Offline            (disabled)
        TreeNode favourites{"Favourites"};
        favourites.children = {{"Tidewater", "63 h"}, {"Kiln"}};
        TreeNode library{"Library"};
        library.expanded = true;
        library.children = {{"Recent", "12"}, favourites};
        TreeNode saves{"Saves"};
        saves.children = {{"Slot 1"}};
        TreeNode offline{"Offline"};
        offline.disabled = true;
        tree_.set_nodes({library, saves, offline});
        tree_.set_bounds({100.0f, 100.0f, 500.0f, 400.0f});

        std::vector<hui::ui::JumpEntry> letters = JumpBar::alphabet();
        index_.set_entries(letters);
        index_.set_enabled(index_.find("B"), false);
        index_.set_enabled(index_.find("Z"), false);
        index_.set_bounds({1700.0f, 200.0f, 44.0f, 700.0f});
    }

    // Sends one input to a component, then lets it animate for half a second.
    template <typename Component> Event send(Component &component, const hui::InputFrame &input)
    {
        feedback_.clear();
        const Event event = component.handle(input, feedback_);
        settle(component);
        return event;
    }

    template <typename Component> void settle(Component &component, float seconds = 0.5f)
    {
        for (int i = 0, frames = static_cast<int>(seconds / kFrame); i < frames; ++i)
            component.update(kFrame);
    }

    // The left stick pushed all the way at an angle, in degrees clockwise from up.
    static hui::InputFrame stick(float degrees, float amount = 1.0f)
    {
        hui::InputFrame input = idle();
        const float angle = degrees * 3.14159265f / 180.0f;
        input.stick_x = std::sin(angle) * amount;
        input.stick_y = -std::cos(angle) * amount;
        return input;
    }

    RadialMenu wheel_;
    Wizard wizard_;
    Accordion folds_;
    TreeView tree_;
    JumpBar index_;
};

TEST_F(ComponentsStructure, RadialStickPointsWithHysteresisOnAngleAndDeflection)
{
    EXPECT_EQ(send(wheel_, stick(90.0f)), Event::none) << "closed: it takes no input";
    wheel_.open(feedback_);
    EXPECT_TRUE(asked(Cue::modal_open));
    EXPECT_TRUE(wheel_.is_open());
    EXPECT_EQ(wheel_.focus(), 0);

    EXPECT_EQ(send(wheel_, stick(90.0f)), Event::moved);
    EXPECT_EQ(wheel_.focus(), 2);
    EXPECT_TRUE(asked(Cue::focus));
    // The boundary to the next wedge is at 112.5 degrees. A thumb resting just
    // past it keeps the wedge; clearly inside the neighbour, the focus leaves.
    EXPECT_EQ(send(wheel_, stick(116.0f)), Event::none);
    EXPECT_EQ(wheel_.focus(), 2);
    EXPECT_EQ(send(wheel_, stick(124.0f)), Event::moved);
    EXPECT_EQ(wheel_.focus(), 3);
    // Below the engage threshold a new direction does nothing ...
    EXPECT_EQ(send(wheel_, idle()), Event::none);
    EXPECT_EQ(send(wheel_, stick(270.0f, 0.3f)), Event::none);
    EXPECT_EQ(wheel_.focus(), 3);
    // ... and once engaged, the stick keeps aiming down to the release one.
    EXPECT_EQ(send(wheel_, stick(270.0f, 0.5f)), Event::moved);
    EXPECT_EQ(send(wheel_, stick(0.0f, 0.3f)), Event::moved);
    EXPECT_EQ(wheel_.focus(), 0);
    // Letting go keeps what was pointed at.
    EXPECT_EQ(send(wheel_, idle()), Event::none);
    EXPECT_EQ(wheel_.focus(), 0);
}

TEST_F(ComponentsStructure, RadialDpadTurnsTheDialAndConfirmChooses)
{
    wheel_.open(feedback_);
    EXPECT_EQ(send(wheel_, nav(Direction::left)), Event::moved) << "a ring has no end";
    EXPECT_EQ(wheel_.focus(), 7);
    EXPECT_EQ(send(wheel_, nav(Direction::right)), Event::moved);
    EXPECT_EQ(send(wheel_, nav(Direction::right)), Event::moved);
    EXPECT_EQ(wheel_.focus(), 1);
    // Up and down head for the top and bottom wedge and refuse once there.
    EXPECT_EQ(send(wheel_, nav(Direction::up)), Event::moved);
    EXPECT_EQ(wheel_.focus(), 0);
    EXPECT_EQ(send(wheel_, nav(Direction::up)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    hui::InputFrame held = nav(Direction::up);
    held.nav_repeat = true;
    EXPECT_EQ(send(wheel_, held), Event::refused);
    EXPECT_TRUE(feedback_.cues.empty());
    // A step the shell derived from the stick is not a D-pad press.
    hui::InputFrame from_stick = nav(Direction::right);
    from_stick.nav_from_stick = true;
    EXPECT_EQ(send(wheel_, from_stick), Event::none);

    // A disabled wedge can be pointed at but refuses, and the wheel stays.
    wheel_.set_focus(3);
    EXPECT_EQ(send(wheel_, press(Action::confirm)), Event::refused);
    EXPECT_TRUE(wheel_.is_open());
    EXPECT_EQ(wheel_.choice(), -1);

    wheel_.set_focus(4);
    EXPECT_EQ(send(wheel_, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(asked(Cue::select));
    EXPECT_EQ(wheel_.choice(), 4);
    EXPECT_FALSE(wheel_.is_open());
    settle(wheel_, 2.0f);
    EXPECT_FALSE(wheel_.visible());

    // It reopens on the last choice; back closes it.
    wheel_.open(feedback_);
    EXPECT_EQ(wheel_.focus(), 4);
    EXPECT_EQ(send(wheel_, press(Action::back)), Event::cancelled);
    EXPECT_TRUE(asked(Cue::back));
    EXPECT_FALSE(wheel_.is_open());

    wheel_.style.close_on_activate = false;
    wheel_.open(feedback_);
    EXPECT_EQ(send(wheel_, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(wheel_.is_open());
}

TEST_F(ComponentsStructure, RadialQuickModeChoosesWhenTheButtonIsReleased)
{
    wheel_.style.mode = hui::ui::RadialMode::quick;
    wheel_.open(feedback_);
    wheel_.set_held(true);
    EXPECT_EQ(send(wheel_, idle()), Event::none);
    EXPECT_EQ(send(wheel_, stick(180.0f)), Event::moved);
    EXPECT_TRUE(wheel_.is_open());
    wheel_.set_held(false);
    EXPECT_EQ(send(wheel_, idle()), Event::activated);
    EXPECT_TRUE(asked(Cue::select));
    EXPECT_EQ(wheel_.choice(), 4);
    EXPECT_FALSE(wheel_.is_open());

    // Released on a wedge that cannot be used: refused, and the wheel goes.
    wheel_.open(feedback_);
    wheel_.set_held(true);
    send(wheel_, stick(135.0f));
    EXPECT_EQ(wheel_.focus(), 3);
    wheel_.set_held(false);
    EXPECT_EQ(send(wheel_, stick(135.0f)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_FALSE(wheel_.is_open());
    EXPECT_EQ(wheel_.choice(), -1);
}

TEST_F(ComponentsStructure, WizardStepsForwardAndBack)
{
    EXPECT_EQ(wizard_.step(), 0);
    feedback_.clear();
    EXPECT_EQ(wizard_.back(feedback_), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    feedback_.clear();
    EXPECT_EQ(wizard_.next(feedback_), Event::changed);
    EXPECT_TRUE(asked(Cue::tab));
    EXPECT_EQ(wizard_.step(), 1);
    wizard_.set_step(3);
    EXPECT_FALSE(wizard_.finished());
    feedback_.clear();
    EXPECT_EQ(wizard_.next(feedback_), Event::activated) << "the last step was completed";
    EXPECT_TRUE(asked(Cue::select));
    EXPECT_TRUE(wizard_.finished());
    EXPECT_EQ(wizard_.step(), wizard_.count());
    EXPECT_EQ(wizard_.next(feedback_), Event::refused);
    EXPECT_EQ(wizard_.back(feedback_), Event::changed);
    EXPECT_EQ(wizard_.step(), 3);
    wizard_.set_error(1, true);
    EXPECT_TRUE(wizard_.steps()[1].error);
    wizard_.set_step(99);
    EXPECT_EQ(wizard_.step(), 4) << "clamped to 'all done'";
}

TEST_F(ComponentsStructure, WizardButtonRowDrivesTheStepsAndHandsTheFocusOn)
{
    // Indicator only: it takes no input.
    EXPECT_EQ(send(wizard_, press(Action::confirm)), Event::none);
    EXPECT_EQ(wizard_.step(), 0);

    wizard_.style.buttons = true;
    EXPECT_EQ(wizard_.button(), 1) << "the focus starts on Next";
    EXPECT_EQ(send(wizard_, press(Action::confirm)), Event::changed);
    EXPECT_EQ(wizard_.step(), 1);
    EXPECT_EQ(send(wizard_, nav(Direction::right)), Event::refused);
    EXPECT_EQ(send(wizard_, nav(Direction::left)), Event::moved);
    EXPECT_TRUE(asked(Cue::focus));
    EXPECT_EQ(wizard_.button(), 0);
    EXPECT_EQ(send(wizard_, press(Action::confirm)), Event::changed);
    EXPECT_EQ(wizard_.step(), 0);
    EXPECT_EQ(send(wizard_, press(Action::confirm)), Event::refused) << "nothing before the first";
    EXPECT_EQ(send(wizard_, nav(Direction::left)), Event::refused);

    // Edges that are exits report the way out instead.
    wizard_.style.exits.left = true;
    wizard_.style.exits.down = true;
    EXPECT_EQ(send(wizard_, nav(Direction::left)), Event::none);
    EXPECT_EQ(wizard_.exit(), Direction::left);
    EXPECT_EQ(send(wizard_, nav(Direction::down)), Event::none);
    EXPECT_EQ(wizard_.exit(), Direction::down);
    EXPECT_EQ(send(wizard_, nav(Direction::up)), Event::none);
    EXPECT_EQ(wizard_.exit(), Direction::none);

    // Back goes one step up, and leaves the flow on the first step.
    wizard_.set_step(2);
    EXPECT_EQ(send(wizard_, press(Action::back)), Event::changed);
    EXPECT_EQ(wizard_.step(), 1);
    wizard_.set_step(0);
    EXPECT_EQ(send(wizard_, press(Action::back)), Event::cancelled);
    EXPECT_TRUE(asked(Cue::back));

    // The two buttons sit side by side inside the bounds, in every kind.
    for (hui::ui::WizardKind kind : {hui::ui::WizardKind::horizontal, hui::ui::WizardKind::vertical,
                                     hui::ui::WizardKind::compact})
    {
        wizard_.style.kind = kind;
        const Rect back = wizard_.button_rect(0);
        const Rect next = wizard_.button_rect(1);
        EXPECT_LT(back.x + back.w, next.x + 0.5f);
        EXPECT_LE(next.x + next.w, 1300.0f + 0.5f);
        EXPECT_GE(back.y, 300.0f - 0.5f);
        EXPECT_LE(back.y + back.h, 500.0f + 0.5f);
    }
}

TEST_F(ComponentsStructure, AccordionOpensClosesAndKeepsOneOpenWhenSingle)
{
    EXPECT_FALSE(folds_.is_open(0));
    EXPECT_EQ(send(folds_, press(Action::confirm)), Event::changed);
    EXPECT_TRUE(asked(Cue::toggle));
    EXPECT_TRUE(folds_.is_open(0));
    EXPECT_EQ(send(folds_, nav(Direction::right)), Event::refused) << "already open";
    EXPECT_EQ(send(folds_, nav(Direction::down)), Event::moved);
    EXPECT_TRUE(asked(Cue::focus));
    EXPECT_EQ(folds_.focus(), 1);
    EXPECT_EQ(send(folds_, nav(Direction::right)), Event::changed);
    EXPECT_TRUE(folds_.is_open(1));
    EXPECT_FALSE(folds_.is_open(0)) << "single: the other one closed";
    EXPECT_EQ(send(folds_, nav(Direction::left)), Event::changed);
    EXPECT_FALSE(folds_.is_open(1));
    EXPECT_EQ(send(folds_, nav(Direction::left)), Event::refused);

    // A disabled section can be focused but does not open.
    EXPECT_EQ(send(folds_, nav(Direction::down)), Event::moved);
    EXPECT_EQ(send(folds_, press(Action::confirm)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_FALSE(folds_.is_open(2));

    folds_.style.single = false;
    folds_.set_focus(0);
    EXPECT_EQ(send(folds_, press(Action::confirm)), Event::changed);
    folds_.set_focus(3);
    EXPECT_EQ(send(folds_, press(Action::confirm)), Event::changed);
    EXPECT_TRUE(folds_.is_open(0));
    EXPECT_TRUE(folds_.is_open(3));

    // Ends refuse, stay silent on a held direction, or hand the focus on.
    folds_.set_focus(4);
    EXPECT_EQ(send(folds_, nav(Direction::down)), Event::refused);
    hui::InputFrame held = nav(Direction::down);
    held.nav_repeat = true;
    EXPECT_EQ(send(folds_, held), Event::refused);
    EXPECT_TRUE(feedback_.cues.empty());
    folds_.style.exits.down = true;
    folds_.style.exits.left = true;
    EXPECT_EQ(send(folds_, nav(Direction::down)), Event::none);
    EXPECT_EQ(folds_.exit(), Direction::down);
    EXPECT_EQ(send(folds_, nav(Direction::left)), Event::none) << "closed: left is the way out";
    EXPECT_EQ(folds_.exit(), Direction::left);
    EXPECT_EQ(send(folds_, press(Action::back)), Event::cancelled);
}

TEST_F(ComponentsStructure, AccordionAnimatesItsHeightAndScrollsToTheFocus)
{
    folds_.measure(fonts_);
    const float closed = folds_.header_rect(1).y;
    feedback_.clear();
    EXPECT_EQ(folds_.handle(press(Action::confirm), feedback_), Event::changed);
    folds_.update(kFrame);
    const float moving = folds_.header_rect(1).y;
    EXPECT_GT(moving, closed) << "the next header is on its way down";
    settle(folds_, 2.0f);
    const float open = folds_.header_rect(1).y;
    EXPECT_GT(open, moving) << "... and it did not get there in one frame";
    EXPECT_NEAR(folds_.content_rect(0).y, folds_.header_rect(0).y + 68.0f + 18.0f, 0.5f);

    // Taller than its bounds, it keeps the focused header inside them.
    folds_.style.single = false;
    folds_.set_bounds({100.0f, 100.0f, 600.0f, 220.0f});
    for (int i = 0; i < 4; ++i)
    {
        send(folds_, nav(Direction::down));
        const Rect header = folds_.header_rect(folds_.focus());
        EXPECT_GE(header.y, 100.0f - 0.5f) << i;
        EXPECT_LE(header.y + header.h, 320.0f + 0.5f) << i;
    }
    EXPECT_EQ(folds_.focus(), 4);
    // Restyling keeps what is open and where the focus is.
    folds_.style.theme = hui::ui::themes()[10];
    folds_.measure(fonts_);
    EXPECT_TRUE(folds_.is_open(0));
    EXPECT_EQ(folds_.focus(), 4);
}

TEST_F(ComponentsStructure, TreeWalksOpensAndClosesBranches)
{
    EXPECT_EQ(tree_.count(), 8);
    EXPECT_EQ(tree_.depth(3), 2);
    EXPECT_EQ(tree_.parent(3), 2);
    EXPECT_EQ(tree_.child_count(0), 2);
    EXPECT_TRUE(tree_.is_showing(2));
    EXPECT_FALSE(tree_.is_showing(3)) << "Favourites starts closed";

    EXPECT_EQ(send(tree_, nav(Direction::up)), Event::refused);
    EXPECT_EQ(send(tree_, nav(Direction::right)), Event::moved) << "open: into the first child";
    EXPECT_EQ(tree_.focus(), 1);
    EXPECT_EQ(send(tree_, nav(Direction::right)), Event::refused) << "a leaf has nothing inside";
    EXPECT_EQ(send(tree_, nav(Direction::down)), Event::moved);
    EXPECT_EQ(tree_.focus(), 2);
    EXPECT_EQ(send(tree_, nav(Direction::down)), Event::moved);
    EXPECT_EQ(tree_.focus(), 5) << "hidden rows are stepped over";
    EXPECT_EQ(send(tree_, nav(Direction::up)), Event::moved);

    EXPECT_EQ(send(tree_, nav(Direction::right)), Event::changed);
    EXPECT_TRUE(asked(Cue::toggle));
    EXPECT_TRUE(tree_.is_expanded(2));
    EXPECT_TRUE(tree_.is_showing(4));
    EXPECT_EQ(send(tree_, nav(Direction::right)), Event::moved);
    EXPECT_EQ(tree_.focus(), 3);
    EXPECT_EQ(send(tree_, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(asked(Cue::select));
    EXPECT_EQ(send(tree_, nav(Direction::left)), Event::moved) << "out to the parent";
    EXPECT_EQ(tree_.focus(), 2);
    EXPECT_EQ(send(tree_, nav(Direction::left)), Event::changed);
    EXPECT_FALSE(tree_.is_expanded(2));
    EXPECT_EQ(send(tree_, nav(Direction::left)), Event::moved);
    EXPECT_EQ(tree_.focus(), 0);

    // Confirm on a branch toggles it, unless the screen wants the activation.
    EXPECT_EQ(send(tree_, press(Action::confirm)), Event::changed);
    EXPECT_FALSE(tree_.is_expanded(0));
    EXPECT_EQ(send(tree_, nav(Direction::left)), Event::refused) << "a closed root";
    tree_.style.confirm_toggles = false;
    EXPECT_EQ(send(tree_, press(Action::confirm)), Event::activated);

    // The disabled row is the last one: it refuses confirm, the end refuses down.
    EXPECT_EQ(send(tree_, nav(Direction::down)), Event::moved);
    EXPECT_EQ(send(tree_, nav(Direction::down)), Event::moved);
    EXPECT_EQ(tree_.focus(), 7);
    EXPECT_EQ(send(tree_, press(Action::confirm)), Event::refused);
    EXPECT_EQ(send(tree_, nav(Direction::down)), Event::refused);
    tree_.style.exits.right = true;
    EXPECT_EQ(send(tree_, nav(Direction::right)), Event::none);
    EXPECT_EQ(tree_.exit(), Direction::right);
    tree_.style.wrap = true;
    EXPECT_EQ(send(tree_, nav(Direction::down)), Event::moved);
    EXPECT_EQ(tree_.focus(), 0);
}

TEST_F(ComponentsStructure, TreeRowsSlideAndTheFocusLeavesAClosingBranch)
{
    const float before = tree_.row_rect(5).y;
    tree_.set_expanded(2, true);
    tree_.update(kFrame);
    const float moving = tree_.row_rect(5).y;
    EXPECT_GT(moving, before);
    settle(tree_, 2.0f);
    EXPECT_NEAR(tree_.row_rect(5).y, before + 2.0f * 60.0f, 0.5f) << "two rows of 56 + 4";

    tree_.set_focus(4);
    EXPECT_EQ(tree_.focus(), 4);
    tree_.set_expanded(2, false);
    EXPECT_EQ(tree_.focus(), 2) << "the focus cannot stay on a row that goes away";
    // Focusing a hidden node opens the way to it.
    tree_.collapse_all(true);
    EXPECT_EQ(tree_.focus(), 0);
    tree_.set_focus(4);
    EXPECT_TRUE(tree_.is_expanded(0));
    EXPECT_TRUE(tree_.is_expanded(2));
    EXPECT_TRUE(tree_.is_showing(4));

    // More rows than fit: the focused one stays inside the bounds.
    tree_.expand_all(true);
    tree_.set_bounds({100.0f, 100.0f, 500.0f, 200.0f});
    tree_.set_focus(0);
    for (int i = 0; i < 7; ++i)
    {
        send(tree_, nav(Direction::down));
        const Rect row = tree_.row_rect(tree_.focus());
        EXPECT_GE(row.y, 100.0f - 0.5f) << i;
        EXPECT_LE(row.y + row.h, 300.0f + 0.5f) << i;
    }
    EXPECT_EQ(tree_.focus(), 7);
}

TEST_F(ComponentsStructure, JumpBarSkipsEmptyLettersAndReportsTheLabel)
{
    EXPECT_EQ(index_.label(), "A");
    EXPECT_EQ(send(index_, nav(Direction::down)), Event::changed);
    EXPECT_TRUE(asked(Cue::slider));
    EXPECT_EQ(index_.label(), "C") << "nothing under B";
    EXPECT_EQ(send(index_, nav(Direction::up)), Event::changed);
    EXPECT_EQ(send(index_, nav(Direction::up)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    // A vertical bar leaves left and right to the screen.
    EXPECT_EQ(send(index_, nav(Direction::left)), Event::none);
    EXPECT_EQ(index_.exit(), Direction::none);
    index_.style.exits.left = true;
    EXPECT_EQ(send(index_, nav(Direction::left)), Event::none);
    EXPECT_EQ(index_.exit(), Direction::left);

    // A shortcut button drives it through step(); the list through set_current().
    feedback_.clear();
    EXPECT_EQ(index_.step(1, idle(), feedback_), Event::changed);
    EXPECT_EQ(index_.label(), "C");
    index_.set_current(index_.find("Y"));
    EXPECT_TRUE(feedback_.cues.size() == 1u) << "set_current is silent";
    EXPECT_EQ(index_.step(1, idle(), feedback_), Event::refused) << "Z is empty: Y is the end";
    index_.style.wrap = true;
    EXPECT_EQ(index_.step(1, idle(), feedback_), Event::changed);
    EXPECT_EQ(index_.label(), "A");
    EXPECT_EQ(index_.step(-1, idle(), feedback_), Event::changed);
    EXPECT_EQ(index_.label(), "Y");
    EXPECT_EQ(index_.find("?"), -1);
    EXPECT_EQ(send(index_, press(Action::confirm)), Event::activated);

    // Lying down, it steps on left and right, and every label has its place.
    index_.style.vertical = false;
    index_.set_bounds({400.0f, 300.0f, 520.0f, 44.0f});
    EXPECT_EQ(send(index_, nav(Direction::left)), Event::changed);
    EXPECT_EQ(index_.label(), "X");
    EXPECT_EQ(send(index_, nav(Direction::down)), Event::none);
    const Rect first = index_.item_rect(0);
    const Rect last = index_.item_rect(25);
    EXPECT_GE(first.x, 400.0f - 0.5f);
    EXPECT_LE(last.x + last.w, 920.0f + 0.5f);
    EXPECT_LT(first.x, last.x);
}

TEST_F(ComponentsStructure, DrawsInEveryThemeAndVariant)
{
    using hui::ui::HighlightKind;
    wheel_.icon = [](hui::ui::Canvas &target, const Rect &box, const RadialItem &, int, float,
                     hui::gfx::Color ink) { target.list.circle(box.cx(), box.cy(), 8.0f, ink); };
    wheel_.item(1).description = "A description long enough to need two lines in the hub";
    wheel_.item(1).value = "3 left";
    wheel_.open(feedback_);
    send(wheel_, stick(135.0f)); // on the disabled wedge
    folds_.measure(fonts_);
    folds_.content =
        [](hui::ui::Canvas &target, const Rect &area, const hui::ui::AccordionSection &, int, float)
    { target.list.rounded_rect(area, 0.0f, {1, 1, 1, 1}); };
    folds_.set_open(0, true);
    folds_.set_open(3, true);
    tree_.icon = [](hui::ui::Canvas &target, const Rect &box, const TreeNode &, int, float,
                    hui::gfx::Color ink) { target.list.circle(box.cx(), box.cy(), 8.0f, ink); };
    tree_.node(1).badge = "NEW";
    tree_.set_focus(4);
    wizard_.set_step(2);
    wizard_.set_error(3, true);
    wizard_.step_at(1).caption = "Optional";
    index_.set_focused(true);
    index_.set_current(index_.find("M"));

    const HighlightKind highlights[] = {HighlightKind::tint, HighlightKind::fill,
                                        HighlightKind::bar, HighlightKind::ring};
    const hui::ui::RadialLabels labels[] = {
        hui::ui::RadialLabels::outside, hui::ui::RadialLabels::inside, hui::ui::RadialLabels::none,
        hui::ui::RadialLabels::outside};
    const hui::ui::WizardKind kinds[] = {
        hui::ui::WizardKind::horizontal, hui::ui::WizardKind::vertical,
        hui::ui::WizardKind::compact, hui::ui::WizardKind::horizontal};
    for (const hui::ui::Theme &theme : hui::ui::themes())
    {
        for (int variant = 0; variant < 4; ++variant)
        {
            wheel_.style.theme = theme;
            wheel_.style.labels = labels[variant];
            wheel_.style.scrim_color =
                variant == 3 ? hui::gfx::Color{0, 0, 0, 1} : hui::gfx::Color{0, 0, 0, 0};
            wheel_.style.hub_radius = variant == 2 ? 0.0f : 150.0f;
            wheel_.style.reduced_motion = variant == 1;
            wizard_.style.theme = theme;
            wizard_.style.kind = kinds[variant];
            wizard_.style.buttons = variant != 3;
            wizard_.style.numbers = variant != 1;
            wizard_.set_step(variant == 3 ? 4 : 2);
            wizard_.set_bounds({400.0f, 300.0f, 900.0f, variant == 1 ? 520.0f : 200.0f});
            folds_.style.theme = theme;
            folds_.style.highlight.kind = highlights[variant];
            folds_.style.cards = variant % 2 == 0;
            folds_.style.dividers = variant == 1;
            folds_.style.panel = variant == 3;
            folds_.style.chevron = static_cast<hui::ui::AccordionChevron>(variant % 3);
            // The short one overflows, so the scrolling path is drawn too.
            folds_.set_bounds({100.0f, 100.0f, 600.0f, variant == 2 ? 180.0f : 640.0f});
            tree_.style.theme = theme;
            tree_.style.highlight.kind = highlights[variant];
            tree_.style.guides = variant != 1;
            tree_.style.panel = variant % 2 == 1;
            tree_.style.icon_width = variant == 0 ? 0.0f : 26.0f;
            tree_.set_bounds({100.0f, 100.0f, 500.0f, variant == 2 ? 150.0f : 600.0f});
            index_.style.theme = theme;
            index_.style.vertical = variant % 2 == 0;
            index_.style.track = variant != 3;
            index_.style.bubble = static_cast<hui::ui::JumpBubble>(variant % 3);
            index_.set_bounds(variant % 2 == 0 ? Rect{1700.0f, 200.0f, 44.0f, 700.0f}
                                               : Rect{400.0f, 300.0f, 700.0f, 44.0f});
            for (int frame = 0; frame < 3; ++frame)
            {
                wheel_.update(kFrame);
                wizard_.update(kFrame);
                folds_.update(kFrame);
                tree_.update(kFrame);
                index_.update(kFrame);
            }
            hui::ui::Canvas target = canvas();
            wizard_.draw(target);
            folds_.draw(target);
            tree_.draw(target);
            index_.draw(target);
            wheel_.draw(target);
            expect_drawn(theme.id);
        }
    }
    // Restyling thirty times lost nothing.
    EXPECT_TRUE(wheel_.is_open());
    EXPECT_EQ(wheel_.focus(), 3);
    EXPECT_TRUE(folds_.is_open(3));
    EXPECT_EQ(tree_.focus(), 4);
    EXPECT_EQ(index_.label(), "M");
}

TEST_F(ComponentsStructure, GalleryPageTourLeavesItAsItWasFound)
{
    const std::unique_ptr<hui::concepts::gallery::Page> page =
        hui::concepts::gallery::make_structure_page(context_);
    const std::string first = page->variant();
    const hui::ui::Hint *hints = page->hints().data();
    int pictures = 0;
    std::size_t steps = 0;
    for (const hui::app::TourStep &step : page->tour())
    {
        ++steps;
        // The tour holds the stick and the held buttons while a step waits:
        // the quick wheel depends on both.
        hui::InputFrame waiting = idle();
        waiting.stick_x = step.stick_x;
        waiting.stick_y = step.stick_y;
        waiting.held = step.hold;
        for (int frame = 0, frames = static_cast<int>(step.wait / kFrame); frame < frames; ++frame)
            page->update(waiting, kFrame, feedback_);
        if (step.capture != nullptr)
        {
            EXPECT_EQ(std::strncmp(step.capture, "structure", 9), 0) << step.capture;
            ++pictures;
            hui::ui::Canvas target = canvas();
            page->draw(target);
            page->draw_modal(target);
            expect_drawn(step.capture);
        }
        hui::InputFrame input = waiting;
        input.pressed = step.press;
        input.held = step.press | step.hold;
        input.nav = step.nav;
        feedback_.clear();
        page->update(input, kFrame, feedback_);
    }
    EXPECT_GE(steps, 10u);
    EXPECT_LE(steps, 25u);
    EXPECT_GE(pictures, 3);
    EXPECT_LE(pictures, 5);
    EXPECT_EQ(first, page->variant());
    EXPECT_EQ(page->hints().data(), hints) << "the wheel is closed";
}

} // namespace
