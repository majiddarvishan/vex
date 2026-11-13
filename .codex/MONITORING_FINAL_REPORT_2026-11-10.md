# Monitoring final acceptance and Release benchmark — 2026-11-10

## Scope and provenance

Owner-supplied terminal transcript for `feature/monitoring-correctness-tests`:
`./tools/verify_monitoring.sh all` followed by
`./tools/benchmark_monitoring.sh 3`. Both completed successfully.
This run **includes the subsequent scoped-ownership safeguard** (the
RAII guard source rebuilt in all verification modes) and the newer
two-shape benchmark target. The transcript does not include
`git rev-parse HEAD` or a git cleanliness check, so do not claim a
cryptographically verified local SHA. The connected GitHub branch HEAD
immediately before this record was `c08f64deb827f9d5dd09ba6a5ec57227f49c4a69`.

## Acceptance evidence

| Stage | Result | CTest time |
|---|---:|---:|
| Baseline | 9/9 PASS | 0.11 s |
| Threaded | 9/9 PASS | 0.12 s |
| ASan + UBSan | 9/9 PASS | 0.30 s |
| Clang 20 TSan (`/usr/bin/clang++-20`) | 9/9 PASS | 0.31 s |
| Stress initial | 9/9 PASS | 0.12 s |
| Stress repeat (`--repeat until-fail:100`) | 8 C++ tests × 100 = **800/800 PASS** | 7.77 s |
| Full foundation with package consumer | 12/12 PASS | 1.34 s |

Terminal conclusion: `VEX MONITORING ACCEPTANCE PASS all`.
No compiler, test, AddressSanitizer, UndefinedBehaviorSanitizer or
ThreadSanitizer errors are visible in the provided transcript. These
are successful **observed paths**, not a proof of universal race
freedom or that arbitrary external Prometheus API mutations are safe.

## Release benchmark evidence

Command `./tools/benchmark_monitoring.sh 3` built and ran
`vex_monitoring_benchmark` successfully in Release configuration.
Environment: Ubuntu GCC **13.3.0** (`/usr/bin/c++`),
Linux `6.8.0-147-generic`, 12 logical CPUs, timestamp
`2026-11-10T12:41:47Z`. Three measurements per scenario; CSV stored
at [benchmarks/2026-11-10-monitoring-release.csv](benchmarks/2026-11-10-monitoring-release.csv).

Measured **median** time in ms, not inferred throughput:

| Shape | Count | Registration | 100 increments/series | Collect |
|---|---:|---:|---:|---:|
| One family / many series | 10 | 0.059 | 0.023 | 0.075 |
| Many families / one series each | 10 | 0.029 | 0.023 | 0.020 |
| One family / many series | 100 | 0.282 | 0.202 | 0.073 |
| Many families / one series each | 100 | 0.373 | 0.211 | 0.136 |
| One family / many series | 1000 | 2.812 | 2.088 | 0.750 |
| Many families / one series each | 1000 | 17.268 | 2.145 | 1.387 |

At 1,000 metric instances, registration of 1,000 distinct Families
was **~6.14× slower** than creation of 1,000 labeled series under
one Family in the measured scenario. This is an **investigation
lead**; it is not a confirmed defect or a production SLA.
At 10 instances, timing noise and warmup dominate. Neither RSS,
heap allocations, lock contention, p95/p99, nor a real HTTP scrape
was measured. Do not extrapolate such values.

## Current disposition

1. **Monitoring functional acceptance gate: PASS** for the supplied
   matrix including the latest scoped-ownership fix.
2. **Release benchmark baseline: COLLECTED**, no performance acceptance
   threshold or regression verdict defined.
3. **Follow-up tasks remain OPEN**, with priorities and acceptance
   criteria in [MONITORING_TASKS.md](MONITORING_TASKS.md).
4. PR #3 remains **Draft/unmerged**, awaiting owner review and decision.
   No Logger/ObjectPool/ThreadPool behavioral changes are authorized.
