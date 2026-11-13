# Monitoring MON-OPEN-06: controlled Counter contention and peak-RSS verification

**Evidence:** Owner's Linux Release benchmark output from `2026-11-13T21:26:41Z`,
`./tools/benchmark_monitoring_load.sh 3 --controls > build/monitoring-load-controls.csv`
on branch `feature/monitoring-correctness-tests`; GCC through `/usr/bin/c++`, kernel
`Linux 6.8.0-147-generic`, 12 logical CPUs.

**Source of truth:** [2026-11-13-monitoring-load-controls-3.csv](benchmarks/2026-11-13-monitoring-load-controls-3.csv).
Owner reported `wc -l` **61**; archived CSV independently validated: 1 header,
**60 data rows** = 3 repetitions × 20 workloads. All rows have 14 expected
fields; 3 samples for each combination. The owner terminal output
does not include `git rev-parse HEAD`; exact local checkout SHA remains
unverified. The benchmark binary **compiled and linked successfully**,
then emitted all expected rows without a runtime failure.

## Concurrent Counter throughput — medians of 3 rounds

Rates in **millions of Counter::Increment operations per second**;
construction/registration of Counters is outside the timed update region.

| Type | 1 thread | 4 threads | 8 threads |
|---|---:|---:|---:|
| One shared Counter, Vex-created | 43.889 | 8.811 | 8.730 |
| One shared Counter, direct Prometheus | 44.664 | 9.361 | 8.783 |
| One Counter per thread, Vex-created | 43.971 | 170.957 | 223.972 |
| One Counter per thread, direct Prometheus | 45.133 | 172.752 | 222.250 |

The sharded-vs-shared throughput ratio in this benchmark for Vex-created
Counters is **19.4× at four threads** and **25.7× at eight threads**.
Direct Prometheus has a similar trend (**18.5×** and **25.3×**).

**Conclusion for this workload:** contention on an individual hot Counter
is the principal plausible cause of the shared-update throughput collapse,
not an extra Vex metric-manager lock on the update hot path. Both
`managed` and `raw` modes call the underlying
`prometheus::Counter::Increment()` directly during the timed section;
Vex manages creation, not each increment. Per-run scheduling noise is
substantial (e.g. 4-thread Vex shared: 10.123–34.554 ms and 8-thread
raw sharded: 0.886–1.851 ms), so 3-round medians must not be presented
as production throughput guarantees.

One Counter per thread is a **different logical metric topology**, with
more labeled series, storage/scrape and cardinality overhead. It should
be used only when the semantics and quotas allow per-worker series;
do not universally recommend extra labels or replace an aggregate
Counter without considering aggregation, identity and lifecycle.

## Concurrent registration of 1000 series

| Path | 1 thread | 4 threads | 8 threads |
|---|---:|---:|---:|
| Vex-created: ops/second median | 335,887 | 238,584 | 232,773 |
| Direct Prometheus: ops/second median | 979,507 | 666,810 | 663,644 |

Adding worker threads did **not** improve this single-Family registration
workload; Vex and upstream Registry both show lower throughput. This is
consistent with internally synchronized registration and short-workload
overheads; it does **not** attribute precise CPU time to a particular
mutex. Avoid speculative changes to MetricsManager without profiling.

## Collection, HTTP, current/peak RSS

| Operation | Median of per-run p95 |
|---|---:|
| 100 Registry::Collect calls while another thread updates 1000 series | **774.616 µs** |
| 50 sequential loopback HTTP /metrics scrapes over 100 series | **187.720 µs** |

They are medians of **three within-run p95 values**, not one p95 from
all pooled requests. The HTTP scenario is sequential, not concurrent
HTTP clients.

- Current RSS snapshots across 60 samples: **5,560–7,648 KiB**.
- Linux `peak_rss_kib` now uses the `VmHWM` process-status field
  and is **greater than or equal to current RSS in all 60/60 records**.
  Previously, `getrusage` peak was below current RSS in 32 of 33
  records. That inconsistency is resolved by the new instrumentation.
- Peak RSS is a **whole-process high-water mark** and snapshots are
  influenced by allocator caching, other workloads and prior phases;
  these values are **not** per-metric retained memory and do **not**
  prove absence of a leak.
- `heap_live_bytes` comes from `mallinfo2().uordblks` (when
  available), not an allocation count or call-site attribution.
  `rss_delta_kib` may be zero or negative and is not a leak signal.

## Task disposition

**MON-OPEN-06 measured acceptance:** New controls **PASS** for complete
schema, sample counts, final Counter totals (validated by the binary),
absence of peak/current RSS contradictions (60/60), and comparison of
Vex-created versus direct underlying Counter updates. CPU/throughput
observations and limitations are recorded. The previously observed
hot-Counter contention is **not evidence of a Vex-specific bug**.
No production runtime code change is justified by this data.

**Remaining narrower research / optional future evidence:** allocation
call-site profiling requires heaptrack (only if already installed);
full retained-memory/leak characterization requires duration/steady-state
controls and more memory samples; the historical +6.1% one-family
1000-series registration change cannot be judged causally without
running pre- and post-index versions under **the same controlled harness**
with sufficient repeats. Optional targeted `clang-tidy` remains an
unexecuted MON-OPEN-05 static-analysis subgate. Keep these caveats
explicit; no unmeasured result is called PASS.

**Other gates:** MON-OPEN-01/02/03/04 completed;
MON-OPEN-05 compiler/linkage/package-consumer matrix **four modes ×
11/11 PASS**; MON-OPEN-07 requires owner's explicit review/merge
decision. PR #3 stays Draft/unmerged and is stacked on Foundation.
No code from Logger, ObjectPool or ThreadPool was changed.
