#!/usr/bin/env bash
# Controlled Release benchmark with warmups and rotated scenario ordering.
# CSV is stdout; build/environment diagnostics stay on stderr.
set -Eeuo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
runs="${1:-15}"
case "$runs" in
  9|15|21) ;;
  *) printf 'Usage: tools/benchmark_monitoring_controlled.sh [9|15|21]\n' >&2; exit 2 ;;
esac
"$root/tools/benchmark_monitoring.sh" 1 >/dev/null
build_dir="${VEX_MONITORING_BENCH_BUILD_DIR:-${root}/build/monitoring-bench-release}"
printf 'BENCH_CONTROLLED warmup=yes ordering=rotated runs=%s expected_csv_rows=%s\n' "$runs" "$((runs * 12))" >&2
"$build_dir/modules/monitoring/vex_monitoring_benchmark" --controlled "$runs"
