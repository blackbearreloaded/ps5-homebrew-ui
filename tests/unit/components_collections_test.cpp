// ps5-homebrew-ui - Tests: Card, GridView and Carousel behaviour.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "component_fixture.hpp"
#include "ui/components/carousel.hpp"
#include "ui/components/grid.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

namespace
{

using hui::Action;
using hui::Direction;
using hui::audio::Cue;
using hui::gfx::Rect;
using hui::ui::CardItem;
using hui::ui::CardText;
using hui::ui::Carousel;
using hui::ui::CarouselMode;
using hui::ui::Event;
using hui::ui::GridView;
using hui::ui::GridWrap;

std::vector<CardItem> make_items(int count)
{
    std::vector<CardItem> items(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        CardItem &item = items[static_cast<std::size_t>(i)];
        item.title = "Title " + std::to_string(i);
        item.subtitle = "Subtitle";
        item.top = hui::gfx::Color::rgb(0x4b2a9c);
        item.bottom = hui::gfx::Color::rgb(0x120a2e);
        item.tag = i;
    }
    return items;
}

class ComponentsCollections : public hui::testing::ComponentFixture
{
  protected:
    ComponentsCollections()
    {
        // Three columns over seven items: rows of 3, 3 and 1.
        grid_.style.columns = 3;
        grid_.set_items(make_items(7));
        grid_.set_bounds({100.0f, 100.0f, 600.0f, 320.0f});
        shelf_.set_items(make_items(10));
        shelf_.set_bounds({100.0f, 100.0f, 1000.0f, 320.0f});
    }

    Event send(GridView &grid, const hui::InputFrame &input)
    {
        feedback_.clear();
        const Event event = grid.handle(input, feedback_);
        for (int i = 0; i < 60; ++i)
            grid.update(kFrame);
        return event;
    }
    Event send(Carousel &shelf, const hui::InputFrame &input)
    {
        feedback_.clear();
        const Event event = shelf.handle(input, feedback_);
        for (int i = 0; i < 90; ++i)
            shelf.update(kFrame);
        return event;
    }
    static hui::InputFrame held(Direction direction)
    {
        hui::InputFrame input = nav(direction);
        input.nav_repeat = true;
        return input;
    }

    GridView grid_;
    Carousel shelf_;
};

TEST_F(ComponentsCollections, CardMeasuresItself)
{
    hui::ui::CardLook look;
    look.art_aspect = 2.0f;
    const float text = hui::ui::card_text_height(look);
    EXPECT_GT(text, look.title_size);
    EXPECT_FLOAT_EQ(hui::ui::card_height(look, 300.0f), 150.0f + text);
    const Rect card{10.0f, 20.0f, 300.0f, 150.0f + text};
    const Rect art = hui::ui::card_art(look, card);
    EXPECT_FLOAT_EQ(art.w, 300.0f);
    EXPECT_FLOAT_EQ(art.h, 150.0f);
    // Text over the art takes no room of its own.
    look.text = CardText::over;
    EXPECT_FLOAT_EQ(hui::ui::card_text_height(look), 0.0f);
    // A plate pads the art on every side and becomes what the focus surrounds.
    look.plate = true;
    look.plate_padding = 10.0f;
    EXPECT_FLOAT_EQ(hui::ui::card_height(look, 300.0f), 140.0f + 20.0f);
    EXPECT_FLOAT_EQ(hui::ui::card_frame(look, card).h, card.h);
    // No travel under reduced motion: the card neither grows nor rises.
    hui::ui::ComponentStyle style;
    hui::ui::CardState focused;
    focused.focus = 1.0f;
    EXPECT_GT(hui::ui::card_scale(style, look, focused), 1.0f);
    style.reduced_motion = true;
    EXPECT_FLOAT_EQ(hui::ui::card_scale(style, look, focused), 1.0f);
    EXPECT_FLOAT_EQ(hui::ui::card_lift(style, look, focused), 0.0f);
}

TEST_F(ComponentsCollections, GridMovesAndPlaysTheMoveCue)
{
    EXPECT_EQ(send(grid_, nav(Direction::right)), Event::moved);
    EXPECT_EQ(grid_.focus(), 1);
    EXPECT_TRUE(asked(Cue::focus));
    EXPECT_EQ(send(grid_, nav(Direction::down)), Event::moved);
    EXPECT_EQ(grid_.focus(), 4);
    EXPECT_EQ(grid_.rows(), 3);
}

TEST_F(ComponentsCollections, GridRemembersTheColumnAcrossAShortRow)
{
    grid_.set_focus(5); // second row, third column
    EXPECT_EQ(send(grid_, nav(Direction::down)), Event::moved);
    EXPECT_EQ(grid_.focus(), 6); // the last row has one cell: it takes the focus
    EXPECT_EQ(send(grid_, nav(Direction::up)), Event::moved);
    EXPECT_EQ(grid_.focus(), 5); // ... and gives the third column back
}

TEST_F(ComponentsCollections, GridEdgesRefuseOnceAndStaySilentOnRepeat)
{
    for (Direction edge : {Direction::left, Direction::up})
    {
        EXPECT_EQ(send(grid_, nav(edge)), Event::refused);
        EXPECT_TRUE(asked(Cue::error));
        EXPECT_EQ(send(grid_, held(edge)), Event::refused);
        EXPECT_TRUE(feedback_.cues.empty());
        EXPECT_EQ(grid_.focus(), 0);
    }
    grid_.set_focus(6);
    EXPECT_EQ(send(grid_, nav(Direction::down)), Event::refused);
    EXPECT_EQ(send(grid_, nav(Direction::right)), Event::refused);
    EXPECT_EQ(grid_.exit(), Direction::none);
}

TEST_F(ComponentsCollections, GridReportsAnExitInsteadOfRefusing)
{
    grid_.style.exits.up = true;
    grid_.style.wrap = GridWrap::rows; // an exit beats the wrap on its edge
    EXPECT_EQ(send(grid_, nav(Direction::up)), Event::none);
    EXPECT_EQ(grid_.exit(), Direction::up);
    EXPECT_TRUE(feedback_.cues.empty());
    EXPECT_EQ(grid_.focus(), 0);
    // The report lasts until the next handle().
    EXPECT_EQ(send(grid_, idle()), Event::none);
    EXPECT_EQ(grid_.exit(), Direction::none);
}

TEST_F(ComponentsCollections, GridWrapsByRowOrInReadingOrder)
{
    grid_.style.wrap = GridWrap::rows;
    grid_.set_focus(2);
    EXPECT_EQ(send(grid_, nav(Direction::right)), Event::moved);
    EXPECT_EQ(grid_.focus(), 0);
    EXPECT_EQ(send(grid_, nav(Direction::up)), Event::moved);
    EXPECT_EQ(grid_.focus(), 6);
    // Going round is a decision: a held direction stops at the edge.
    EXPECT_EQ(send(grid_, held(Direction::down)), Event::refused);

    grid_.style.wrap = GridWrap::flow;
    grid_.set_focus(2);
    EXPECT_EQ(send(grid_, nav(Direction::right)), Event::moved);
    EXPECT_EQ(grid_.focus(), 3);
    grid_.set_focus(6);
    EXPECT_EQ(send(grid_, nav(Direction::right)), Event::moved);
    EXPECT_EQ(grid_.focus(), 0);
}

TEST_F(ComponentsCollections, GridScrollsByRowsAndKeepsTheFocusInside)
{
    const Rect bounds = grid_.bounds();
    for (Direction step : {Direction::down, Direction::down, Direction::up})
    {
        send(grid_, nav(step));
        const Rect cell = grid_.cell_rect(grid_.focus());
        EXPECT_GE(cell.y, bounds.y - 0.5f);
        EXPECT_LE(cell.y + cell.h, bounds.y + bounds.h + 0.5f);
    }
    // Restyling keeps the state: the focus stays where it was.
    const int focus = grid_.focus();
    grid_.style.theme = hui::ui::themes()[5];
    grid_.style.columns = 4;
    send(grid_, idle());
    EXPECT_EQ(grid_.focus(), focus);
}

TEST_F(ComponentsCollections, GridConfirmActivatesSelectsOrRefuses)
{
    EXPECT_EQ(send(grid_, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(asked(Cue::select));
    grid_.item(0).disabled = true;
    EXPECT_EQ(send(grid_, press(Action::confirm)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));

    grid_.style.select_on_confirm = true;
    grid_.set_focus(1);
    EXPECT_EQ(send(grid_, press(Action::confirm)), Event::changed);
    EXPECT_TRUE(asked(Cue::toggle));
    EXPECT_TRUE(grid_.items()[1].selected);
    EXPECT_EQ(send(grid_, press(Action::confirm)), Event::changed);
    EXPECT_FALSE(grid_.items()[1].selected);
    EXPECT_EQ(send(grid_, press(Action::back)), Event::cancelled);
    EXPECT_TRUE(asked(Cue::back));
}

TEST_F(ComponentsCollections, GridOverACountKeepsNothingPerCell)
{
    GridView grid;
    grid.style.columns = 4;
    int drawn = 0;
    int highest = -1;
    grid.content = [&](hui::ui::Canvas &, const Rect &, const CardItem &item, int index, float)
    {
        ++drawn;
        highest = std::max(highest, index);
        EXPECT_TRUE(item.title.empty());
    };
    grid.set_count(50000);
    grid.set_bounds({100.0f, 100.0f, 900.0f, 400.0f});
    EXPECT_EQ(grid.count(), 50000);
    EXPECT_EQ(grid.rows(), 12500);
    EXPECT_TRUE(grid.items().empty());

    EXPECT_EQ(send(grid, nav(Direction::right)), Event::moved);
    EXPECT_EQ(grid.focus(), 1);
    EXPECT_EQ(send(grid, press(Action::confirm)), Event::activated);
    // Selecting belongs to items: over a count, confirm only activates.
    grid.style.select_on_confirm = true;
    EXPECT_EQ(send(grid, press(Action::confirm)), Event::activated);

    hui::ui::Canvas view = canvas();
    grid.draw(view);
    EXPECT_GT(drawn, 0);
    EXPECT_LT(drawn, 40) << "only the rows in view are drawn";

    // The count changes under the grid (a search narrows it): the focus stays
    // inside, and the far end is as reachable as the start.
    grid.set_focus(49999);
    send(grid, idle());
    drawn = 0;
    highest = -1;
    grid.draw(view);
    EXPECT_EQ(highest, 49999);
    grid.set_count(10);
    EXPECT_EQ(grid.focus(), 9);

    // The light around the focused cell comes from the accent callback.
    int asked_for = -1;
    grid.accent = [&](int index)
    {
        asked_for = index;
        return hui::gfx::Color::rgb(0xff8800);
    };
    grid.set_focus(3);
    EXPECT_EQ(asked_for, 3);

    // set_items() takes the grid back.
    grid.set_items(make_items(5));
    EXPECT_EQ(grid.count(), 5);
    EXPECT_EQ(grid.items().size(), 5U);
}

TEST_F(ComponentsCollections, GridJumpsWithTheFocusedRowAtTheTop)
{
    GridView grid;
    grid.style.columns = 4;
    grid.content = [](hui::ui::Canvas &, const Rect &, const CardItem &, int, float) {};
    grid.set_count(4000);
    grid.set_bounds({100.0f, 100.0f, 900.0f, 900.0f}); // several rows in view
    const float top = grid.cell_rect(0).y;

    // set_focus() scrolls the least that shows the row: it ends at the bottom.
    grid.set_focus(2001);
    EXPECT_GT(grid.cell_rect(2001).y, top + 1.0f);
    // set_focus_at_top() puts that row first, with no glide.
    grid.set_focus_at_top(2001);
    EXPECT_EQ(grid.focus(), 2001);
    EXPECT_NEAR(grid.cell_rect(2001).y, top, 0.5f);
    // Near the end the list allows less: the last row stays inside the view.
    grid.set_focus_at_top(3999);
    const Rect last = grid.cell_rect(3999);
    EXPECT_GE(last.y, top - 0.5f);
    EXPECT_LE(last.y + last.h, grid.bounds().y + grid.bounds().h + 0.5f);
}

TEST_F(ComponentsCollections, GridEntranceStartsAtTheRowsInView)
{
    GridView grid;
    grid.style.columns = 4;
    grid.content = [](hui::ui::Canvas &view, const Rect &cell, const CardItem &, int, float)
    { view.list.ring(cell.cx(), cell.cy(), 20.0f, 4.0f, hui::gfx::Color::rgb(0xffffff)); };
    grid.set_count(40000);
    grid.set_bounds({100.0f, 100.0f, 900.0f, 400.0f});
    // The light the cells in view carry a moment after the entrance began.
    const auto arrived = [&](int index)
    {
        grid.set_focus_at_top(index);
        grid.enter();
        for (int i = 0; i < 12; ++i)
            grid.update(kFrame);
        hui::ui::Canvas view = canvas();
        grid.draw(view);
        float total = 0.0f;
        for (const hui::gfx::Instance &instance : list_.instances())
            total += instance.color_top[3];
        return total;
    };
    // A jump of thousands of rows arrives as quickly as the top of the list.
    const float at_top = arrived(0);
    EXPECT_GT(at_top, 0.0f);
    EXPECT_NEAR(arrived(30000), at_top, at_top * 0.01f);
}

TEST_F(ComponentsCollections, CarouselMovesAndRefusesAtItsEnds)
{
    EXPECT_EQ(send(shelf_, nav(Direction::left)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_EQ(send(shelf_, held(Direction::left)), Event::refused);
    EXPECT_TRUE(feedback_.cues.empty());
    EXPECT_EQ(send(shelf_, nav(Direction::right)), Event::moved);
    EXPECT_TRUE(asked(Cue::focus));
    EXPECT_EQ(shelf_.focus(), 1);
    // Up and down are the screen's: untouched and silent.
    EXPECT_EQ(send(shelf_, nav(Direction::down)), Event::none);
    EXPECT_TRUE(feedback_.cues.empty());
    EXPECT_EQ(send(shelf_, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(asked(Cue::select));
    // An end that is an exit reports instead of refusing.
    shelf_.style.exits.right = true;
    shelf_.set_focus(9);
    EXPECT_EQ(send(shelf_, nav(Direction::right)), Event::none);
    EXPECT_EQ(shelf_.exit(), Direction::right);
}

TEST_F(ComponentsCollections, CarouselModesPlaceTheFocus)
{
    const Rect bounds = shelf_.bounds();
    for (int i = 0; i < 5; ++i)
        send(shelf_, nav(Direction::right));
    // leading: the focused item rests `peek` from the left edge.
    Rect item = shelf_.item_rect(shelf_.focus());
    EXPECT_GE(item.x, bounds.x + shelf_.style.peek - 0.5f);
    EXPECT_LE(item.x, bounds.x + shelf_.style.peek + 20.0f);

    // centered: it sits in the middle, wherever it is in the row.
    shelf_.style.mode = CarouselMode::centered;
    for (int focus : {5, 0, 9})
    {
        shelf_.set_focus(focus);
        send(shelf_, idle());
        item = shelf_.item_rect(focus);
        EXPECT_NEAR(item.cx(), bounds.cx(), 1.0f) << focus;
    }
    EXPECT_GT(shelf_.preferred_height(), item.h);
}

TEST_F(ComponentsCollections, CarouselPagedTurnsAPageAtATime)
{
    shelf_.style.mode = CarouselMode::paged;
    shelf_.style.per_page = 3;
    shelf_.set_focus(0);
    EXPECT_EQ(shelf_.pages(), 4);
    const float first = shelf_.item_rect(0).x;
    send(shelf_, nav(Direction::right));
    EXPECT_TRUE(asked(Cue::focus));
    send(shelf_, nav(Direction::right));
    // Moving inside a page does not scroll the row.
    EXPECT_FLOAT_EQ(shelf_.item_rect(0).x, first);
    EXPECT_EQ(shelf_.page(), 0);
    EXPECT_EQ(send(shelf_, nav(Direction::right)), Event::moved);
    EXPECT_TRUE(asked(Cue::tab)); // a page turn has its own cue
    EXPECT_EQ(shelf_.page(), 1);
    EXPECT_LT(shelf_.item_rect(0).x, first - 100.0f);
    const Rect item = shelf_.item_rect(shelf_.focus());
    EXPECT_GE(item.x, shelf_.bounds().x - 0.5f);
    EXPECT_LE(item.x + item.w, shelf_.bounds().x + shelf_.bounds().w + 0.5f);
}

TEST_F(ComponentsCollections, CarouselWrapsAsARowOrAsARing)
{
    // A row runs back to its other end, but not on a held direction.
    shelf_.style.wrap = true;
    EXPECT_EQ(send(shelf_, held(Direction::left)), Event::refused);
    EXPECT_EQ(send(shelf_, nav(Direction::left)), Event::moved);
    EXPECT_EQ(shelf_.focus(), 9);
    EXPECT_EQ(send(shelf_, nav(Direction::right)), Event::moved);
    EXPECT_EQ(shelf_.focus(), 0);

    // A centered shelf with enough items is a ring: it keeps turning, and the
    // focused item is in the middle on either side of the seam.
    shelf_.style.mode = CarouselMode::centered;
    shelf_.set_focus(0);
    EXPECT_EQ(send(shelf_, held(Direction::left)), Event::moved);
    EXPECT_EQ(shelf_.focus(), 9);
    EXPECT_NEAR(shelf_.item_rect(9).cx(), shelf_.bounds().cx(), 1.0f);
    EXPECT_EQ(send(shelf_, nav(Direction::right)), Event::moved);
    EXPECT_EQ(shelf_.focus(), 0);
    EXPECT_NEAR(shelf_.item_rect(0).cx(), shelf_.bounds().cx(), 1.0f);
}

TEST_F(ComponentsCollections, DrawsInEveryThemeAndVariant)
{
    grid_.item(1).badge = "NEW";
    grid_.item(1).selected = true;
    grid_.item(2).progress = 0.4f;
    grid_.item(3).disabled = true;
    shelf_.title = "A shelf";
    hui::ui::Card card;
    int slot_calls = 0;
    for (const hui::ui::Theme &theme : hui::ui::themes())
    {
        for (int variant = 0; variant < 4; ++variant)
        {
            const CardText text =
                variant == 0 ? CardText::below : (variant == 1 ? CardText::over : CardText::none);
            grid_.style.theme = theme;
            grid_.style.card.text = text;
            grid_.style.card.plate = variant == 2;
            grid_.style.card.glow = variant == 1;
            grid_.style.card.dim = variant == 1 ? 0.4f : 0.0f;
            grid_.style.columns = 3 + variant;
            grid_.style.reduced_motion = variant == 3;
            grid_.content = nullptr;
            grid_.art = nullptr;
            if (variant == 3)
                grid_.content =
                    [&](hui::ui::Canvas &to, const Rect &cell, const CardItem &, int, float)
                {
                    ++slot_calls;
                    to.list.rounded_rect(cell, 0.0f, theme.surface);
                };
            if (variant == 2)
                grid_.art =
                    [&](hui::ui::Canvas &to, const Rect &art, float radius, const CardItem &, float)
                {
                    ++slot_calls;
                    to.list.rounded_rect(art, radius, theme.accent);
                };
            send(grid_, variant % 2 == 0 ? nav(Direction::right) : nav(Direction::down));
            hui::ui::Canvas grid_target = canvas();
            grid_.draw(grid_target);
            expect_drawn(theme.id);

            shelf_.style.theme = theme;
            shelf_.style.mode = variant == 0
                                    ? CarouselMode::leading
                                    : (variant == 1 ? CarouselMode::centered : CarouselMode::paged);
            shelf_.style.per_page = variant == 3 ? 1 : 0;
            shelf_.style.wrap = variant == 1;
            shelf_.style.card.text = text;
            shelf_.style.neighbour_fade = variant == 1 ? 0.25f : 0.0f;
            shelf_.set_active(variant != 2);
            send(shelf_, nav(Direction::right));
            hui::ui::Canvas shelf_target = canvas();
            shelf_.draw(shelf_target);
            expect_drawn(theme.id);

            card.style.theme = theme;
            card.style.text = text;
            card.style.plate = variant == 2;
            hui::ui::Canvas card_target = canvas();
            card.draw(card_target, {40.0f, 40.0f, 300.0f, card.height_for(300.0f)},
                      grid_.items()[1], 0.25f * static_cast<float>(variant + 1),
                      variant == 1 ? 1.0f : 0.0f);
            expect_drawn(theme.id);
        }
    }
    EXPECT_GT(slot_calls, 0);

    // Nothing to show is not an error.
    GridView empty;
    EXPECT_EQ(empty.handle(nav(Direction::right), feedback_), Event::none);
    empty.update(kFrame);
    hui::ui::Canvas target = canvas();
    empty.draw(target);
    Carousel bare;
    EXPECT_EQ(bare.handle(nav(Direction::right), feedback_), Event::none);
    bare.update(kFrame);
    bare.draw(target);
    EXPECT_TRUE(list_.empty());
}

} // namespace
