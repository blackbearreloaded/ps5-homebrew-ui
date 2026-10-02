// ps5-homebrew-ui - Describes every design and theme as JSON (host tool).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/shell.hpp"

namespace hui::host
{

// Writes {"designs": [...], "themes": [...]} to path.
bool write_manifest(const char *path, const app::Shell &shell);

} // namespace hui::host
