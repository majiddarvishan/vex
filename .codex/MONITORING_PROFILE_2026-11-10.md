# Monitoring registration scaling: matched Vex vs prometheus-cpp

**Evidence received:** 2026-11-10, 14:19:52 UTC  
**Command:** `./tools/benchmark_monitoring.sh 3 > build/monitoring-profile.csv`  
**GitHub branch at receipt:** `feature/monitoring-correctness-tests` / `4855c05323ab03a2254eacad85b5caa0c653645a` (owner checkout SHA not printed).  
**Environment:** Linux 6.8.0-147-generic, 12 logical CPUs, `/usr/bin/c++`, Release configuration (previous benchmark showed GCC 13.3.0).  
**Observed:** Build and benchmark execution succeeded; `wc -l` reported **37** (1 CSV header + 36 records).  
**Source:** user-pasted full 36-row CSV, archived byte-for-byte at [benchmarks/2026-11-10-monitoring-managed-vs-raw.csv](benchmarks/2026-11-10-monitoring-managed-vs-raw.csv).

## Median registration time (ms) — 3 repeats

| N | Vex one family / N series | Raw one family / N series | Vex N families | Raw N families |
|---:|---:|---:|---:|---:|
| 10 | 0.062 | 0.011 | 0.027 | 0.014 |
| 100 | 0.299 | 0.090 | 0.363 | 0.200 |
| 1000 | 3.070 | 0.930 | 17.151 | 9.568 |

At 1000:
- `many_series`: Vex 3.070 ms vs raw 0.930 ms; **+2.140 ms**, approx **3.30×** ratio.
- `many_families`: Vex 17.151 ms vs raw 9.568 ms; **+7.583 ms**, approx **1.79×** ratio.
- Raw 1000 families vs raw 1000 series: approx **10.29×** registration ratio.
- Vex `update_ms` and raw `update_ms` are much closer in this run;
  don't rewrite the update hot path based on this evidence.

These comparisons are descriptive only: timings include different
validation/accounting behavior by design, order/warmup effects, and
only three repetitions. A ratio is not proof that a specific function
consumed the time or that a production SLA is violated.

## Source-level explanation to investigate

`prometheus-cpp` v1.3.0 `Registry::Add` in
`core/src/registry.cc` searches each typed Family vector via
`std::find_if` by family name, and checks Family names in other
typed vectors, so *creating many different Families* involves
repeated linear scans (cumulative quadratic growth). This is
visible in the upstream implementation, and the raw benchmark
still incurs ~9.568 ms for 1000 Families.

Vex's `MetricsManager::RegisterTracked` additionally stores a
`std::vector<TrackedMetricSeries>`, uses `std::remove_if` to
check expired Registries on **every registration** and
`std::find_if` to locate matching Registry/name/type entries.
These are additional linear scans and can also scale quadratically
with Family count. That is a **source-level candidate explanation**,
not CPU profiler attribution. The API contract currently requires
weak Registry identities, HELP/bucket signature consistency, bounded
series quotas and rollback of failed registrations.

## Action

1. Replace Vex's per-registration linear metadata lookup with an
   ownership-correct indexed structure, preserving Registry weak
   ownership, quota/help conflict behavior and rollback.
2. Make expired Registry garbage collection amortized rather than
   scanning all tracked families for every insertion.
3. Run complete Monitoring acceptance after implementation.
4. Rerun **the same paired Release benchmark** on the owner's machine,
   with the same toolchain/configuration. Compare medians and ranges
   for Vex and raw paths. Only then claim a measured improvement.
5. Do not modify the upstream prometheus-cpp implementation in
   this Vex PR. Longer-term schema guidance: favor bounded labels in
   a Family where metric semantics permit.

**Current task:** MON-OPEN-01 evidence collected; optimization
candidate needs a fresh real-host build/TSan and before/after
benchmark. Do not mark MON-OPEN-01 complete yet.


## Follow-up indexed-ledger optimization candidate (not yet benchmarked)

The measured raw CSV above is the **before** baseline. After the owner
submitted it, Vex Monitoring-only changes were committed:

- Replace `tracked_metric_series_`'s vector of Registry/family records
  and repeated `remove_if` / `find_if` full scans with a
  `std::map<TrackedMetricKey, TrackedMetricSeries, TrackedMetricKeyLess>`.
  A key consists of weak Registry ownership identity, metric name
  and type. `std::owner_less<weak_ptr<Registry>>` handles different
  owners even when the underlying allocation address is recycled.
  Ordered lookup is logarithmic in the number of tracked Families,
  rather than scanning all of them per registration.
- Amortize removal of expired Registry ledger entries by sweeping
  after every 256 registration calls, under the existing manager mutex.
  Weak ownership continues to ensure Registry lifetime is never
  extended by bookkeeping. Reset clears the ledger and the sweep
  counter.
- Preserve per-Family HELP and histogram signatures, quotas and
  exception rollback. Failed first registration erases its provisional
  `std::map` entry by iterator instead of the old vector pop_back.
- Add a validation regression that constructs 320 short-lived Registry
  owners, confirms the manager does not extend their lifetimes, checks
  that the long-lived owner's quota persists, and ensures a later
  Registry with a distinct owner can use the same metric name and
  different HELP without inheriting old metadata.

**Verification state:** A small standalone C++17 owner-less comparator
and map insertion/sweep fixture compiled and passed locally with GCC
14.2.0. This is not a Vex build. **The full Vex baseline,
ASan/UBSan, Clang TSan, stress and package consumer have NOT been
run for these changes.** The user's earlier 10/10 and 900/900
test results and the CSV median values all belong to the pre-index
code. No after-change performance results exist, so **do not claim
any measured speedup or close MON-OPEN-01**.

Next test commands on the latest branch:

```sh
git pull --ff-only origin feature/monitoring-correctness-tests
git rev-parse HEAD
git status --short
./tools/verify_monitoring.sh all
./tools/benchmark_monitoring.sh 3 > build/monitoring-profile-after.csv
```

Compare before/after results on the same machine and with identical
Release toolchain and shape definitions. If CTest or sanitizer fails,
fix the regression first; report both improvement and any tradeoffs
(e.g. quota registry cleanup behavior, RSS allocations).
