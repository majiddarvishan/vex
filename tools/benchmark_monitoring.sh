#!/usr/bin/env bash
# Optional Monitoring microbenchmark (Release mode, no sanitizer flags).
# It does not replace CTest, TSan, ASan, a real load test, or code review.
set -Eeuo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
repeats="${1:-3}"
case "$repeats" in
    1|3|5) ;;
    *)
        printf 'Usage: tools/benchmark_monitoring.sh [1|3|5]\n' >&2
        exit 2
        ;;
esac

build_dir="${VEX_MONITORING_BENCH_BUILD_DIR:-${root}/build/monitoring-bench-release}"
cmake -S "$root" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_LOGGER=OFF \
    -DBUILD_MONITORING=ON \
    -DBUILD_THREADPOOL=OFF \
    -DBUILD_OBJECT_POOL=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_TESTING=OFF \
    -DENABLE_METRICS_THREADING=ON \
    -DVEX_ALLOW_DEPENDENCY_FETCH="${VEX_ALLOW_DEPENDENCY_FETCH:-ON}" \
    -DVEX_BUILD_MONITORING_BENCHMARK=ON

# Refuse an accidentally reused sanitized build cache.
if grep -Eq '^CMAKE_CXX_FLAGS:STRING=.*-fsanitize=' "$build_dir/CMakeCache.txt"; then
    printf 'Sanitizer flags present in benchmark CMake cache; use a clean build directory.\n' >&2
    exit 2
fi

cmake --build "$build_dir" --config Release \
    --target vex_monitoring_benchmark --parallel "${VEX_JOBS:-2}"

printf 'BENCHMARK_ENV date=%s host_kernel=%s cpu_count=%s\n' \
    "$(date -u '+%Y-%m-%dT%H:%M:%SZ')" \
    "$(uname -sr)" "$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo unknown)" >&2
printf 'BENCHMARK_COMPILER ' >&2
if [[ -n "${CXX:-}" ]]; then
    "$CXX" --version | head -n 1 >&2
else
    grep '^CMAKE_CXX_COMPILER:FILEPATH=' "$build_dir/CMakeCache.txt" >&2 || true
fi
printf 'BENCHMARK_NOTE microbenchmark results are machine-specific and not production guarantees\n' >&2

"$build_dir/modules/monitoring/vex_monitoring_benchmark" "$repeats"
