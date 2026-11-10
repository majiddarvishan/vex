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
    env -i PATH="$temp" "$(type -P bash)" "$finder"
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
assert_eq "$(CXX=clang++-18 run)" "$temp/clang++-18"
assert_eq "$(CXX="$temp/clang++-20" run)" "$temp/clang++-20"

printf '#!/bin/sh\nexit 0\n' > "$temp/clang++"
chmod +x "$temp/clang++"
assert_eq "$(run)" "$temp/clang++"
assert_eq "$(CXX=clang++-20 run)" "$temp/clang++-20"
if CXX=clang++-42 run >/dev/null 2>&1; then
    echo 'Unexpected success for a missing explicit Clang version' >&2
    exit 1
fi
echo 'VEX CLANG COMPILER RESOLVER PASS'
