## 2026-11-13 owner results: Lifetime 4096/4096; static analysis 5 findings

Owner provided both redirected files. The 8×512 Registry lifetime
benchmark completed all expiry checks, with 8/8 coherent RSS/HWM.
RSS stayed at 4,940 KiB from rounds 2–8 while glibc `uordblks`
rose by 16,112 B from round 1 to 8. Heap cause **not attributed**.
LLVM 20.1.2 clang-tidy **FAIL: checked=5 failed=3 translation units,
five diagnostics**. Five local contract-specific suppressions
were committed, requiring fresh clang-tidy verification; a
raw prometheus-cpp baseline mode was added for allocator attribution.
Full evidence: [MONITORING_LIFETIME_STATIC_FINDINGS_2026-11-13.md](MONITORING_LIFETIME_STATIC_FINDINGS_2026-11-13.md).

---

## 2026-11-13: Lifetime build successful; static tool available; redirected outputs needed

Owner's checkout SHA `6df93958decb1ccc9f68176e0f817af2602e04b9` ran the 8×512 Registry lifetime tool;
GCC 13.3 CMake/build completed. `clang-tidy` 20.1.2 discovery PASS.
The Lifetime CSV and full static log were redirected and **not
included** in the terminal text. No numeric memory trend,
`MONITORING_STATIC_PASS` or 4096-expiration acceptance is inferred
without those files. See
[MONITORING_LIFETIME_STATIC_PRELIM_2026-11-13.md](MONITORING_LIFETIME_STATIC_PRELIM_2026-11-13.md).

---

# Vex Monitoring — open work and acceptance (2026-11-14)

**Repository:** `majiddarvishan/vex`  
**Working branch:** `feature/monitoring-correctness-tests`  
**PR:** [#3](https://github.com/majiddarvishan/vex/pull/3) — Draft, stacked on `feature/cmake-ctest-foundation`, **unmerged**.

## Verified work (owner-run evidence, before this tool-only follow-up)

- [x] **MON-OPEN-01 (P1)** — Managed 1000-Family registration median improved **17.151ms → 11.169ms (−34.9%)** in matched three-run Release benchmark. A separate controlled, warmed 15-run measurement of the current revision gives **11.061ms**; do not conflate those separate protocols.
- [x] **MON-OPEN-02 (P1)** — Scoped RAII label-ownership snapshot regression; concurrent uncoordinated raw Family mutation remains unsupported.
- [x] **MON-OPEN-03 (P1)** — Managed quota rollback and HELP metadata consistency, including failure and owner-churn tests; raw external mutations bypass ledger/quota.
- [x] **MON-OPEN-04 (P2)** — Real loopback HTTP `/metrics` integration, sample values and Registry lifecycle.
- [x] **MON-OPEN-05 portability/package subgate (P2)** — GCC 13.3 and Clang 20 × Static and Shared **11/11 PASS each** with installed external CMake-package consumers. The tested Linux `$ORIGIN` RUNPATH resolves Shared dependency loading.
- [x] **MON-OPEN-06 measured workload subgate (P2)** — Controlled 15-run Release benchmark (180 records), load 33-record baseline and new 60-record raw/shared/sharded Counter comparisons. Latest Linux peak RSS internally consistent **60/60** records. Shared hot Counter bottleneck appears in direct prometheus-cpp as well; no Vex-specific update-path defect shown.

**Established system acceptance (owner-run):** baseline/threaded/ASan+UBSan/Clang20 TSan **10/10 per mode**, stress **900/900**, foundation/installed consumer **13/13**. Benchmark outcomes are platform/workload specific and do not prove leak freedom or production SLAs.

## Remaining work on this same branch

| Task | Priority | Scope | Acceptance/evidence |
|---|---|---|---|
| MON-OPEN-05 static-analysis follow-up | P2 | **Owner run FAIL:** clang-tidy 20.1.2, 5 actionable errors across 3/5 files (2 swappable constructor arguments, 1 shared_ptr-by-value, 2 intentional cleanup catches). Five scoped NOLINTNEXTLINE explanations committed after log. | Rerun focused clang-tidy, require `MONITORING_STATIC_PASS checked=5 failed=0`. If warnings persist, fix only the relevant Monitoring contract; no blanket disable. |
| MON-OPEN-06 memory/retention follow-up | P2 | **Owner-run lifetime PASS for 4096 expired Registry checks and 8/8 consistent HWM/RSS.** RSS plateau 4,940 KiB rounds 2–8, but glibc uordblks increased **88,672→104,784 B** (+16,112 B) from round 1 to 8; cause not attributed. Added optional isolated raw Prometheus comparison, not yet run. | Rerun 8×512 managed and raw in fresh processes, compare allocator trends; consider preinstalled heaptrack if growth persists. No leak-freedom assertion. |
| MON-OPEN-06 historical registration noise | P2 / research | The old-vs-new 1000-series one-family +6.1% in three ordered repeats | Requires both revisions measured by the **same** warmed/rotated harness on one host to establish causality. Do not call it a confirmed regression. |
| MON-OPEN-07 | Process | Final owner review, release/merge decision | Resolve reviews, freeze reviewed SHA, retain stacked base and require explicit approval for any merge or Ready-for-Review change. |

## Most recent added tooling (NOT owner-run)

1. `tools/bench_monitoring_lifetime.cpp` and `tools/benchmark_monitoring_lifetime.sh` with a Linux-only, bounded 8 × 512-Registry default churn probe, 1 live persistent Registry, quota isolation checks, `weak_ptr::expired()` checks, and RSS/HWM/glibc heap trend samples.
2. `tools/verify_monitoring_static.sh` with targeted warning-as-error `clang-tidy`. It uses `build/monitoring-portability-clang-static/compile_commands.json` if available; otherwise generates it by running the previously verified Clang Static portability suite. It never edits source.

These tools have **not been built, run or proven clean** on the owner's machine. Avoid claiming new test PASS.

## Suggested commands (only remaining focused probes)

```sh
git pull --ff-only origin feature/monitoring-correctness-tests
git rev-parse HEAD
git status --short

./tools/benchmark_monitoring_lifetime.sh 8 512 > build/monitoring-lifetime.csv
wc -l build/monitoring-lifetime.csv

./tools/verify_monitoring_static.sh check-tools
./tools/verify_monitoring_static.sh > build/monitoring-static.log 2>&1
```

The last command only works if `clang-tidy` is installed; a missing tool should not trigger an automatic installation.

## References

- [Most recent Counter/RSS verification](MONITORING_LOAD_CONTROLS_2026-11-13.md)
- [Raw 60-record Counter control CSV](benchmarks/2026-11-13-monitoring-load-controls-3.csv)
- [Four-mode package and controlled/load analysis](MONITORING_PORTABILITY_LOAD_RESULTS_2026-11-10.md)
- [Historical old-vs-new registration comparison](MONITORING_PROFILE_BEFORE_AFTER_2026-11-10.md)
- [Memory-lifetime and static check acceptance plan](MONITORING_FOLLOWUP_PLAN_2026-11-14.md)

Older task-board state snapshots are archived in
[MONITORING_TASKS_HISTORY_BEFORE_2026-11-14.md](MONITORING_TASKS_HISTORY_BEFORE_2026-11-14.md).

**Boundaries:** No Logger, ObjectPool or ThreadPool runtime changes. No merge or Draft-state change without owner authorization.
