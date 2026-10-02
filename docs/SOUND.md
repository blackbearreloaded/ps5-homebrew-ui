# Sound

A UI without sound feels unfinished however it looks. This page covers the
cue vocabulary, the two sound sets shipped here, how to use them well, and
how to add your own recordings.

## How it works

```
design  --feedback.play(Cue)-->  shell  --SoundBank::play(set, cue)-->  Mixer  -->  audio thread
```

- A design asks for a **cue**: what happened (`focus`, `select`, `error`).
- The shell plays it with the design's **sound set** (`ConceptInfo::sounds`).
- The **sound bank** picks the next recorded variation, detunes it by up to
  3 %, and posts it to the mixer. A cue the set does not record falls back to
  another set, then to a short synthesized tone, so a cue never fails.
- The **mixer** (`audio/mixer.hpp`) mixes 32 voices and two music decks into
  48 kHz stereo on its own thread, with three buses (`ui`, `sfx`, `music`), a
  volume per bus and a soft limiter. Posting a sound never blocks or
  allocates.

## The cue vocabulary

Interface cues play on the `ui` bus, feedback cues on the `sfx` bus, so the
player can balance "menu sounds" against "game sounds".

| Cue | When | paper | glass |
| --- | --- | :---: | :---: |
| `focus` | The highlight moved | 3 | 2 |
| `select` | Confirm | 1 | 1 |
| `back` | Cancel, leave | 1 | 1 |
| `tab` | Page, tab, filter or row changed | 1 | 1 |
| `open` | Entered a screen or drawer | | 1 |
| `modal_open` / `modal_close` | Dialog or pause menu | 1 / 1 | 1 / 1 |
| `toggle` | Switch flipped | 1 | 3 |
| `slider` | Slider or stepper moved one step | 2 | 3 |
| `error` | Refused: list end, invalid input | 1 | 1 |
| `notify` | Toast, achievement | 1 | 1 |
| `launch` | Started something big | 1 | 1 |
| `favorite_on` / `favorite_off` | Starred, unstarred | 1 / 1 | |
| `saved` | Settings stored | | 1 |
| `welcome` | The app opened | | 1 |
| `resume` | Back into the content | | 1 |
| `tick` | Fine movement: a cursor, a dial detent | 2 | |
| `type` | A character was entered | 2 | |
| `place`, `mark`, `erase` | Put down, flag, remove | 2, 2, 1 | |
| `rotate`, `slide`, `flip` | Turn, move along a track, turn over | 1, 2, 1 | |
| `pickup`, `drop`, `connect` | Grab, release, link | 1, 1, 1 | |
| `reveal`, `cascade`, `merge`, `spawn` | Uncover, chain, combine, appear | 1, 1, 2, 1 | |
| `invalid`, `undo`, `redo` | Not allowed, step back, step forward | 1, 1, 1 | |
| `new_game`, `restart`, `solve` | Fresh start, reset, long reveal | 1, 1, 1 | |
| `complete`, `new_record` | Success fanfare, personal best | 1, 1 | |
| `explode`, `game_over` | Failure with weight, the end | 1, 1 | |

The numbers are how many variations each set records. An empty cell means the
set borrows the other set's recording.

**paper** (from ProsperoPuzzles) is warm, wooden and tactile: paper, felt,
soft mallets. **glass** (from ProsperoEden) is soft and airy: tuned chimes in
one key. Press Square on the Toolbox design to hear every cue in both sets.

## Using sound well

- **Focus ticks are the quietest sound** and the most frequent. If navigation
  is tiring after a minute, the tick is too loud or too bright.
- **Pan by position.** `feedback.play(Cue::focus, 1.0f, ui::pan_for_x(x))`.
  The pan is deliberately small (at most 0.55).
- **Pitch by meaning.** `feedback.play(Cue::slider, 0.9f + 0.03f * value)`
  makes a slider rise as it fills. Rows lower on the screen can sound lower.
  Keep pitch between 0.7 and 1.4.
- **One sound per action.** When confirming also opens a dialog, play
  `modal_open`, not `select` and `modal_open`.
- **Refusals are soft.** `feedback.play(Cue::error, 1.0f, 0.0f, 0.6f)` at the
  end of a list, and nothing at all for a held direction.
- **Big cues are rare.** `launch`, `complete`, `welcome` last a second or
  more; they mark moments, not button presses. `complete` and `welcome` duck
  the music automatically.
- **Do not chain cues on a timer** to fake a melody; record one sound.

## Levels

The recordings are levelled so that cues sit together without per-cue gain:

| Kind | Target (RMS) | Peak |
| --- | --- | --- |
| Ticks (`focus`, `tick`, `slider`) | about -35 dBFS | under -19 |
| Interface (`select`, `back`, `tab`, `toggle`) | -28 to -31 | under -12 |
| Chimes and fanfares (`launch`, `complete`, `welcome`) | -23 to -27 | under -1 |

`tools/process-sfx.py` trims, fades and levels raw takes to these targets;
`make audio-check` validates what is in `assets/audio`.

## Adding or replacing sounds

1. Record or generate the sound: 48 kHz, 16-bit WAV, mono for short ticks,
   stereo for anything with space.
2. Name it `<cue>_NN.wav` (`focus_01.wav`, `focus_02.wav` for variations) and
   put it in `assets/audio/sfx/<set>/`.
3. Run `make audio-check`. Rebuild.

A new **set** is a new folder plus one line in `audio::SoundSet` and
`kSetNames` (`src/audio/cues.cpp`). A new **cue** is one line in `audio::Cue`
and one in the `kCues` table (name, bus and the placeholder tone).

The sounds shipped here were made for ProsperoPuzzles and ProsperoEden and are
reused unchanged apart from their file names. See
[THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).

## Music

`audio/music.hpp` streams OGG Vorbis from `assets/audio/music/` (48 kHz),
decoded on the main thread about 0.7 seconds ahead of the audio thread. It
plays the folder as a playlist: the order is shuffled afresh every time the
app starts, each song plays once with a short pause before the next, and after
the last one the same order starts again. It ducks by 6 dB under big cues, and
the music bus has its own volume in the settings.

Adding a song is dropping a file into the folder. Keep to 48 kHz stereo, about
-18 LUFS integrated with true peaks at or below -1 dBTP (so the interface
sounds stay on top), and a plain file name; `tools/audio-check.py` checks all
of it. The three songs shipped are "First Light", "Open Strings" and "Quiet
Hours".

## Controller rumble

`feedback.rumble(strength, seconds)` drives the DualSense motors. Use it as
punctuation: 0.25 for 0.05 s on a refusal, 0.7 for 0.18 s on a launch. The
player can turn it off (`Settings::haptics`). The controller's light bar
follows the active design's accent colour (`Settings::light_bar`).
