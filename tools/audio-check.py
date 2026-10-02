#!/usr/bin/env python3
# ps5-homebrew-ui - Validates delivered sound effects and music .
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks assets/audio/sfx/<set>/*.wav and assets/audio/music/*.ogg.

Errors (the file would be rejected or misplayed) make the exit status 1.
Warnings point at targets such as length, loudness and fades; they are
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

# Target lengths in seconds for the cues that have one; other cues are only
# checked for format, level and fades.
LENGTHS = {
    "focus": (0.02, 0.06), "select": (0.08, 0.2), "back": (0.08, 0.2), "tab": (0.1, 0.22),
    "toggle": (0.05, 0.12), "slider": (0.03, 0.06), "error": (0.1, 0.3), "tick": (0.015, 0.04),
    "type": (0.06, 0.12), "notify": (0.3, 0.8), "launch": (0.3, 1.3), "complete": (1.5, 4.0),
    "new_record": (1.0, 2.5), "welcome": (0.8, 1.5),
}
# Every .ogg in the music folder joins the shuffled playlist; keep names simple.
MUSIC_NAMES = re.compile(r"^[A-Za-z0-9][A-Za-z0-9 _.-]*$")
SFX_NAME = re.compile(r"^(?P<cue>[a-z_]+?)(?:_(?P<n>\d\d))?$")


def cue_names():
    """The cue vocabulary, read from the table the app itself uses."""
    source = (ROOT / "src/audio/cues.cpp").read_text()
    return set(re.findall(r'^\s*\{"(\w+)", Bus::', source, re.M))


def set_names():
    source = (ROOT / "src/audio/cues.cpp").read_text()
    match = re.search(r"kSetNames\[kSoundSetCount\] = \{([^}]*)\}", source)
    return re.findall(r'"(\w+)"', match.group(1)) if match else []


def dbfs(value):
    return 20 * math.log10(value) if value > 0 else -math.inf


class Report:
    def __init__(self):
        self.errors = 0
        self.warnings = 0

    def error(self, path, text):
        self.errors += 1
        print(f"ERROR {path.parent.name}/{path.name}: {text}")

    def warn(self, path, text):
        self.warnings += 1
        print(f"warn  {path.parent.name}/{path.name}: {text}")


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


def check_sfx(report, cues):
    files = []
    sets = set_names()
    for folder in sorted(p for p in SFX.iterdir() if p.is_dir()) if SFX.is_dir() else []:
        if folder.name not in sets:
            print(f"ERROR {folder.name}/: not a sound set (known: {', '.join(sets)})")
            report.errors += 1
            continue
        files += sorted(folder.glob("*.wav"))
    for path in files:
        match = SFX_NAME.match(path.stem)
        if not match or match.group("cue") not in cues:
            report.error(path, "unknown cue name (see audio::Cue in src/audio/cues.hpp)")
            continue
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
        if match.group("cue") in LENGTHS:
            low, high = LENGTHS[match.group("cue")]
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
    sfx = check_sfx(report, cue_names())
    music = check_music(report)
    print(f"audio-check: {sfx} sound effect(s), {music} music track(s), "
          f"{report.errors} error(s), {report.warnings} warning(s)")
    return 1 if report.errors else 0


if __name__ == "__main__":
    sys.exit(main())
