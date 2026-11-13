#!/usr/bin/env bash
# Opt-in, bounded Linux Registry-churn benchmark. CSV only on stdout.
set -Eeuo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
rounds="${1:-8}"
batch="${2:-512}"
mode="${3:-managed}"
if [[ $# -gt 3 || ! "$rounds" =~ ^[0-9]+$ ||
      ! "$batch" =~ ^[0-9]+$ ||
      $rounds -lt 1 || $rounds -gt 20 ||
      $batch -lt 256 || $batch -gt 2048 ||
      ( "$mode" != managed && "$mode" != raw ) ]]; then
    printf 'Usage: tools/benchmark_monitoring_lifetime.sh [rounds=1..20] [registries_per_round=256..2048] [managed|raw]\n' >&2
    exit 2
fi
if [[ "$(uname -s)" != Linux ]]; then
    printf 'This benchmark requires Linux.\n' >&2
    exit 2
fi
build_dir="${VEX_MONITORING_LIFETIME_BUILD_DIR:-${root}/build/monitoring-lifetime-release}"
cmake -S "$root" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_LOGGER=OFF -DBUILD_MONITORING=ON \
    -DBUILD_THREADPOOL=OFF -DBUILD_OBJECT_POOL=OFF \
    -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF \
    -DENABLE_METRICS_THREADING=ON \
    -DVEX_BUILD_MONITORING_BENCHMARK=ON \
    "-DVEX_ALLOW_DEPENDENCY_FETCH=${VEX_ALLOW_DEPENDENCY_FETCH:-ON}" >&2
if grep -Eq '^CMAKE_CXX_FLAGS:STRING=.*-fsanitize=' "$build_dir/CMakeCache.txt"; then
    printf 'Sanitizer flags detected in Release benchmark cache.\n' >&2
    exit 2
fi
cmake --build "$build_dir" --target vex_monitoring_lifetime_benchmark \
    --parallel "${VEX_JOBS:-2}" >&2
printf 'BENCH_LIFETIME_ENV time=%s kernel=%s git_head=%s\n' \
    "$(date -u '+%Y-%m-%dT%H:%M:%SZ')" \
    "$(uname -sr)" \
    "$(git -C "$root" rev-parse HEAD 2>/dev/null || echo unknown)" >&2
printf 'BENCH_LIFETIME_COMPILER ' >&2
grep '^CMAKE_CXX_COMPILER:FILEPATH=' "$build_dir/CMakeCache.txt" >&2 || true
printf 'BENCH_LIFETIME_NOTE RSS, VmHWM and glibc heap are process snapshots; they cannot prove leak freedom.\n' >&2
printf 'BENCH_LIFETIME_EXPECTED_CSV_LINES %s\n' "$((rounds + 1))" >&2
printf 'BENCH_LIFETIME_MODE %s\n' "$mode" >&2
if [[ "$mode" == raw ]]; then
    "$build_dir/modules/monitoring/vex_monitoring_lifetime_benchmark" \
        "$rounds" "$batch" raw
else
    "$build_dir/modules/monitoring/vex_monitoring_lifetime_benchmark" \
        "$rounds" "$batch"
fi
