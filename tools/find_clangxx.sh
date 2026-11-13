#!/usr/bin/env bash
# Discover Clang's C++ compiler; versioned installations need no global symlink.
# Explicit CXX wins; otherwise prefer clang++, then highest clang++-N in PATH.
# Print only the absolute executable path to stdout.
set -Eeuo pipefail

resolve_executable() {
    local requested="$1" executable directory filename
    if [[ "$requested" == */* ]]; then
        executable="$requested"
    else
        executable="$(type -P -- "$requested" || true)"
    fi
    if [[ -z "$executable" || ! -f "$executable" || ! -x "$executable" ]]; then
        printf 'Clang resolver: executable not found: %s\n' "$requested" >&2
        return 1
    fi
    directory="$(dirname -- "$executable")"
    filename="$(basename -- "$executable")"
    printf '%s/%s\n' "$(cd -- "$directory" && pwd -P)" "$filename"
}

if [[ -n "${CXX:-}" ]]; then
    resolve_executable "$CXX"
    exit
fi

if type -P -- clang++ >/dev/null 2>&1; then
    resolve_executable clang++
    exit
fi

best_version=-1
best_path=""
IFS=':' read -r -a path_entries <<< "${PATH:-}"
for directory in "${path_entries[@]}"; do
    [[ -n "$directory" ]] || directory="."
    for candidate in "$directory"/clang++-[0-9]*; do
        [[ -f "$candidate" && -x "$candidate" ]] || continue
        name="${candidate##*/}"
        [[ "$name" =~ ^clang\+\+-([0-9]+)$ ]] || continue
        version=$((10#${BASH_REMATCH[1]}))
        if (( version > best_version )); then
            best_path="$candidate"
            best_version=$version
        fi
    done
done

if [[ -z "$best_path" ]]; then
    printf '%s\n' 'Clang resolver: no clang++ or clang++-N executable found in PATH.' >&2
    printf '%s\n' 'Set CXX explicitly or install a supported Clang compiler.' >&2
    exit 127
fi
resolve_executable "$best_path"
