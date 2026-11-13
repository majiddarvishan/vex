#!/usr/bin/env bash
# Diagnose TSan runtime compatibility independently of Vex or Prometheus.
# Does not change ASLR, sysctl values, kernel settings, or the source tree.
set -Eeuo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
use_clang=false
if [[ "${1:-}" == "--clang" ]]; then
    use_clang=true
    shift
fi
repeats="${1:-10}"
if [[ ! "${repeats}" =~ ^[1-9][0-9]*$ ]] || (( repeats > 100 )); then
    echo "Usage: tools/diagnose_tsan.sh [--clang] [runs:1..100]" >&2
    exit 2
fi

if [[ "$use_clang" == true ]]; then
    compiler="$("${root}/tools/find_clangxx.sh")"
    default_dir="${root}/build/tsan-runtime-probe-$(basename -- "$compiler")"
else
    compiler="${CXX:-c++}"
    default_dir="${root}/build/tsan-runtime-probe"
fi
build_dir="${VEX_TSAN_DIAG_DIR:-${default_dir}}"
mkdir -p "${build_dir}"

printf 'TSAN PROBE compiler=%s repeats=%s\n' "${compiler}" "${repeats}"
"${compiler}" --version | head -n 1
uname -sr
if [[ -r /proc/sys/vm/mmap_rnd_bits ]]; then
    printf 'vm.mmap_rnd_bits='
    cat /proc/sys/vm/mmap_rnd_bits
fi

"${compiler}" -std=c++17 -g -O1 -fno-omit-frame-pointer \
    -fsanitize=thread -fPIE -pie -pthread \
    "${root}/tests/tsan_runtime_probe.cpp" \
    -o "${build_dir}/tsan_runtime_probe"

pass=0
mapping=0
race=0
other=0

for ((i=1; i<=repeats; i++)); do
    log="${build_dir}/probe-${i}.log"
    if "${build_dir}/tsan_runtime_probe" >"${log}" 2>&1; then
        (( pass += 1 ))
        printf 'RUN %d PASS\n' "${i}"
    else
        rc=$?
        if grep -Eqi 'ThreadSanitizer: (unexpected memory mapping|encountered an incompatible memory layout)' "${log}"; then
            (( mapping += 1 ))
            category=RUNTIME_MAPPING
        elif grep -Eq 'WARNING: ThreadSanitizer|data race' "${log}"; then
            (( race += 1 ))
            category=TSAN_REPORT
        else
            (( other += 1 ))
            category=OTHER_FAILURE
        fi
        printf 'RUN %d %s exit=%d log=%s\n' "${i}" "${category}" "${rc}" "${log}"
        sed -n '1,8p' "${log}"
    fi
done

printf 'TSAN PROBE SUMMARY pass=%d mapping=%d sanitizer_report=%d other=%d runs=%d logs=%s\n' \
    "${pass}" "${mapping}" "${race}" "${other}" "${repeats}" "${build_dir}"

# A race in the independent probe is a separate severe toolchain concern.
# Any failure remains nonzero: never re-label a flaky sanitizer as PASS.
if (( race > 0 )); then
    exit 4
elif (( mapping > 0 )); then
    exit 3
elif (( other > 0 )); then
    exit 5
fi
