# Monitoring Tasks — Current Status (2026-11-10)

**Scope:** `feature/monitoring-correctness-tests`, stacked Draft
[PR #3](https://github.com/majiddarvishan/vex/pull/3).
Current evidence: [MONITORING_ACCEPTANCE_2026-11-10_HTTP.md](MONITORING_ACCEPTANCE_2026-11-10_HTTP.md).
Original task ledger and interim notes:
[MONITORING_TASKS_HISTORY.md](MONITORING_TASKS_HISTORY.md).

## Complete / verified in owner's acceptance output

- [x] MON-OPEN-02 (**P1**) — Resolve scoped-series label-snapshot
  ownership TOCTOU; concurrency regression passes. External raw
  `Family::Add/Remove`, `Registry::Remove` and violation of
  Registry/Family lifetime contracts are explicitly **not supported**.
- [x] MON-OPEN-03 (**P1**) — Roll back provisional managed quota
  metadata on registration failures, reject conflicting HELP,
  recover from strict-registry errors. No quota promise for raw
  third-party Family mutation bypassing the Vex manager.
- [x] MON-OPEN-04 (**P2**) — Real loopback HTTP `/metrics`,
  main/subsystem/expired Registry lifecycle, Counter/Gauge/Histogram
  values, Exposer teardown. Test passed under all four modes and
  100 repeated stress runs.
- [x] Earlier Monitoring CMake/CTest, MetricsManager, HealthCheck,
  Decorators, RAII, validation, Clang-version discovery, installed
  package consumer and baseline Release benchmark.

**Verification:** Baseline 10/10, Threaded 10/10, ASan/UBSan 10/10,
Clang 20 TSan 10/10, Stress 900/900 (9 C++ tests × 100),
full foundation/package consumer 13/13, and final all-mode PASS.
Exact local git SHA was not emitted in the attached terminal output.

## Open work

| ID | Priority | Task | Acceptance criteria |
|---|---|---|---|
| MON-OPEN-01 | P1 | Profile registration scaling for 1000 distinct Families (17.268 ms median vs 2.812 ms for 1000 series in 1 Family in previous Release baseline) | Isolate Vex vs prometheus-cpp overhead with controlled matched workloads; code-path attribution / profiling; measure before/after without assuming root cause or committing a speculative optimization |
| MON-OPEN-05 | P2 | Portability and static review (GCC/Clang warnings, tidy, static/shared build/install) | Record actual compiler/OS versions, full clean build/consumer evidence; fix Monitoring-only issues |
| MON-OPEN-06 | P2 | Expand performance/resource study | Measure RSS, allocation rates, multi-threaded register/update/Collect and scrape latency under controlled load; report distribution, hardware and uncertainty |
| MON-OPEN-07 | Process | Owner review and merge/release decision | Confirm reviewed SHA, resolve PR comments, preserve stacked foundation dependency, obtain explicit owner merge approval |

## Recommended next action

Investigate **MON-OPEN-01** with an opt-in isolated **Release
benchmark** comparing the wrapper and direct upstream registration
paths. Favor one family with bounded labels when semantically suitable;
do not rewrite schemas or claim a defect solely from timing differences.
Then MON-OPEN-06, MON-OPEN-05, and owner decision MON-OPEN-07.

Do not change Logger/ObjectPool/ThreadPool runtime code. Do not
merge the Draft PR without owner consent.
