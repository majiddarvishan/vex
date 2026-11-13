# Vex Monitoring handoff — 2026-11-13 Counter controls verified

Branch `feature/monitoring-correctness-tests` · [Draft PR #3](https://github.com/majiddarvishan/vex/pull/3),
stacked on Foundation. No merge or non-Monitoring runtime changes.

## Verified to date

- Baseline/threaded/ASan+UBSan/Clang TSan: **10/10 per mode**;
  repeated stress **900/900**, full foundation with installed
  package consumer **13/13**.
- GCC/Clang Static/Shared: **11/11 PASS in all four modes**,
  including each installed external consumer; `$ORIGIN`
  fixes Shared RUNPATH in tested staging layout.
- 15-round controlled paired registration: 180 samples;
  Vex many-families N=1000 median **11.061ms**,
  Vex one-family N=1000 median **3.237ms**.
- Previous resource/load benchmark: 33 samples; showed hot
  shared Counter contention, but `getrusage` peak RSS was
  inconsistent in 32/33 records.
- **New owner-verified `--controls` load benchmark: 60 samples,
  all 60/60 peak RSS >= current RSS.** Vex shared Counter throughput
  ~43.89M / 8.81M / 8.73M ops/s at 1/4/8 threads,
  raw Prometheus ~44.66M / 9.36M / 8.78M; Vex per-thread
  sharded ~43.97M / 170.96M / 223.97M, raw sharded
  ~45.13M / 172.75M / 222.25M.
- Concurrent Collect p95 **774.616µs** and sequential HTTP
  p95 **187.720µs** (medians of three run-level p95s).

[Complete new analysis](MONITORING_LOAD_CONTROLS_2026-11-13.md),
[raw 60-row CSV](benchmarks/2026-11-13-monitoring-load-controls-3.csv),
[task board](MONITORING_TASKS.md).

**Conclusion:** Shared Counter contention is reproducible in the
underlying Prometheus Counter; no Vex-specific update lock can be
inferred. Peak accounting is internally consistent. Additional
cardinality from per-worker Counter sharding is not a universal
production recommendation.

**Remaining:** MON-OPEN-05 optional clang-tidy not run (four-mode
packaging passed); MON-OPEN-06 allocation sites/long-running retention
and historical same-harness revision A/B optional/research still open.
MON-OPEN-07 explicit owner review/merge authorization. If heaptrack
is installed, `./tools/profile_monitoring_allocations.sh` can
collect allocations; it never installs the profiler. Do not
claim heap profile or leak absence without such evidence.

Only Monitoring benchmark/docs updated after owner's production
test acceptance; PR stays Draft/unmerged.
