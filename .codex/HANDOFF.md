# Current Vex Monitoring handoff — portability verified; optional Counter controls pending

Branch `feature/monitoring-correctness-tests`, Draft PR #3 stacked on Foundation; no merge.

Owner-provided evidence: baseline/threaded/ASan/TSan 10/10 each; stress 900/900; full foundation 13/13; GCC/Clang Static/Shared **11/11 each** including installed-package consumers. `$ORIGIN` Shared loader resolution PASS in tested staging layout. Optional clang-tidy NOT RUN; pinned prometheus-cpp CPM cache emits dirty warning.

Raw data archived: [180 controlled samples](benchmarks/2026-11-10-monitoring-controlled-15.csv), [33 load samples](benchmarks/2026-11-10-monitoring-load-3.csv). Managed 1000-Family registration median 11.061ms and 1000-series median 3.237ms; hot shared Counter median 1/4/8 threads ~19.74M/2.35M/4.30M increments/s. Collect/HTTP p95 759.385µs/193.933µs. Previous `peak_rss` invalid for 32/33 records.

**Source changes after owner measurements are benchmark-only and NOT YET owner-tested**: `tools/bench_monitoring_load.cpp` uses Linux `VmHWM` instead of conflicting `getrusage` process peak, flags inconsistencies as unavailable (-1), and offers opt-in raw/managed shared vs per-thread sharded Counter controls. The existing no-options benchmark keeps its former schema and 34-line output. New `--controls` mode expects **61 CSV lines** (1 header + 3 rounds × 20 workloads).

Smallest next owner action:
```sh
git pull --ff-only origin feature/monitoring-correctness-tests
git rev-parse HEAD
./tools/benchmark_monitoring_load.sh 3 --controls > build/monitoring-load-controls.csv
wc -l build/monitoring-load-controls.csv
cat build/monitoring-load-controls.csv
```

Do not claim new measurement success without these outputs. MON-OPEN-05 portability/package PASS; optional static analysis still unverified. MON-OPEN-06 OPEN for enhanced controls and memory validity. MON-OPEN-07 owner review/merge decision remains open. Non-Monitoring runtime modules remain frozen.

Full context: [MONITORING_PORTABILITY_LOAD_RESULTS_2026-11-10.md](MONITORING_PORTABILITY_LOAD_RESULTS_2026-11-10.md), [MONITORING_TASKS.md](MONITORING_TASKS.md).
