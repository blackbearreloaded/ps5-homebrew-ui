#!/usr/bin/env bash
# ps5-homebrew-ui - Render the pictures and clips the documentation shows.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# usage: tools/render-media.sh [pictures|clips|all]
#
# Everything under docs/media is produced by the app itself, running its tour
# on the PC (see tools/host-snapshots.sh):
#   docs/media/designs/<id>.jpg, <id>-<state>.jpg   one picture per tour state
#   docs/media/designs/<id>.webp                    a clip of the whole tour
#   docs/media/designs/components-<page>.webp       one clip per Component Library page
#   docs/media/themes/<theme>.jpg, <theme>.webp     every theme of the Theme Lab
#   docs/media/switcher.webp                        L1/R1 through every design
# Needs ffmpeg (clips, with libwebp) and Python with Pillow (pictures).

set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
what=${1:-all}
media="$root/docs/media"
work="$root/build/media"
mkdir -p "$media/designs" "$media/themes" "$work"

if [[ $what == pictures || $what == all ]]; then
    rm -rf "$work/pictures"
    bash "$root/tools/host-snapshots.sh" "$work/pictures" all >/dev/null
    python3 - "$work/pictures" "$media" <<'PY'
import sys
from pathlib import Path
from PIL import Image

source, media = Path(sys.argv[1]), Path(sys.argv[2])
for old in list((media / "designs").glob("*.jpg")) + list((media / "themes").glob("*.jpg")):
    old.unlink()
count = 0
for path in sorted(source.glob("[0-9][0-9]-*.png")):
    name = path.stem[3:]                      # drop the "NN-" order prefix
    if name.startswith("themes-") and name != "themes-dialog":
        target = media / "themes" / (name[len("themes-"):] + ".jpg")
    else:
        target = media / "designs" / (name + ".jpg")
    image = Image.open(path).convert("RGB").resize((1280, 720), Image.LANCZOS)
    image.save(target, quality=88, optimize=True, progressive=True)
    count += 1
print(f"{count} pictures in {media}")
PY
fi

if [[ $what == clips || $what == all ]]; then
    command -v ffmpeg >/dev/null || { echo "ffmpeg is needed for clips" >&2; exit 2; }
    rm -rf "$work/clips" "$work/theme-clips" "$work/switch"
    mkdir -p "$work/clips" "$work/theme-clips" "$work/switch"
    # One clip per design, its whole tour.
    HUI_REEL="$work/clips" bash "$root/tools/host-snapshots.sh" "$work/scratch" all 640 360 >/dev/null
    # The Theme Lab again, cut into one clip per theme.
    HUI_REEL="$work/theme-clips" HUI_REEL_SPLIT=1 \
        bash "$root/tools/host-snapshots.sh" "$work/scratch" themes 640 360 >/dev/null
    # L1/R1 through everything.
    HUI_REEL="$work/switch" HUI_REEL_SWITCH=1 \
        bash "$root/tools/host-snapshots.sh" "$work/scratch" all 960 540 >/dev/null
    # The Component Library's tour is minutes long: it is cut at its
    # pictures, and each page keeps the clip that leads up to its first one.
    rm -rf "$work/component-clips"
    mkdir -p "$work/component-clips"
    HUI_REEL="$work/component-clips" HUI_REEL_SPLIT=1 \
        bash "$root/tools/host-snapshots.sh" "$work/scratch" components 640 360 >/dev/null
    rm -f "$media"/designs/*.webp "$media"/themes/*.webp
    for clip in "$work"/clips/*.webp; do
        case $(basename "$clip") in
        themes.webp | components.webp) ;;
        *) cp "$clip" "$media/designs/" ;;
        esac
    done
    for clip in "$work"/component-clips/*-components-*.webp; do
        name=$(basename "$clip" .webp)
        name=${name#*-components-}
        # Page clips are named after the page alone ("forms", not "forms-wide").
        [[ $name == *-* ]] || cp "$clip" "$media/designs/components-$name.webp"
    done
    cp "$media/designs/components-collections.webp" "$media/designs/components.webp"
    for clip in "$work"/theme-clips/*-themes-*.webp; do
        name=$(basename "$clip" .webp)
        name=${name#*-themes-}
        [[ $name == dialog ]] || cp "$clip" "$media/themes/$name.webp"
    done
    cp "$work/switch/switcher.webp" "$media/switcher.webp"
    du -sh "$media/designs" "$media/themes" "$media/switcher.webp"
fi
