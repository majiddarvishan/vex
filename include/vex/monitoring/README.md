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
attempting to do so throws `std::invalid_argument`. Concurrent changes to default
labels are resolved to a single validated snapshot per new guard;
they do not migrate existing series. Do not mutate/remove the Family
directly while guards are active. Details:
[API.md](API.md).


### Direct prometheus-cpp comparison for MON-OPEN-01

The optional `./tools/benchmark_monitoring.sh 3` now emits **four**
`shape` values (preserving the existing CSV column names):

- `many_series` — Vex manager, one family with N labeled series
- `raw_many_series` — direct prometheus-cpp, identical schema
- `many_families` — Vex manager, N families with one series each
- `raw_many_families` — direct prometheus-cpp, identical schema

Each scenario uses a **fresh Registry**, 100 Counter increments per
series, and `Registry::Collect()`. The benchmark checks that each
shape produces the expected family/sample counts and fails if it does
not. Runs for N=10,100,1000, three repeats by default (36 CSV rows).
This provides a controlled comparison of Vex-tracking overhead vs
upstream registration cost; it does not prove bottleneck location
without profiling and does not replace ASan/TSan/CTest. **Not yet
compiled/run on the owner's checkout.**

## Optional resource/load and packaging validation (2026-11-10)

The following independent scripts do **not** replace normal CTest and
sanitizer acceptance:

```sh
./tools/benchmark_monitoring_controlled.sh 15 > build/monitoring-controlled.csv
./tools/benchmark_monitoring_load.sh 3 > build/monitoring-load.csv
./tools/verify_monitoring_portability.sh all
```

The load benchmark uses a Linux Release build, loopback-only HTTP,
1/4/8-thread registration and shared Counter updates, parallel
Collect+updates, and reports p50/p95/p99 for sequential HTTP
requests and Collect calls. Memory values are process snapshots;
`heap_live_bytes` is glibc allocator state, **not** allocation count.
Optional `./tools/profile_monitoring_allocations.sh` uses a previously
installed heaptrack without installing anything.

The controlled registration benchmark alternates scenario order and
warm-ups over 9/15/21 runs to investigate the noisy one-family series
regression signal. No tool result or performance SLA is asserted until
these commands are run on a real host.

### Shared build PIC compatibility

For the opt-in GCC/Clang × Static/Shared portability matrix, installed
static prometheus-cpp archives must have been built with `-fPIC` to
link into `libvex_monitoring.so`. The project's original
find-package-first policy can select non-PIC archives from
`/usr/local/lib`. The revised
`./tools/verify_monitoring_portability.sh all` avoids that in Shared
modes by building the project's pinned prometheus-cpp dependency
inside an isolated build tree (without changing installed packages).
This optional test may require fetching source if not cached. To
explicitly test known PIC-compatible *installed* artifacts instead,
set `VEX_PORTABILITY_SHARED_DEPENDENCY=installed`.

The old GCC Static 11/11 run passed; GCC Shared failed to link against
previous installed archives; Clang modes did not run. The new script
and C++17 warning fixes **are pending verification on owner checkout**.
Details: [Monitoring partial report](../../../.codex/MONITORING_PORTABILITY_PARTIAL_2026-11-10.md).

### Runtime lookup of installed Shared dependencies

When Shared Monitoring and prometheus-cpp are installed in the
same ELF library directory, `$ORIGIN` in the installed
`libvex_monitoring.so` and fetched `prometheus-cpp-pull` allows
the loader to find adjacent third-party libraries without setting
`LD_LIBRARY_PATH`. This is a candidate packaging fix following the
owner's GCC/Clang Shared consumer load failure; **a fresh installed
consumer run is still required**. Details are documented in
[the package report](../../../.codex/MONITORING_PORTABILITY_RUNTIME_2026-11-10.md).

### Optional Counter contention attribution and peak memory integrity

After analyzing 15-run controlled and 3-run multi-thread datasets, a further opt-in Linux Release benchmark mode was added:

```sh
./tools/benchmark_monitoring_load.sh 3 --controls > build/monitoring-load-controls.csv
```

It compares raw Prometheus and Vex-managed shared Counter updates with raw/managed per-thread sharded Counter updates. The existing default run remains available unchanged. The new control mode adds 9 records per repetition: **61 CSV lines** for 3 rounds versus **34** for the old default. Peak RSS now reads Linux `VmHWM`, returning `-1` for contradictory measurements rather than reporting a smaller peak than current RSS. Neither new control data nor validated peak readings have yet been obtained on the owner host.

### Optional long-running Registry churn and static-analysis probes (2026-11-14)

```sh
./tools/benchmark_monitoring_lifetime.sh 8 512 > build/monitoring-lifetime.csv
./tools/verify_monitoring_static.sh check-tools
./tools/verify_monitoring_static.sh > build/monitoring-static.log 2>&1
```

The bounded Linux lifetime probe creates 4096 short-lived Registries
across eight rounds, checks weak ownership/manager quota isolation and
reports VmRSS, VmHWM and optional glibc Heap trends. It **does not**
prove memory-leak freedom or provide allocation call-site data. The
clang-tidy script performs a focused warning-as-error check of the
Monitoring source and public headers, without automatic source fixes;
it never installs clang-tidy if absent. Both tools are **optional and
unverified on the owner's checkout** as of this documentation change.

Details: [Monitoring follow-up plan](../../../.codex/MONITORING_FOLLOWUP_PLAN_2026-11-14.md).
