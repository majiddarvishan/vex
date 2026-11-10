# Remediation backlog and gates

## Owner-approved execution order (2026-11-10)

**FOUNDATION + MONITORING REGRESSIONS PASSED (owner-run 2026-11-10):** CMake/CTest full 6/6 PASS; Clang 20 TSan 3/3 PASS; baseline/threaded/ASan 2/2 each and 100x smoke+contract stress PASS. Wider Monitoring review remains active, not completed. Extended build-matrix remains open. **ACTIVE NEXT P1:** Monitoring module reliability, API lifetime, reset, concurrency, semantics and tests. **DEFERRED / FROZEN:** All other changes, including critical logger, object pool, threadpool and performance work, are **record-only until the owner explicitly authorizes starting them**. The historical phases below are a risk ranking, **not** authorization to implement them.


Baseline `71e74d221f182f0567f43531f7f933104cb58c68`. This backlog is a proposal; nothing here is claimed implemented.

| Phase | Priority | Issues | Deliverable and exit gate |
|---|---|---|---|
| 0 — Reproducible build | P0 | VEX-004,005,006,013,018 | Clean offline-aware CMake matrix, valid public targets, tests registered; install consumer works |
| 1 — Memory safety | P0 | VEX-002,007,008,014,015 | Throwing constructor, registry reset/replace, metrics error paths pass ASan/UBSan |
| 2 — Concurrency + lifecycle | P0 | VEX-001,003,009,010,011 | TSan-green stress; logger queue correctness; ThreadPool deterministic shutdown; metrics lock policy |
| 3 — Data integrity | P1 | VEX-012,016,017 | Disk-failure/error accounting; data acknowledged vs persisted; expected Prometheus semantics |
| 4 — Maintenance/docs | P1 | VEX-019 and scripts | Accurate README and package examples, version authority, formatting/static analysis |
| 5 — Performance | P2 | all affected | Repeatable benchmarks with allocation profile and metrics-cardinality budget; no correctness regressions |

**Foundation status (2026-11-10):** CMake and CTest wiring has been implemented on `feature/cmake-ctest-foundation`, but full repository and Prometheus-dependent validation is outstanding. Do not mark Phase 0 complete until real checkout and consumer gates pass. Monitoring is the only next authorized implementation phase; do not begin other work packages without owner permission.

## Proposed work packages
**WP01 (CMake)**: consolidate module feature flags; separate build examples from library; add BUILD_TESTING/CTest; make object_pool dependency design deliberate; package/export test and lint.

**WP02 (ObjectPool)**: explicit object lifetime states, correct cleanup of placement-new failure, exception-safety guarantee across map/shared_ptr creation, checked registry handles and stable ownership; tests for pool teardown and failures.

**WP03 (Logger)**: define data acceptance/durability contract, replace queue with correct bounded implementation, validate zero/small capacities, initialize backpressure enum, drop strategy behavior, file write/rename failures, clean shutdown.

**WP04 (ThreadPool)**: bound work queue, reject after stop, handle zero workers and construction failures, contain/report task exceptions, provide tested shutdown semantics.

**WP05 (Monitoring)**: reconcile compile/runtime threading flags, registry generation/reset ownership, avoid global non-owning lifetime leaks, cardinality controls, test guard semantics.

**WP06 (Documentation/quality)**: replace stale API descriptions after implementations pass; CI with GCC/Clang and sanitizers; isolated consumer tests.

## Definition of done for each issue
Repro added and failing before change; reviewed design/ownership; minimal implementation; unit/integration pass; sanitizer result captured; no undocumented public API change; docs updated; commit SHA attached in handoff.

## Do not do
Do not silently remove counters, swallow failed writes, use an uncaught-exception worker policy, make queue lock-free without proof, or claim that `ctest` runs tests before `enable_testing()`/registration exists.
