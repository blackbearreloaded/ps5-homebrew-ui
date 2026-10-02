#!/usr/bin/env python3
# ps5-homebrew-ui - Validates delivered sound effects and music .
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks assets/audio/sfx/*.wav and assets/audio/music/*.ogg.

Errors (the file would be rejected or misplayed) make the exit status 1.
Warnings point at spec targets such as length, loudness and fades; they are
advice for the mix, not failures. Music loudness needs ffmpeg.
"""

import array
import json
import math
import re
import shutil
import subprocess
import sys
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SFX = ROOT / "assets/audio/sfx"
MUSIC = ROOT / "assets/audio/music"

# Cue -> (min seconds, max seconds) from the Appendix A tables.
CUES = {
    "ui_focus": (0.02, 0.06), "ui_select": (0.08, 0.15), "ui_back": (0.08, 0.15),
    "ui_tab": (0.1, 0.2), "ui_favorite_on": (0.2, 0.4), "ui_favorite_off": (0.1, 0.2),
    "ui_launch": (0.3, 0.7), "ui_pause_open": (0.15, 0.3), "ui_pause_close": (0.15, 0.3),
    "ui_toggle": (0.06, 0.12), "ui_slider": (0.03, 0.06), "ui_error": (0.1, 0.2),
    "ui_notify": (0.3, 0.6), "cursor": (0.015, 0.04), "place": (0.06, 0.15),
    "mark": (0.06, 0.15), "erase": (0.06, 0.15), "digit": (0.06, 0.12),
    "rotate": (0.08, 0.2), "slide": (0.08, 0.2), "flip": (0.06, 0.15),
    "pickup": (0.06, 0.15), "drop": (0.06, 0.15), "connect": (0.15, 0.3),
    "reveal": (0.06, 0.12), "cascade": (0.2, 0.5), "merge": (0.1, 0.2),
    "spawn": (0.06, 0.12), "invalid": (0.1, 0.2), "undo": (0.08, 0.15),
    "redo": (0.08, 0.15), "new_game": (0.3, 0.7), "restart": (0.3, 0.6),
    "solve_reveal": (0.5, 1.5), "complete": (1.5, 4.0), "new_record": (1.0, 2.5),
    "explode": (0.8, 2.0), "game_over": (1.0, 3.0),
}
# Every .ogg in the music folder joins the shuffled playlist; keep names simple.
MUSIC_NAMES = re.compile(r"^[A-Za-z0-9][A-Za-z0-9 _.-]*$")
SFX_NAME = re.compile(r"^(?:(?P<game>[a-z0-9]+)\.)?(?P<cue>[a-z_]+?)(?:_(?P<n>\d\d))?$")


def game_ids():
    ids = {"g2048", "tenfold"}
    meta = ROOT / "src/games/sgt/upstream_meta.inc"
    ids.update(re.findall(r"^SGT_GAME\((\w+),", meta.read_text(), re.M))
    return ids


def dbfs(value):
    return 20 * math.log10(value) if value > 0 else -math.inf


class Report:
    def __init__(self):
        self.errors = 0
        self.warnings = 0

    def error(self, path, text):
        self.errors += 1
        print(f"ERROR {path.name}: {text}")

    def warn(self, path, text):
        self.warnings += 1
        print(f"warn  {path.name}: {text}")


def read_wav(path):
    with wave.open(str(path), "rb") as w:
        rate, channels, width, frames = (w.getframerate(), w.getnchannels(), w.getsampwidth(),
                                         w.getnframes())
        raw = w.readframes(frames)
    if width == 2:
        samples = array.array("h", raw)
        scale = 32768.0
    elif width == 3:
        samples = array.array("i", (int.from_bytes(raw[i:i + 3], "little", signed=True)
                                    for i in range(0, len(raw), 3)))
        scale = 8388608.0
    else:
        samples = array.array("h")
        scale = 1.0
    return rate, channels, width, frames, samples, scale


def check_sfx(report, games):
    files = sorted(SFX.glob("*.wav")) if SFX.is_dir() else []
    for path in files:
        match = SFX_NAME.match(path.stem)
        if not match or match.group("cue") not in CUES:
            report.error(path, "unknown cue name (see PLAN.md Appendix A)")
            continue
        if match.group("game") and match.group("game") not in games:
            report.error(path, f"unknown game id '{match.group('game')}'")
        try:
            rate, channels, width, frames, samples, scale = read_wav(path)
        except (wave.Error, EOFError) as exc:
            report.error(path, f"not a PCM WAV file ({exc})")
            continue
        if rate != 48000:
            report.error(path, f"{rate} Hz (need 48000)")
        if channels not in (1, 2):
            report.error(path, f"{channels} channels (need 1 or 2)")
        if width not in (2, 3):
            report.error(path, f"{width * 8}-bit (need 16 or 24)")
            continue
        if frames == 0:
            report.error(path, "no audio")
            continue
        seconds = frames / rate
        low, high = CUES[match.group("cue")]
        if seconds < low * 0.5 or seconds > high * 1.5:
            report.warn(path, f"{seconds:.3f} s (target {low}-{high} s)")
        peak = max(abs(s) for s in samples) / scale
        if dbfs(peak) > -1.0:
            report.warn(path, f"peak {dbfs(peak):.1f} dBFS (keep at or below -1)")
        threshold = scale * 10 ** (-60 / 20)
        lead = next((i for i, s in enumerate(samples) if abs(s) > threshold), len(samples))
        lead_ms = lead / channels / rate * 1000
        if lead_ms > 5:
            report.warn(path, f"{lead_ms:.1f} ms of leading silence (keep under 5 ms)")
        tail = samples[-max(1, int(rate * 0.005) * channels):]
        if dbfs(max(abs(s) for s in tail) / scale) > -40:
            report.warn(path, "tail is not faded to silence")
    return len(files)


def probe(path):
    out = subprocess.run(["ffprobe", "-v", "error", "-show_streams", "-show_format", "-of", "json",
                          str(path)], capture_output=True, text=True, check=False)
    return json.loads(out.stdout or "{}")


def loudness(path):
    out = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", str(path), "-filter:a",
                          "ebur128=peak=true", "-f", "null", "-"], capture_output=True, text=True,
                         check=False).stderr
    integrated = re.findall(r"I:\s+(-?[\d.]+) LUFS", out)
    peak = re.findall(r"Peak:\s+(-?[\d.]+) dBFS", out)
    return (float(integrated[-1]) if integrated else None, float(peak[-1]) if peak else None)


def check_music(report):
    files = sorted(MUSIC.glob("*.ogg")) if MUSIC.is_dir() else []
    have_ffmpeg = shutil.which("ffprobe") and shutil.which("ffmpeg")
    if files and not have_ffmpeg:
        print("note  ffmpeg/ffprobe not found: music is checked by name only")
    for path in files:
        if not MUSIC_NAMES.match(path.stem):
            report.error(path, "use letters, digits, spaces, '.', '_' or '-' in song names")
        if not have_ffmpeg:
            continue
        info = probe(path)
        streams = [s for s in info.get("streams", []) if s.get("codec_type") == "audio"]
        if not streams or streams[0].get("codec_name") != "vorbis":
            report.error(path, "not an OGG Vorbis stream")
            continue
        stream = streams[0]
        if int(stream.get("sample_rate", 0)) != 48000:
            report.error(path, f"{stream.get('sample_rate')} Hz (need 48000)")
        if int(stream.get("channels", 0)) not in (1, 2):
            report.error(path, f"{stream.get('channels')} channels (need 1 or 2)")
        elif int(stream.get("channels", 0)) == 1:
            report.warn(path, "mono (stereo is expected for music)")
        seconds = float(info.get("format", {}).get("duration", 0))
        if not 60 <= seconds <= 480:
            report.warn(path, f"{seconds:.0f} s long (songs are usually 1-8 minutes)")
        tags = {k.upper(): v for k, v in info.get("format", {}).get("tags", {}).items()}
        tags.update({k.upper(): v for k, v in stream.get("tags", {}).items()})
        if "LOOPLENGTH" in tags and "LOOPSTART" not in tags:
            report.error(path, "LOOPLENGTH without LOOPSTART")
        integrated, peak = loudness(path)
        if integrated is not None and abs(integrated + 18) > 2:
            report.warn(path, f"{integrated:.1f} LUFS integrated (target -18)")
        if peak is not None and peak > -1.0:
            report.warn(path, f"true peak {peak:.1f} dBTP (keep at or below -1)")
    return len(files)


def main():
    report = Report()
    sfx = check_sfx(report, game_ids())
    music = check_music(report)
    print(f"audio-check: {sfx} sound effect(s), {music} music track(s), "
          f"{report.errors} error(s), {report.warnings} warning(s)")
    return 1 if report.errors else 0


if __name__ == "__main__":
    sys.exit(main())
