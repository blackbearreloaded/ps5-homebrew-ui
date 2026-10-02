// ps5-homebrew-ui - Tests: ListView behaviour.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "component_fixture.hpp"
#include "ui/components/list.hpp"

#include <gtest/gtest.h>

namespace
{

using hui::Action;
using hui::Direction;
using hui::ui::Event;
using hui::ui::ListItem;
using hui::ui::ListView;

class ComponentsList : public hui::testing::ComponentFixture
{
  protected:
    ComponentsList()
    {
        std::vector<ListItem> items(6);
        items[0].title = "Section";
        items[0].header = true;
        items[1].title = "First";
        items[2].title = "Second";
        items[3].title = "Locked";
        items[3].disabled = true;
        items[4].title = "Fourth";
        items[5].title = "Last";
        list_view_.set_items(items);
        list_view_.set_bounds({100.0f, 100.0f, 500.0f, 200.0f});
    }

    Event send(const hui::InputFrame &input)
    {
        feedback_.clear();
        const Event event = list_view_.handle(input, feedback_);
        for (int i = 0; i < 30; ++i)
            list_view_.update(kFrame);
        return event;
    }

    ListView list_view_;
};

TEST_F(ComponentsList, FocusSkipsHeaders)
{
    EXPECT_EQ(list_view_.focus(), 1);
    EXPECT_EQ(send(nav(Direction::up)), Event::refused);
    EXPECT_EQ(list_view_.focus(), 1);
}

TEST_F(ComponentsList, MovesAndPlaysTheMoveCue)
{
    EXPECT_EQ(send(nav(Direction::down)), Event::moved);
    EXPECT_EQ(list_view_.focus(), 2);
    EXPECT_TRUE(asked(hui::audio::Cue::focus));
}

TEST_F(ComponentsList, EndRefusesOnceAndStaysSilentOnRepeat)
{
    list_view_.set_focus(5);
    EXPECT_EQ(send(nav(Direction::down)), Event::refused);
    EXPECT_TRUE(asked(hui::audio::Cue::error));
    hui::InputFrame held = nav(Direction::down);
    held.nav_repeat = true;
    EXPECT_EQ(send(held), Event::refused);
    EXPECT_TRUE(feedback_.cues.empty());
}

TEST_F(ComponentsList, WrapGoesRound)
{
    list_view_.style.wrap = true;
    list_view_.set_focus(5);
    EXPECT_EQ(send(nav(Direction::down)), Event::moved);
    EXPECT_EQ(list_view_.focus(), 1);
}

TEST_F(ComponentsList, DisabledRowRefusesConfirm)
{
    list_view_.set_focus(3);
    EXPECT_EQ(send(press(Action::confirm)), Event::refused);
    list_view_.set_focus(4);
    EXPECT_EQ(send(press(Action::confirm)), Event::activated);
    EXPECT_TRUE(asked(hui::audio::Cue::select));
}

TEST_F(ComponentsList, SilencedCueIsNotPlayed)
{
    list_view_.style.sounds.move = hui::audio::Cue::count;
    EXPECT_EQ(send(nav(Direction::down)), Event::moved);
    EXPECT_TRUE(feedback_.cues.empty());
}

TEST_F(ComponentsList, ScrollKeepsTheFocusedRowInside)
{
    list_view_.set_focus(1);
    for (int i = 0; i < 4; ++i)
        send(nav(Direction::down));
    const hui::gfx::Rect row = list_view_.row_rect(list_view_.focus());
    EXPECT_GE(row.y, 100.0f - 0.5f);
    EXPECT_LE(row.y + row.h, 300.0f + 0.5f);
}

TEST_F(ComponentsList, DrawsInEveryThemeAndHighlight)
{
    using hui::ui::HighlightKind;
    for (const hui::ui::Theme &theme : hui::ui::themes())
    {
        for (HighlightKind kind :
             {HighlightKind::ring, HighlightKind::fill, HighlightKind::tint, HighlightKind::bar,
              HighlightKind::underline, HighlightKind::glow, HighlightKind::none})
        {
            list_view_.style.theme = theme;
            list_view_.style.highlight.kind = kind;
            list_view_.style.cards = kind == HighlightKind::fill;
            list_view_.style.panel = kind == HighlightKind::bar;
            hui::ui::Canvas target = canvas();
            list_view_.draw(target);
            expect_drawn(theme.id);
        }
    }
}

} // namespace
