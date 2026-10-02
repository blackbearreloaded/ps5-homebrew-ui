#!/usr/bin/env bash
# ps5-homebrew-ui - Vendors stb_vorbis (public domain / MIT) at a pinned commit.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The file is stored as stb_vorbis.inc so the PS5 build, which compiles every
# .c under src/, only compiles it once through src/third_party/stb/vorbis.c.

set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
commit=${1:-master}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

git clone --quiet https://github.com/nothings/stb.git "$work/stb"
git -C "$work/stb" checkout --quiet "$commit"
resolved=$(git -C "$work/stb" rev-parse HEAD)

destination="$root/src/third_party/stb"
mkdir -p "$destination"
cp "$work/stb/stb_vorbis.c" "$destination/stb_vorbis.inc"
cp "$work/stb/LICENSE" "$destination/LICENSE"
echo "$resolved" > "$destination/UPSTREAM"
echo "stb_vorbis vendored at $resolved"
