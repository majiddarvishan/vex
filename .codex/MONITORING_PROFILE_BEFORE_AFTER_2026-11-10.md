# Monitoring MON-OPEN-01 — before/after indexed-ledger benchmark

**Date:** 2026-11-10.  
**Branch:** `feature/monitoring-correctness-tests` (owner's exact local SHA not reported).  
**Before:** [raw paired CSV](benchmarks/2026-11-10-monitoring-managed-vs-raw.csv) at 14:19:52 UTC.  
**After:** [raw paired CSV](benchmarks/2026-11-10-monitoring-managed-vs-raw-after.csv), benchmark at 14:33:12 UTC, subsequently supplied in chat.  
**Environment:** Linux 6.8.0-147-generic, 12 logical CPUs, CMake C++ compiler `/usr/bin/c++`, Release; the previous benchmark used GCC 13.3.0.  
**Validation:** Each CSV has 37 lines (header + 36 samples), three repetitions for all four shapes at 10, 100, 1000. Family and sample counts validated for the corresponding shape.

## Registration results: medians of 3 executions

| Shape | N | Before median (ms) | After median (ms) | Delta (ms) | Change | Before range (ms) | After range (ms) |
|---|---:|---:|---:|---:|---:|---|---|
| `many_series` | 10 | 0.062 | 0.045 | -0.017 | -27.4% | 0.056–0.127 | 0.040–0.134 |
| `raw_many_series` | 10 | 0.011 | 0.011 | 0.000 | 0.0% | 0.011–0.011 | 0.011–0.012 |
| `many_families` | 10 | 0.027 | 0.028 | 0.001 | 3.7% | 0.026–0.030 | 0.027–0.028 |
| `raw_many_families` | 10 | 0.014 | 0.015 | 0.001 | 7.1% | 0.014–0.015 | 0.014–0.015 |
| `many_series` | 100 | 0.299 | 0.335 | 0.036 | 12.0% | 0.296–0.323 | 0.310–0.361 |
| `raw_many_series` | 100 | 0.090 | 0.095 | 0.005 | 5.6% | 0.090–0.091 | 0.092–0.099 |
| `many_families` | 100 | 0.363 | 0.324 | -0.039 | -10.7% | 0.358–0.372 | 0.316–0.454 |
| `raw_many_families` | 100 | 0.200 | 0.204 | 0.004 | 2.0% | 0.191–0.203 | 0.201–0.208 |
| `many_series` | 1000 | 3.070 | 3.257 | 0.187 | 6.1% | 3.040–3.396 | 3.106–3.285 |
| `raw_many_series` | 1000 | 0.930 | 0.938 | 0.008 | 0.9% | 0.915–0.953 | 0.927–0.951 |
| `many_families` | 1000 | 17.151 | 11.169 | -5.982 | -34.9% | 17.087–17.272 | 11.087–11.169 |
| `raw_many_families` | 1000 | 9.568 | 9.575 | 0.007 | 0.1% | 9.561–9.572 | 9.468–9.831 |

## Interpretation

- **Primary objective achieved in this microbenchmark:** 1000 Vex managed Families register in 11.169 ms after, down from 17.151 ms before; **34.9% lower latency**, or 1.54× faster in the observed scenario. Before range 17.087–17.272 ms and after range 11.087–11.169 ms do not overlap.
- **Upstream control stable:** Direct `prometheus-cpp` 1000-family median 9.568 → 9.575 ms (change 0.07%). The residual observed Vex-vs-raw gap dropped from 7.583 to 1.594 ms (**79.0% reduction in the difference**).
- **Other workload:** Vex 1000-series/one-family registration increased from 3.070 to 3.257 ms (**+6.1%**), while direct upstream changed from 0.930 to 0.938 ms. The before and after per-run ranges overlap (Vex 3.040–3.396 vs 3.106–3.285 ms), so this is a **potential** regression signal, not conclusive.
- At 1000, update times remain broadly similar between the managed and raw scenarios; the improvement primarily concerns **registration**, not per-counter increments. Collection sample sizes remain identical between before/after shapes.
- Absolute measurements are in milliseconds for the **whole synthetic workload**; they are not per-metric latency, production p95/p99, RSS or allocation data. N=3 and fixed workload ordering limit confidence and generalization.

## Source-level rationale

The pre-change Vex manager looked up tracked metric families through a vector scan, and swept expired Registry records on each registration. The updated code indexes them with an ownership-aware ordered map (`std::owner_less` over weak Registry control blocks) and periodically cleans expired entries. The upstream `prometheus-cpp` Registry family registration remains unchanged, and its raw timings remain similar. This is **consistent with** reduced Vex bookkeeping overhead, though a CPU or allocation profiler has not independently attributed all costs.

## Acceptance and remaining work

The owner's prior post-index `verify_monitoring.sh all` output PASSED: baseline/threaded/ASan+UBSan/Clang TSan 10/10 each, stress 900/900, and foundation/package consumer 13/13. Evidence: [MONITORING_INDEXED_ACCEPTANCE_2026-11-10.md](MONITORING_INDEXED_ACCEPTANCE_2026-11-10.md).

**MON-OPEN-01 primary objective: measured improvement and correctness gates PASS.** Treat the targeted optimization as completed **within this benchmark scope**. Remaining resource/scale characterization (RSS, heap allocations, thread contention and whether the small 1000-series regression persists) is explicitly assigned to **MON-OPEN-06**, not silently waived.

No additional runtime changes were made after the owner's test run; only .codex evidence and task status updates were committed. Do not infer production readiness from a microbenchmark, or merge the Draft PR without explicit owner approval.
