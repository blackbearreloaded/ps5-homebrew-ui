// ps5-homebrew-ui - The single compilation unit for the vendored stb_vorbis.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Music is read into memory and decoded from there, so stdio is left out.

#define STB_VORBIS_NO_STDIO 1
#define STB_VORBIS_NO_PUSHDATA_API 1
#ifndef alloca
#define alloca __builtin_alloca
#endif
#include "stb_vorbis.inc"
