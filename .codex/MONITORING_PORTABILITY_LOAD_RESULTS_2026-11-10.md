# Monitoring: verified four-mode portability and resource/controlled benchmarks

**Owner evidence:** `Pasted text(8).txt`, terminal log from branch `feature/monitoring-correctness-tests`. Exact local HEAD SHA was not printed. These are owner-run measurements, not assistant-executed benchmarks. Dated 2026-11-10 per previous runs (uploaded later).

## MON-OPEN-05 — portability and installed package consumers

| Mode | CTest total | Installed consumer | Status |
|---|---:|---|---|
| GCC 13.3 Static | 11/11 | PASS | PASS (preceding owner log) |
| GCC 13.3 Shared | **11/11** | **PASS** | **PASS, newly confirmed** |
| Clang 20.1.2 Static | 11/11 | PASS | PASS (preceding owner log) |
| Clang 20.1.2 Shared | **11/11** | **PASS** | **PASS, newly confirmed** |

The isolated PIC-compatible prometheus-cpp build removed the original Shared link blocker; the installed ELF `$ORIGIN` RUNPATH subsequently resolved the `libprometheus-cpp-pull.so.1.3` loader failure for both Shared installed-package consumers. No `LD_LIBRARY_PATH`, modification to `/usr/local`, or global package reinstall was needed in the provided logs. Both Shared test runners ended with `PORTABILITY SUMMARY passed=1 failed=0`.

Remaining caveats: `CPM: Cache for prometheus-cpp ... is dirty` appears in both Shared configurations (nonfatal reproducibility warning); optional `clang-tidy` was **NOT RUN**, so static-analysis findings cannot be assumed absent. Cross-OS portability was not tested.

Earlier post-macro/full-source verification remained PASS: baseline/threaded/ASan+UBSan/Clang20 TSan **10/10 each**, stress **900/900**, full foundation/package **13/13**. No Monitoring runtime source change was made after that acceptance; only install RUNPATH, docs and test tooling.

## MON-OPEN-06 — 15-repeat warmed/rotated registration

Raw data: [controlled benchmark, 180 records](benchmarks/2026-11-10-monitoring-controlled-15.csv). File has **181 lines**, four shapes × three sizes × 15 samples each.

| Registration shape, N=1000 | Median (ms) | Observed range (ms) |
|---|---:|---:|
| Managed, 1000 series in 1 Family | **3.237** | 3.105–3.487 |
| Raw Prometheus, 1000 series | **0.942** | 0.929–1.002 |
| Managed, 1000 distinct Families | **11.061** | 10.801–11.322 |
| Raw Prometheus, 1000 Families | **9.678** | 9.484–10.051 |

For the 1000-Family workload, measured post-index registration is close to the earlier post-index median 11.169ms, and substantially below the historical pre-index 17.151ms. These are **different benchmark protocols**, so do not claim a controlled pre/post test from this comparison.

For 1000 series in one Family, historical original pre/post medians were 3.070ms and 3.257ms (+6.1%, n=3, overlapping distributions); current warmed/rotated value is 3.237ms (n=15). This does not conclusively prove or disprove a true regression without an old-revision run under the same harness.

## MON-OPEN-06 — concurrency, scrape latency and process memory

Raw data: [multithread and resource benchmark, 33 records](benchmarks/2026-11-10-monitoring-load-3.csv). File has **34 lines**, three rounds × eleven workloads. Rates are the **median of 3 reported ops/s samples**, rounded; short-run scheduler noise is apparent.

| Scenario | 1 thread | 4 threads | 8 threads |
|---|---:|---:|---:|
| Managed registration of 1000 series (ops/s) | 168,810 | 121,512 | 247,699 |
| Raw registration of 1000 series (ops/s) | 338,614 | 217,555 | 491,112 |
| Updates of **one shared Counter** (ops/s) | 19.74M | 2.35M | 4.30M |

The shared-Counter loss of aggregate throughput with more writers is a **measured contention symptom** for this particular hot series, but not proof of a Vex wrapper lock: the benchmark updates the returned prometheus-cpp Counter directly. There is **no raw-counter update control** or per-thread isolated-counter workload yet. Registration 1/4/8-thread rates fluctuate and are not demonstrably monotonic.

| Operation | Median per-run p50 | Median per-run p95 | Median per-run p99 |
|---|---:|---:|---:|
| Collect while another thread updates 1000 Counters (µs) | 711.230 | **759.385** | 831.812 |
| Sequential loopback HTTP GET /metrics with 100 series (µs) | 168.279 | **193.933** | 198.267 |

These are medians of three per-run percentiles, **not** percentiles pooled over all runs; the HTTP workload measures sequential requests, not concurrent scrape clients.

RSS snapshots across phases range **5,512–7,992 KiB** in the single benchmark process. `heap_live_bytes` is glibc allocator state, **not allocation count**, and per-phase RSS differences are not per-series retained memory or evidence of a leak. A critical instrumentation limitation: `peak_rss_kib` was less than simultaneous `rss_kib` in **32 of 33 rows**, so the reported process peak is **not valid for acceptance** and requires fixing/reevaluation.

## Task decisions

**MON-OPEN-05:** all four compiler/linkage/package-consumer cases PASS. Retain optional targeted clang-tidy/static analysis and dirty-cache reproducibility as separate unverified follow-up, rather than mislabeling them failures of the passed packaging matrix.

**MON-OPEN-06:** data collection and basic performance/resource characterization complete, but still OPEN for peak-RSS instrumentation correction, shared-vs-sharded and raw Counter control, optional allocation profiling (only if heaptrack already installed), and a matched old/new methodology if regression attribution is required.

**MON-OPEN-07:** owner-only code review/merge decision; PR #3 stays Draft/unmerged. Logger, ObjectPool, ThreadPool behavior remains unchanged.

## Subsequent benchmark diagnostic enhancement — not yet owner tested

To test whether the measured throughput collapse on one shared Counter originates from intrinsic prometheus-cpp cache-line contention rather than Vex wrapper overhead, an **optional**, opt-in mode was added:
```sh
./tools/benchmark_monitoring_load.sh 3 --controls > build/monitoring-load-controls.csv
```
The old invocation `benchmark_monitoring_load.sh 3` is preserved and still emits 34 CSV lines. The new mode emits **61 lines**: 3 runs × (3 thread counts × [managed/raw registration, existing managed shared update, new raw shared update, managed sharded update, raw sharded update] + Collect + HTTP) = 60 records plus header. The new controls use the same 25,000 increments/thread, distinguish hot shared vs independent writer-local series, check final counts, and keep registration/counter construction outside the timed update region. The results are **not comparable** to the previous 33-row CSV as an identical workload mix because additional cases change execution order and process state.

For memory integrity, Linux peak RSS is now read via `VmHWM` in the same `/proc/self/status` accounting family as `VmRSS`, instead of comparing `getrusage` to `VmRSS`. If a peak still falls below current RSS, it is emitted as **-1 (invalid/unavailable)**, not as a plausible value. This does not guarantee transient memory peaks were captured and does not substitute for heap profiling.

**No full Vex build or benchmark run has occurred for this latest tool change.** Previous owner results above remain historical observations; do not close MON-OPEN-06 until a fresh controlled-load output is inspected.
