#!/usr/bin/env bash
# Opt-in Monitoring-only clang-tidy with a genuine warning-as-error gate.
# No source edits, automatic fixes, installations, or non-Monitoring targets.
set -Eeuo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mode="${1:-run}"
case "$mode" in
    run|check-tools) ;;
    *) printf 'Usage: tools/verify_monitoring_static.sh [run|check-tools]\n' >&2; exit 2 ;;
esac

clang_tidy="${VEX_CLANG_TIDY:-}"
if [[ -z "$clang_tidy" ]]; then
    for command_name in clang-tidy-20 clang-tidy-19 clang-tidy-18 clang-tidy; do
        if command -v "$command_name" >/dev/null 2>&1; then
            clang_tidy="$(command -v "$command_name")"
            break
        fi
    done
fi
if [[ -z "$clang_tidy" || ! -x "$clang_tidy" ]]; then
    printf 'MONITORING_STATIC_NOT_RUN clang-tidy missing. Install/configure it separately or set VEX_CLANG_TIDY.\n' >&2
    exit 2
fi
printf 'MONITORING_STATIC_TOOL %s\n' "$("$clang_tidy" --version | head -n 1)"
if [[ "$mode" == "check-tools" ]]; then
    printf 'MONITORING_STATIC_CHECK_TOOLS PASS\n'
    exit 0
fi

build_dir="${VEX_MONITORING_STATIC_BUILD_DIR:-${root}/build/monitoring-portability-clang-static}"
if [[ ! -f "$build_dir/compile_commands.json" ]]; then
    printf 'MONITORING_STATIC_PREPARE no compilation database; building clang-static package/CTest prerequisites\n' >&2
    "$root/tools/verify_monitoring_portability.sh" clang-static
fi
if [[ ! -f "$build_dir/compile_commands.json" ]]; then
    printf 'MONITORING_STATIC_FAIL missing compile_commands.json in %s\n' "$build_dir" >&2
    exit 1
fi

# Only relevant C++ sources and their Vex Monitoring public headers.
# Avoid stylistic naming/format checks and unrelated third-party code.
checks="-*,clang-analyzer-*,bugprone-*,performance-*"
printf 'MONITORING_STATIC_CHECKS %s\n' "$checks"
printf 'MONITORING_STATIC_BUILD_DIR %s\n' "$build_dir"
count=0
failed=0
for source in "$root"/src/monitoring/*.cpp; do
    [[ -f "$source" ]] || continue
    count=$((count + 1))
    printf 'MONITORING_STATIC_FILE %s\n' "$source"
    if ! "$clang_tidy" \
        --quiet \
        --warnings-as-errors='*' \
        "--checks=$checks" \
        "--header-filter=^$root/include/vex/monitoring/" \
        -p "$build_dir" "$source"; then
        failed=$((failed + 1))
        printf 'MONITORING_STATIC_FAIL source=%s\n' "$source" >&2
    fi
done
if [[ $count -eq 0 || $failed -gt 0 ]]; then
    printf 'MONITORING_STATIC_SUMMARY checked=%s failed=%s\n' "$count" "$failed" >&2
    exit 1
fi
printf 'MONITORING_STATIC_PASS checked=%s failed=0\n' "$count"
