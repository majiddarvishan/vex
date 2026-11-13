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
