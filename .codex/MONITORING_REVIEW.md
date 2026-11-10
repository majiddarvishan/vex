# Monitoring follow-up code review — 2026-11-10

**Repository:** `majiddarvishan/vex`  
**Branch:** `feature/monitoring-correctness-tests`  
**PR:** https://github.com/majiddarvishan/vex/pull/3 (Draft, stacked)
**Review scope:** Monitoring code, tests, runtime instrumentation and
benchmarks. Other module runtime fixes remain deferred.

## Baseline evidence

Owner-run `./tools/verify_monitoring.sh all` on the prior Monitoring
implementation PASSED: baseline 9/9, threaded 9/9, ASan/UBSan 9/9,
Clang 20 TSan 9/9, 800/800 C++ stress reruns, and full foundation
12/12 with package consumer. This is confirmed from the owner-pasted
terminal log. The exact local checkout commit was not printed.

## Additional issues discovered during follow-up

| ID | Severity | Observation | Action | Current validation |
|---|---|---|---|---|
| MON-REV-01 | High | ScopedCounter/Gauge previously called Family::Add, adopted pre-existing non-guard-owned series with identical labels, and deleted them at scope exit | Detect already present Family::Has(effective labels), reject an externally owned series with invalid_argument; do not erase it | **New regression added; NOT YET RUN** |
| MON-REV-02 | Medium | Scope ownership tests assumed no default/dynamic labels; e.g. CONTAINER_ID set in CI could change effective labels | Configure deterministic default label, compare Has() with MergeLabels() and test external collision under merged labels | **New regression added; NOT YET RUN** |
| MON-REV-03 | Medium | Opt-in benchmark only measured single-family registration/collect, missing updates and many-family scaling | Add both shapes, 100 updates per series, clean CSV separated from build/environment diagnostics | **Benchmark binary/runner NOT YET BUILT or RUN** |

## Explicitly supported lifetime/ownership contract

- A caller must keep the Registry and Family alive while accessing
  raw metric references, ScopedTimer, ScopedCounter, ScopedGauge or
  ScopedInFlightGauge. `MetricsManager::Reset()` is quiescent-only.
- `ScopedCounter/ScopedGauge` reject adoption of a series that was
  externally added before their first guard. They refcount multiple
  overlapping guards only within the scope-guard ownership domain.
- Callers must not externally `Family::Remove` or replace series
  while guards are active. Default labels must remain stable during
  scoped guard acquisition. `SetDefaultLabels` itself is protected
  by a manager mutex, **but the full Family::Has + add across two
  helper calls is not an atomic live label-reconfiguration protocol**.
- Manager-created metrics are protected by per-family series caps
  only when callers opt in. Direct raw prometheus-cpp Family::Add
  bypasses those manager quota limits. This is documented.
- Registration and Reset are not guaranteed to preserve old raw
  metric references when the last Registry owner drops.

## Performance baseline methodology

Run `./tools/benchmark_monitoring.sh 3` in a dedicated Release-mode
build directory. Report the environment and machine configuration,
and keep CSV rows for:
- many_series: 10/100/1000 distinct labels in one Counter family
- many_families: 10/100/1000 separate Counter families
- registration_ms, update_operations (100 per series), update_ms,
  collect_ms, family count and sample count

No measured performance values exist in this review. Comparisons
across machines/toolchains require environment matching. Inspect
scaling before changing the registration data structure.

## Acceptance gates to rerun on latest code

```sh
git pull --ff-only origin feature/monitoring-correctness-tests
./tools/verify_monitoring.sh all
./tools/benchmark_monitoring.sh 3
```

The first command is a **required regression gate** for the ownership
fix. The second is an optional empirical performance baseline. Prior
successful runs do not cover the new ownership change or unbuilt
benchmark code. Never mark them PASS without new owner logs.
Do not merge or remove Draft status until owner review/decision.
