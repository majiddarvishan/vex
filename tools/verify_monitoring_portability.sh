#!/usr/bin/env bash
# MON-OPEN-05: Monitoring-only GCC/Clang, static/shared packaging matrix.
# Uses distinct build directories and never changes source or main.
set -Eeuo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mode="${1:-all}"
case "$mode" in
  all) modes=(gcc-static gcc-shared clang-static clang-shared) ;;
  gcc-static|gcc-shared|clang-static|clang-shared) modes=("$mode") ;;
  *) printf 'Usage: tools/verify_monitoring_portability.sh [all|gcc-static|gcc-shared|clang-static|clang-shared]\n' >&2; exit 2 ;;
esac
gcc_compiler="${VEX_GXX:-$(command -v g++ || true)}"
if [[ -z "$gcc_compiler" ]]; then
  printf 'g++ not found: set VEX_GXX to a GCC C++ compiler.\n' >&2
  exit 2
fi
clang_compiler=""
for selected in "${modes[@]}"; do
  if [[ "$selected" == clang-* ]]; then
    clang_compiler="$("$root/tools/find_clangxx.sh")"
    break
  fi
done
build_root="${VEX_PORTABILITY_BUILD_ROOT:-${root}/build}"
for selected in "${modes[@]}"; do
  toolchain="${selected%%-*}"
  linkage="${selected#*-}"
  if [[ "$toolchain" == gcc ]]; then
    compiler="$gcc_compiler"
  else
    compiler="$clang_compiler"
  fi
  shared=OFF
  [[ "$linkage" != shared ]] || shared=ON
  build_dir="$build_root/monitoring-portability-$selected"

  if [[ -f "$build_dir/CMakeCache.txt" ]]; then
    cached="$(sed -n 's/^CMAKE_CXX_COMPILER:FILEPATH=//p' "$build_dir/CMakeCache.txt" | head -n 1)"
    if [[ -n "$cached" && "$(readlink -f "$cached")" != "$(readlink -f "$compiler")" ]]; then
      printf 'Compiler cache mismatch for %s. Set a fresh VEX_PORTABILITY_BUILD_ROOT.\n' "$selected" >&2
      exit 2
    fi
  fi

  printf '\n===== MONITORING PORTABILITY %s, compiler=%s =====\n' "$selected" "$compiler"
  "$compiler" --version | head -n 1
  cmake -S "$root" -B "$build_dir" \
    "-DCMAKE_CXX_COMPILER=$compiler" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    "-DENABLE_SHARED=$shared" "-DBUILD_SHARED_LIBS=$shared" \
    -DBUILD_LOGGER=OFF -DBUILD_MONITORING=ON \
    -DBUILD_THREADPOOL=OFF -DBUILD_OBJECT_POOL=OFF \
    -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    "-DVEX_ALLOW_DEPENDENCY_FETCH=${VEX_ALLOW_DEPENDENCY_FETCH:-ON}" \
    "-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic -Werror=return-type -Werror=unused-value"
  cmake --build "$build_dir" --parallel "${VEX_JOBS:-2}"
  ctest --test-dir "$build_dir" --output-on-failure \
    -R '^vex[.]monitoring[.]|^vex[.]package[.]consumer$'

  # Optional static analysis: run only against Vex Monitoring source;
  # no automatic fixes and no unrelated source targets.
  if [[ "$toolchain" == clang && "${VEX_RUN_CLANG_TIDY:-OFF}" == ON ]]; then
    tidy="${VEX_CLANG_TIDY:-$(command -v clang-tidy || true)}"
    if [[ -z "$tidy" ]]; then
      printf 'clang-tidy requested but not found; set VEX_CLANG_TIDY.\n' >&2
      exit 2
    fi
    for f in "$root"/src/monitoring/*.cpp; do
      "$tidy" -p "$build_dir" "$f" --quiet
    done
  fi
  printf 'VEX MONITORING PORTABILITY PASS mode=%s\n' "$selected"
done
printf 'VEX MONITORING PORTABILITY PASS selected_modes=%s\n' "$mode"
