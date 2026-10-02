# Third-party notices

## Credits and acknowledgements

ps5-homebrew-ui exists thanks to the maintainers and contributors of:

- [ps5-opengl](https://github.com/blackbearreloaded/ps5-opengl), with
  [Mesa](https://mesa3d.org/) and OpenGNM PSBC, for OpenGL 4.6 on PS5;
- [PS5 Native App Boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate)
  and the [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk) for the
  reproducible native foundation;
- [Inter](https://github.com/rsms/inter),
  [Montserrat](https://github.com/JulietaUla/Montserrat),
  [DejaVu](https://dejavu-fonts.github.io/),
  [Press Start 2P](https://fonts.google.com/specimen/Press+Start+2P) and
  [Patrick Hand](https://fonts.google.com/specimen/Patrick+Hand) for the
  interface typefaces, and
  [stb](https://github.com/nothings/stb) for music decoding, font baking and
  PNG writing;
- [MkPFS](https://github.com/PSBrew/MkPFS),
  [UFS2Tool](https://github.com/SvenGDK/UFS2Tool), LLVM/Clang, Python, zlib and
  GoogleTest for build, packaging and validation tooling;
- Inigo Quilez, whose published signed-distance functions the shape shader
  follows (rounded box, triangle, star and arc).

The rendering kit, the sound mixer and the sound effects come from
[ProsperoPuzzles](https://github.com/blackbearreloaded/ProsperoPuzzles) and
ProsperoEden, both by BlackBearReloaded.

ps5-homebrew-ui is Copyright (C) 2026 BlackBearReloaded and licensed under
GPL-3.0-or-later. It is built on `ps5-native-app-boilerplate`; the notices
below cover the boilerplate's dependencies and every third-party component the
project adds.

## Fonts and font baking

| Typeface | Files | Copyright | Licence |
| --- | --- | --- | --- |
| Inter Regular, SemiBold | `third_party/fonts/Inter-*.ttf` | (c) 2016 The Inter Project Authors | SIL Open Font License 1.1 (`Inter-LICENSE.txt`) |
| Montserrat Medium | `third_party/fonts/Montserrat-Medium.ttf` | (c) 2011 The Montserrat Project Authors | SIL Open Font License 1.1 (`Montserrat-LICENSE.txt`) |
| DejaVu Sans Mono | `third_party/fonts/DejaVuSansMono.ttf` | (c) 2003 Bitstream, Inc.; DejaVu changes are public domain | Bitstream Vera licence (`DejaVu-LICENSE.txt`) |
| Press Start 2P | `third_party/fonts/PressStart2P-Regular.ttf` | (c) 2012 The Press Start 2P Project Authors | SIL Open Font License 1.1 (`PressStart2P-LICENSE.txt`) |
| Patrick Hand | `third_party/fonts/PatrickHand-Regular.ttf` | (c) 2010-2012 Patrick Wagesreiter | SIL Open Font License 1.1 (`PatrickHand-LICENSE.txt`) |

`assets/fonts/*.huifont` are distance-field renderings of these typefaces
produced by `tools/bake-fonts.sh`; the licence texts ship beside them.

The baker uses [stb_truetype](https://github.com/nothings/stb)
(`third_party/stb/stb_truetype.h`, public domain or MIT) and the host snapshot
tool uses stb_image_write (`third_party/stb/stb_image_write.h`, same terms).
Both are host-only and are not linked into the PS5 application.

## Sound effects and music

`assets/audio/sfx/paper/*.wav` are the sound effects of ProsperoPuzzles and
`assets/audio/sfx/glass/*.wav` those of ProsperoEden. They were generated for
those projects with ElevenLabs Sound Effects v2 and prepared with
`tools/process-sfx.py` (trimmed, faded and levelled). No sound effect was
generated for this repository. `assets/audio/music/*.ogg` ("First Light",
"Open Strings" and "Quiet Hours") are the project's own background music,
supplied by its owner and encoded to 48 kHz OGG Vorbis at -18 LUFS. All are
Copyright (C) 2026 BlackBearReloaded and distributed under the project licence.

## Music decoding

Music is decoded by [stb_vorbis](https://github.com/nothings/stb) (public
domain or MIT; see `src/third_party/stb/LICENSE`), vendored at the commit in
`src/third_party/stb/UPSTREAM` by `tools/update-stb.sh`. It is linked into the
PS5 application.

## Shader distance functions

The fragment shader in `src/gfx/gl_batch.cpp` adapts the two-dimensional
signed-distance functions published by Inigo Quilez
(https://iquilezles.org/articles/distfunctions2d/), which he releases under
the MIT licence.

## OpenGL runtime

The PS5 build statically links the
[ps5-opengl](https://github.com/blackbearreloaded/ps5-opengl) SDK
(GPL-3.0-or-later), which contains Mesa (MIT) and OpenGNM PSBC components under
their own licenses. The SDK is fetched or selected at build time and is not
stored in this repository; its license texts ship inside the SDK archive.

## Native build dependencies

The application build uses LLVM/Clang/lld, zlib 1.3.2, and the public
[PS5 payload SDK](https://github.com/ps5-payload-dev/sdk). The bootstrapper
downloads SDK v0.42 after verifying SHA-256
`8cfbc7cd5811e719eb4f0c47eea668d3dc7b40bc8ab11c4a5031d40c23ec02da`.
It downloads zlib 1.3.2 from the upstream source archive after verifying
SHA-256 `bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16`
and compiles its static archive locally. Both dependencies remain under ignored
`.deps/native/`, retain their upstream licenses, and are not distributed by
this repository. No Sony SDK file is included.

Target C++ compilation uses the LLVM libc++ headers distributed by the public
SDK. Those headers retain the Apache-2.0 WITH LLVM-exception license recorded
upstream. The application does not redistribute or dynamically load the
complete libc++ or libc++abi archives.

The PS5 ELF converter and FSELF writer in `tooling/native/` are derived from
[SharpProspero](https://github.com/SvenGDK/SharpProspero), Copyright (C) 2026
SvenGDK, GPL-3.0, and were translated to C++ and modified by BlackBearReloaded.

## Host test dependency

The host unit-test target downloads
[GoogleTest](https://github.com/google/googletest) 1.17.0 after verifying
SHA-256 `65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c`.
It remains under ignored `.deps/test/`, retains its BSD-3-Clause license, and
is not linked into any PS5 application, runtime, or package artifact.

## Optional PacBrew dependencies

When selected through `PACBREW_*` build variables, the build downloads the prebuilt ports image
from [ps5-payload-dev/pacbrew-repo](https://github.com/ps5-payload-dev/pacbrew-repo)
release `v0.40.2`, verifies its published SHA-256, and extracts only the
`target/user/homebrew` prefix under ignored `.deps/pacbrew/`. It does not
replace the pinned SDK or install files globally. PacBrew recipes and every
linked third-party library retain their upstream licenses; applications must
review those terms before redistribution.

## Optional UFS2Tool dependency

When `.ffpkg` output is requested, the platform bootstrapper fetches
[SvenGDK/UFS2Tool](https://github.com/SvenGDK/UFS2Tool) at commit
`b5307a60d5b4e3a68ba680e0e33cfadf05017c77` into the ignored
`.deps/UFS2Tool` cache and builds it with the host .NET SDK. UFS2Tool is
BSD-2-Clause software and is not distributed by this repository.

## Optional MkPFS dependency

When `.ffpfsc` output is requested, the platform bootstrapper fetches
[PSBrew/MkPFS](https://github.com/PSBrew/MkPFS) at commit
`6cb8313dfe0c988ac52617794553f343243d3a56` into the ignored `.deps/MkPFS`
cache and installs its Python dependencies into an ignored virtual environment
there. MkPFS and its dependencies retain their own licenses and are not
distributed by this repository.

## Independently authored runtime shim

`tooling/native/libc_builder.cpp` and the manifests under
`tooling/native/runtime/` are independently authored for this project and
licensed under GPL-3.0-or-later. The generated `runtime/libc.prx` contains
project-authored compatibility stubs, startup code, and semantic loader
metadata. It contains no Sony runtime implementation.

Original ps5-native-app-boilerplate code is Copyright (C) 2026
BlackBearReloaded and licensed under GPL-3.0-or-later. Source and script files
carry matching SPDX identifiers.

## Presentation assets

`sce_sys/icon0.png`, `sce_sys/pic0.dds` (shown while the title is selected) and
`sce_sys/pic1.dds` (shown while it starts) are the project's own artwork,
supplied by its owner; `background-source.png` and
`launch-background-source.png` are the pictures the two backgrounds were made
from. `sce_sys/snd0.at9`, the music the console plays while the title is
selected, is the owner's own track, encoded with
[ps5-at9-converter](https://github.com/blackbearreloaded/ps5-at9-converter)
(57.6 s, 192 kb/s, -28 LUFS, whole-track loop).
`HUI_ICON=<file> tools/host-snapshots.sh` still renders a plain icon
with the project's own renderer, for forks that want one of their own. The
cover art inside the app is rendered at start-up from invented titles; no real
product's name, artwork or trademark is used.

No proprietary runtime module, encryption key, or game file is included.
