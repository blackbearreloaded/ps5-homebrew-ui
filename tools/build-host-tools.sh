#!/usr/bin/env bash
# ps5-native-app-boilerplate - Shared host tools built with Ninja and ccache.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$root/tools/ninja-build.sh"
native="$root/tooling/native"
build="$root/build/host"
zlib_root="$root/.deps/native/zlib/root"
zlib_archive=$(find "$zlib_root" -type f -name libz.a -print -quit)
if [[ -n ${CXX:-} ]]; then
    cxx=$(command -v "$CXX")
else
    cxx=$(command -v clang++-18 || command -v clang++)
fi
ninja_begin "$build/build.ninja"
objects=()
for name in native_app_builder self_container elf_object sce_module_writer libc_builder; do
    object="$build/$name.o"
    ninja_inputs=("$native/$name.cpp" "$cxx")
    ninja_edge CXX "$object" "${compiler_cache[@]}" "$cxx" \
        -std=c++20 -O2 -Wall -Wextra -Werror -I "$zlib_root/usr/include" \
        -MD -MF "$object.d" -c "$native/$name.cpp" -o "$object"
    [[ $name == libc_builder ]] || objects+=("$object")
done
ninja_inputs=("${objects[@]}" "$zlib_archive" "$cxx")
ninja_edge LINK "$build/ps5-native-tool" "$cxx" "${objects[@]}" "$zlib_archive" -o "$build/ps5-native-tool"
ninja_inputs=("$build/libc_builder.o" "$cxx")
ninja_edge LINK "$build/libc-builder" "$cxx" "$build/libc_builder.o" -o "$build/libc-builder"
ninja_run
