# Monitoring portability/load partial results and PIC fix — 2026-11-10

## Provenance

Owner-pasted terminal log `Pasted text(6).txt` (157 lines),
from `feature/monitoring-correctness-tests`. Exact local git
SHA was not printed. The log was produced after the previous
MON-OPEN-05/06 tooling commit and **before** the warning/PIC script
fixes recorded below.

## Observed tool status

| Tool / mode | Outcome | Evidence |
|---|---|---|
| `benchmark_monitoring_controlled.sh 15` | **Build and command completed successfully** | C++ benchmark linked; runner printed warmup=yes, ordering=rotated, runs=15; values redirected to `build/monitoring-controlled.csv`, not supplied |
| `benchmark_monitoring_load.sh 3` | **Build and command completed successfully** | GCC 13.3.0, Linux 6.8.0-147-generic, 12 logical CPUs; load executable linked; values redirected to `build/monitoring-load.csv`, not supplied |
| `verify_monitoring_portability.sh all` GCC static | **11/11 PASS** | Monitoring (10 tests including compiler discovery) and installed package consumer; CTest wall time 1.25 s |
| GCC shared | **FAIL: linker, before tests** | `/usr/local/lib/libprometheus-cpp-core.a` and `libprometheus-cpp-pull.a` were selected; `R_X86_64_PC32` relocation cannot be used inside shared object; linker explicitly requests `-fPIC` |
| Clang static | **NOT RUN** | Old script exited immediately on GCC shared failure |
| Clang shared | **NOT RUN** | Old script exited immediately on GCC shared failure |
| Optional clang-tidy / heaptrack | **NOT RUN** | Not present in owner log |

The failed shared link is attributable to existing **non-PIC
static third-party dependency artifacts** in `/usr/local/lib`.
The root Vex build already uses `CMAKE_POSITION_INDEPENDENT_CODE=ON`;
that flag cannot retroactively make installed archives PIC. No
conclusion about Shared Vex/Prometheus behavior follows until a
PIC-compatible dependency is selected and tests pass.

Compiler warnings in GCC static (warnings, not failed tests):

1. `tests/monitoring_contract.cpp:94`: ISO C++11 variadic macro
   must receive an argument for `...`.
2. `tests/monitoring_contract.cpp:102`: same variadic macro warning.
3. `tests/monitoring_validation_contract.cpp:118`:
   `-Wdangling-reference` on `const auto&` bound to CreateHistogram
   result. This is GCC's diagnostic; underlying method returns a
   Registry-owned reference and tests passed. Treat as a warning
   requiring source review, not as a proven use-after-free.

## Changes committed following the failure (not yet owner-run)

- `type_safety.hpp`: `SAFE_CREATE_COUNTER/GAUGE/HISTOGRAM` macros now
  accept the HELP argument through a required `__VA_ARGS__` tail
  rather than relying on GCC's nonstandard `, ##__VA_ARGS__`
  extension. Three-argument and extended calls remain supported.
- Test `monitoring_validation_contract.cpp`: use `auto&` for the
  returned histogram object rather than a const reference to avoid
  confusing the compiler's temporary-lifetime warning.
- `tests/CMakeLists.txt`: `-Werror=pedantic` for Monitoring macro
  contract target so this class of warning fails future builds.
- `verify_monitoring_portability.sh`: shared builds default to a
  **pinned, isolated CPM-built prometheus-cpp 1.3.0** dependency by
  setting `CMAKE_DISABLE_FIND_PACKAGE_prometheus-cpp=ON` for the
  shared build. CMake's position-independent code flag remains ON.
  This bypasses locally installed non-PIC archive candidates without
  touching `/usr/local` or installing packages system-wide. It may
  need source downloads if not cached, consistent with current
  `VEX_ALLOW_DEPENDENCY_FETCH=ON` policy.
- Independent GCC/Clang static/shared cases now continue after a
  failure, collect per-mode results and exit nonzero if **any**
  requested mode fails. No failing mode is silently marked PASS.
- If the user deliberately has a PIC-compatible installed dependency,
  they may choose `VEX_PORTABILITY_SHARED_DEPENDENCY=installed`.
  This is not assumed in the default configuration.

**Important:** The new warning/PIC changes have NOT passed a real
owner run yet. The earlier 11/11 applies to the pre-fix GCC Static
candidate; the Shared build previously failed. Do not mark
MON-OPEN-05 fully verified until fresh GCC and Clang matrix results
and installed package consumers pass.

## Required owner follow-up

```sh
git pull --ff-only origin feature/monitoring-correctness-tests
git rev-parse HEAD
git status --short
./tools/verify_monitoring.sh all
./tools/verify_monitoring_portability.sh all
wc -l build/monitoring-controlled.csv build/monitoring-load.csv
cat build/monitoring-controlled.csv
cat build/monitoring-load.csv
```

Expected CSV counts, if complete: **181 lines** for controlled
15-repeat (1 header +180 rows), **34 lines** for load 3-repeat
(1 header +33 rows). The posted terminal log has neither CSV
contents; therefore no memory/RSS, percentile, contention or
controlled-variance result is available yet. Do not close
MON-OPEN-06 based only on successful executable termination.

If Shared still fails with an isolated dependency, retain its
full CMake/build output for diagnosis; do not force system-wide
reinstallation or edit third-party files on the host. Do not
disable or hide failures simply to make the matrix green.

PR #3 remains Draft/unmerged. No Logger/ObjectPool/ThreadPool runtime
changes or other unrelated behavior changes were made.
