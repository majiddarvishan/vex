# Monitoring indexed-ledger acceptance — 2026-11-10

## Provenance

Source: owner-supplied `Pasted text(5).txt`, 2250 lines.
On branch `feature/monitoring-correctness-tests`, the owner ran
`./tools/verify_monitoring.sh all` and
`./tools/benchmark_monitoring.sh 3 > build/monitoring-profile-after.csv`.
The uploaded log does **not** print the local `git rev-parse HEAD`,
`git status`, or the redirected after-benchmark CSV.

GitHub branch HEAD at the time this report was filed:
`9f0c1adc7cccaecaac230f393f4afc03accdd4f7`. It is not independently proved to be the owner's exact
local checkout SHA; the log does show rebuild of the changed
`metrics_manager.cpp` and `monitoring_validation_contract.cpp`.

## Verification: observed results (latest indexed-ledger code)

| Stage | Result | CTest wall-clock time |
|---|---|---:|
| Baseline | **10/10 PASS** | 0.38 s |
| Threaded | **10/10 PASS** | 0.41 s |
| ASan + UBSan | **10/10 PASS** | 0.63 s |
| Clang 20 TSan | **10/10 PASS** | 0.71 s |
| Stress initial | **10/10 PASS** | 0.49 s |
| Stress 100 repetitions per C++ test | **900/900 PASS** | 31.60 s |
| Full foundation / installed package consumer | **13/13 PASS** | 1.72 s |

All 9 repeated C++ tests were explicitly counted in the uploaded log:
`smoke`, `contract`, `decorators`, `raii`, `validation`,
`lifecycle`, `registration`, `legacy`, and `http`.
Each ran 100 times and passed, total 900 successful executions.
No compiler error, failed CTest, sanitizer diagnostic or runtime
crash was reported.

Final terminal result:
```text
VEX FOUNDATION PASS mode=full
VEX MONITORING ACCEPTANCE PASS all
```

## Performance after-change evidence

The opt-in Release benchmark built successfully, including
`vex_monitoring_benchmark`, and ran at
`2026-11-10T14:33:12Z` on Linux
`6.8.0-147-generic`, 12 logical CPUs,
`CMAKE_CXX_COMPILER:FILEPATH=/usr/bin/c++`.
No error or failure is visible in the benchmark invocation.

However, stdout was redirected to
`build/monitoring-profile-after.csv`.
**The uploaded file contains no CSV rows**, so neither new medians
nor speedup / regression can be calculated. No claim of performance
improvement is made.

Previous *before-change* benchmark:
[MONITORING_PROFILE_2026-11-10.md](MONITORING_PROFILE_2026-11-10.md),
[raw CSV](benchmarks/2026-11-10-monitoring-managed-vs-raw.csv).
Median before-change registration (N=1000):
- one family / many series: Vex 3.070 ms, raw 0.930 ms;
- many families: Vex 17.151 ms, raw 9.568 ms.

## Task disposition

- **MON-OPEN-01 correctness gate: PASS** for the owner-run supported
  Monitoring test matrix including the new weak-owner ordered map and
  320 short-lived Registry churn regression.
- **MON-OPEN-01 performance verdict: PENDING** receipt and analysis
  of `build/monitoring-profile-after.csv`. The task remains open.
- MON-OPEN-05 (portability/static), MON-OPEN-06
  (multi-thread resource performance), and MON-OPEN-07
  (owner review and merge/release) remain open.
- PR #3 remains **Draft and unmerged**, stacked on PR #2.
  Logger, ObjectPool and ThreadPool runtime fixes remain deferred.

## Needed evidence

```sh
wc -l build/monitoring-profile-after.csv
cat build/monitoring-profile-after.csv
```

Expect 37 lines, 1 header and 36 data rows, if all benchmark
measurements were emitted. Do not assume content from a successful
executable exit alone.

## Addendum: after CSV received later on 2026-11-10

The owner's `build/monitoring-profile-after.csv` was subsequently pasted in full: 37 lines, all 36 matched measurements. The earlier “CSV not provided” statement above is historical, **superseded by this addendum**. Median 1000-family Vex registration improved from **17.151 to 11.169 ms** (−34.9%); upstream raw remained **9.568 vs 9.575 ms**. The related 1000-series median increased from 3.070 to 3.257 ms (+6.1%), with overlapping measurement ranges and only three repeats. Full computed table and raw data: [MONITORING_PROFILE_BEFORE_AFTER_2026-11-10.md](MONITORING_PROFILE_BEFORE_AFTER_2026-11-10.md). Remaining resource/variation checks are MON-OPEN-06.
