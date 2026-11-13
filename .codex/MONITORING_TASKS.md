## Follow-up tool improvement after owner results (not yet built)

The user-supplied load report revealed a hot shared Counter throughput drop and invalid `peak_rss_kib` in 32/33 records. An opt-in `--controls` load mode adds direct Prometheus shared Counter and per-thread sharded Counter comparisons (Vex/raw) while preserving the original 34-line output when omitted. Peak reporting now derives from Linux `VmHWM` and returns -1 for inconsistent values. **No owner-run verification yet for this newest benchmark code.** See [MONITORING_PORTABILITY_LOAD_RESULTS_2026-11-10.md](MONITORING_PORTABILITY_LOAD_RESULTS_2026-11-10.md).

---

## Update: four-mode package acceptance PASS; benchmark data received

All GCC/Clang static/shared consumers now PASS 11/11 each. Controlled 180 and load 33 raw samples are archived; shared Counter contention and invalid peak RSS identified. [Full result](MONITORING_PORTABILITY_LOAD_RESULTS_2026-11-10.md). Optional clang-tidy not run; MON-OPEN-06 targeted diagnostics remain open.

---

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
| MON-OPEN-05 | P2 | **Portability/package gate PASS: GCC/Clang × Static/Shared, 11/11 each, external consumer PASS.** Shared RUNPATH fixed. `CPM` cache dirty warning persists; optional clang-tidy NOT RUN. | If comprehensive static analysis is mandatory, run targeted clang-tidy; do not reopen the passed 4-mode packaging matrix without new evidence |
| MON-OPEN-06 | P2 | **Owner evidence archived:** 180 controlled and 33 resource samples; measured hot shared Counter contention and invalid peak RSS in 32/33 prior records. **Benchmark diagnostic enhancement committed, NOT YET TESTED:** Linux VmHWM consistency with invalid=-1, opt-in raw/shared and Vex/raw per-thread-sharded Counter control. | Run `./tools/benchmark_monitoring_load.sh 3 --controls > build/monitoring-load-controls.csv` (61 lines expected), analyze controls and validated peak RSS. Optional heaptrack and historical same-harness A/B remain future/owner-optional |
| MON-OPEN-07 | Process | Final owner code review and release/merge decision | Freeze reviewed SHA, resolve PR comments, preserve stacked foundation dependency, require explicit owner authorization before merge |

## Evidence and boundaries

- [Most recent all-mode acceptance](MONITORING_INDEXED_ACCEPTANCE_2026-11-10.md): 10/10 × four modes; 900/900 Stress; 13/13 foundation.
- [Before/after comparison](MONITORING_PROFILE_BEFORE_AFTER_2026-11-10.md), [before raw CSV](benchmarks/2026-11-10-monitoring-managed-vs-raw.csv), [after raw CSV](benchmarks/2026-11-10-monitoring-managed-vs-raw-after.csv).
- Only three sequential Release repetitions per scenario. Observed 1000-family improvement is credible for that measured setup; do not claim absent race conditions for unsupported operations or that other machine configurations share identical performance.
- PR #3 remains **Draft and unmerged**. No Logger, ObjectPool or ThreadPool behavioral changes authorized.

**Next recommended order:** MON-OPEN-06 (resource/load and 1000-series concern), MON-OPEN-05 (portability/static), MON-OPEN-07 (owner-only merge decision).
