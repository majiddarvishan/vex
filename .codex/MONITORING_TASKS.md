## Latest report — Full Monitoring PASS, shared package runtime still failing

Owner's `Pasted text(7).txt` confirms **10/10 each**
baseline/threaded/ASan+UBSan/Clang20 TSan; **900/900** stress;
**13/13** full foundation/package.
GCC Static and Clang Static: **11/11 PASS** each.
GCC Shared and Clang Shared: **10/11** each; ten Monitoring
cases pass, installed consumer cannot load `libprometheus-cpp-pull.so.1.3`.
The previous PIC link blocker is fixed. A Monitoring-only
`$ORIGIN` installed RUNPATH candidate is now committed, but
**NOT YET OWNER-RUN**. See
[MONITORING_PORTABILITY_RUNTIME_2026-11-10.md](MONITORING_PORTABILITY_RUNTIME_2026-11-10.md).

---

## Latest 2026-11-10 owner report — GCC static PASS, shared PIC dependency BLOCKED

See [MONITORING_PORTABILITY_PARTIAL_2026-11-10.md](MONITORING_PORTABILITY_PARTIAL_2026-11-10.md).
Benchmarks built/executed but CSVs not provided; GCC static 11/11 PASS;
GCC shared link failed on installed non-PIC prometheus-cpp archives.
Clang variants never ran. A source-level C++17 macro warning fix and
an isolated PIC dependency strategy were subsequently committed;
**both remain unverified on owner checkout**.

---

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
| MON-OPEN-05 | P2 | **PARTIAL, 2/4 modes PASS.** After isolated pinned PIC dependency: GCC/Clang Static **11/11 each PASS**; GCC/Clang Shared build + 10 Monitoring tests PASS, but installed consumer **FAIL** loading `libprometheus-cpp-pull.so.1.3` (each 10/11). ELF `$ORIGIN` RUNPATH candidate added **AFTER** owner log, NOT YET TESTED. | Re-run both Shared modes and verify external package consumer without `LD_LIBRARY_PATH`; inspect installed RPATH if failure persists; optional clang-tidy remains unverified |
| MON-OPEN-06 | P2 | **Tools built and executed** (controlled 15, load 3) but numeric CSVs not provided. No RSS, concurrent-load percentiles, or registration variance verdict yet. | Obtain controlled and load CSVs (181/34 lines) and analyze throughput, RSS, collect/HTTP p95/p99, 1000-series variability. Optional heaptrack not run |
| MON-OPEN-07 | Process | Final owner code review and release/merge decision | Freeze reviewed SHA, resolve PR comments, preserve stacked foundation dependency, require explicit owner authorization before merge |

## Evidence and boundaries

- [Most recent all-mode acceptance](MONITORING_INDEXED_ACCEPTANCE_2026-11-10.md): 10/10 × four modes; 900/900 Stress; 13/13 foundation.
- [Before/after comparison](MONITORING_PROFILE_BEFORE_AFTER_2026-11-10.md), [before raw CSV](benchmarks/2026-11-10-monitoring-managed-vs-raw.csv), [after raw CSV](benchmarks/2026-11-10-monitoring-managed-vs-raw-after.csv).
- Only three sequential Release repetitions per scenario. Observed 1000-family improvement is credible for that measured setup; do not claim absent race conditions for unsupported operations or that other machine configurations share identical performance.
- PR #3 remains **Draft and unmerged**. No Logger, ObjectPool or ThreadPool behavioral changes authorized.

**Next recommended order:** MON-OPEN-06 (resource/load and 1000-series concern), MON-OPEN-05 (portability/static), MON-OPEN-07 (owner-only merge decision).
