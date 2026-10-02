#!/usr/bin/env bash
# ps5-homebrew-ui - Converts songs from any tool into the game's music format.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# usage: tools/prepare-music.sh <folder with songs>
#
# Every audio file ffmpeg can read (MP3, WAV, FLAC, M4A, OGG...) becomes
# assets/audio/music/<name>.ogg: OGG Vorbis, 48 kHz stereo, quality 6, levelled
# to -18 LUFS with true peak at or below -1 dBTP (-2 before encoding, as Vorbis
# overshoots a little), silence trimmed at both ends and a short fade at the
# end. All of them join the shuffled playlist.

set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source_dir=${1:?usage: tools/prepare-music.sh <folder with songs>}
out="$root/assets/audio/music"
mkdir -p "$out"

shopt -s nullglob nocaseglob
count=0
for song in "$source_dir"/*.{mp3,wav,flac,m4a,aac,ogg,opus}; do
    name=$(basename "${song%.*}" | tr -c 'A-Za-z0-9 ._\n-' '_')
    # Measure the length after trimming so the fade lands at the real end.
    trimmed=$(mktemp --suffix=.wav)
    ffmpeg -v error -y -i "$song" -af \
        "silenceremove=start_periods=1:start_threshold=-50dB,areverse,silenceremove=start_periods=1:start_threshold=-50dB,areverse" \
        -ar 48000 -ac 2 "$trimmed"
    duration=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$trimmed")
    fade_start=$(awk -v d="$duration" 'BEGIN { s = d - 2; if (s < 0) s = 0; print s }')
    ffmpeg -v error -y -i "$trimmed" -af \
        "loudnorm=I=-18:TP=-2:LRA=11,afade=t=in:d=0.05,afade=t=out:st=$fade_start:d=2" \
        -ar 48000 -ac 2 -c:a libvorbis -q:a 6 "$out/$name.ogg"
    rm -f "$trimmed"
    printf '%s -> %s.ogg (%.0f s)\n' "$(basename "$song")" "$name" "$duration"
    count=$((count + 1))
done
echo "prepared $count song(s) into $out"
python3 "$root/tools/audio-check.py"
