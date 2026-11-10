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
    tsan|tsan-clang)
        flags="-fsanitize=thread -fno-omit-frame-pointer -g -O1"
        ;;
    *)
        echo "Usage: tools/verify_monitoring.sh [baseline|threaded|stress|asan|tsan|tsan-clang]" >&2
        exit 2
        ;;
esac

compiler_args=()
if [[ "${mode}" == "tsan-clang" ]]; then
    clang_compiler="$("${root}/tools/find_clangxx.sh")"
    # Require a real Clang toolchain, even if CXX was explicitly specified.
    compiler_banner="$("$clang_compiler" --version)"
    if [[ "$compiler_banner" != *clang* && "$compiler_banner" != *Clang* ]]; then
        printf 'Requested compiler is not Clang: %s\n' "$clang_compiler" >&2
        exit 2
    fi
    build_dir="${VEX_BUILD_DIR:-${root}/build/monitoring-tsan-$(basename -- "$clang_compiler")}"
    compiler_args=("-DCMAKE_CXX_COMPILER=${clang_compiler}")
    if [[ -f "${build_dir}/CMakeCache.txt" ]]; then
        cached_compiler="$(sed -n 's/^CMAKE_CXX_COMPILER:FILEPATH=//p' "${build_dir}/CMakeCache.txt" | head -n 1)"
        if [[ -n "$cached_compiler" && "$cached_compiler" != "$clang_compiler" ]]; then
            printf 'Compiler mismatch: cached=%s requested=%s\n' "$cached_compiler" "$clang_compiler" >&2
            printf 'Use a fresh VEX_BUILD_DIR rather than reusing a GCC build.\n' >&2
            exit 2
        fi
    fi
    printf 'MONITORING TSAN CLANG compiler=%s build=%s\n' "$clang_compiler" "$build_dir"
else
    build_dir="${VEX_BUILD_DIR:-${root}/build/monitoring-${mode}}"
fi
threading=OFF
[[ "${mode}" == "baseline" ]] || threading=ON

cmake -S "${root}" -B "${build_dir}" \
    "${compiler_args[@]}" \
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
    if [[ "${mode}" == "tsan" || "${mode}" == "tsan-clang" ]]; then
        printf 'TSan failed. If the log says "unexpected memory mapping",\n' >&2
        printf 'run ./tools/diagnose_tsan.sh 10 to test TSan runtime without Vex.\n' >&2
        printf 'Do not ignore actual ThreadSanitizer data-race reports.\n' >&2
    fi
    exit 1
fi
if [[ "$mode" == "stress" ]]; then
    printf 'MONITORING STRESS: 100 repeat runs per monitoring C++ contract, fail fast\n'
    ctest --test-dir "$build_dir" --output-on-failure \
        --repeat until-fail:100 -R '^vex[.]monitoring[.](smoke|contract|decorators|raii|validation|lifecycle|registration|legacy)
    printf 'VEX MONITORING STRESS PASS repetitions=100 each_test=smoke,contract,decorators,raii,validation,lifecycle,registration,legacy\n'
else
    printf 'VEX MONITORING PASS mode=%s\n' "$mode"
fi

    printf 'VEX MONITORING STRESS PASS repetitions=100 each_test=smoke,contract\n'
else
    printf 'VEX MONITORING PASS mode=%s\n' "$mode"
fi
