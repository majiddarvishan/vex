# Vex Monitoring

A C++17 Prometheus monitoring module. Public header `vex/monitoring/monitoring.hpp`, namespace `vex`, CMake target `Vex::monitoring`.

## Configure and test

```sh
cmake -S . -B build/monitoring-only -DBUILD_MONITORING=ON \
  -DBUILD_OBJECT_POOL=OFF -DBUILD_THREADPOOL=OFF \
  -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=ON
cmake --build build/monitoring-only
ctest --test-dir build/monitoring-only --output-on-failure
```

Use `ENABLE_METRICS_THREADING=ON` for the legacy mode-reporting flag; mutable MetricsManager state is synchronized in both configurations. The build resolves installed prometheus-cpp first, then optionally fetches it with VEX_ALLOW_DEPENDENCY_FETCH.

An installed CMake consumer calls `find_package(Vex CONFIG REQUIRED)` and links `Vex::monitoring`.

## Minimal example

```cpp
#include <vex/monitoring/monitoring.hpp>
int main() {
    (void)vex::MetricsManager::Init(true);
    auto registry = vex::MetricsManager::GetRegistry();
    auto& completed = vex::MetricsManager::CreateCounter(
      registry, "jobs_completed_total", "Completed jobs");
    auto& active = vex::MetricsManager::CreateGauge(
      registry, "jobs_active", "In-flight jobs");
    {
        vex::ScopedInFlightGauge guard(active);
        completed.Increment();
    }
    return (completed.Value() == 1.0 && active.Value() == 0.0) ? 0 : 1;
}
```

## Safety and lifetime

- A Counter is cumulative: keep `*_total` series alive. An in-flight Gauge increments and decrements. `ScopedCounter`/`ScopedGauge` delete ephemeral series when the last matching guard ends; do not use them for persistent cumulative metrics.
- Hold a Registry shared_ptr as long as any raw metric reference, guard, decorator, or exposed family is in use. `Reset` requires all users to quiesce.
- Opt-in cardinality caps are available with `SetMaxSeriesPerMetric` and `SetMaxSubsystemRegistries`. Defaults are unlimited for compatibility. Direct raw Prometheus operations bypass these limits.
- HealthCheck safely refuses to access metrics after its weak Registry expires, but external deletion of a still-live Family is not supported.
- Avoid unbounded labels (raw user IDs, request IDs, subscriber numbers).

The historical `vex::monitoring::record_metric(name, value)` is now a real persistent Gauge adapter (not a no-op) and requires prior MetricsManager initialization. Full contracts and exceptions: [API.md](API.md).

## Test and benchmark commands

```sh
./tools/verify_monitoring.sh all
```

`tsan-clang` auto-discovers clang++ or versioned clang++-N (or CXX override). GCC 13.3 TSan was unstable on one independently tested host; the owner previously passed the older suite with Clang 20. That historical PASS does not validate later code changes.

Optional Release-mode benchmark for registration, 100 updates per series,
and collection at 10/100/1000 samples using both **many-series** and
**many-families** workloads:

```sh
./tools/benchmark_monitoring.sh 3
```

The script prints the environment to stderr and structured CSV to stdout.
Build and execution are optional and not part of the correctness gate.
No performance numbers are asserted until owner measurements are supplied.

**Scoped metric ownership:** An existing externally created counter
or gauge series is not adopted by `ScopedCounter`/`ScopedGauge`;
attempting to do so throws `std::invalid_argument`. Configure
default labels once before acquiring scoped guards, and do not
modify/remove the Family directly while guards are active. Details:
[API.md](API.md).
