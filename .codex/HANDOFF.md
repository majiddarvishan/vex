# VEX handoff — 2026-11-14 owner 300s soak evidence

- Branch: `feature/monitoring-correctness-tests` ONLY; PR #3 open/Draft; base `feature/cmake-ctest-foundation`; no merge, base, or Draft-state changes.
- Source for owner run: `63d90c07ae78435f387ece30fac6a43d9f58a90f`; Linux 6.8.0-147-generic, /usr/bin/c++, GNU13.3.0 Release, 300s/10s sample/30s warmup/2000 ops/s.
- **PASS:** new soak Build, shell check, 5s Managed and Raw smoke. Both 300s invocations output unique CSV/log filenames without reported errors.
- **Raw 300s CSV fully validated:** `monitoring-soak-raw-sbHznA.csv`, 63 data rows, 300.000s, 599999 churn and weak-owner expiry checks, rate 1999.994/s. Raw RSS=4500 KiB and glibc uordblks=81504 B from 30 to 300s, no second-half slope; phases 255/0 modeled only, no actual sweep/quota in Raw.
- **Managed numeric CSV MISSING in conversation**, despite command completion. Its run ID is `monitoring-soak-managed-enfJiz` and file exists on owner's machine at `build/monitoring-soak-results/monitoring-soak-managed-enfJiz.csv`. Owner's included log contains only metadata. Full MON-OPEN-06 300s gate **OPEN**, not leak-free proof.
- **Only next owner action:** paste or attach the existing Managed CSV, ideally with `cat build/monitoring-soak-results/monitoring-soak-managed-enfJiz.csv`; **do not rerun** merely to supply existing evidence.
- Next agent action: check Managed 300s elapsed, expiry/quota/sweep phase, post-warmup and 150→300s RSS/heap trend, pre/post Sweep effect; document technical conclusion in `.codex/MONITORING_MEMORY_SOAK_2026-11-14.md`, `MONITORING_TASKS.md`, `TEST_MATRIX.md`, `HANDOFF.md`. No runtime change/profiler without evidence. No MON-OPEN-07/merge.
- For short continuation, user says «ادامه VEX». First inspect GitHub HEAD and read this handoff.

---

# VEX handoff — 2026-11-14 soak tool added

- Repository majiddarvishan/vex; ONLY branch feature/monitoring-correctness-tests, Draft PR #3, base feature/cmake-ctest-foundation; do not merge or change Draft/base.
- Current: optional `tools/benchmark_monitoring_soak.sh` with Managed/Raw synthetic Registry churn, 300s default, time-bounded and phase-aligned pre/post-sweep samples; unique CSV/log, quota/expiry checks. **Build/Smoke/300s NOT RUN.**
- Owner next: run `bash -n tools/benchmark_monitoring_soak.sh`, then `./tools/benchmark_monitoring_soak.sh 5 managed 1 1 2000`, `./tools/benchmark_monitoring_soak.sh 5 raw 1 1 2000`, then each mode at 300 10 30 2000. Send CSV and log pairs. If offline CMake dependency unavailable, mark BLOCKED; do not install packages.
- Agent next: validate CSV phase/expiry/quota, compare RSS/heap over post-warmup and second half, profile only if sustained unexplained growth, update `.codex`. No merge.
- Detailed [soak report](MONITORING_MEMORY_SOAK_2026-11-14.md). User can simply say 'ادامه VEX' in new chat; read this handoff and current GitHub HEAD first.

---

# Vex Monitoring handoff — sweep-aligned lifetime evidence received

**Branch:** `feature/monitoring-correctness-tests`, [Draft PR #3](https://github.com/majiddarvishan/vex/pull/3)
stacked on Foundation; **unmerged**, no Ready-for-Review change.

## Current owner-verified state

- Latest source checkout before this docs commit:
  `eee490533c98fc66568d2915ff7bfa45761f9743`.
- GCC/Clang Static/Shared package consumer **11/11 each PASS**;
  main Monitoring baseline/threaded/ASan+UBSan/Clang TSan
  **10/10 each**, stress **900/900**, foundation+consumer **13/13**.
- Focused LLVM20 clang-tidy `MONITORING_STATIC_PASS checked=5 failed=0`.
- Registration optimized for many Families in matched microbenchmark:
  **17.151→11.169ms (−34.9%)** at N=1000.
- 60-record shared/sharded raw-vs-managed Counter controls:
  60/60 internally consistent RSS/HWM; hot Counter contention
  reproduced directly in prometheus-cpp.
- Lifecycle tests: managed 8×512 4096 expired Registry checks,
  raw 8×512 4096, and **new managed 8×509 4072**;
  all run successfully, with all RSS/HWM checks coherent.

**Heap source-level explanation supported:** the Manager sweeps
expired weak Registry records on every **256th** registration
attempt. The Lifetime runner performs `batch + 3`
tracked attempts/round. Batch 512 drifts by 3 attempts/round;
batch 509 aligns to exactly 512 attempts per round.
Live allocator heap growth over eight rounds fell from
**16,096 B** (batch 512) to **1,536 B** (batch 509)
(−90.5%), with RSS stable after round 2. This supports
periodic, delayed weak-ledger reclamation; does not prove
no leaks under arbitrary longer workloads. Without further
registration, expired bookkeeping may remain until the
next sweep/Reset, but external Registry ownership is not
kept alive.

**Decisions:** MON-OPEN-01/02/03/04 PASS;
MON-OPEN-05 portability/selected static PASS;
MON-OPEN-06 targeted resource and weak-owner check PASS.
Optional heaptrack/longer duration and historically suspected
1000-series regression remain research tasks requiring
matched-methodology data, not assumed failures.
MON-OPEN-07 owner-only PR review/merge decision open.

No additional tests are needed to record current findings.
Do not modify production Runtime or non-Monitoring modules
without a reproducible issue. No merge without approval.

[Detailed result](MONITORING_LIFETIME_ALIGNED_RESULT_2026-11-13.md);
[raw aligned CSV](benchmarks/2026-11-13-monitoring-lifetime-managed-aligned-8x509.csv);
[task register](MONITORING_TASKS.md).
