# Contributing

New to the repository? Start with [AGENTS.md](AGENTS.md) (it is written for
people too) and [docs/CRAFT.md](docs/CRAFT.md). A new design follows
[docs/BUILDING_A_DESIGN.md](docs/BUILDING_A_DESIGN.md) and comes with tour
pictures you have looked at and behaviour tests.

Before committing a change:

1. Run `make test` and `make lint`; add a focused unit or integration regression
   for behavior changed by the patch.
2. Run `make`; it must reproduce the clean-room `runtime/libc.prx` digest.
3. Confirm the build reports zero static FSELF errors.
4. Do not commit `.env`, `build/`, `dist/`, `.local/`, `results/`, proprietary
   PRXs, game files, SDK binaries, generated `runtime/libc.prx`, console dumps,
   keys, or credentials.
5. Include the firmware and loader context for platform-specific behavioral
   claims.
6. Name release tags with the exact `sce_sys/param.json` `contentVersion`
   (`NN.NNN.NNN`, without a `v` prefix).

Every comment-capable project file must start with this header (adapt the
comment syntax and the one-line description):

```cpp
// ps5-homebrew-ui - One-line description of the file.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
```

Files inherited from `ps5-native-app-boilerplate` keep their original header.
Vendored upstream code under `src/third_party/` and `third_party/` keeps its
upstream license header and is listed in `THIRD_PARTY_NOTICES.md`; local changes to it live
as patches under `patches/`.

Changes to `tooling/native/` must include a deterministic host check and a
narrowly scoped static-format regression. Loader-visible changes also require
hardware results before release.
