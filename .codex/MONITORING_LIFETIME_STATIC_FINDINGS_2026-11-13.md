# Vex Monitoring — 4096-Registry lifetime and LLVM20 clang-tidy findings

**Owner execution:** 2026-11-13 21:48:56 UTC, `feature/monitoring-correctness-tests`,
checkout **`6df93958decb1ccc9f68176e0f817af2602e04b9`**.
OS Linux 6.8.0-147-generic, GCC 13.3.0 for Lifetime Release,
`clang-tidy` Ubuntu LLVM **20.1.2**.

## Lifetime acceptance: completed / weak ownership correctness PASS

Raw evidence: [2026-11-13-monitoring-lifetime-managed-8x512.csv](benchmarks/2026-11-13-monitoring-lifetime-managed-8x512.csv).
Owner-supplied `wc -l` = **9**, file contains 1 header + 8 round
records; counts independently checked at 512, 1024, 1536, 2048,
2560, 3072, 3584 and **4096**. The benchmark completed without an
ownership, quota, HELP or `VmHWM < VmRSS` exception. In program logic,
every created transient Registry is weakly referenced and must expire
before `expired_registry_checks` increments; a live Registry's
Counter value and per-family quota are checked each round.

| Metric | After round 1 | After round 8 | Interpretation |
|---|---:|---:|---|
| Completed weak-owner expiry checks | 512 | **4096** | all bounded operations completed |
| Current RSS | 4,808 KiB | 4,940 KiB | +132 KiB after round 1; flat at 4,940 KiB for rounds 2–8 |
| Peak RSS | 4,808 KiB | 4,940 KiB | coherent with current RSS in 8/8 |
| glibc live allocator `mallinfo2().uordblks` | 88,672 B | 104,784 B | **+16,112 B over 7 inter-round intervals**, monotonically increasing |
| Heap delta versus benchmark initialization | +7,904 B | +24,016 B | baseline computed from initial heap 80,768 B |

**Important memory finding:** while observed RSS stabilized,
`uordblks` grew from round to round by approximately 2–3 KiB.
This is a **real allocator-accounting observation**, not a confirmed
Vex leak. It might reflect glibc allocator state, lazy internal
caches/containers, or weak ownership bookkeeping. `VmRSS` stability
does not refute it; `uordblks` also does not identify allocation sites
or reliably separate retained memory from allocator effects.

**Further attribution planned (unverified):** the Lifetime tool gained
an optional `raw` mode for a distinct-process direct prometheus-cpp
Registry+Counter baseline. The default `managed` mode preserves the
previous CSV schema; raw mode bypasses manager quota tracking
intentionally, so it is not a perfect equivalent of full managed
validation. Compare trends over 8 rounds on the same machine and
revision; use a profiler if unexplained growth persists.
Do not assert leak freedom or leak presence on these eight samples.

## Static analysis: 5 actionable errors, 3 failed translation units

Owner-supplied `build/monitoring-static.log` is **45 lines**.
It concluded with:
```text
MONITORING_STATIC_SUMMARY checked=5 failed=3
```

| File:line (original) | Check | Finding |
|---|---|---|
| `src/monitoring/decorators.cpp:33` | `bugprone-easily-swappable-parameters` | ResultTracker(success_counter, failure_counter) positional public constructor |
| `src/monitoring/decorators.cpp:45` | `bugprone-easily-swappable-parameters` | TimedResultTracker(duration_histogram, success_counter, failure_counter) positional public constructor |
| `src/monitoring/health_check.cpp:6` | `performance-unnecessary-value-param` | HealthCheck::RegisterHealthMetrics(std::shared_ptr<Registry>) retains an owning argument during registration |
| `src/monitoring/raii_guards.cpp:87` | `bugprone-empty-catch` | rollback removal secondary exception suppressed while original bookkeeping exception rethrows |
| `src/monitoring/raii_guards.cpp:121` | `bugprone-empty-catch` | noexcept guard destructor/move-assignment cleanup cannot propagate removal exceptions |

The five checked source files were
`decorators.cpp`, `health_check.cpp`, `metrics.cpp`,
`metrics_manager.cpp` and `raii_guards.cpp`.
The latter two listed findings belong to `raii_guards.cpp`;
`metrics.cpp` and `metrics_manager.cpp` produced **no surfaced
actionable diagnostic**. The tool also printed large
`warnings generated` counts from dependencies/suppressed scopes;
those counts do **not** mean the project has 26,000 actionable
Monitoring findings. The configured local warnings-as-errors policy
correctly failed on the five reported diagnostics.

## Changes after the owner run (NOT YET VERIFIED)

1. Added **five narrowly targeted** `NOLINTNEXTLINE` comments on the
   exact flagged parameter or catch lines, with explicit rationale.
   The public success/failure Counter constructor API is preserved;
   `HealthCheck` keeps owning shared_ptr semantics during
   registration; rollback and noexcept guard cleanup preserve the
   original exception/termination contracts. **No code execution
   or signature semantics were altered** by these changes.
2. Added optional fresh-process `prometheus-cpp` raw lifetime mode
   to the existing benchmark as a control for process/allocator
   trends (managed default unchanged, 9-line format unchanged).
   Raw mode intentionally does not check Vex's per-Family quota.
3. No global check disablement or generic warning suppression was
   introduced. **Static analysis is FAIL on the owner-tested
   previous commit and requires a new run before PASS is claimed.**

## Minimal owner follow-up

```sh
git pull --ff-only origin feature/monitoring-correctness-tests

./tools/verify_monitoring_static.sh \
  > build/monitoring-static-after.log 2>&1

./tools/benchmark_monitoring_lifetime.sh 8 512 managed \
  > build/monitoring-lifetime-managed-new.csv
./tools/benchmark_monitoring_lifetime.sh 8 512 raw \
  > build/monitoring-lifetime-raw.csv

tail -n 30 build/monitoring-static-after.log
cat build/monitoring-lifetime-managed-new.csv
cat build/monitoring-lifetime-raw.csv
```

Expected 9 lines in each Lifetime CSV. For static analysis, require
`MONITORING_STATIC_PASS checked=5 failed=0`. Any new diagnostics
must be inspected; do not claim PASS merely because the shell prompt
returned. Follow up the heap trend using controls/profiler evidence
without inventing a memory leak threshold.

**Other verified gates remain intact:** baseline/threaded/ASan+UBSan/
Clang20 TSan 10/10 each; 900/900 stress; foundation 13/13; GCC/Clang
Static/Shared installed package consumers 11/11 each; 60/60 coherent
RSS/peak RSS in earlier concurrency tests. PR #3 stays Draft and
unmerged, stacked on Foundation; unrelated runtime modules unchanged.
