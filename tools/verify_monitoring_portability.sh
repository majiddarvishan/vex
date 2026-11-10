#!/usr/bin/env bash
# Independent Monitoring-only compiler/linkage/package matrix.
# Continue after a failed mode, but exit nonzero if ANY selected mode fails.
# No system package installation and no modifications to /usr/local.
set -Eeuo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mode="${1:-all}"
case "$mode" in
    all) modes=(gcc-static gcc-shared clang-static clang-shared) ;;
    gcc-static|gcc-shared|clang-static|clang-shared) modes=("$mode") ;;
    *)
        printf 'Usage: tools/verify_monitoring_portability.sh [all|gcc-static|gcc-shared|clang-static|clang-shared]\n' >&2
        exit 2
        ;;
esac

# Shared mode needs POSITION-INDEPENDENT dependency archives. Existing
# /usr/local/lib/libprometheus-cpp-*.a may have been built without -fPIC.
# By default, bypass installed prometheus-cpp for shared builds and use
# the project's pinned CPM dependency in the isolated build tree.
# For a known-good PIC-installed dependency, explicitly select "installed".
shared_dependency="${VEX_PORTABILITY_SHARED_DEPENDENCY:-fetch}"
case "$shared_dependency" in
    fetch|installed) ;;
    *)
        printf 'VEX_PORTABILITY_SHARED_DEPENDENCY must be fetch or installed.\n' >&2
        exit 2
        ;;
esac

build_root="${VEX_PORTABILITY_BUILD_ROOT:-${root}/build}"
passed=()
failed=()

for selected in "${modes[@]}"; do
    printf '\n===== MONITORING PORTABILITY %s =====\n' "$selected"
    toolchain="${selected%%-*}"
    linkage="${selected#*-}"
    compiler=""
    if [[ "$toolchain" == gcc ]]; then
        compiler="${VEX_GXX:-$(command -v g++ || true)}"
    else
        # Resolve Clang per mode so a missing Clang does not suppress
        # an independent GCC result earlier in the same all-mode run.
        if ! compiler="$("$root/tools/find_clangxx.sh")"; then
            printf 'PORTABILITY FAIL %s: no Clang compiler available\n' "$selected" >&2
            failed+=("$selected:clang-compiler")
            continue
        fi
    fi
    if [[ -z "$compiler" ]]; then
        printf 'PORTABILITY FAIL %s: compiler unavailable\n' "$selected" >&2
        failed+=("$selected:compiler")
        continue
    fi

    shared=OFF
    fetch_args=()
    if [[ "$linkage" == shared ]]; then
        shared=ON
        if [[ "$shared_dependency" == fetch ]]; then
            if [[ "${VEX_ALLOW_DEPENDENCY_FETCH:-ON}" == OFF ]]; then
                printf 'PORTABILITY FAIL %s: shared dependency requires fetch, but VEX_ALLOW_DEPENDENCY_FETCH=OFF\n' "$selected" >&2
                failed+=("$selected:dependency-disabled")
                continue
            fi
            fetch_args+=("-DCMAKE_DISABLE_FIND_PACKAGE_prometheus-cpp=ON")
            printf 'Dependency policy: force isolated pinned prometheus-cpp build with PIC, not installed non-PIC static archives\n'
        else
            fetch_args+=("-DCMAKE_DISABLE_FIND_PACKAGE_prometheus-cpp=OFF")
            printf 'Dependency policy: user-selected installed prometheus-cpp; must be PIC-compatible\n'
        fi
    else
        fetch_args+=("-DCMAKE_DISABLE_FIND_PACKAGE_prometheus-cpp=OFF")
    fi

    build_dir="$build_root/monitoring-portability-$selected"
    if [[ -f "$build_dir/CMakeCache.txt" ]]; then
        cached="$(sed -n 's/^CMAKE_CXX_COMPILER:FILEPATH=//p' "$build_dir/CMakeCache.txt" | head -n 1)"
        if [[ -n "$cached" && "$(readlink -f "$cached")" != "$(readlink -f "$compiler")" ]]; then
            printf 'PORTABILITY FAIL %s: cached compiler mismatch; use fresh VEX_PORTABILITY_BUILD_ROOT\n' "$selected" >&2
            failed+=("$selected:compiler-cache")
            continue
        fi
    fi

    printf 'Compiler: %s\n' "$compiler"
    "$compiler" --version | head -n 1

    if ! cmake -S "$root" -B "$build_dir" \
        "-DCMAKE_CXX_COMPILER=$compiler" \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        "-DENABLE_SHARED=$shared" "-DBUILD_SHARED_LIBS=$shared" \
        -DBUILD_LOGGER=OFF -DBUILD_MONITORING=ON \
        -DBUILD_THREADPOOL=OFF -DBUILD_OBJECT_POOL=OFF \
        -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=ON \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        "-DVEX_ALLOW_DEPENDENCY_FETCH=${VEX_ALLOW_DEPENDENCY_FETCH:-ON}" \
        "-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic -Werror=return-type -Werror=unused-value" \
        "${fetch_args[@]}"; then
        printf 'PORTABILITY FAIL mode=%s step=configure\n' "$selected" >&2
        failed+=("$selected:configure")
        continue
    fi
    if ! cmake --build "$build_dir" --parallel "${VEX_JOBS:-2}"; then
        printf 'PORTABILITY FAIL mode=%s step=build\n' "$selected" >&2
        failed+=("$selected:build")
        continue
    fi
    if ! ctest --test-dir "$build_dir" --output-on-failure \
        -R '^vex[.]monitoring[.]|^vex[.]package[.]consumer$'; then
        printf 'PORTABILITY FAIL mode=%s step=ctest-or-package-consumer\n' "$selected" >&2
        failed+=("$selected:ctest")
        continue
    fi

    if [[ "$toolchain" == clang && "${VEX_RUN_CLANG_TIDY:-OFF}" == ON ]]; then
        tidy="${VEX_CLANG_TIDY:-$(command -v clang-tidy || true)}"
        if [[ -z "$tidy" ]]; then
            printf 'PORTABILITY FAIL %s: clang-tidy requested but unavailable\n' "$selected" >&2
            failed+=("$selected:clang-tidy-not-found")
            continue
        fi
        tidy_failed=0
        for source in "$root"/src/monitoring/*.cpp; do
            if ! "$tidy" -p "$build_dir" "$source" --quiet; then
                tidy_failed=1
            fi
        done
        if [[ "$tidy_failed" == 1 ]]; then
            printf 'PORTABILITY FAIL mode=%s step=clang-tidy\n' "$selected" >&2
            failed+=("$selected:clang-tidy")
            continue
        fi
    fi

    passed+=("$selected")
    printf 'VEX MONITORING PORTABILITY PASS mode=%s\n' "$selected"
done

printf '\nPORTABILITY SUMMARY passed=%s failed=%s\n' "${#passed[@]}" "${#failed[@]}"
if (( ${#failed[@]} > 0 )); then
    printf 'Failed modes: %s\n' "${failed[*]}" >&2
    exit 1
fi
printf 'VEX MONITORING PORTABILITY PASS selected_modes=%s\n' "$mode"
