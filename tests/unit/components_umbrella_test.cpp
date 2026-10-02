// ps5-homebrew-ui - Tests: the umbrella header and the kit's independence.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/components.hpp"

#include <gtest/gtest.h>

namespace
{

// Including everything at once must compile without name clashes, and a
// default-constructed component must start from the first built-in theme.
TEST(ComponentsUmbrella, EveryHeaderTogether)
{
    hui::ui::ListView list;
    hui::ui::GridView grid;
    hui::ui::Carousel carousel;
    hui::ui::TabBar tabs;
    hui::ui::Dialog dialog;
    hui::ui::Form form;
    hui::ui::ProgressBar bar;
    EXPECT_STREQ(list.style.theme.id, hui::ui::default_theme().id);
    EXPECT_STREQ(grid.style.theme.id, carousel.style.theme.id);
    EXPECT_STREQ(tabs.style.theme.id, dialog.style.theme.id);
    EXPECT_STREQ(form.style.theme.id, bar.style.theme.id);
}

TEST(ComponentsUmbrella, RestylingKeepsState)
{
    hui::ui::ListView list;
    std::vector<hui::ui::ListItem> items(4);
    for (std::size_t i = 0; i < items.size(); ++i)
        items[i].title = "Row";
    list.set_items(items);
    list.set_focus(2);
    for (const hui::ui::Theme &theme : hui::ui::themes())
    {
        list.style.theme = theme;
        EXPECT_EQ(list.focus(), 2) << theme.id;
    }
}

} // namespace
