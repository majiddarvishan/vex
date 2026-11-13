#!/usr/bin/env bash
# Linux-only, optional, non-destructive Release load benchmark.
# Build and host metadata to stderr; structured CSV only to stdout.
set -Eeuo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
repeats="${1:-3}"
mode="${2:-}"
if [[ $# -gt 2 ]]; then
    printf 'Usage: tools/benchmark_monitoring_load.sh [1|3|5] [--controls]\n' >&2
    exit 2
fi
case "$mode" in
    ""|--controls) ;;
    *) printf 'Usage: tools/benchmark_monitoring_load.sh [1|3|5] [--controls]\n' >&2; exit 2 ;;
esac
case "$repeats" in
    1|3|5) ;;
    *) printf 'Usage: tools/benchmark_monitoring_load.sh [1|3|5]\n' >&2; exit 2 ;;
esac
if [[ "$(uname -s)" != Linux ]]; then
    printf 'This benchmark currently supports Linux only.\n' >&2
    exit 2
fi
build_dir="${VEX_MONITORING_LOAD_BUILD_DIR:-${root}/build/monitoring-load-release}"
cmake -S "$root" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_LOGGER=OFF -DBUILD_MONITORING=ON \
    -DBUILD_THREADPOOL=OFF -DBUILD_OBJECT_POOL=OFF \
    -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF \
    -DENABLE_METRICS_THREADING=ON \
    -DVEX_ALLOW_DEPENDENCY_FETCH="${VEX_ALLOW_DEPENDENCY_FETCH:-ON}" \
    -DVEX_BUILD_MONITORING_BENCHMARK=ON >&2
if grep -Eq '^CMAKE_CXX_FLAGS:STRING=.*-fsanitize=' "$build_dir/CMakeCache.txt"; then
    printf 'Sanitizer flags in benchmark cache; select a clean build directory.\n' >&2
    exit 2
fi
cmake --build "$build_dir" --config Release \
    --target vex_monitoring_load_benchmark --parallel "${VEX_JOBS:-2}" >&2
printf 'BENCH_LOAD_ENV date=%s kernel=%s cpu_count=%s branch=%s\n' \
    "$(date -u '+%Y-%m-%dT%H:%M:%SZ')" \
    "$(uname -sr)" \
    "$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo unknown)" \
    "$(git -C "$root" branch --show-current 2>/dev/null || echo unknown)" >&2
printf 'BENCH_LOAD_COMPILER ' >&2
grep '^CMAKE_CXX_COMPILER:FILEPATH=' "$build_dir/CMakeCache.txt" >&2 || true
printf 'BENCH_LOAD_NOTE RSS/heap are process snapshots, not allocation counts. Percentiles apply to Collect/HTTP only and use microseconds.\n' >&2
printf 'BENCH_LOAD_NOTE loopback-only and OS-assigned port; do not execute alongside production load.\n' >&2
args=("$repeats")
if [[ "$mode" == "--controls" ]]; then
    args+=("--controls")
    printf 'BENCH_LOAD_CONTROLS raw_vs_managed=yes shared_vs_sharded=yes expected_csv_lines=%s\n' "$((1 + repeats * 20))" >&2
fi
"$build_dir/modules/monitoring/vex_monitoring_load_benchmark" "${args[@]}"
