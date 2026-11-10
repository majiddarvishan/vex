## MON-OPEN-04 HTTP integration test candidate (not yet run)

A real loopback-only /metrics integration test has been added after
the last owner-run acceptance. It uses `prometheus::Exposer("127.0.0.1:0")`
and `GetListeningPorts()` as provided by prometheus-cpp v1.3.0.
No fixed port, external Prometheus instance, curl dependency, or
outbound network request is needed. Assertions cover:
1. HTTP 200 and counter, gauge, histogram names/labels/samples
   from both main and subsystem registries.
2. Registration and expiration of an independent transient Registry.
3. Owner release, manager Reset and a subsequent scrape with no
   expired Registry metrics.
4. Clean Exposer teardown.

The test is UNIX-only and has a 30-second CTest timeout. **No owner-run
evidence yet**, so MON-OPEN-04 is an implementation candidate,
not closed. Main acceptance matrix after this addition is expected
to have 10 Monitoring CTests and 13 full foundation CTests on Linux.

---

## Additional MON-OPEN-02/03 correctness review — candidate, not run

- **Confirmed bug:** Scoped guard acquisition computed effective labels
  before `Family::Has`, then called `MetricsManager::add_counter/gauge`
  which **merged labels a second time**. A concurrent
  `SetDefaultLabels` could change the actual series identity between
  ownership check and Add. The scoped code now validates one snapshot
  and passes it directly to both Prometheus operations; this removes
  the internal reconfiguration TOCTOU.
- **Unsupported raw mutations:** Prometheus `Family::Has` and
  `Family::Add` are separate locked calls. Concurrent external
  `Family::Add/Remove` cannot be made atomic through the existing
  public Vex wrapper. Explicit contract: no uncoordinated direct
  Family/Registry mutations while a scoped guard is active or being
  acquired; no destruction of the owning Registry/Family. The
  manager cannot promise protection against callers violating this.
- **Confirmed metadata issue:** `RegisterTracked` appended a new
  family ledger entry before confirming registration and removed only
  its label signature after `create()` failed, leaving an empty entry.
  The new implementation unwinds the provisional family record on
  exception. Quota rejection and allocation error paths are included.
- **Confirmed HELP mismatch:** prometheus-cpp 1.3 Registry Merge
  returns an existing Family without checking a different HELP
  argument. Vex's own tracked families now reject incompatible HELP
  across manager API calls with `std::invalid_argument`.
- Tests added: default label flip with persistent external series,
  invalid label via scoped guard, strict-registry repeated registration
  failure and recovery, HELP text mismatch and quota preservation.
  **These additions have NOT been built or run on the owner checkout.**


---

## Final owner-run validation after ownership fix (2026-11-10)

**PASS**: baseline/threaded/ASan+UBSan/Clang TSan all 9/9; stress 800/800; full package 12/12. Release benchmark built and produced 18 measurements across 10/100/1000 series. The prior 'not tested' statement below is historical and superseded. See [MONITORING_FINAL_REPORT_2026-11-10.md](MONITORING_FINAL_REPORT_2026-11-10.md) and [MONITORING_TASKS.md](MONITORING_TASKS.md).

---

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
