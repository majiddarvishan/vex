#!/usr/bin/env bash
# Verification of only the Monitoring module (other behavior remains frozen).
set -Eeuo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mode="${1:-threaded}"
case "${mode}" in
    baseline)
        flags=""
        ;;
    threaded)
        flags=""
        ;;
    stress)
        flags=""
        ;;
    asan)
        flags="-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1"
        ;;
    tsan)
        flags="-fsanitize=thread -fno-omit-frame-pointer -g -O1"
        ;;
    *)
        echo "Usage: tools/verify_monitoring.sh [baseline|threaded|stress|asan|tsan]" >&2
        exit 2
        ;;
esac

build_dir="${VEX_BUILD_DIR:-${root}/build/monitoring-${mode}}"
threading=OFF
[[ "${mode}" == "baseline" ]] || threading=ON

cmake -S "${root}" -B "${build_dir}" \
    -DBUILD_LOGGER=OFF \
    -DBUILD_THREADPOOL=OFF \
    -DBUILD_OBJECT_POOL=OFF \
    -DBUILD_MONITORING=ON \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_TESTING=ON \
    -DENABLE_METRICS_THREADING="${threading}" \
    -DVEX_ALLOW_DEPENDENCY_FETCH="${VEX_ALLOW_DEPENDENCY_FETCH:-ON}" \
    "-DCMAKE_CXX_FLAGS=${flags}" \
    "-DCMAKE_EXE_LINKER_FLAGS=${flags}"
cmake --build "${build_dir}" --parallel "${VEX_JOBS:-2}"
# The external package consumer has intentionally no sanitizer link flags.
# Its install/package gate is covered by verify_foundation.sh full.
if ! ctest --test-dir "${build_dir}" --output-on-failure -R '^vex[.]monitoring[.]'; then
    if [[ "${mode}" == "tsan" ]]; then
        printf 'TSan failed. If the log says "unexpected memory mapping",\n' >&2
        printf 'run ./tools/diagnose_tsan.sh 10 to test TSan runtime without Vex.\n' >&2
        printf 'Do not ignore actual ThreadSanitizer data-race reports.\n' >&2
    fi
    exit 1
fi
if [[ "$mode" == "stress" ]]; then
    printf 'MONITORING STRESS: 100 repeat runs per monitoring test, fail fast\n'
    ctest --test-dir "$build_dir" --output-on-failure \
        --repeat until-fail:100 -R '^vex[.]monitoring[.](smoke|contract)$'
    printf 'VEX MONITORING STRESS PASS repetitions=100 each_test=smoke,contract\n'
else
    printf 'VEX MONITORING PASS mode=%s\n' "$mode"
fi
