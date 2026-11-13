#!/usr/bin/env bash
# Optional heap allocation profiling; requires preinstalled heaptrack.
# No package installation or privileged/prod environment changes.
set -Eeuo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if ! command -v heaptrack >/dev/null 2>&1; then
    printf 'heaptrack is not installed. Optional allocation profiling is unavailable; no installation attempted.\n' >&2
    exit 2
fi
# Ensure the unsanitized Release benchmark exists, then profile the same
# executable. Heaptrack timings must never be compared with Release timings.
"$root/tools/benchmark_monitoring_load.sh" 1 >/dev/null
build_dir="${VEX_MONITORING_LOAD_BUILD_DIR:-${root}/build/monitoring-load-release}"
out_dir="${VEX_MONITORING_HEAPTRACK_DIR:-${root}/build/monitoring-heaptrack}"
mkdir -p "$out_dir"
printf 'MONITORING_HEAPTRACK benchmark=%s output=%s\n' \
    "$build_dir/modules/monitoring/vex_monitoring_load_benchmark" "$out_dir" >&2
heaptrack -o "$out_dir/monitoring.heaptrack.gz" \
    "$build_dir/modules/monitoring/vex_monitoring_load_benchmark" 1 \
    > "$out_dir/profiled-load.csv"
printf 'MONITORING_HEAPTRACK_DONE profile=%s\n' \
    "$out_dir/monitoring.heaptrack.gz" >&2
printf 'The instrumented CSV is NOT suitable for release latency comparisons.\n' >&2
