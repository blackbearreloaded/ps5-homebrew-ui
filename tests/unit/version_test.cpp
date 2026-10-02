// ps5-homebrew-ui - App version parsing tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/version.hpp"

#include <gtest/gtest.h>

#ifndef HUI_SOURCE_DIR
#define HUI_SOURCE_DIR "."
#endif

TEST(Version, ReadsContentVersion)
{
    EXPECT_EQ(hui::content_version(R"({"titleId": "PPSA99006", "contentVersion": "01.002.030"})"),
              "01.002.030");
    EXPECT_EQ(hui::content_version(R"({"contentVersion":"12.345.678","x":1})"), "12.345.678");
}

TEST(Version, RejectsMissingOrMalformed)
{
    EXPECT_EQ(hui::content_version("{}"), "");
    EXPECT_EQ(hui::content_version(R"({"contentVersion": "1.0.0"})"), "");
    EXPECT_EQ(hui::content_version(R"({"contentVersion": "01.00a.000"})"), "");
    EXPECT_EQ(hui::content_version(R"({"contentVersion": )"), "");
}

TEST(Version, TheRepositoryParamJsonHasOne)
{
    const std::string version =
        hui::read_content_version(std::string(HUI_SOURCE_DIR) + "/sce_sys/param.json");
    EXPECT_EQ(version.size(), 10u) << version;
    EXPECT_EQ(hui::read_content_version("/nonexistent/param.json"), "");
}
