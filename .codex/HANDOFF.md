# Vex Monitoring handoff: four-mode packaging PASS, resource diagnostics pending

Branch `feature/monitoring-correctness-tests`, [Draft PR #3](https://github.com/majiddarvishan/vex/pull/3), foundation base. No merge.

**Verified:** full Monitoring baseline/threaded/ASan/TSan 10/10 each, stress 900/900, foundation/package 13/13; GCC & Clang × Static & Shared **11/11 each**, including installed-package consumers. `$ORIGIN` RUNPATH resolved previous Shared loader failures.

**New raw CSVs:** [180 controlled rows](benchmarks/2026-11-10-monitoring-controlled-15.csv), [33 concurrent/resource rows](benchmarks/2026-11-10-monitoring-load-3.csv). Latest medians at N=1000: managed many_families **11.061ms** vs raw **9.678ms**; managed many_series **3.237ms** vs raw **0.942ms**. Shared single-Counter throughput 1/4/8 threads **19.74M/2.35M/4.30M ops/s**. Concurrent Collect p95 **759.385µs**, sequential HTTP p95 **193.933µs**.

**Important:** peak RSS < current RSS in 32/33 records; unusable. The existing benchmark lacks raw/sharded Counter controls and a matched historical warmup/order pre-vs-post comparison. No allocation count/profile was supplied. MON-OPEN-06 remains open pending minimal benchmark corrections. MON-OPEN-05 package/compiler matrix PASS but optional clang-tidy remains unverified; dirty CPM dependency cache is noted. MON-OPEN-07 is owner-only review/merge.

[Complete result and limitations](MONITORING_PORTABILITY_LOAD_RESULTS_2026-11-10.md) and [task board](MONITORING_TASKS.md).

No Monitoring core runtime or unrelated module behavior was changed by data-report commits.
