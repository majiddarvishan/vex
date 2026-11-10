# Vex Monitoring handoff — optional MON-OPEN-05/06 tools added

**Branch:** `feature/monitoring-correctness-tests`; Draft PR #3 stacked on foundation PR #2.
No merge or changes to non-Monitoring runtime modules.

## Last verified results (before new tooling)

Owner-run Monitoring all-mode acceptance: **10/10** in baseline,
threaded, ASan/UBSan and Clang TSan; **900/900** repeated stress
checks; full installed package and foundation **13/13**.
The indexed-ledger change measurably reduced synthetic 1000-Family
registration from 17.151 to 11.169 ms (34.9% lower median).
The possible single-Family 1000-series +6.1% difference was based
on only three ordered runs with overlapping ranges.

## New unverified opt-in tooling

- `tools/benchmark_monitoring_load.sh`: Linux Release
  multi-thread registration and shared Counter, concurrent Collect,
  real loopback HTTP percentiles, process RSS and glibc heap snapshots.
- `tools/benchmark_monitoring_controlled.sh`: 9/15/21 repeated
  warm-up and rotated paired registration measurements.
- `tools/verify_monitoring_portability.sh`: GCC/Clang ×
  static/shared, warnings, CTest and installed consumer; optional
  explicit clang-tidy.
- `tools/profile_monitoring_allocations.sh`: heaptrack, only
  if already installed, no automatic installation.
- Optional new benchmark CMake target
  `vex_monitoring_load_benchmark` on UNIX.

**None of these new binaries/scripts have been executed on the
owner machine**, so MON-OPEN-05 and MON-OPEN-06 remain OPEN.
These additions are isolated benchmark/build tooling and do not
modify MetricsManager/Logger/ObjectPool/ThreadPool runtime behavior.

Follow [MONITORING_PERFORMANCE_PLAN.md](MONITORING_PERFORMANCE_PLAN.md).
Suggested owner commands:
```sh
git pull --ff-only origin feature/monitoring-correctness-tests
git rev-parse HEAD
git status --short
./tools/benchmark_monitoring_controlled.sh 15 > build/monitoring-controlled.csv
./tools/benchmark_monitoring_load.sh 3 > build/monitoring-load.csv
./tools/verify_monitoring_portability.sh all
```
Optional heaptrack only if installed.
Then share logs + CSVs for data-driven analysis.
Keep PR Draft/unmerged and request owner decision for MON-OPEN-07.

Evidence for previous release benchmark:
[MONITORING_PROFILE_BEFORE_AFTER_2026-11-10.md](MONITORING_PROFILE_BEFORE_AFTER_2026-11-10.md).
Task board: [MONITORING_TASKS.md](MONITORING_TASKS.md).
