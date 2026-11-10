# Vex Monitoring API (C++17)

Header: `#include <vex/monitoring/monitoring.hpp>`; namespace `vex`.
CMake installed target: `Vex::monitoring`; dependency: prometheus-cpp 1.3+.

## Registry and lifetime

- Call `MetricsManager::Init()` once. It returns true on first initialization, false if already initialized.
- The manager's registry map and metadata are synchronized in all builds. `ENABLE_METRICS_THREADING` controls the legacy `IsThreadingEnabled()` mode-reporting flag when Init(true) is used.
- `GetRegistry()` and `GetSubsystemRegistry(name)` return owning shared_ptr snapshots.
- `Reset()` is for quiescent teardown/testing, NOT live registry migration. It does not protect clients still using raw metric references from destruction of their owning Registry.
- Hold a Registry alive for as long as any raw Metric, RAII guard, or Decorator refers to it; never externally Remove the Family during that lifetime.
- Set up the Exposer once using `RegisterAllWithExposer`; newly added subsystem registries need explicit registration or Exposer reconfiguration.

## Metrics and labels

- `CreateCounter`, `CreateGauge`, and `CreateHistogram` take a Registry shared_ptr, a name, HELP and optional labels.
- Names use `[a-zA-Z_:][a-zA-Z0-9_:]*`, labels use `[a-zA-Z_][a-zA-Z0-9_]*`; leading `__` is rejected.
- Label values must be well-formed UTF-8: invalid continuation bytes, overlong encodings, surrogate sequences and codepoints above U+10FFFF are rejected. ASCII controls other than TAB and DEL are rejected. As always, bound untrusted input sizes upstream.
- Default labels are merged first and explicit labels override them. Histograms reject the reserved `le` label.
- Histogram buckets must be finite and strictly increasing. Empty buckets use default boundaries: 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10.
- Conflicting buckets when re-registering an existing managed histogram series throw invalid_argument.
- Invalid input and null Registries throw invalid_argument. Prometheus itself may reject conflicting types/families.

## Optional cardinality safeguards

- `SetMaxSeriesPerMetric(N)` limits distinct label sets created through MetricsManager's Create* APIs, per Registry/name/type. Identical label sets reuse the existing series. When the limit is reached, new series throw length_error.
- `SetMaxSubsystemRegistries(N)` limits new subsystem names. Existing subsystem names reuse their Registry. New names beyond the cap throw length_error.
- Both default to 0 (unlimited) for compatibility and may be read with corresponding GetMax* methods. Reset resets them to unlimited.
- Direct calls to Prometheus Family::Add / Registry APIs bypass these wrapper limits. Do not mix raw Family operations with strict managed cardinality quotas.
- Use bounded labels such as status/operation, not subscriber IDs, request IDs, or arbitrary paths.

## RAII and cumulative semantics

- `ScopedCounter` and `ScopedGauge` are EPHEMERAL metric-series owners. Overlapping identical Family/label guards are reference-counted; the last destructor removes that series. An existing metric with the same effective labels that was created outside the RAII guards is rejected with std::invalid_argument rather than adopted and deleted.
- Never use ScopedCounter for a persistent `*_total` counter. Use `MetricsManager::CreateCounter` and retain its Registry.
- `ScopedInFlightGauge` increments a persistent Gauge in the constructor and decrements on destruction (also supports move). It does not remove that metric.
- All guard classes require their Family/Gauge and Registry to outlive the guard. Do not mix external removal with active guards.

## Decorators

- `ScopedTimer` observes one histogram sample on scope exit, including exceptions. It is not copyable/movable.
- `TimeFunction` accepts void, value, reference and move-only return values.
- `TimeFunctionWithDuration` returns pair<result, seconds> for non-void functions; returns elapsed seconds (double) for void functions. It observes even when the callable throws.
- `ResultTracker::Track` and `TimedResultTracker::Track` accept void/value/reference returns; `TrackVoid` remains supported for explicit void call sites.
- The timed tracker records a histogram sample on both success and exception.

## HealthCheck

- `RegisterHealthMetrics` creates health_status, uptime_seconds and memory_usage_bytes; repeated registration against the same live Registry is idempotent.
- The internal Registry reference is weak; mutators temporarily hold shared ownership. After Registry expiry, IsRegistered returns false and mutators throw runtime_error.
- Failed registration publishes no invalid handles, although a partially created Prometheus family may remain. Resolve the underlying family collision then retry.
- Clients must not externally remove/replace HealthCheck's metric Families while registered.

## Repeatable verification

```sh
./tools/verify_monitoring.sh baseline
./tools/verify_monitoring.sh threaded
./tools/verify_monitoring.sh asan
./tools/verify_monitoring.sh tsan-clang
./tools/verify_monitoring.sh stress
./tools/verify_foundation.sh full
```

The Clang TSan mode automatically detects unversioned or versioned clang++ and uses a compiler-specific build directory. A passing suite is scoped to the exercised paths, not a proof of all possible races. The legacy record_metric(name,value) helper now publishes a persistent Gauge instead of silently doing nothing; call MetricsManager::Init first.

Optional benchmarking: enable `VEX_BUILD_MONITORING_BENCHMARK=ON`; results are measured CSV, not fixed claims.