#!/usr/bin/env bash
# ps5-native-app-boilerplate - Shared incremental compilation rules.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Generates Ninja edges from shell argument arrays without losing quoting.

ninja_escape() {
    local value=$1
    value=${value//\$/\$\$}
    value=${value// /\$ }
    value=${value//:/\$:}
    printf '%s' "$value"
}

ninja_begin() {
    command -v ninja >/dev/null || { echo 'Install ninja-build first' >&2; exit 2; }
    jobs=${BUILD_JOBS:-$(nproc 2>/dev/null || printf '2')}
    [[ $jobs =~ ^[1-9][0-9]*$ ]] || { echo 'BUILD_JOBS must be positive' >&2; exit 2; }
    compiler_cache=()
    if [[ ${USE_CCACHE:-1} != 0 ]]; then
        command -v ccache >/dev/null || { echo 'Install ccache or set USE_CCACHE=0' >&2; exit 2; }
        compiler_cache=(ccache)
    fi
    ninja_file=$1
    mkdir -p "$(dirname "$ninja_file")"
    printf 'builddir = %s\n' "$(ninja_escape "$(dirname "$ninja_file")")" >"$ninja_file"
    ninja_index=0
}

# Set ninja_inputs before each edge; compile commands must emit OUTPUT.d.
ninja_edge() {
    local kind=$1 output=$2 command input
    shift 2
    printf -v command '%q ' "$@"
    command=${command//\$/\$\$}
    ((ninja_index += 1))
    {
        printf 'rule step%s\n  command = %s\n' "$ninja_index" "$command"
        printf '  description = %s %s\n' "$kind" "$(ninja_escape "${output##*/}")"
        if [[ $kind == CXX || $kind == CC ]]; then
            printf '  depfile = %s.d\n  deps = gcc\n' "$(ninja_escape "$output")"
        fi
        printf 'build %s: step%s' "$(ninja_escape "$output")" "$ninja_index"
        for input in "${ninja_inputs[@]}"; do
            printf ' %s' "$(ninja_escape "$input")"
        done
        printf '\n'
    } >>"$ninja_file"
}

ninja_run() {
    printf '==> [ninja] %s jobs; compiler cache: %s\n' "$jobs" "${compiler_cache[*]:-disabled}"
    ninja -f "$ninja_file" -j "$jobs" "$@"
}
