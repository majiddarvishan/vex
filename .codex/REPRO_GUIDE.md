# Focused reproductions for the audit

These are **proposed tests**, not tests that have already been run. Build the real checked-out repository at the SHA recorded in `HANDOFF.md`. Avoid applying tests to a live service.

## R01 — Queue producer collision (VEX-001)
Create a test-only logger queue with 1 shard and enough capacity to avoid normal saturation; start 32 producers using a barrier, each sending indexed unique IDs, drain on a single consumer; assert every accepted ID appears exactly once. Run TSan. The current `Shard::try_push` has no unique producer slot reservation. Also exercise DropOldest with producer and worker both entering `try_pop`.

## R02 — Throwing placement-new (VEX-002)
Use a test class whose constructor can throw on the *second* construction and whose destructor counts instances. With `std::make_shared<vex::ObjectPool>()`, create/return one object, then call `create<T>` with throw activated. Catch exception, assert no destructor runs for an unconstructed object and no leak/invalid free; run ASan/UBSan.

## R03 — Uninitialized strategy & shard bounds (VEX-003/010)
Initialize logger with each valid policy, saturate queue, verify observed behavior and return values. Try capacity 0, 1, capacity < shards, shards 0; require validation errors, not modulo-zero. On the same producer thread, use logger/queue instances with 8 shards and then 1 shard; prevent cached shard indices leaking across instances.

## R04 — Registry lifecycle (VEX-007/008/011/014)
1. Set ObjectPool metrics registry A, create/release to initialize handles, switch to registry B, release A and perform metrics update under ASan.
2. MetricsManager::Init, HealthCheck::RegisterHealthMetrics, MetricsManager::Reset; check HealthCheck::IsRegistered and ensure SetHealthy does not access freed gauge.
3. TSan concurrent Init/Reset/GetUptimeSeconds, both `METRICS_MULTI_THREADED` variants.
4. Force metric-family registration failure and ensure no null-pointer dereference.

## R05 — Thread pool (VEX-009)
Schedule a throwing task followed by a completing task; require documented failure reporting without terminating the process. Construct with 0 workers and verify well-defined rejection or fallback. Saturate tasks, verify configured bounds. Arrange concurrent enqueue and shutdown and inspect accepted/rejected tasks.

## R06 — Package smoke (VEX-004/005/006/013/018)
Clean out-of-source builds with both example values and every module combination; assert exported targets are valid; install to a temporary path; compile a separate C++ consumer using `find_package(Vex CONFIG REQUIRED)` and each enabled public header. Enable tests and verify `ctest -N` is nonzero and each registered test executes. No manual include paths to source tree.

## R07 — Logger I/O/recovery (VEX-012/016)
Inject filesystem write failure and rename across devices; verify error counts, accepted-vs-written semantics, incomplete-file preservation and recovery, synchronous flush/shutdown agreement. Never require destructive disk operations for an injected failure test.

## R08 — Metrics semantics (VEX-014/015/017)
With two types and 1000 transient threads, verify metric family cardinality stays within budget; successful and failed object transitions count as expected. Repeated scoped Counter creation/destruction should not falsely erase expected service-wide cumulative metrics.

## Evidence template
- Baseline/branch/commit:
- Compiler, CMake, prometheus-cpp version, OS:
- Command:
- Expected result:
- Observed result / sanitizer trace:
- Test file and diff:
- Severity/status update:
