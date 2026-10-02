# Validating on the console

Pictures rendered on a PC prove a design looks right. Only the console proves
it runs: that the shaders compile there, the frame rate holds, the sound
plays and the app closes cleanly. The app has a self-driving run mode for
exactly that, so a validation takes one launch and no controller.

## The tour run

Two facts about a title's sandbox shape the mechanism:

- It has no `/data`. The app's output (log, pictures, report) goes to its own
  storage, `/download0/hui/dev`, which exists only while the title runs. A PC
  can *read* it over FTP as `/mnt/sandbox/<TITLE_ID>_000/download0/hui/dev`,
  but cannot write there.
- The install folder is writable from a PC and the app sees it as `/app0`.
  So requests travel that way: `/data/homebrew/<TITLE_ID>/dev/request.txt`
  holds one line, `tour <design id|all> <token>` or `quit - <token>`. The app
  honours a request once per token (it remembers the last one), so a request
  left in place does not make every later launch a tour.

When the app finds a tour request it has not honoured yet, it:

1. shows every design in turn and replays the inputs its `tour()` describes;
2. saves a small picture (480 x 270) of each tour state to
   `dev/tour/NN-<id>[-<state>].bmp`;
3. measures every presented frame per design;
4. writes `dev/tour/report.txt` and logs the same lines;
5. stays up for four more minutes, because closing unmounts the storage and
   the evidence with it;
6. asks the system to close it (`sceSystemServiceLoadExec("exit", NULL)`).

`dev/app.log` is the app's log. Put the request in place **before** launching:
the tool does not write into the install folder of a running title.

The design id `all` tours every design; one id (`aurora`) tours only that one.

The report has one line per design:

```
tour 01 aurora       frames=412 avg_ms=16.67 worst_ms=16.92 draws=14 shapes=373
```

`avg_ms` near 16.67 with a `worst_ms` under about 20 means the design held
60 frames per second through its whole tour, including dialogs and blur.

## Running it

`tools/console-tour.py` does the whole cycle from a PC on the same network.
It needs FTP (2121), the kernel log (3232), an ELF loader (9021) and
[ShadowMount Plus](https://github.com/drakmor/ShadowMountPlus) on the
console, and the launch helper from
[ps5-homebrew-dev-protocol](https://github.com/blackbearreloaded/ps5-homebrew-dev-protocol).

```bash
make                                            # build dist/<TITLE_ID>
PS5_HOST=<console address> tools/console-tour.py results/run-1
```

It uploads the build (every file read back and compared by hash), writes the
trigger, waits until the console has registered the title, records the kernel
log, launches the title, waits for the app's own exit, downloads the report,
the pictures and `app.log`, and checks the log for errors. It never kills the
app and never retries.

A title copied to `/data/homebrew` for the first time is not known to the
console yet. ShadowMount Plus registers it once the folder has stopped
changing (about ten seconds), which creates `/user/app/<TITLE_ID>/mount.lnk`;
the tool waits for that file. Launching earlier only shows the system's
"title not found" screen: nothing starts and nothing is harmed, but the run
is wasted.

What to look at afterwards, in `results/run-1/`:

| File | Check |
| --- | --- |
| `report.txt` | Every design present; `avg_ms` about 16.67; no outlier `worst_ms` |
| `tour/*.bmp` | Compare with `build/snapshots/*.png` from the PC: same layout, colours and text |
| `app.log` | `gl 4.6`, `sounds files=67 rejected=0`, no `fatal`, `shader ... failed` or `rejected` lines; `tour finished`; `quit requested` |
| `klog.txt` | The title's start and its clean exit; no crash report |
| `shadowmount.txt` | The title's lines from the console's lifecycle log: registered, started, released |

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
