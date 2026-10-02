# Validating on the console

Pictures rendered on a PC prove a design looks right. Only the console proves
it runs: that the shaders compile there, the frame rate holds, the sound
plays and the app closes cleanly. The app has a self-driving run mode for
exactly that, so a validation takes one launch and no controller.

## The tour run

When the app starts and finds `/data/ps5-homebrew-ui/tour.txt`, it:

1. deletes the file (one run per request);
2. shows every design in turn and replays the inputs its `tour()` describes;
3. saves a quarter-size picture of each tour state to
   `/data/ps5-homebrew-ui/tour/NN-<id>[-<state>].bmp`;
4. measures every presented frame per design;
5. writes `/data/ps5-homebrew-ui/tour/report.txt` and logs the same lines;
6. asks the system to close it (`sceSystemServiceLoadExec("exit", NULL)`).

`tour.txt` may be empty or contain `all` (every design), or one design id
(`aurora`) to run only that one.

The report has one line per design:

```
tour 01 aurora       frames=412 avg_ms=16.67 worst_ms=16.92 draws=14 shapes=373
```

`avg_ms` near 16.67 with a `worst_ms` under about 20 means the design held
60 frames per second through its whole tour, including dialogs and blur.

## Running it

`tools/console-tour.py` does the whole cycle from a PC on the same network.
It needs FTP (2121), the kernel log (3232) and an ELF loader (9021) on the
console, and the launch helper from
[ps5-homebrew-dev-protocol](https://github.com/blackbearreloaded/ps5-homebrew-dev-protocol).

```bash
make                                            # build dist/<TITLE_ID>
PS5_HOST=<console address> tools/console-tour.py results/run-1
```

It uploads the build (verifying every file by size and the executable by
hash), writes the trigger, records the kernel log, launches the title, waits
for the app's own exit, downloads the report, the pictures and `app.log`, and
checks the log for errors. It never kills the app and never retries.

What to look at afterwards, in `results/run-1/`:

| File | Check |
| --- | --- |
| `report.txt` | Every design present; `avg_ms` about 16.67; no outlier `worst_ms` |
| `tour/*.bmp` | Compare with `build/snapshots/*.png` from the PC: same layout, colours and text |
| `app.log` | `gl 4.6`, `sounds files=67 rejected=0`, no `fatal`, `shader ... failed` or `rejected` lines; `tour finished`; `quit requested` |
| `klog.txt` | The title's start and its clean exit; no crash report |

## Rules for console runs

These come from hard experience with shared development consoles:

- **Finish on the PC first.** Tests and snapshots must pass before a console
  is involved.
- **One run at a time, a handful per session.** Long unattended series of
  launches are how consoles get lost. Check that the console still answers
  between runs and stop at the first anomaly.
- **Let the app end itself.** The tour exits through the system. Do not send
  a kill to a title that is still rendering.
- **Take the console's lock** if it is shared, and release only your own.
- **Verify what you uploaded** after a pause: files written just before an
  unclean shutdown can come back empty.
- **Never touch the console's settings**, and never run anything that was not
  declared for the test.

## What a PC cannot tell you

State these as "not verified" until someone has checked them by hand:

- how the sounds actually sound through a TV, and their balance against music;
- how the controller rumble feels, and the light bar colour;
- how focus movement feels on a real stick (dead zone, repeat rate);
- text legibility from the sofa on the actual TV.
