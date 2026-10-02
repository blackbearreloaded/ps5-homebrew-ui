# ps5-homebrew-ui

The instructions for coding agents are in [AGENTS.md](AGENTS.md); they apply
to Claude Code as they do to every other agent, and are imported below.

The one habit that matters most here: **preview on the PC and look at the
pictures before calling any UI change done.**

```bash
tools/host-snapshots.sh build/snapshots <design id|all>   # PNG frames in build/snapshots/
make test-unit                                            # tests, sanitizers on
make                                                      # the PS5 app folder in dist/
make lint                                                 # format, static analysis, metadata
```

The preview renders the same UI code as the console, off-screen, with no
console and no GPU ("Local preview" in AGENTS.md has the options, the output
names and what it cannot show). Never launch on a console without asking the
owner first.

@AGENTS.md
