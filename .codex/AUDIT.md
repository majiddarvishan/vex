# Source-level engineering audit — Vex

**Post-warning-fix Monitoring verification (owner, 2026-11-10):**
Clang 20 TSan **3/3 PASS** (no reported races, prior unused-value
compiler warning absent); full foundation CTest **6/6 PASS**
including package consumer. Baseline/threaded/ASan and 100-repeat
stress cases also passed earlier. This supports candidate VEX-008 and
partial VEX-011 remediation for tested paths only. Wider Monitoring
ownership, RAII counter semantics, concurrent lifecycle and
registration/error/cardinality edge cases remain open. Source:
[MONITORING_RUNS.md](MONITORING_RUNS.md). No non-Monitoring issue
status or authorization changed.


**Latest owner-run TSan evidence (2026-11-10):**
Clang 20.1.2 TSan independent probe **10/10 PASS twice**; actual
Monitoring TSan CTest **3/3 PASS**, with no data-race reports observed
on exercised paths. Existing GCC 13.3.0 TSan instability is **toolchain-
specific**, not a clean GCC result. This gives positive Clang TSan
evidence for the candidate fixes to VEX-008 and portions of VEX-011,
but does not close broader registry/metric lifetime or concurrent
reset concerns. One SAFE_CREATE_GAUGE unused-value warning was fixed
after these test logs; new commit not yet compiled on owner's checkout.
All non-Monitoring findings remain deferred. See
[MONITORING_RUNS.md](MONITORING_RUNS.md).


**Baseline:** `71e74d221f182f0567f43531f7f933104cb58c68` (`main`). **Date:** 2026-11-10.

**Monitoring remediation candidate (PARTIALLY VERIFIED):** Branch `feature/monitoring-correctness-tests` addresses VEX-008 and portions of VEX-011 (metadata synchronization and registry lifetime), and SAFE_* namespace qualification and UTF-8 value validation. Owner-run baseline/threaded/ASan tests passed (2/2 each), and full foundation CTest passed (5/5); **TSan NOT PASS** due to `unexpected memory mapping` on the smoke executable (contract passed). See [MONITORING_RUNS.md](MONITORING_RUNS.md). Do not mark Monitoring concurrency completely fixed without clean TSan plus broader stress coverage. Non-Monitoring findings remain frozen.
**Method:** GitHub source inspection of first-party headers, C++ sources, examples, tests and CMake. **Execution:** configure/build/tests/sanitizers/benchmarks NOT RUN due to unavailable local checkout/network dependency. Findings below are code-backed; runtime consequences should be verified with the regression cases in `REPRO_GUIDE.md`.

Severity is an initial technical assessment, not a CVSS score. Lines refer to the above baseline. Paths below are relative to repository root.

## Critical

### VEX-001 — Broken multi-producer queue protocol
**Where:** `include/vex/logging/segmented_logger.hpp:348-368,426-440,443-459,488-494,588-599`.
**Evidence:** `Shard::try_push` loads and increments `write_idx_` without compare/exchange or a producer lock, then writes `buffer_[current_write]`. Two producers mapping to the same shard can claim the same cell. `DropOldest` additionally calls `try_pop` from producers while the worker is consuming, despite the single-consumer design.
**Impact:** data races, corrupted or missing log records, queue corruption; TSan is expected to detect producer collisions.
**Fix:** use a proven bounded MPMC algorithm or a mutex+CV queue initially; only optimize after testing; remove producer-side popping unless the queue explicitly supports multi-consumer semantics.
**Regression:** 16-64 producers, few shards, unique sequence IDs, forced saturation and concurrent DropOldest; check duplicates/missing events and TSan.

### VEX-002 — Invalid destruction after reused-object constructor throws
**Where:** `include/vex/object_pool/object_pool.hpp:56-69`.
**Evidence:** the old `T` is explicitly destroyed; placement new can throw; catch then does `delete raw`, which attempts to destroy a `T` whose lifetime never began.
**Impact:** undefined behavior, double destruction, resource corruption or a process failure on a normal exception path.
**Fix:** use correct raw-storage deallocation (`::operator delete` with matching allocation/alignment) without invoking a destructor; prefer a properly encapsulated allocator/ownership guard for exception-safe reuse, or avoid destruct/reconstruct reuse until tested.
**Regression:** class with tracked destructor and a throwing constructor; reuse from pool, throw, inspect destructor count and ASan/UBSan behavior.

### VEX-003 — Uninitialized logger backpressure strategy
**Where:** `include/vex/logging/segmented_logger.hpp:550-558,670-690,1067-1069`.
**Evidence:** `backpressure_` is default-constructed but neither initialized in the constructor member initializer list nor stored by `apply_config`; `record` loads it. `load_config` also omits loading backpressure/queue capacity/flush interval values (while `LoggerConfig` defines them).
**Impact:** undefined/indeterminate strategy selection; potentially lost records and inconsistent admission semantics.
**Fix:** initialize to `cfg.backpressure` before worker/use; validate parsed enum and source all configured fields; add reload/update policy tests.
**Regression:** one logger per policy with full queue; check deterministic policy and config propagation.

## High

### VEX-004 — Logger is not a buildable tracked module
**Where:** `include/vex/logging/segmented_logger.hpp:3-5`, `modules.cmake:5-20`.
**Evidence:** tracked header includes `pa/config.hpp` and `logging.hpp` but dependency integration and `logging.hpp` are absent from tree; `BUILD_LOGGER` never adds `modules/logging`.
**Impact:** advertised logger cannot be consumed as a supported install target, and standalone header inclusion is not reproducible without undocumented external code.
**Fix:** decide on supported API/third-party dependency, vendor or declare it, build-test a header smoke target, and connect CMake target/installation.
**Regression:** compile a minimal consumer including the public logging header.

### VEX-005 — CMake feature options diverge and tests are not active
**Where:** `modules.cmake:5-20`; `examples/monitoring/CMakeLists.txt:4-17`; `CMakeLists.txt:76-80`.
**Evidence:** `BUILD_MONITORING` is declared but `ENABLE_MONITORING` gates the module. Example force-sets `ENABLE_MONITORING` after module processing. Root never configures testing/adds `tests/`. Defaults include examples and trigger monitoring dependency resolution even when not requested.
**Impact:** surprising/irreproducible builds, missing expected targets, tests silently absent.
**Fix:** use one public option; isolate examples; add CTest/`BUILD_TESTING`; ensure enabled modules build regardless of example flags.
**Regression:** CMake configuration matrix in `TEST_MATRIX.md`.

### VEX-006 — Object pool cannot be used independently as configured
**Where:** `include/vex/object_pool/object_pool.hpp:3`, `include/vex/object_pool/internal/prometheus_metrics.hpp:15-18`, `modules/object_pool/CMakeLists.txt:1-5`.
**Evidence:** object_pool public headers require Prometheus, yet the exported interface target has no link/dependency on prometheus-cpp. Disabling examples/monitoring can leave required headers unavailable.
**Impact:** consumer compilation/link failure; deployment couples supposedly modular object pooling with metrics.
**Fix:** make metrics truly optional behind an interface abstraction or declare a correct imported public dependency; test stand-alone consumers.
**Regression:** isolated `BUILD_OBJECT_POOL=ON`, monitoring/examples OFF, clean exported package consumer.

### VEX-007 — Registry replacement leaves cached raw metric handles
**Where:** `include/vex/object_pool/internal/prometheus_metrics.hpp:33-38,50-57,85-104,156-159`.
**Evidence:** `set_registry` replaces the owning shared_ptr but keeps `handles_`, whose raw counter/gauge pointers were created in the old registry.
**Impact:** use-after-free/dangling pointers when old registry expires, or writes to retired registry and missing current metrics.
**Fix:** immutable registry after first metric registration, or switch generation and recreate/clear family handles with safe ownership; synchronize with concurrent usage.
**Regression:** create metrics, set a different registry, drop old registry, release an object; run under ASan.

### VEX-008 — HealthCheck retains dangling pointers across metrics Reset
**Where:** `src/monitoring/metrics_manager.cpp:52-61`; `include/vex/monitoring/health_check.hpp:19-21`; `src/monitoring/health_check.cpp:15-35,38-78`.
**Evidence:** `MetricsManager::Reset` resets registries, but `HealthCheck`'s static raw gauge pointers are never cleared/rebound and `IsRegistered` still checks for non-null.
**Impact:** stale-pointer access / use-after-free after reset or consumer registry teardown.
**Fix:** couple health handles to registry lifetime with an owning/weak generation handle or an explicit invalidation mechanism; test resets/re-registration.
**Regression:** Init/Register/Reset/IsRegistered/SetHealthy with registry destroyed under ASan.

### VEX-009 — ThreadPool does not contain task failures and has unsafe lifecycle edges
**Where:** `include/vex/thread/threadpool.hpp:17-45,47-66`.
**Evidence:** task() executes outside a try/catch, so uncaught exceptions terminate the process. Zero-worker construction silently permits enqueues that never run until destructor; unbounded queue accepts arbitrary work; enqueue has no stop check. Worker construction exceptions can destroy joinable earlier-created threads during stack unwinding.
**Impact:** process termination or unbounded memory / silent work loss; reject-on-shutdown contract missing.
**Fix:** choose error reporting (futures or exception callbacks), validate worker count, define bounded queue and shutdown API; implement exception-safe partial startup.
**Regression:** throwing tasks, zero workers, heavy queue growth, shutdown/enqueue race, failing worker startup injection.

### VEX-010 — Logger queue settings permit modulo-zero and out-of-range sharding
**Where:** `include/vex/logging/segmented_logger.hpp:336-345,418-456,488-499`.
**Evidence:** no validation of `num_shards` or `capacity/num_shards`; modulo by zero possible. Static thread-local `cached_shard` is shared across *instances* of the class template despite differing `num_shards_`, allowing out-of-range access. Ring reserves one cell, so advertised capacity differs from effective capacity.
**Impact:** crash/undefined behavior on configuration and multi-instance use, misleading saturation.
**Fix:** enforce `num_shards >= 1` and per-shard ring capacity >= 2, compute shard selection per instance, document usable capacity.
**Regression:** zero/small capacity/shards and two logger instances with different shard counts on same threads.

### VEX-011 — Metrics threading switch is disconnected from actual locking
**Where:** `modules.cmake:9`; `include/vex/monitoring/metrics_manager.hpp:19-24,153-165`; `src/monitoring/metrics_manager.cpp:251-259,261-271`.
**Evidence:** CMake option `ENABLE_METRICS_THREADING` never defines `METRICS_MULTI_THREADED`. `GetUptimeSeconds` writes `start_time_` without a lock, regardless of compile mode; other calls read global registry after returning from `EnsureInitialized` lock.
**Impact:** data races and inconsistent concurrent reset/configuration behavior.
**Fix:** CMake PUBLIC compile definitions or runtime locking policy, guard all shared state, document supported concurrency/Reset semantics.
**Regression:** TSan with Init/Reset/Uptime/GetRegistry on multiple threads, enabled and disabled variants.

### VEX-012 — Log acceptance/write counters do not ensure persisted data
**Where:** `include/vex/logging/segmented_logger.hpp:904-947,1026-1056`.
**Evidence:** stream writes and flush are not checked before `records_written++`. Shutdown drains without updating statistics and can discard data when open fails. `flush()` only requests async work and does not wait for durability.
**Impact:** silent log loss/inaccurate monitoring during disk-full/I/O failure, abrupt shutdown or process crashes.
**Fix:** define accepted/written/flushed/durable semantics; check stream errors, propagate alarm/backpressure, support awaited flush/shutdown with result.
**Regression:** unwritable target and injected write/flush failures, compare acknowledged vs durable counts.

### VEX-013 — Install/export package is not hermetic
**Where:** `modules/thread/CMakeLists.txt:3`, `modules/object_pool/CMakeLists.txt:3`, `cmake/VexConfig.cmake.in:1-3`.
**Evidence:** interface include dirs are only absolute source directories, not `BUILD_INTERFACE/INSTALL_INTERFACE`; package config does not resolve exported monitoring dependencies (e.g. `find_dependency`).
**Impact:** installation/export CMake errors or a broken downstream `find_package(Vex)`.
**Fix:** generator expressions for both interfaces and dependency discovery in package config; test install into temporary prefix and out-of-tree consumer.
**Regression:** clean prefix and consumer for each feature configuration.

## Medium

### VEX-014 — Prometheus metrics may be disabled or invalid under common conditions
**Where:** `include/vex/object_pool/internal/prometheus_metrics.hpp:50-119`; `include/vex/object_pool/object_pool.hpp:149-165,215-225`.
**Evidence:** a registration error returns all-null `Handles`, then callers dereference `.dropped`/`.returning`; catching C++ exceptions does not protect against null-pointer UB. `created` and `in_pool` metric pointers exist but are not updated in `ObjectPool`. Every transient thread ID is a label, causing unbounded series cardinality.
**Impact:** crashes in error paths, misleading metrics, memory pressure.
**Fix:** non-null guarded metrics/no-op adapter; register families once; aggregate stable type labels, instrument each lifecycle transition.
**Regression:** registration failure injection, many short-lived threads and metric count inspection.

### VEX-015 — Pool-size and exceptional-path semantics are incomplete
**Where:** `include/vex/object_pool/object_pool.hpp:26-36,78-85,128-179`.
**Evidence:** `set_global_max_pool_size` applies *per type*, not an aggregate global cap, and lowering the limit does not trim cached objects. `lifetime_map_` insertion can throw after acquiring/constructing raw object, leaking it.
**Impact:** surprising retention and leak on allocation failures.
**Fix:** rename/document policy vs implement true aggregate cap; trim available items safely; RAII ownership until metadata and shared_ptr construction succeed.
**Regression:** multiple types with cap 1, shrink after return, injected map-allocation exception.

### VEX-016 — Circular/RPC-time and filesystem concerns in logger
**Where:** `include/vex/logging/segmented_logger.hpp:764-805,807-902`.
**Evidence:** incomplete-file recovery assumes timestamp-only stems; file open uses truncate-capable mode; rename may fail between filesystems; error logging occurs while configuration mutex is held and without a known logging dependency contract.
**Impact:** failed rotation/recovery, potential filename collisions and path-dependent loss; deadlock risk if log macros reenter same logger (requires dependency analysis).
**Fix:** explicit file naming/collision policy, error fallback and independent diagnostics, same-filesystem atomic-rename contract or safe cross-device strategy.
**Regression:** recovery with malformed stems, target already exists, cross-device rename, directory permissions.

### VEX-017 — API/data integrity of RAII and metrics decorators needs validation
**Where:** `src/monitoring/raii_guards.cpp:14-27,73-85`; `include/vex/monitoring/decorators.hpp:25-42,53-66`.
**Evidence:** scoped counters are removed at end of scope, so cumulative metric history disappears rather than accumulating; decorators rely on non-owning references and `Track` expects non-void return (separate `TrackVoid`).
**Impact:** misleading telemetry and accidental use-after-free if registries outlive guards incorrectly.
**Fix:** clarify RAII semantics and choose gauge-for-inflight vs persistent counter; document reference lifetimes; test value retention.
**Regression:** repeated operations followed by scrape; move/destruct guard cases.

### VEX-018 — Tests and developer tooling contain stale/broken paths
**Where:** `tests/pool_tsan_test.cpp:10-12`; `tests/prometheus_metrics_cpp/test_main.cpp:8-14`; `tests/prometheus_metrics_cpp/CMakeLists.txt:6-18`; `tools/check_code_quality.sh:1`.
**Evidence:** nonexistent `vex/vex.h` and namespace `vex::object_pool`; test_main contains shell heredoc remnants; CMake references test files not present and obsolete `metrics-cpp` target; quality script points to missing `test/code_analysis.cpp` and `vex` target names differ.
**Impact:** misleading quality status; tests cannot be enabled wholesale without repairs.
**Fix:** rewrite tests around current public headers and targets, wire CTest, remove dead scaffold scripts.
**Regression:** `ctest --test-dir build --output-on-failure` must discover and run real assertions.

### VEX-019 — Docs, versioning and feature claims are stale
**Where:** `README.md:1-95`; `include/vex/monitoring/README.md:55-115`; `include/vex/version.hpp`; `cmake/version.hpp.in`.
**Evidence:** README names nonexistent networking and `vex/vex.h`, obsolete `BUILD_NETWORKING` and `vex` library; monitoring README refers to `metrics::` and `metrics-cpp` while current API is `vex::`; version template is unused and fixed header reports 0.0.1.
**Impact:** new consumers cannot follow advertised build/usage instructions.
**Fix:** update only after restoring actual module/package behavior; single version source; maintained examples as documentation tests.

## Low / forward-looking
- No tracked GitHub Actions or comparable CI pipeline; add a matrix only after local reproducibility is achieved. Maintain a no-network/offline option for supply-chain audit. (Repository tree/CMake)
- `tools/check_format.sh` and `tools/fix_format.sh` depend on relative working directory, unchecked `find`/`xargs` usage and hardcoded clang-format-16; tighten quoting and exit on failure. (scripts)
- Avoid speculative performance claims such as `zero-overhead`. Benchmark against clear correctness-first baselines (queue throughput, allocation/reuse rate, metric cardinality, shutdown latency).

## Security/supply-chain notes
No proven externally exploitable vulnerability was validated. The UB findings can become security-relevant if untrusted log/config data or high-concurrency callers reach the relevant components. Inspect access control to log paths and metrics bind addresses in consuming applications. Pin dependency revisions, scan vendored CPM before releases, and keep HTTP exposition opt-in with localhost default in examples. Never use a performance benchmark as a substitute for data-race verification.

## Review disposition
**Not release-ready as advertised.** At minimum, VEX-001 through VEX-013 require fixes or an explicit statement that affected components are experimental/unsupported. Static evidence confidence is highest for VEX-001/002/003/005/006/008/009/010/011/013/018; runtime consequences and compatibility need independent validation.
