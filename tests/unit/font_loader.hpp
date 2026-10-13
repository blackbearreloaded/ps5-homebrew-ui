// ps5-homebrew-ui - Loads a baked font from assets/fonts for a test.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/save_file.hpp"
#include "gfx/font.hpp"

#include <gtest/gtest.h>

#include <string>

#ifndef HUI_SOURCE_DIR
#define HUI_SOURCE_DIR "."
#endif

namespace hui::testing
{

// name is the file name without its extension ("inter-regular").
inline gfx::Font load_font(const char *name)
{
    gfx::Font font;
    std::string data;
    const std::string path = std::string(HUI_SOURCE_DIR) + "/assets/fonts/" + name + ".huifont";
    EXPECT_TRUE(save::read_file(path, &data)) << path;
    EXPECT_TRUE(font.load(data)) << font.error();
    return font;
}

} // namespace hui::testing
