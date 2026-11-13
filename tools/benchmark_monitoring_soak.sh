#!/usr/bin/env bash
# Linux Monitoring-only, time-bounded soak; unique CSV/log per invocation.
set -Eeuo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
duration="${1:-300}"
mode="${2:-managed}"
interval="${3:-10}"
warmup="${4:-30}"
rate="${5:-2000}"
out_dir="${6:-$root/build/monitoring-soak-results}"
if [[ $# -gt 6 || ! "$duration" =~ ^[1-9][0-9]*$ ||
      ! "$interval" =~ ^[1-9][0-9]*$ ||
      ! "$warmup" =~ ^(0|[1-9][0-9]*)$ ||
      ! "$rate" =~ ^[1-9][0-9]*$ ||
      ( "$mode" != managed && "$mode" != raw ) ||
      $duration -lt 2 || $duration -gt 3600 ||
      $interval -gt $duration || $warmup -ge $duration ||
      $rate -gt 20000 ]]; then
    printf 'Usage: %s [duration=300] [managed|raw] [sample_seconds=10] [warmup_seconds=30] [target_ops_per_sec=2000] [output_dir]\n' "$0" >&2
    exit 2
fi
if [[ "$(uname -s)" != Linux ]]; then
    printf 'Monitoring soak requires Linux /proc.\n' >&2
    exit 2
fi
build_dir="${VEX_MONITORING_SOAK_BUILD_DIR:-$root/build/monitoring-soak-release}"
cmake -S "$root" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_LOGGER=OFF -DBUILD_MONITORING=ON \
    -DBUILD_THREADPOOL=OFF -DBUILD_OBJECT_POOL=OFF \
    -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF \
    -DENABLE_METRICS_THREADING=ON \
    -DVEX_BUILD_MONITORING_BENCHMARK=ON \
    -DVEX_ALLOW_DEPENDENCY_FETCH=OFF >&2
if grep -Eq '^CMAKE_CXX_FLAGS:STRING=.*-fsanitize=' "$build_dir/CMakeCache.txt"; then
    printf 'Sanitizer flags detected in Release benchmark cache.\n' >&2
    exit 2
fi
cmake --build "$build_dir" --target vex_monitoring_soak_benchmark \
    --parallel "${VEX_JOBS:-2}" >&2
bin="$build_dir/modules/monitoring/vex_monitoring_soak_benchmark"
mkdir -p "$out_dir"
partial="$(mktemp "$out_dir/monitoring-soak-$mode-XXXXXX.partial")"
csv="${partial%.partial}.csv"
log="${partial%.partial}.log"
trap 'rm -f "$partial"' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
export VEX_SOAK_RUN_ID="$(basename "${csv%.csv}")"
export VEX_SOAK_STARTED_UTC="$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
export VEX_SOAK_GIT_SHA="$(git -C "$root" rev-parse HEAD 2>/dev/null || printf unknown)"
export VEX_SOAK_KERNEL="$(uname -sr)"
export VEX_SOAK_COMPILER="$(sed -n 's/^CMAKE_CXX_COMPILER:FILEPATH=//p' "$build_dir/CMakeCache.txt" | head -n 1)"
export VEX_SOAK_BUILD_TYPE="Release"
{
    printf 'run_id=%s\nsource_sha=%s\nmode=%s\nstarted_utc=%s\nkernel=%s\ncompiler=%s\n' \
        "$VEX_SOAK_RUN_ID" "$VEX_SOAK_GIT_SHA" "$mode" \
        "$VEX_SOAK_STARTED_UTC" "$VEX_SOAK_KERNEL" "$VEX_SOAK_COMPILER"
    printf 'duration=%s interval=%s warmup=%s target_ops_per_second=%s\n' \
        "$duration" "$interval" "$warmup" "$rate"
    printf 'RSS/HWM/glibc uordblks are snapshots, not a leak proof.\n'
    printf 'Raw bypasses manager quotas and has only a synthetic sweep phase.\n'
} > "$log"
if "$bin" "$duration" "$mode" "$interval" "$warmup" "$rate" \
    > "$partial" 2>> "$log"; then
    mv -- "$partial" "$csv"
    printf 'MONITORING_SOAK_OUTPUT csv=%s log=%s\n' "$csv" "$log" >&2
else
    printf 'MONITORING_SOAK_FAIL log=%s (partial CSV removed)\n' "$log" >&2
    exit 1
fi
