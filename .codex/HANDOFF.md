# Vex Monitoring handoff — static PASS; sweep-phase heap test pending

Branch `feature/monitoring-correctness-tests`, Draft [PR #3](https://github.com/majiddarvishan/vex/pull/3),
base Foundation branch. No merge; no other module runtime edits.

## Owner-verified at source commit 34587f82

- Focused `clang-tidy` 20.1.2, 5/5 Monitoring source files,
  `MONITORING_STATIC_PASS checked=5 failed=0` on configured
  analyzer/bugprone/performance checks with warnings-as-errors.
  Existing five local, reasoned NOLINT annotations remain.
- 8×512 managed and raw Lifetime benchmarks built/reran in
  separate process invocations; each 9-line CSV, total 4096
  expired Registry checks and 8/8 coherent VmHWM/RSS.
- RSS plateaus after round2: managed **4892 KiB**, raw **4444 KiB**.
  Glibc `uordblks`: managed 88672→104768 B (+16096 B),
  raw 81136→81136 B (+0 B). Neither proves leak freedom.

## Source-level explanation under test

Manager sweeps expired weak-key ledgers every **256
registration attempts**. Managed Lifetime does **512 ephemeral
+ 1 fresh + 2 expected quota-failing registrations = 515
attempts per round**, plus one persistent registration at start.
This shifts each end-of-round sample by **three additional
attempts** beyond a sweep phase, plausibly accumulating bounded
unswept weak keys. Running **509 ephemeral Registries per round**
aligns each round to exactly 512 registration attempts. This
diagnostic needs NO new code change.

**Next minimal command:**
```sh
git pull --ff-only origin feature/monitoring-correctness-tests
./tools/benchmark_monitoring_lifetime.sh 8 509 managed \
  > build/monitoring-lifetime-managed-aligned.csv
wc -l build/monitoring-lifetime-managed-aligned.csv
cat build/monitoring-lifetime-managed-aligned.csv
```
Expected 9 CSV lines, 4072 expired checks. Evaluate live heap
trend after warmup. Do NOT conclude leak absent if the trend
merely stabilizes over eight rounds; a profiler/longer run may
still be justified.

The previous owner-verified acceptance remains: 10/10 each
baseline/threaded/ASan/Clang TSan, stress 900/900,
foundation+consumer 13/13; GCC/Clang Static/Shared 11/11
each. MON-OPEN-01/02/03/04 completed.
MON-OPEN-05 four-mode and selected static gates PASS.
MON-OPEN-06 pending the aligned heap trend and optional
allocation-site attribution. MON-OPEN-07 owner-only review.

[Current report](MONITORING_STATIC_LIFETIME_COMPARISON_2026-11-13.md),
[task board](MONITORING_TASKS.md).
