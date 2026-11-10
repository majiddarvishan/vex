## 2026-11-10 paired raw-vs-managed benchmark measured

Owner supplied 36 CSV rows, valid 37-line file including header.
Median (3 samples) registration at 1000:
- one-family-many-series: Vex **3.070ms**, raw **0.930ms**
- many-families: Vex **17.151ms**, raw **9.568ms**

Both upstream Registry and Vex wrapper contribute. Vex's
`tracked_metric_series_` vector is scanned for expired entries and
matching Family on every registration; the upstream
`Registry::Add` also performs linear scans of Family vectors.
The former is an opportunity for indexed manager bookkeeping;
any improvement must be verified, not assumed.

Full analysis: [MONITORING_PROFILE_2026-11-10.md](MONITORING_PROFILE_2026-11-10.md).
Raw CSV: [benchmarks/2026-11-10-monitoring-managed-vs-raw.csv](benchmarks/2026-11-10-monitoring-managed-vs-raw.csv).

---

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
| MON-OPEN-01 | P1 | **Indexed ledger implementation committed, unverified.** Owner's pre-change raw-vs-Vex medians at 1000: one family 0.930/3.070 ms, many families 9.568/17.151 ms. Tracked-series lookup now uses owner-aware std::map and amortized weak-registry expiry, plus 320-Registry churn regression. | Re-run complete 10/10, 900/900, 13/13 acceptance and the exact Release benchmark; document measured before/after and memory tradeoffs before closing |
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

## Next MON-OPEN-01 evidence collection

The benchmark now compares matched **Vex managed** and **raw upstream** registration paths in the same Release build. Its output CSV has the same 8 columns but four shapes (new `raw_*` rows), 36 rows with three repeats. Run:

```sh
./tools/benchmark_monitoring.sh 3 > build/monitoring-profile.csv
```

Release runner build/config messages go to stderr; CSV to stdout. Do not invent results or claim the new benchmark is validated until the owner reports actual output.

**Latest unverified source change:** indexed weak-owner ledger with periodic expiry sweep, plus 320-Registry churn test. The previous paired CSV is the **before** baseline. Do not claim any speedup or tests passed on this latest commit without fresh owner logs. See [MONITORING_PROFILE_2026-11-10.md](MONITORING_PROFILE_2026-11-10.md).
