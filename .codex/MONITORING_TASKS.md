# Vex Monitoring — current task register (2026-11-10)

**Branch:** `feature/monitoring-correctness-tests` · [Draft PR #3](https://github.com/majiddarvishan/vex/pull/3), stacked on foundation PR #2. Only Monitoring behavior is within the approved remediation scope.

## Verified completed work

- [x] **MON-OPEN-01 — indexed-ledger registration improvement (P1).** Full post-change automated acceptance: baseline/threaded/ASan+UBSan/Clang 20 TSan **10/10 each**, stress **900/900**, foundation/package **13/13**. Paired Release CSV shows 1000 managed-Family registration median **17.151 → 11.169 ms (34.9% lower)**, versus raw **9.568 → 9.575 ms** (essentially unchanged). The target bottleneck is improved within this synthetic workload. Source, all 36 before/after rows, and variability: [MONITORING_PROFILE_BEFORE_AFTER_2026-11-10.md](MONITORING_PROFILE_BEFORE_AFTER_2026-11-10.md). No production SLA implied.
- [x] **MON-OPEN-02 (P1).** Fix scoped-series identity/ownership TOCTOU. Concurrent direct raw Prometheus Family mutations remain outside supported contract.
- [x] **MON-OPEN-03 (P1).** Managed cardinality rollback and HELP metadata validation under failure. Raw external Family mutation bypasses wrapper quota accounting.
- [x] **MON-OPEN-04 (P2).** Loopback real HTTP `/metrics` integration, values, transient Registry expiration and teardown.
- [x] Monitoring baseline CMake/CTest, existing decorators, RAII, health, labels, installed consumer and sanitizer/stress suites.

## Still open

| ID | Priority | Work | Acceptance gate |
|---|---|---|---|
| MON-OPEN-05 | P2 | **Portability matrix implemented, unverified:** `verify_monitoring_portability.sh` covers GCC/Clang × static/shared, installed consumer, warnings and optional clang-tidy | Run the four isolated configs; record compiler/OS versions and actual CTest/consumer output; document static analysis where available |
| MON-OPEN-06 | P2 | **Load + controlled benchmark harness implemented, unverified:** 1/4/8-thread registration and shared Counter, concurrent Collect, HTTP percentiles, RSS/glibc heap snapshots, warmed rotated paired comparisons; optional heaptrack allocation profiling | Run new benchmarks and examine output, quantify contention and variability, obtain heaptrack allocation sites only if available; avoid assuming the +6.1% single-family change is a proven regression |
| MON-OPEN-07 | Process | Final owner code review and release/merge decision | Freeze reviewed SHA, resolve PR comments, preserve stacked foundation dependency, require explicit owner authorization before merge |

## Evidence and boundaries

- [Most recent all-mode acceptance](MONITORING_INDEXED_ACCEPTANCE_2026-11-10.md): 10/10 × four modes; 900/900 Stress; 13/13 foundation.
- [Before/after comparison](MONITORING_PROFILE_BEFORE_AFTER_2026-11-10.md), [before raw CSV](benchmarks/2026-11-10-monitoring-managed-vs-raw.csv), [after raw CSV](benchmarks/2026-11-10-monitoring-managed-vs-raw-after.csv).
- Only three sequential Release repetitions per scenario. Observed 1000-family improvement is credible for that measured setup; do not claim absent race conditions for unsupported operations or that other machine configurations share identical performance.
- PR #3 remains **Draft and unmerged**. No Logger, ObjectPool or ThreadPool behavioral changes authorized.

**Next recommended order:** MON-OPEN-06 (resource/load and 1000-series concern), MON-OPEN-05 (portability/static), MON-OPEN-07 (owner-only merge decision).
