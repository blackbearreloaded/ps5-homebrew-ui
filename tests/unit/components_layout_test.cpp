// ps5-homebrew-ui - Tests: the layout group (FocusGroup, SplitView, ScrollArea and friends).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "component_fixture.hpp"
#include "concepts/components/page.hpp"
#include "ui/components/focus_group.hpp"
#include "ui/components/layout.hpp"
#include "ui/components/scroll_area.hpp"
#include "ui/components/split_view.hpp"
#include "ui/components/surface.hpp"
#include "ui/components/transition.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;
using hui::gfx::Rect;
using hui::ui::Event;
using hui::ui::FocusGroup;
using hui::ui::FocusItem;
using hui::ui::LayoutAlign;
using hui::ui::LayoutChild;
using hui::ui::ScrollArea;
using hui::ui::SplitView;
using hui::ui::Transition;
using hui::ui::TransitionKind;
using hui::ui::TransitionPhase;

// The small arithmetic is constexpr: a layout of constants costs nothing.
constexpr Rect kBox{100.0f, 50.0f, 400.0f, 200.0f};
static_assert(hui::ui::inset(kBox, hui::ui::Edges::all(10.0f)).w == 380.0f);
static_assert(hui::ui::split(kBox, hui::ui::Side::top, 40.0f, 10.0f).rest.h == 150.0f);
static_assert(hui::ui::split(kBox, hui::ui::Side::right, 100.0f).strip.x == 400.0f);
static_assert(hui::ui::center_in(kBox, 100.0f, 100.0f).x == 250.0f);
static_assert(hui::ui::aspect_fit(kBox, 1.0f).w == 200.0f);
static_assert(hui::ui::aspect_fill(kBox, 1.0f).h == 400.0f);
static_assert(hui::ui::space(3.0f) == 24.0f);

class ComponentsLayout : public hui::testing::ComponentFixture
{
  protected:
    // An irregular arrangement: three items of different sizes above a wide
    // one and a small one.
    //   [1: A ] [2: B ] [3: C, short]
    //   [4: D, wide   ] [5: E ]
    void build(FocusGroup &group)
    {
        group.add({1, {0.0f, 0.0f, 100.0f, 100.0f}});
        group.add({2, {120.0f, 0.0f, 100.0f, 100.0f}});
        group.add({3, {240.0f, 10.0f, 100.0f, 40.0f}});
        group.add({4, {0.0f, 120.0f, 220.0f, 100.0f}});
        group.add({5, {240.0f, 120.0f, 100.0f, 100.0f}});
    }

    Event send(FocusGroup &group, const hui::InputFrame &input)
    {
        feedback_.clear();
        const Event event = group.handle(input, feedback_);
        for (int i = 0; i < 30; ++i)
            group.update(kFrame);
        return event;
    }

    template <typename Component> void settle(Component &component, float seconds = 3.0f)
    {
        for (int i = 0, frames = static_cast<int>(seconds / kFrame); i < frames; ++i)
            component.update(kFrame);
    }

    static void expect_rect(const Rect &r, float x, float y, float w, float h)
    {
        EXPECT_NEAR(r.x, x, 0.01f);
        EXPECT_NEAR(r.y, y, 0.01f);
        EXPECT_NEAR(r.w, w, 0.01f);
        EXPECT_NEAR(r.h, h, 0.01f);
    }
};

TEST_F(ComponentsLayout, StacksShareTheRoomByWeightWithinLimits)
{
    // Fixed, flexible, flexible with a maximum: the capped child gives its
    // surplus to the other flexible one.
    hui::ui::Row row{10.0f};
    const LayoutChild children[] = {LayoutChild::fixed(100.0f), LayoutChild::flexible(1.0f),
                                    LayoutChild::flexible(1.0f, 0.0f, 50.0f)};
    const std::vector<Rect> r = row.layout({0.0f, 0.0f, 400.0f, 60.0f}, children);
    ASSERT_EQ(r.size(), 3u);
    expect_rect(r[0], 0.0f, 0.0f, 100.0f, 60.0f);
    expect_rect(r[1], 110.0f, 0.0f, 230.0f, 60.0f);
    expect_rect(r[2], 350.0f, 0.0f, 50.0f, 60.0f);
    EXPECT_NEAR(row.content_size(children), 120.0f, 0.01f);

    // A column with padding; spare room goes where `main` says, and a child
    // with a size across the axis is placed by `cross`.
    hui::ui::Column column{8.0f, hui::ui::Edges::all(10.0f)};
    column.main = LayoutAlign::end;
    column.cross = LayoutAlign::center;
    const LayoutChild two[] = {LayoutChild::fixed(40.0f, 100.0f), LayoutChild::fixed(40.0f)};
    const std::vector<Rect> c = column.layout({0.0f, 0.0f, 220.0f, 220.0f}, two);
    expect_rect(c[0], 60.0f, 122.0f, 100.0f, 40.0f);
    expect_rect(c[1], 10.0f, 170.0f, 200.0f, 40.0f);

    // stretch along the axis puts the spare room between the children.
    column.main = LayoutAlign::stretch;
    const std::vector<Rect> apart = column.layout({0.0f, 0.0f, 220.0f, 220.0f}, two);
    EXPECT_NEAR(apart[0].y, 10.0f, 0.01f);
    EXPECT_NEAR(apart[1].y + apart[1].h, 210.0f, 0.01f);

    // A minimum is honoured even when there is no room for it.
    const LayoutChild tight[] = {LayoutChild::flexible(1.0f, 80.0f), LayoutChild::fixed(60.0f)};
    const std::vector<Rect> t = hui::ui::Row{}.layout({0.0f, 0.0f, 100.0f, 20.0f}, tight);
    EXPECT_NEAR(t[0].w, 80.0f, 0.01f);
    // Spacers are children like any other.
    const LayoutChild bar[] = {LayoutChild::fixed(50.0f), hui::ui::Spacer::flex(),
                               LayoutChild::fixed(50.0f)};
    const std::vector<Rect> b = hui::ui::Row{}.layout({0.0f, 0.0f, 300.0f, 20.0f}, bar);
    EXPECT_NEAR(b[2].x, 250.0f, 0.01f);
    // Equal children by count.
    EXPECT_NEAR(hui::ui::Row{10.0f}.layout({0.0f, 0.0f, 320.0f, 20.0f}, 3)[2].x, 220.0f, 0.01f);
}

TEST_F(ComponentsLayout, GridAndWrapPlaceChildrenAndReportTheirExtent)
{
    hui::ui::GridLayout grid;
    grid.columns = 3;
    grid.gap_x = 10.0f;
    grid.gap_y = 10.0f;
    grid.row_height = 50.0f;
    // A 2x2 feature, then single cells: they fill the holes beside it.
    const hui::ui::GridSpan spans[] = {{2, 2}, {1, 1}, {1, 1}, {3, 1}, {1, 1}};
    const std::vector<Rect> r = grid.layout({0.0f, 0.0f, 320.0f, 0.0f}, spans);
    EXPECT_NEAR(grid.cell_width(320.0f), 100.0f, 0.01f);
    expect_rect(r[0], 0.0f, 0.0f, 210.0f, 110.0f);
    expect_rect(r[1], 220.0f, 0.0f, 100.0f, 50.0f);
    expect_rect(r[2], 220.0f, 60.0f, 100.0f, 50.0f);
    expect_rect(r[3], 0.0f, 120.0f, 320.0f, 50.0f);
    expect_rect(r[4], 0.0f, 180.0f, 100.0f, 50.0f);
    expect_rect(hui::ui::bounds_of(r), 0.0f, 0.0f, 320.0f, 230.0f);

    // Without a row height the cells take an aspect, or share the bounds.
    grid.row_height = 0.0f;
    grid.cell_aspect = 2.0f;
    EXPECT_NEAR(grid.layout({0.0f, 0.0f, 320.0f, 0.0f}, 4)[3].y, 60.0f, 0.01f);
    grid.cell_aspect = 0.0f;
    EXPECT_NEAR(grid.layout({0.0f, 0.0f, 320.0f, 210.0f}, 6)[5].h, 100.0f, 0.01f);

    hui::ui::Wrap wrap;
    wrap.gap_x = 10.0f;
    wrap.gap_y = 10.0f;
    const hui::ui::LayoutSize sizes[] = {
        {100.0f, 30.0f}, {100.0f, 50.0f}, {100.0f, 30.0f}, {500.0f, 30.0f}};
    std::vector<Rect> w = wrap.layout({0.0f, 0.0f, 250.0f, 0.0f}, sizes);
    expect_rect(w[1], 110.0f, 0.0f, 100.0f, 50.0f);
    // The third does not fit the first line; the line was as tall as its tallest.
    expect_rect(w[2], 0.0f, 60.0f, 100.0f, 30.0f);
    // Something wider than a line is cut to it.
    expect_rect(w[3], 0.0f, 100.0f, 250.0f, 30.0f);
    wrap.line = LayoutAlign::stretch;
    wrap.cross = LayoutAlign::stretch;
    w = wrap.layout({0.0f, 0.0f, 250.0f, 0.0f}, sizes);
    expect_rect(w[0], 0.0f, 0.0f, 120.0f, 50.0f);
    EXPECT_NEAR(w[1].x + w[1].w, 250.0f, 0.01f);
}

TEST_F(ComponentsLayout, SpringLayoutGlidesToANewArrangement)
{
    const std::vector<Rect> before = hui::ui::Row{10.0f}.layout({0.0f, 0.0f, 320.0f, 40.0f}, 3);
    const std::vector<Rect> after = hui::ui::Column{10.0f}.layout({0.0f, 0.0f, 320.0f, 140.0f}, 3);
    hui::ui::SpringLayout springs;
    hui::ui::ComponentStyle style;
    springs.target(before); // the first target snaps
    EXPECT_TRUE(springs.settled());
    expect_rect(springs.rect(2), before[2].x, before[2].y, before[2].w, before[2].h);

    springs.target(after);
    springs.update(kFrame, style);
    EXPECT_FALSE(springs.settled());
    EXPECT_GT(springs.rect(2).x, after[2].x);
    EXPECT_LT(springs.rect(2).x, before[2].x);
    for (int i = 0; i < 180; ++i)
        springs.update(kFrame, style);
    EXPECT_TRUE(springs.settled());
    expect_rect(springs.rect(2), after[2].x, after[2].y, after[2].w, after[2].h);

    // A child that appears later starts in its place; reduced motion snaps.
    std::vector<Rect> more = before;
    more.push_back({500.0f, 0.0f, 40.0f, 40.0f});
    springs.target(more);
    expect_rect(springs.rect(3), 500.0f, 0.0f, 40.0f, 40.0f);
    style.reduced_motion = true;
    springs.update(kFrame, style);
    EXPECT_TRUE(springs.settled());
    expect_rect(springs.rect(0), before[0].x, before[0].y, before[0].w, before[0].h);

    // With a stagger the children leave one after another.
    style.reduced_motion = false;
    springs.stagger = 0.1f;
    springs.target(after);
    springs.update(kFrame, style);
    springs.update(kFrame, style);
    EXPECT_NE(springs.rect(0).w, before[0].w);
    EXPECT_EQ(springs.rect(2).x, before[2].x);
}

TEST_F(ComponentsLayout, FocusMovesTheWayItLooks)
{
    FocusGroup group;
    build(group);
    EXPECT_EQ(group.focus(), 1); // the first item added
    EXPECT_EQ(send(group, nav(Direction::right)), Event::moved);
    EXPECT_EQ(group.focus(), 2);
    EXPECT_TRUE(asked(Cue::focus));
    EXPECT_EQ(send(group, nav(Direction::right)), Event::moved);
    EXPECT_EQ(group.focus(), 3);
    // Down from the short item: the one under it, not the wide one beside.
    EXPECT_EQ(send(group, nav(Direction::down)), Event::moved);
    EXPECT_EQ(group.focus(), 5);
    EXPECT_EQ(send(group, nav(Direction::left)), Event::moved);
    EXPECT_EQ(group.focus(), 4);
    // Up from the wide item: two items line up equally; the first added wins.
    EXPECT_EQ(group.neighbour(Direction::up), 1);
    // The wide item below is not "to the right" of anything above it.
    group.set_focus(2);
    EXPECT_EQ(group.neighbour(Direction::down), 4);
    EXPECT_LT(FocusGroup::cost({120.0f, 0.0f, 100.0f, 100.0f}, {0.0f, 120.0f, 400.0f, 100.0f},
                               Direction::right, group.style),
              0.0f);

    // Straight ahead beats nearer but off to the side, up to beam_bias.
    const Rect from{0.0f, 0.0f, 100.0f, 100.0f};
    const Rect off_beam{150.0f, 110.0f, 100.0f, 100.0f};
    const float beside = FocusGroup::cost(from, off_beam, Direction::right, group.style);
    EXPECT_LT(FocusGroup::cost(from, {500.0f, 0.0f, 100.0f, 100.0f}, Direction::right, group.style),
              beside);
    EXPECT_GT(FocusGroup::cost(from, {900.0f, 0.0f, 100.0f, 100.0f}, Direction::right, group.style),
              beside);

    // Overrides: a fixed neighbour, and "nothing that way".
    group.set_focus(1);
    group.set_neighbour(1, Direction::right, 5);
    EXPECT_EQ(group.neighbour(Direction::right), 5);
    group.set_neighbour(1, Direction::right, hui::ui::kNeighbourNone);
    EXPECT_EQ(send(group, nav(Direction::right)), Event::refused);
    group.set_neighbour(1, Direction::right, hui::ui::kNeighbourAuto);
    // A disabled item is passed over.
    group.set_enabled(2, false);
    EXPECT_EQ(group.neighbour(Direction::right), 3);
}

TEST_F(ComponentsLayout, FocusEdgesRefuseWrapOrExit)
{
    FocusGroup group;
    build(group);
    EXPECT_EQ(send(group, nav(Direction::left)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_EQ(group.focus(), 1);
    hui::InputFrame held = nav(Direction::left);
    held.nav_repeat = true;
    EXPECT_EQ(send(group, held), Event::refused);
    EXPECT_TRUE(feedback_.cues.empty());

    // Wrap: past the first item of a row comes its last; never on a repeat.
    group.style.wrap = true;
    EXPECT_EQ(send(group, held), Event::refused);
    EXPECT_EQ(send(group, nav(Direction::left)), Event::moved);
    EXPECT_EQ(group.focus(), 3);
    EXPECT_EQ(send(group, nav(Direction::right)), Event::moved);
    EXPECT_EQ(group.focus(), 1);
    group.style.wrap = false;

    // An edge that hands the focus on: no event, no sound, exit() names it.
    group.style.exits.up = true;
    EXPECT_EQ(send(group, nav(Direction::up)), Event::none);
    EXPECT_EQ(group.exit(), Direction::up);
    EXPECT_TRUE(feedback_.cues.empty());
    EXPECT_EQ(send(group, nav(Direction::right)), Event::moved);
    EXPECT_EQ(group.exit(), Direction::none);

    // Confirm and back pass through with their cues.
    EXPECT_EQ(send(group, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(asked(Cue::select));
    EXPECT_EQ(send(group, press(Action::back)), Event::cancelled);
    EXPECT_TRUE(asked(Cue::back));
    group.style.sounds.move = Cue::count;
    EXPECT_EQ(send(group, nav(Direction::left)), Event::moved);
    EXPECT_TRUE(feedback_.cues.empty());
}

TEST_F(ComponentsLayout, FocusRemembersEachScopeAndModalLayersRestoreIt)
{
    FocusGroup group;
    // Two panes of two items each, and a dialog.
    group.add({1, {0.0f, 0.0f, 100.0f, 100.0f}, 1});
    group.add({2, {0.0f, 120.0f, 100.0f, 100.0f}, 1});
    group.add({3, {200.0f, 0.0f, 100.0f, 100.0f}, 2});
    group.add({4, {200.0f, 120.0f, 100.0f, 100.0f}, 2});
    group.add({10, {400.0f, 400.0f, 100.0f, 50.0f}, 9});
    group.add({11, {520.0f, 400.0f, 100.0f, 50.0f}, 9});

    group.set_focus(2);
    EXPECT_EQ(group.focus_scope(), 1);
    send(group, nav(Direction::right));
    EXPECT_EQ(group.focus(), 4); // nothing remembered yet: by geometry
    send(group, nav(Direction::up));
    EXPECT_EQ(group.focus(), 3);
    send(group, nav(Direction::left));
    EXPECT_EQ(group.focus(), 2); // geometry says 1; the pane remembers 2
    send(group, nav(Direction::right));
    EXPECT_EQ(group.focus(), 3); // and the other pane remembers 3
    group.forget(1);
    send(group, nav(Direction::left));
    EXPECT_EQ(group.focus(), 1);
    group.style.remember = false;
    send(group, nav(Direction::down));
    send(group, nav(Direction::right));
    EXPECT_EQ(group.focus(), 4);

    // A modal layer: the focus goes in, stays in, and comes back out.
    group.push_scope(9, 11);
    EXPECT_EQ(group.scope_depth(), 1);
    EXPECT_EQ(group.focus(), 11);
    EXPECT_EQ(send(group, nav(Direction::up)), Event::refused);
    EXPECT_EQ(send(group, nav(Direction::left)), Event::moved);
    EXPECT_EQ(group.focus(), 10);
    EXPECT_EQ(send(group, nav(Direction::left)), Event::refused);
    group.pop_scope();
    EXPECT_EQ(group.scope_depth(), 0);
    EXPECT_EQ(group.focus(), 4);
    // Pushed again without an id, it opens where it was left.
    group.push_scope(9);
    EXPECT_EQ(group.focus(), 10);
    group.pop_scope();

    // An item that goes away hands the focus to the nearest one left.
    group.update(kFrame);
    group.remove(4);
    group.update(kFrame);
    EXPECT_EQ(group.focus(), 3);
    // Rebuilding the items keeps the focus as long as its id comes back.
    group.clear();
    EXPECT_EQ(group.focus(), -1);
    group.add({7, {0.0f, 0.0f, 10.0f, 10.0f}});
    group.add({3, {200.0f, 0.0f, 100.0f, 100.0f}, 2});
    group.update(kFrame);
    EXPECT_EQ(group.focus(), 3);
}

TEST_F(ComponentsLayout, FocusHighlightGlidesAndTravelsWithItsItem)
{
    FocusGroup group;
    build(group);
    group.update(kFrame);
    EXPECT_NEAR(group.focus_amount(1), 1.0f, 0.01f);
    EXPECT_NEAR(group.focus_amount(5), 0.0f, 0.01f);

    // A move glides: one frame later the highlight is between the two.
    feedback_.clear();
    group.handle(nav(Direction::right), feedback_);
    group.update(kFrame);
    const float x = group.highlight_rect(0.0f).x;
    EXPECT_GT(x, 0.0f);
    EXPECT_LT(x, 120.0f);
    for (int i = 0; i < 60; ++i)
        group.update(kFrame);
    EXPECT_NEAR(group.highlight_rect(0.0f).x, 120.0f, 0.1f);
    EXPECT_NEAR(group.focus_amount(2), 1.0f, 0.01f);

    // The focused item scrolls away: the highlight is on it at once, not
    // chasing it.
    group.set_rect(2, {120.0f, -300.0f, 100.0f, 100.0f});
    group.update(kFrame);
    EXPECT_NEAR(group.highlight_rect(0.0f).y, -300.0f, 0.1f);
    EXPECT_NEAR(group.focus_amount(2), 1.0f, 0.01f);

    // A group without the screen's focus hides its highlight.
    group.set_active(false);
    for (int i = 0; i < 60; ++i)
        group.update(kFrame);
    EXPECT_NEAR(group.focus_amount(2), 0.0f, 0.01f);
    hui::ui::Canvas target = canvas();
    group.draw(target);
    EXPECT_TRUE(list_.empty());
    group.style.inactive = 0.4f;
    group.draw(target);
    EXPECT_FALSE(list_.empty());
}

TEST_F(ComponentsLayout, SplitViewPlacesPanesAnimatesItsRatioAndCrosses)
{
    SplitView split;
    split.style.gap = 20.0f;
    split.style.ratio = 0.4f;
    split.set_bounds({0.0f, 0.0f, 1000.0f, 500.0f});
    expect_rect(split.pane_rect(0), 0.0f, 0.0f, 392.0f, 500.0f); // before any update
    split.update(kFrame);
    expect_rect(split.pane_rect(1), 412.0f, 0.0f, 588.0f, 500.0f);
    expect_rect(split.divider_rect(), 392.0f, 0.0f, 20.0f, 500.0f);

    split.set_ratio(0.6f);
    EXPECT_NEAR(split.target_pane_rect(0).w, 588.0f, 0.01f);
    split.update(kFrame);
    EXPECT_TRUE(split.moving());
    EXPECT_GT(split.pane_rect(0).w, 392.0f);
    EXPECT_LT(split.pane_rect(0).w, 588.0f);
    settle(split);
    EXPECT_FALSE(split.moving());
    EXPECT_NEAR(split.pane_rect(0).w, 588.0f, 0.01f);

    // The focused pane is emphasised; the other dims.
    split.style.dim = 0.4f;
    split.set_focus(1);
    settle(split);
    EXPECT_NEAR(split.pane_opacity(1), 1.0f, 0.01f);
    EXPECT_NEAR(split.pane_opacity(0), 0.6f, 0.01f);

    // The focus outside the view: no pane dims, and there is nothing to cross.
    split.set_focus(-1);
    settle(split);
    EXPECT_NEAR(split.pane_opacity(0), 1.0f, 0.01f);
    EXPECT_EQ(split.pane_toward(Direction::left), -1);
    split.set_focus(1, true);

    // Crossing: left from the second pane, then nothing further left.
    feedback_.clear();
    EXPECT_EQ(split.pane_toward(Direction::up), -1);
    EXPECT_TRUE(split.cross(Direction::left, feedback_));
    EXPECT_EQ(split.focus(), 0);
    EXPECT_TRUE(asked(Cue::focus));
    feedback_.clear();
    EXPECT_EQ(split.handle_exit(Direction::left, nav(Direction::left), feedback_), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_EQ(split.handle_exit(Direction::right, nav(Direction::right), feedback_), Event::moved);
    EXPECT_EQ(split.focus(), 1);

    // A collapsed first pane gives everything to the second, gap included,
    // and cannot be crossed into.
    split.set_preset(hui::ui::SplitPreset::collapsed);
    settle(split);
    EXPECT_TRUE(split.collapsed());
    expect_rect(split.pane_rect(1), 0.0f, 0.0f, 1000.0f, 500.0f);
    EXPECT_EQ(split.pane_toward(Direction::left), -1);
    EXPECT_NEAR(split.pane_opacity(0), 0.0f, 0.01f);
    split.set_preset(hui::ui::SplitPreset::balanced, true);
    EXPECT_NEAR(split.pane_rect(0).w, 490.0f, 0.01f);

    // Stacked, with a third pane and panels: three rectangles that tile the
    // bounds, each inside its panel's padding.
    split.style.axis = hui::ui::SplitAxis::vertical;
    split.style.third = 0.25f;
    split.style.panels = true;
    split.style.panel_padding = 10.0f;
    settle(split);
    EXPECT_EQ(split.pane_count(), 3);
    EXPECT_NEAR(split.pane_frame(0).h, 230.0f, 0.01f);
    EXPECT_NEAR(split.pane_frame(2).y + split.pane_frame(2).h, 500.0f, 0.01f);
    EXPECT_NEAR(split.pane_rect(2).h, split.pane_frame(2).h - 20.0f, 0.01f);
    split.set_focus(1);
    EXPECT_EQ(split.pane_toward(Direction::down), 2);
    EXPECT_EQ(split.pane_toward(Direction::right), -1);

    // Dimming: a fade pushes an opacity and draws nothing; a veil draws a
    // wash over the pane that has not the focus, and none over the one that has.
    split.style.dim_mode = hui::ui::SplitDim::fade;
    hui::ui::Canvas target = canvas();
    split.begin_pane(target, 0);
    split.end_pane(target, 0);
    EXPECT_TRUE(list_.empty());
    split.style.dim_mode = hui::ui::SplitDim::veil;
    split.begin_pane(target, 1);
    split.end_pane(target, 1);
    EXPECT_TRUE(list_.empty());
    split.begin_pane(target, 0);
    split.end_pane(target, 0);
    EXPECT_FALSE(list_.empty());
}

TEST_F(ComponentsLayout, ScrollAreaFollowsTheFocusAndTheStick)
{
    ScrollArea scroll;
    scroll.set_bounds({100.0f, 100.0f, 400.0f, 300.0f});
    scroll.set_content_size(900.0f, 1000.0f);
    EXPECT_NEAR(scroll.max_y(), 700.0f, 0.01f);
    EXPECT_NEAR(scroll.max_x(), 0.0f, 0.01f); // a vertical area never moves sideways

    const Rect card{0.0f, 600.0f, 400.0f, 100.0f};
    // At the top nothing is hidden above, so the first row does not fade; a
    // card the bottom edge cuts does, and one below the view is not there.
    EXPECT_NEAR(scroll.visibility({0.0f, 0.0f, 400.0f, 100.0f}), 1.0f, 0.01f);
    EXPECT_NEAR(scroll.visibility({0.0f, 250.0f, 400.0f, 100.0f}), 0.625f, 0.01f);
    EXPECT_NEAR(scroll.visibility(card), 0.0f, 0.01f);
    scroll.reveal(card);
    scroll.update(kFrame);
    EXPECT_GT(scroll.offset_y(), 0.0f);
    EXPECT_LT(scroll.offset_y(), 428.0f);
    settle(scroll);
    // In view, with the margin to spare below it.
    EXPECT_NEAR(scroll.offset_y(), 600.0f + 100.0f + scroll.style.margin - 300.0f, 0.01f);
    const Rect on_screen = scroll.to_screen(card);
    EXPECT_GE(on_screen.y, 100.0f);
    EXPECT_LE(on_screen.y + on_screen.h, 400.0f);
    expect_rect(scroll.to_content(on_screen), card.x, card.y, card.w, card.h);
    EXPECT_NEAR(scroll.visibility(card), 1.0f, 0.01f);
    EXPECT_NEAR(scroll.visibility({0.0f, 0.0f, 400.0f, 100.0f}), 0.0f, 0.01f);
    // Half out through the top: half faded (edge_fade is a share of its size).
    scroll.style.edge_fade = 1.0f;
    EXPECT_NEAR(scroll.visibility({0.0f, scroll.offset_y() - 50.0f, 400.0f, 100.0f}), 0.5f, 0.01f);
    // Already in view: reveal() moves nothing.
    const float before = scroll.offset_y();
    scroll.reveal(card);
    settle(scroll);
    EXPECT_NEAR(scroll.offset_y(), before, 0.01f);

    // The right stick scrolls; pushed against the end it answers once.
    hui::InputFrame stick = idle();
    stick.stick2_y = 1.0f;
    feedback_.clear();
    EXPECT_EQ(scroll.handle(stick, feedback_), Event::changed);
    EXPECT_TRUE(feedback_.cues.empty());
    Event last = Event::none;
    int refusals = 0;
    for (int i = 0; i < 120; ++i)
    {
        last = scroll.handle(stick, feedback_);
        refusals += last == Event::refused ? 1 : 0;
        scroll.update(kFrame);
    }
    EXPECT_EQ(refusals, 1);
    EXPECT_EQ(last, Event::none);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_NEAR(scroll.progress_y(), 1.0f, 0.01f);
    EXPECT_NEAR(scroll.visible().y, 700.0f, 0.5f);
    stick.stick2_y = 0.0f;
    stick.stick2_x = 1.0f;
    EXPECT_EQ(scroll.handle(stick, feedback_), Event::none);

    // Both axes, snapped; content that fits has nowhere to go.
    scroll.style.axes = hui::ui::ScrollAxes::both;
    scroll.scroll_to(250.0f, 0.0f, true);
    EXPECT_NEAR(scroll.offset_x(), 250.0f, 0.01f);
    EXPECT_NEAR(scroll.progress_x(), 0.5f, 0.01f);
    scroll.set_content_size(300.0f, 200.0f);
    scroll.update(kFrame);
    settle(scroll);
    EXPECT_NEAR(scroll.offset_x(), 0.0f, 0.01f);
    EXPECT_EQ(scroll.handle(stick, feedback_), Event::none);
}

TEST_F(ComponentsLayout, PanelsSayWhereTheirContentGoes)
{
    hui::ui::Panel panel;
    panel.style.padding_x = 20.0f;
    panel.style.padding_y = 10.0f;
    const Rect bounds{100.0f, 100.0f, 400.0f, 300.0f};
    panel.set_bounds(bounds);
    expect_rect(panel.content_rect(), 120.0f, 110.0f, 360.0f, 280.0f);

    // A title bar and a footer take their room; the slots get theirs.
    panel.style.kind = hui::ui::PanelKind::titled;
    panel.style.header_height = 60.0f;
    panel.style.footer_height = 50.0f;
    panel.title = "A title that is much too long for a panel this narrow to show";
    panel.subtitle = "A subtitle";
    Rect slot{};
    Rect foot{};
    panel.header_right = [&slot](hui::ui::Canvas &, const Rect &area) { slot = area; };
    panel.footer = [&foot](hui::ui::Canvas &, const Rect &area) { foot = area; };
    expect_rect(panel.content_rect(), 120.0f, 170.0f, 360.0f, 170.0f);
    expect_rect(panel.header_rect(), 100.0f, 100.0f, 400.0f, 60.0f);
    hui::ui::Canvas target = canvas();
    panel.draw(target);
    expect_drawn("titled panel");
    expect_rect(slot, 360.0f, 100.0f, 120.0f, 60.0f);
    expect_rect(foot, 120.0f, 350.0f, 360.0f, 50.0f);
    // The same panel at another rectangle, without set_bounds().
    expect_rect(panel.content_rect({0.0f, 0.0f, 400.0f, 300.0f}), 20.0f, 70.0f, 360.0f, 170.0f);

    // Without a surface it draws less: the title, the rules and the slots.
    const std::size_t with_surface = list_.instances().size();
    panel.style.surface = false;
    list_.clear();
    panel.draw(target);
    EXPECT_LT(list_.instances().size(), with_surface);

    // A dashed divider is many pieces, a solid one few; a label is text.
    hui::ui::Divider divider;
    divider.set_bounds({0.0f, 0.0f, 400.0f, 16.0f});
    list_.clear();
    divider.draw(target);
    const std::size_t solid = list_.instances().size();
    divider.style.line = hui::ui::DividerLine::dashed;
    list_.clear();
    divider.draw(target);
    EXPECT_GT(list_.instances().size(), solid + 5);
    EXPECT_GE(hui::ui::divider_thickness(divider.style), 1.5f);
    divider.style.thickness = 6.0f;
    EXPECT_EQ(hui::ui::divider_thickness(divider.style), 6.0f);

    hui::ui::SectionHeader header;
    header.title = "Recently played";
    header.action = "Sort";
    header.action_button = hui::ui::Button::triangle;
    EXPECT_EQ(header.count(), -1);
    list_.clear();
    header.draw(target);
    const std::size_t plain = list_.instances().size();
    header.set_count(12);
    header.update(kFrame);
    list_.clear();
    header.draw(target);
    EXPECT_GT(list_.instances().size(), plain); // the badge appeared
    EXPECT_EQ(header.count(), 12);
}

TEST_F(ComponentsLayout, TransitionsEnterLeaveAndPush)
{
    Transition t;
    EXPECT_TRUE(t.visible());
    EXPECT_FALSE(t.running());
    EXPECT_NEAR(t.opacity(), 1.0f, 0.001f);

    // A slide up starts below its place, transparent, and arrives.
    t.start(TransitionKind::slide, Direction::up);
    EXPECT_TRUE(t.running());
    EXPECT_NEAR(t.opacity(), 0.0f, 0.001f);
    EXPECT_NEAR(t.offset_y(), t.style.distance, 0.01f);
    t.update(kFrame * 4.0f);
    EXPECT_GT(t.opacity(), 0.0f);
    EXPECT_LT(t.offset_y(), t.style.distance);
    EXPECT_GT(t.progress(), 0.0f);
    settle(t);
    EXPECT_FALSE(t.running());
    EXPECT_NEAR(t.opacity(), 1.0f, 0.001f);
    EXPECT_NEAR(t.offset_y(), 0.0f, 0.001f);

    // A push: the old content leaves the way the new one travels.
    t.start(TransitionKind::push, Direction::left);
    EXPECT_NEAR(t.opacity(TransitionPhase::outgoing), 1.0f, 0.001f);
    EXPECT_NEAR(t.opacity(TransitionPhase::incoming), 0.0f, 0.001f);
    EXPECT_GT(t.offset_x(TransitionPhase::incoming), 0.0f);
    t.update(kFrame * 6.0f);
    EXPECT_LT(t.offset_x(TransitionPhase::outgoing), 0.0f);
    EXPECT_NEAR(t.offset_y(TransitionPhase::outgoing), 0.0f, 0.001f);
    settle(t);
    EXPECT_NEAR(t.opacity(TransitionPhase::outgoing), 0.0f, 0.001f);

    // An exit ends hidden, and is quicker than an entrance.
    t.start(TransitionKind::scale);
    EXPECT_NEAR(t.scale(), t.style.scale, 0.001f);
    int in_frames = 0;
    for (; t.running() && in_frames < 600; ++in_frames)
        t.update(kFrame);
    t.leave(TransitionKind::scale);
    EXPECT_TRUE(t.leaving());
    EXPECT_TRUE(t.visible());
    int out_frames = 0;
    for (; t.running() && out_frames < 600; ++out_frames)
        t.update(kFrame);
    EXPECT_LT(out_frames, in_frames);
    EXPECT_FALSE(t.visible());
    EXPECT_NEAR(t.opacity(), 0.0f, 0.001f);

    // The pace is the theme's: a faster theme, a shorter transition.
    t.style.theme.omega = t.style.theme.omega * 2.0f;
    t.start(TransitionKind::fade);
    int fast_frames = 0;
    for (; t.running() && fast_frames < 600; ++fast_frames)
        t.update(kFrame);
    EXPECT_LT(fast_frames, in_frames);

    // Reduced motion: nothing travels, nothing scales, it only fades.
    t.style.reduced_motion = true;
    t.start(TransitionKind::slide, Direction::left);
    EXPECT_NEAR(t.offset_x(), 0.0f, 0.001f);
    t.start(TransitionKind::scale);
    EXPECT_NEAR(t.scale(), 1.0f, 0.001f);
    EXPECT_NEAR(t.opacity(), 0.0f, 0.001f);
    t.finish();
    EXPECT_NEAR(t.opacity(), 1.0f, 0.001f);
    t.hide();
    EXPECT_FALSE(t.visible());
    t.show();
    EXPECT_TRUE(t.visible());
}

TEST_F(ComponentsLayout, DrawInEveryThemeAndVariant)
{
    using hui::ui::HighlightKind;
    const HighlightKind kinds[] = {HighlightKind::ring,      HighlightKind::fill,
                                   HighlightKind::tint,      HighlightKind::bar,
                                   HighlightKind::underline, HighlightKind::glow};
    const hui::ui::DividerLine lines[] = {hui::ui::DividerLine::solid, hui::ui::DividerLine::dashed,
                                          hui::ui::DividerLine::inset};
    const hui::ui::PanelKind panels[] = {hui::ui::PanelKind::plain, hui::ui::PanelKind::titled,
                                         hui::ui::PanelKind::well};
    int round = 0;
    for (const hui::ui::Theme &theme : hui::ui::themes())
    {
        for (int variant = 0; variant < 3; ++variant, ++round)
        {
            hui::ui::Canvas target = canvas();
            const bool odd = (round & 1) != 0;

            FocusGroup group;
            group.style.theme = theme;
            group.style.highlight.kind = kinds[round % 6];
            build(group);
            group.set_clip(1, {0.0f, 0.0f, 200.0f, 200.0f});
            group.set_radius(2, 18.0f);
            FocusItem picture{6, {360.0f, 0.0f, 100.0f, 100.0f}};
            picture.picture = odd;
            group.add(picture);
            group.set_focus(variant == 2 ? 6 : 1);
            group.draw(target); // before any update
            group.update(kFrame);
            group.handle(nav(Direction::right), feedback_);
            group.update(kFrame);
            group.draw(target);

            SplitView split;
            split.style.theme = theme;
            split.style.axis = odd ? hui::ui::SplitAxis::vertical : hui::ui::SplitAxis::horizontal;
            split.style.line = lines[variant];
            split.style.panels = variant == 2;
            split.style.panel_kind = panels[round % 3];
            split.style.third = variant == 1 ? 0.3f : 0.0f;
            split.set_bounds({96.0f, 240.0f, 1700.0f, 700.0f});
            split.update(kFrame);
            split.set_ratio(odd ? 0.0f : 0.7f);
            split.update(kFrame);
            split.draw(target);
            for (int pane = 0; pane < split.pane_count(); ++pane)
            {
                split.begin_pane(target, pane);
                split.end_pane(target, pane);
            }

            ScrollArea scroll;
            scroll.style.theme = theme;
            scroll.style.axes = hui::ui::ScrollAxes::both;
            scroll.style.on_panel = odd;
            scroll.set_bounds(split.pane_rect(1));
            scroll.set_content_size(4000.0f, 3000.0f);
            scroll.scroll_to(700.0f, 500.0f);
            scroll.update(kFrame);
            scroll.begin(target);
            scroll.end(target);
            scroll.draw(target);

            hui::ui::Panel panel;
            panel.style.theme = theme;
            panel.style.kind = panels[variant];
            panel.style.footer_height = odd ? 40.0f : 0.0f;
            panel.style.accent_bar = odd;
            panel.style.rule = lines[round % 3];
            panel.style.radius = odd ? 6.0f : -1.0f;
            panel.title = "Storage";
            panel.subtitle = odd ? "412 GB free of 825 GB" : "";
            panel.draw(target, {100.0f, 100.0f, 420.0f, 300.0f});
            panel.draw(target, {100.0f, 100.0f, 30.0f, 12.0f}); // far too small

            hui::ui::Divider divider;
            divider.style.theme = theme;
            divider.style.line = lines[variant];
            divider.style.vertical = odd;
            divider.style.on_panel = odd;
            divider.label = variant == 0 ? "OR" : "";
            divider.draw(target, {600.0f, 100.0f, odd ? 16.0f : 500.0f, odd ? 500.0f : 16.0f});
            divider.draw(target, {600.0f, 100.0f, 1.0f, 1.0f});

            hui::ui::SectionHeader header;
            header.style.theme = theme;
            header.style.rule = static_cast<hui::ui::SectionRule>(variant);
            header.style.line = lines[round % 3];
            header.style.on_panel = odd;
            header.style.caps = odd;
            header.title = "A section whose title is far too long for the room it is given";
            header.action = "Sort by name";
            header.action_button = hui::ui::Button::triangle;
            header.set_count(variant * 60);
            header.update(kFrame);
            header.draw(target, {96.0f, 300.0f, odd ? 300.0f : 900.0f, 44.0f});

            Transition transition;
            transition.style.theme = theme;
            transition.style.clip = odd;
            transition.start(static_cast<TransitionKind>(round % 4), Direction::left);
            transition.update(0.05f);
            transition.begin(target, {0.0f, 0.0f, 400.0f, 300.0f}, TransitionPhase::outgoing);
            transition.end(target);
            transition.begin(target, {0.0f, 0.0f, 400.0f, 300.0f});
            panel.draw(target, {0.0f, 0.0f, 400.0f, 300.0f});
            transition.end(target);
            expect_drawn(theme.id);
        }
    }
}

TEST_F(ComponentsLayout, PageRunsItsScreenFromTheGroupAlone)
{
    const std::unique_ptr<hui::concepts::gallery::Page> page =
        hui::concepts::gallery::make_layout_page(context_);
    const auto send_page = [&](const hui::InputFrame &input, float seconds = 0.6f)
    {
        feedback_.clear();
        page->update(input, kFrame, feedback_);
        const std::vector<hui::audio::CueEvent> cues = feedback_.cues;
        for (int i = 0, frames = static_cast<int>(seconds / kFrame); i < frames; ++i)
            page->update(idle(), kFrame, feedback_);
        feedback_.cues = cues;
    };
    const auto draw_page = [&](const char *what)
    {
        hui::ui::Canvas target = canvas();
        page->draw(target);
        page->draw_modal(target);
        expect_drawn(what);
    };
    const std::string first = page->variant();

    // The modal takes the focus and the hints, and back returns both.
    send_page(nav(Direction::up));
    send_page(nav(Direction::right));
    send_page(nav(Direction::right));
    EXPECT_STREQ(page->hints()[2].label, "Scroll");
    send_page(press(Action::confirm));
    EXPECT_TRUE(asked(Cue::modal_open));
    EXPECT_STREQ(page->hints()[2].label, "Close");
    draw_page("modal");
    send_page(nav(Direction::up));
    EXPECT_TRUE(asked(Cue::error)) << "the scope keeps the focus inside the dialog";
    send_page(press(Action::back));
    EXPECT_STREQ(page->hints()[2].label, "Scroll");

    // Every theme, every variant, with the focus walked through the page.
    const Direction walk[] = {Direction::down, Direction::right, Direction::right,
                              Direction::down, Direction::down,  Direction::left,
                              Direction::left, Direction::up,    Direction::up};
    for (const hui::ui::Theme &theme : hui::ui::themes())
    {
        page->restyle(theme, false);
        page->enter();
        for (int variant = 0; variant < 4; ++variant)
        {
            for (const Direction step : walk)
                send_page(nav(step), 0.1f);
            send_page(press(Action::confirm), 0.05f); // whatever has the focus
            draw_page(theme.id);                      // ... half way through what it started
            send_page(press(Action::back), 0.05f);    // had it opened the modal, close it
            send_page(press(Action::west), 0.05f);
            draw_page(theme.id);
        }
        EXPECT_EQ(first, page->variant()) << "four variants, then the first again";
    }
}

TEST_F(ComponentsLayout, PageTourLeavesThePageAsItFoundIt)
{
    const std::unique_ptr<hui::concepts::gallery::Page> page =
        hui::concepts::gallery::make_layout_page(context_);
    page->restyle(hui::ui::default_theme(), true); // reduced motion: every step settles
    const std::string first = page->variant();
    bool opened = false;
    for (const hui::app::TourStep &step : page->tour())
    {
        hui::InputFrame input = idle();
        input.pressed = step.press;
        input.held = step.press;
        input.nav = step.nav;
        feedback_.clear();
        page->update(input, kFrame, feedback_);
        EXPECT_FALSE(asked(Cue::error)) << "no step of the tour is refused";
        for (int i = 0; i < 30; ++i)
            page->update(idle(), kFrame, feedback_);
        opened = opened || std::string(page->hints()[2].label) == "Close";
        hui::ui::Canvas target = canvas();
        page->draw(target);
        page->draw_modal(target);
        expect_drawn("tour");
    }
    EXPECT_TRUE(opened) << "the tour shows the modal";
    EXPECT_EQ(first, page->variant());
    EXPECT_STREQ(page->hints()[2].label, "Scroll") << "the tour leaves nothing open";
}

} // namespace
