// ps5-homebrew-ui - Tests: TabBar, SideNav, Breadcrumb, PageDots and Menu behaviour.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "component_fixture.hpp"
#include "concepts/components/page.hpp"
#include "ui/components/breadcrumb.hpp"
#include "ui/components/menu.hpp"
#include "ui/components/page_dots.hpp"
#include "ui/components/sidenav.hpp"
#include "ui/components/tabs.hpp"

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
using hui::ui::Breadcrumb;
using hui::ui::Event;
using hui::ui::Menu;
using hui::ui::MenuItem;
using hui::ui::MenuSide;
using hui::ui::NavEntry;
using hui::ui::PageDots;
using hui::ui::SideNav;
using hui::ui::TabBar;

class ComponentsNavigation : public hui::testing::ComponentFixture
{
  protected:
    ComponentsNavigation()
    {
        tabs_.set_tabs(
            {{"Overview"}, {"Saves", 3}, {"Locked", 0, true}, {"Trophies"}, {"Add-ons"}});
        tabs_.set_bounds({400.0f, 300.0f, 900.0f, 56.0f});

        std::vector<NavEntry> entries(5);
        entries[0].label = "Home";
        entries[1].label = "Library";
        entries[2].label = "Friends";
        entries[2].section = "Social";
        entries[3].label = "Offline";
        entries[3].disabled = true;
        entries[4].label = "Collapse";
        entries[4].action = true;
        rail_.style.footer = true;
        rail_.set_entries(entries);
        rail_.set_bounds({96.0f, 240.0f, 0.0f, 700.0f});

        dots_.set_count(4);
        dots_.set_bounds({400.0f, 880.0f, 900.0f, 30.0f});

        std::vector<MenuItem> items(6);
        items[0].label = "Open";
        items[1].label = "Pin to home";
        items[1].checkable = true;
        items[2].separator = true;
        items[3].label = "Move";
        items[3].disabled = true;
        items[4].label = "Rename";
        items[5].label = "Remove";
        items[5].danger = true;
        menu_.set_items(items);
    }

    // Sends one input to a component, then lets it animate for half a second.
    template <typename Component> Event send(Component &component, const hui::InputFrame &input)
    {
        feedback_.clear();
        const Event event = component.handle(input, feedback_);
        settle(component);
        return event;
    }
    template <typename Component> void settle(Component &component, int frames = 30)
    {
        for (int i = 0; i < frames; ++i)
            component.update(kFrame);
    }
    static hui::InputFrame held(Direction direction)
    {
        hui::InputFrame input = nav(direction);
        input.nav_repeat = true;
        return input;
    }

    TabBar tabs_;
    SideNav rail_;
    Breadcrumb crumbs_;
    PageDots dots_;
    Menu menu_;
};

TEST_F(ComponentsNavigation, TabBarChangesTabAndGlidesToIt)
{
    feedback_.clear();
    EXPECT_EQ(tabs_.handle(nav(Direction::right), feedback_), Event::changed);
    EXPECT_EQ(tabs_.active(), 1);
    EXPECT_TRUE(asked(Cue::tab));
    // The value a screen slides its content with is on its way, not there.
    tabs_.update(kFrame);
    EXPECT_GT(tabs_.active_value(), 0.0f);
    EXPECT_LT(tabs_.active_value(), 1.0f);
    settle(tabs_, 90);
    EXPECT_FLOAT_EQ(tabs_.active_value(), 1.0f);
    // Up, down and the buttons are the screen's: the bar leaves them alone.
    EXPECT_EQ(send(tabs_, nav(Direction::down)), Event::none);
    EXPECT_EQ(send(tabs_, press(Action::confirm)), Event::none);
}

TEST_F(ComponentsNavigation, TabBarSkipsDisabledRefusesAtEndsAndWraps)
{
    EXPECT_EQ(send(tabs_, nav(Direction::left)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_EQ(send(tabs_, held(Direction::left)), Event::refused);
    EXPECT_TRUE(feedback_.cues.empty());

    send(tabs_, nav(Direction::right));
    EXPECT_EQ(send(tabs_, nav(Direction::right)), Event::changed);
    EXPECT_EQ(tabs_.active(), 3) << "the disabled tab is stepped over";

    tabs_.set_active(4);
    EXPECT_EQ(send(tabs_, nav(Direction::right)), Event::refused);
    tabs_.style.wrap = true;
    EXPECT_EQ(send(tabs_, nav(Direction::right)), Event::changed);
    EXPECT_EQ(tabs_.active(), 0);
}

TEST_F(ComponentsNavigation, TabBarIsDrivenByStepAndSetActiveAndScrollsToTheActiveTab)
{
    // What a screen does on L2 and R2.
    feedback_.clear();
    EXPECT_EQ(tabs_.step(1, idle(), feedback_), Event::changed);
    EXPECT_TRUE(asked(Cue::tab));
    feedback_.clear();
    tabs_.set_active(4);
    EXPECT_TRUE(feedback_.cues.empty()) << "set_active is silent";
    EXPECT_EQ(tabs_.active(), 4);
    tabs_.set_active(0, true);
    EXPECT_FLOAT_EQ(tabs_.active_value(), 0.0f);

    // Too narrow for five tabs: the row scrolls so the active one is inside.
    tabs_.set_bounds({400.0f, 300.0f, 300.0f, 56.0f});
    for (int index : {0, 3, 4})
    {
        tabs_.set_active(index);
        settle(tabs_, 120);
        const Rect tab = tabs_.tab_rect(fonts_, index);
        EXPECT_GE(tab.x, 400.0f - 0.5f) << index;
        EXPECT_LE(tab.x + tab.w, 700.0f + 0.5f) << index;
    }
    // Restyling keeps the state.
    tabs_.style.theme = hui::ui::themes()[5];
    tabs_.style.kind = hui::ui::TabKind::boxed;
    EXPECT_EQ(tabs_.active(), 4);
}

TEST_F(ComponentsNavigation, SideNavMovesActivatesAndKeepsTheCurrentEntry)
{
    EXPECT_EQ(send(rail_, nav(Direction::up)), Event::refused);
    EXPECT_EQ(send(rail_, nav(Direction::down)), Event::moved);
    EXPECT_TRUE(asked(Cue::focus));
    EXPECT_EQ(rail_.focus(), 1);
    EXPECT_EQ(rail_.current(), 0) << "moving the focus goes nowhere yet";
    EXPECT_EQ(send(rail_, press(Action::confirm)), Event::activated);
    EXPECT_TRUE(asked(Cue::select));
    EXPECT_EQ(rail_.current(), 1);

    rail_.set_focus(3);
    EXPECT_EQ(send(rail_, press(Action::confirm)), Event::refused) << "disabled";
    rail_.set_focus(4);
    EXPECT_EQ(send(rail_, press(Action::confirm)), Event::activated);
    EXPECT_EQ(rail_.current(), 1) << "an action is not a place";
    EXPECT_EQ(send(rail_, nav(Direction::down)), Event::refused);
    EXPECT_EQ(send(rail_, press(Action::back)), Event::cancelled);
    EXPECT_TRUE(asked(Cue::back));

    // A rail that lost the screen's focus waits on the current entry.
    rail_.set_focused(false);
    EXPECT_EQ(rail_.focus(), 1);
}

TEST_F(ComponentsNavigation, SideNavAnimatesBetweenItsWidthsAndPinsTheFooter)
{
    settle(rail_);
    EXPECT_FLOAT_EQ(rail_.rect().w, rail_.style.expanded_width);
    EXPECT_FLOAT_EQ(rail_.expansion(), 1.0f);

    rail_.set_expanded(false);
    rail_.update(kFrame);
    EXPECT_LT(rail_.rect().w, rail_.style.expanded_width);
    EXPECT_GT(rail_.rect().w, rail_.style.collapsed_width);
    settle(rail_, 240);
    EXPECT_NEAR(rail_.rect().w, rail_.style.collapsed_width, 0.01f);
    EXPECT_NEAR(rail_.row_rect(0).w, rail_.style.collapsed_width - 2.0f * rail_.style.padding,
                0.01f);

    feedback_.clear();
    EXPECT_EQ(rail_.toggle(feedback_), Event::changed);
    EXPECT_TRUE(asked(Cue::toggle));
    EXPECT_TRUE(rail_.expanded());

    // The last entry sits at the bottom of the rail, whatever is above it.
    const Rect footer = rail_.row_rect(4);
    EXPECT_NEAR(footer.y + footer.h, 240.0f + 700.0f - rail_.style.padding, 0.01f);
    EXPECT_GT(footer.y, rail_.row_rect(3).y + rail_.style.row_height);
}

TEST_F(ComponentsNavigation, BreadcrumbAnimatesPushAndPop)
{
    crumbs_.set_bounds({400.0f, 240.0f, 900.0f, 40.0f});
    crumbs_.set_path({"Home", "Library"}, false);
    EXPECT_EQ(crumbs_.depth(), 2);
    crumbs_.push("Saves");
    EXPECT_EQ(crumbs_.depth(), 3);
    EXPECT_EQ(crumbs_.path().back(), "Saves");
    EXPECT_TRUE(crumbs_.pop());
    EXPECT_EQ(crumbs_.depth(), 2) << "a leaving segment is no longer part of the path";

    // Replacing the path keeps what both start with.
    crumbs_.set_path({"Home", "Store", "Deals"});
    const std::vector<std::string> expected{"Home", "Store", "Deals"};
    EXPECT_EQ(crumbs_.path(), expected);
    for (int i = 0; i < 60; ++i)
    {
        hui::ui::Canvas target = canvas();
        crumbs_.draw(target);
        expect_drawn("breadcrumb in motion");
        crumbs_.update(kFrame);
    }
    crumbs_.set_path({});
    EXPECT_EQ(crumbs_.depth(), 0);
    EXPECT_FALSE(crumbs_.pop());
}

TEST_F(ComponentsNavigation, BreadcrumbFoldsTheMiddleWhenItDoesNotFit)
{
    const std::vector<std::string> path{"Home", "Library", "Collections", "Lantern Pass", "Saves"};
    const auto settle_drawn = [this]
    {
        // Widths are measured when it draws; the layout follows on update.
        for (int i = 0; i < 4; ++i)
        {
            hui::ui::Canvas target = canvas();
            crumbs_.draw(target);
            crumbs_.update(kFrame);
        }
    };
    crumbs_.set_bounds({400.0f, 240.0f, 1400.0f, 40.0f});
    crumbs_.set_path(path, false);
    settle_drawn();
    EXPECT_EQ(crumbs_.folded(), 0);

    crumbs_.set_bounds({400.0f, 240.0f, 380.0f, 40.0f});
    settle_drawn();
    EXPECT_GT(crumbs_.folded(), 0);
    EXPECT_LE(crumbs_.folded(), 3) << "the first and the last segment always stay";
    EXPECT_EQ(crumbs_.depth(), 5) << "folding hides segments, it does not remove them";

    crumbs_.set_bounds({400.0f, 240.0f, 1400.0f, 40.0f});
    crumbs_.style.max_segments = 3;
    settle_drawn();
    EXPECT_EQ(crumbs_.folded(), 2);
}

TEST_F(ComponentsNavigation, PageDotsTurnsPagesOnInput)
{
    EXPECT_EQ(send(dots_, nav(Direction::right)), Event::changed);
    EXPECT_TRUE(asked(Cue::tab));
    EXPECT_EQ(dots_.page(), 1);
    EXPECT_EQ(send(dots_, nav(Direction::up)), Event::none);

    dots_.set_page(3);
    EXPECT_EQ(send(dots_, nav(Direction::right)), Event::changed) << "wraps by default";
    EXPECT_EQ(dots_.page(), 0);
    dots_.style.wrap = false;
    EXPECT_EQ(send(dots_, nav(Direction::left)), Event::refused);
    EXPECT_TRUE(asked(Cue::error));
    EXPECT_EQ(send(dots_, held(Direction::left)), Event::refused);
    EXPECT_TRUE(feedback_.cues.empty());

    dots_.set_page(2);
    dots_.update(kFrame);
    EXPECT_GT(dots_.value(), 0.0f);
    EXPECT_LT(dots_.value(), 2.0f);
    settle(dots_, 120);
    EXPECT_FLOAT_EQ(dots_.value(), 2.0f);
}

TEST_F(ComponentsNavigation, PageDotsAdvancesByItselfAndCanBePaused)
{
    dots_.style.auto_advance = 1.0f;
    settle(dots_, 30);
    EXPECT_NEAR(dots_.progress(), 0.5f, 0.02f);
    EXPECT_FALSE(dots_.take_advanced());
    settle(dots_, 31);
    EXPECT_EQ(dots_.page(), 1);
    EXPECT_TRUE(dots_.take_advanced());
    EXPECT_FALSE(dots_.take_advanced()) << "reported once";

    dots_.set_paused(true);
    const float before = dots_.progress();
    settle(dots_, 120);
    EXPECT_FLOAT_EQ(dots_.progress(), before);
    EXPECT_EQ(dots_.page(), 1);

    // Without wrap the last page stays, full.
    dots_.set_paused(false);
    dots_.style.wrap = false;
    dots_.set_page(3);
    settle(dots_, 200);
    EXPECT_EQ(dots_.page(), 3);
    EXPECT_FLOAT_EQ(dots_.progress(), 1.0f);
}

TEST_F(ComponentsNavigation, MenuOpensMovesAndReports)
{
    EXPECT_EQ(send(menu_, press(Action::confirm)), Event::none) << "closed: it takes nothing";
    EXPECT_FALSE(menu_.visible());

    feedback_.clear();
    menu_.open({600.0f, 300.0f, 200.0f, 60.0f}, feedback_);
    EXPECT_TRUE(asked(Cue::modal_open));
    EXPECT_TRUE(menu_.is_open());
    EXPECT_EQ(menu_.focus(), 0);

    EXPECT_EQ(send(menu_, nav(Direction::down)), Event::moved);
    EXPECT_EQ(send(menu_, press(Action::confirm)), Event::changed);
    EXPECT_TRUE(menu_.items()[1].checked);
    EXPECT_TRUE(asked(Cue::toggle));
    EXPECT_TRUE(menu_.is_open()) << "a check keeps the menu open";

    EXPECT_EQ(send(menu_, nav(Direction::down)), Event::moved);
    EXPECT_EQ(menu_.focus(), 3) << "the separator is stepped over";
    EXPECT_EQ(send(menu_, press(Action::confirm)), Event::refused) << "disabled";
    EXPECT_TRUE(menu_.is_open());

    EXPECT_EQ(send(menu_, nav(Direction::down)), Event::moved);
    EXPECT_EQ(send(menu_, press(Action::confirm)), Event::activated);
    EXPECT_EQ(menu_.focus(), 4);
    EXPECT_TRUE(asked(Cue::select));
    EXPECT_FALSE(menu_.is_open());
    settle(menu_, 60);
    EXPECT_FALSE(menu_.visible());

    menu_.open({600.0f, 300.0f, 200.0f, 60.0f}, feedback_);
    EXPECT_EQ(send(menu_, nav(Direction::up)), Event::moved) << "wraps to the last item";
    EXPECT_EQ(menu_.focus(), 5);
    EXPECT_EQ(send(menu_, press(Action::back)), Event::cancelled);
    EXPECT_TRUE(asked(Cue::modal_close));
    EXPECT_FALSE(menu_.is_open());
}

TEST_F(ComponentsNavigation, MenuFlipsAndMovesToStayInsideItsBounds)
{
    const Rect screen{48.0f, 48.0f, 1824.0f, 984.0f};
    menu_.set_bounds(screen);
    const auto inside = [&](const Rect &panel)
    {
        return panel.x >= screen.x - 0.5f && panel.y >= screen.y - 0.5f &&
               panel.x + panel.w <= screen.x + screen.w + 0.5f &&
               panel.y + panel.h <= screen.y + screen.h + 0.5f;
    };

    const Rect top{600.0f, 200.0f, 200.0f, 60.0f};
    menu_.open(top, feedback_);
    EXPECT_EQ(menu_.side(), MenuSide::below);
    EXPECT_GE(menu_.panel_rect().y, top.y + top.h);

    const Rect low{600.0f, 900.0f, 200.0f, 60.0f};
    menu_.open(low, feedback_);
    EXPECT_EQ(menu_.side(), MenuSide::above) << "no room below";
    EXPECT_LE(menu_.panel_rect().y + menu_.panel_rect().h, low.y);
    EXPECT_TRUE(inside(menu_.panel_rect()));

    const Rect corner{1780.0f, 940.0f, 80.0f, 60.0f};
    menu_.open(corner, feedback_);
    EXPECT_TRUE(inside(menu_.panel_rect())) << "moved along the anchor";

    menu_.style.side = MenuSide::right;
    menu_.open({100.0f, 400.0f, 200.0f, 60.0f}, feedback_);
    EXPECT_EQ(menu_.side(), MenuSide::right);
    EXPECT_GE(menu_.panel_rect().x, 300.0f);
    menu_.open({1600.0f, 400.0f, 200.0f, 60.0f}, feedback_);
    EXPECT_EQ(menu_.side(), MenuSide::left) << "no room to the right";
    EXPECT_TRUE(inside(menu_.panel_rect()));
}

TEST_F(ComponentsNavigation, DrawsInEveryThemeAndVariant)
{
    using hui::ui::HighlightKind;
    using hui::ui::PageDotsKind;
    using hui::ui::TabKind;
    using hui::ui::TabWidth;
    crumbs_.set_bounds({400.0f, 240.0f, 420.0f, 40.0f});
    crumbs_.set_path({"Home", "Library", "Collections", "Saves"}, false);
    crumbs_.push("Slot 2");
    dots_.set_count(12); // more than fit: the window of marks is exercised
    dots_.set_page(7, true);
    dots_.set_progress(0.4f);
    tabs_.style.glyph_width = 24.0f;
    menu_.style.glyph_width = 24.0f;
    menu_.open({600.0f, 900.0f, 200.0f, 60.0f}, feedback_);
    send(menu_, nav(Direction::up)); // onto the destructive item
    const TabKind kinds[] = {TabKind::pill, TabKind::underline, TabKind::segmented, TabKind::boxed};
    const HighlightKind highlights[] = {HighlightKind::tint, HighlightKind::fill,
                                        HighlightKind::bar, HighlightKind::ring};
    const PageDotsKind marks[] = {PageDotsKind::dots, PageDotsKind::dashes, PageDotsKind::numbers,
                                  PageDotsKind::dots};
    const MenuSide sides[] = {MenuSide::automatic, MenuSide::above, MenuSide::right,
                              MenuSide::left};
    for (const hui::ui::Theme &theme : hui::ui::themes())
    {
        for (int variant = 0; variant < 4; ++variant)
        {
            tabs_.style.theme = theme;
            tabs_.style.kind = kinds[variant];
            tabs_.style.width = static_cast<TabWidth>(variant % 3);
            tabs_.style.on_page = variant % 2 == 0;
            // The narrow bar overflows, so the scrolling path is drawn too.
            tabs_.set_bounds({400.0f, 300.0f, variant == 3 ? 320.0f : 900.0f, 56.0f});
            rail_.style.theme = theme;
            rail_.style.highlight.kind = highlights[variant];
            rail_.set_expanded(variant % 2 == 0);
            rail_.set_bounds({96.0f, 240.0f, 0.0f, variant == 3 ? 260.0f : 700.0f});
            crumbs_.style.theme = theme;
            crumbs_.style.chips = variant == 3;
            crumbs_.style.separator = static_cast<hui::ui::CrumbSeparator>(variant % 3);
            dots_.style.theme = theme;
            dots_.style.kind = marks[variant];
            menu_.style.theme = theme;
            menu_.style.highlight.kind = highlights[variant];
            menu_.style.side = sides[variant];
            menu_.style.scrim = variant == 1 ? 0.4f : 0.0f;
            for (int frame = 0; frame < 3; ++frame)
            {
                tabs_.update(kFrame);
                rail_.update(kFrame);
                crumbs_.update(kFrame);
                dots_.update(kFrame);
                menu_.update(kFrame);
            }
            hui::ui::Canvas target = canvas();
            tabs_.draw(target);
            rail_.draw(target);
            crumbs_.draw(target);
            dots_.draw(target);
            menu_.draw(target);
            expect_drawn(theme.id);
        }
    }
    // Restyling thirty times lost nothing.
    EXPECT_EQ(dots_.page(), 7);
    EXPECT_EQ(crumbs_.depth(), 5);
    EXPECT_TRUE(menu_.is_open());
    EXPECT_EQ(menu_.focus(), 5);
}

TEST_F(ComponentsNavigation, GalleryPageTourLeavesItAsItWasFound)
{
    const std::unique_ptr<hui::concepts::gallery::Page> page =
        hui::concepts::gallery::make_navigation_page(context_);
    const std::string first = page->variant();
    const std::size_t hints = page->hints().size();
    int pictures = 0;
    for (const hui::app::TourStep &step : page->tour())
    {
        for (int frame = 0, frames = static_cast<int>(step.wait / kFrame); frame < frames; ++frame)
            page->update(idle(), kFrame, feedback_);
        if (step.capture != nullptr)
        {
            EXPECT_EQ(std::strncmp(step.capture, "navigation", 10), 0) << step.capture;
            ++pictures;
            hui::ui::Canvas target = canvas();
            page->draw(target);
            expect_drawn(step.capture);
        }
        hui::InputFrame input = idle();
        input.pressed = step.press;
        input.held = step.press;
        input.nav = step.nav;
        feedback_.clear();
        page->update(input, kFrame, feedback_);
    }
    EXPECT_GE(pictures, 3);
    EXPECT_LE(pictures, 5);
    EXPECT_EQ(first, page->variant());
    EXPECT_EQ(page->hints().size(), hints) << "the menu is closed";
}

} // namespace
