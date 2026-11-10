#!/usr/bin/env bash
# Contract test for tools/find_clangxx.sh. No installed Clang required.
set -Eeuo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
finder="$root/tools/find_clangxx.sh"
temp="$(mktemp -d)"
trap 'rm -rf -- "$temp"' EXIT

# Restrict PATH to exercise the fallback regardless of installed compilers.
ln -s "$(type -P bash)" "$temp/bash"
ln -s "$(type -P dirname)" "$temp/dirname"
ln -s "$(type -P basename)" "$temp/basename"
run() {
    # Do not inherit CXX from the caller: tests of *automatic* selection
    # must remain hermetic, including when CTest is launched with CXX set.
    local override="${1:-}"
    if [[ -n "$override" ]]; then
        env -i PATH="$temp" CXX="$override" "$(type -P bash)" "$finder"
    else
        env -i PATH="$temp" "$(type -P bash)" "$finder"
    fi
}
assert_eq() {
    if [[ "$1" != "$2" ]]; then
        printf 'Compiler resolver: expected=%s actual=%s\n' "$2" "$1" >&2
        exit 1
    fi
}
if run >/dev/null 2>&1; then
    echo 'Unexpected success without a Clang executable' >&2
    exit 1
fi
printf '#!/bin/sh\nexit 0\n' > "$temp/clang++-18"
printf '#!/bin/sh\nexit 0\n' > "$temp/clang++-20"
chmod +x "$temp/clang++-18" "$temp/clang++-20"
assert_eq "$(run)" "$temp/clang++-20"
assert_eq "$(run clang++-18)" "$temp/clang++-18"
assert_eq "$(run "$temp/clang++-20")" "$temp/clang++-20"

printf '#!/bin/sh\nexit 0\n' > "$temp/clang++"
chmod +x "$temp/clang++"
assert_eq "$(run)" "$temp/clang++"
assert_eq "$(run clang++-20)" "$temp/clang++-20"
if run clang++-42 >/dev/null 2>&1; then
    echo 'Unexpected success for a missing explicit Clang version' >&2
    exit 1
fi
echo 'VEX CLANG COMPILER RESOLVER PASS'
