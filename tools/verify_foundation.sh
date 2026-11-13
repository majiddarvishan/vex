#!/usr/bin/env bash
# Non-destructive out-of-tree CMake/CTest verification for Vex.
set -Eeuo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mode="${1:-thread}"
if [[ "${mode}" == "--help" || "${mode}" == "-h" ]]; then
    printf 'Usage: tools/verify_foundation.sh [thread|full]\n'
    printf '  thread: offline ThreadPool + installed-package consumer smoke\n'
    printf '  full: all supported modules; requires prometheus-cpp or network\n'
    printf 'Optional: VEX_BUILD_DIR=path, VEX_JOBS=N, VEX_ALLOW_DEPENDENCY_FETCH=ON|OFF\n'
    exit 0
fi

build_dir="${VEX_BUILD_DIR:-${root}/build/foundation-${mode}}"
common=(-DBUILD_TESTING=ON -DBUILD_LOGGER=OFF)
case "${mode}" in
    thread)
        flags=(
            -DBUILD_THREADPOOL=ON
            -DBUILD_OBJECT_POOL=OFF
            -DBUILD_MONITORING=OFF
            -DBUILD_EXAMPLES=OFF
            -DVEX_ALLOW_DEPENDENCY_FETCH=OFF
        )
        ;;
    full)
        flags=(
            -DBUILD_THREADPOOL=ON
            -DBUILD_OBJECT_POOL=ON
            -DBUILD_MONITORING=ON
            -DBUILD_EXAMPLES=ON
            "-DVEX_ALLOW_DEPENDENCY_FETCH=${VEX_ALLOW_DEPENDENCY_FETCH:-ON}"
        )
        ;;
    *)
        printf 'Unknown mode: %s\n' "${mode}" >&2
        exit 2
        ;;
esac

printf 'VEX FOUNDATION mode=%s source=%s build=%s\n' "${mode}" "${root}" "${build_dir}"
cmake -S "${root}" -B "${build_dir}" "${common[@]}" "${flags[@]}"
cmake --build "${build_dir}" --parallel "${VEX_JOBS:-2}"
ctest --test-dir "${build_dir}" --output-on-failure
printf 'VEX FOUNDATION PASS mode=%s\n' "${mode}"
