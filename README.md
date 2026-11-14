# Vex — C++17 utility modules

**Current release: `v0.1.0`.** See [CHANGELOG.md](CHANGELOG.md) for the release scope and [.codex/VERSIONING.md](.codex/VERSIONING.md) for the versioning policy.

Vex is an evolving modular C++17 library. **Source-level correctness and
concurrency issues are still under review.** See [the audit](.codex/AUDIT.md)
and [owner-approved priorities](.codex/DECISIONS.md) before production use.

## Available build targets

| Flag | CMake target | Notes |
|---|---|---|
| `BUILD_THREADPOOL` (ON) | `Vex::thread_pool` | Header-only; worker lifecycle risks tracked |
| `BUILD_OBJECT_POOL` (ON) | `Vex::object_pool` | Header-only, presently requires prometheus-cpp |
| `BUILD_MONITORING` (ON) | `Vex::monitoring` | Compiled; prometheus-cpp core + pull |
| `BUILD_LOGGER` (OFF) | unavailable | Explicitly rejected if ON until missing dependencies are resolved |

Networking is **not implemented** in this repository snapshot. SegmentedLogger
is tracked as experimental/unintegrated. The older README described examples
and `vex/vex.h` that do not exist.

## Build and test with no network access

Use `./tools/verify_foundation.sh thread` for the repeatable offline
foundation checks, or `./tools/verify_foundation.sh full` when
Prometheus dependencies are available. These run CTest, including the
external installed-package consumer. The equivalent commands follow.

ThreadPool-only build (does not need prometheus-cpp):

```sh
cmake -S . -B build/thread-only -DBUILD_EXAMPLES=OFF \
  -DBUILD_TESTING=ON -DBUILD_THREADPOOL=ON \
  -DBUILD_OBJECT_POOL=OFF -DBUILD_MONITORING=OFF \
  -DVEX_ALLOW_DEPENDENCY_FETCH=OFF
cmake --build build/thread-only
ctest --test-dir build/thread-only --output-on-failure
```

CTest registers a ThreadPool smoke test and a **separate installed-package
consumer** that builds against exported `Vex::` targets.

For Monitoring or ObjectPool, install `prometheus-cpp` 1.3.x with `core`
and `pull` CMake targets, or leave `VEX_ALLOW_DEPENDENCY_FETCH=ON` (default)
to allow CPM to obtain version 1.3.0. Set `CMAKE_PREFIX_PATH` when
dependencies are not in a system search path. Build-only dependencies may
also require the dependency's own third-party libraries.

```sh
cmake -S . -B build/full -DBUILD_TESTING=ON -DBUILD_EXAMPLES=ON
cmake --build build/full --parallel
ctest --test-dir build/full --output-on-failure
```

`BUILD_EXAMPLES` is ON by default. Examples are built **only** for enabled
modules; an example never forces a disabled module on.
`ENABLE_METRICS_THREADING` is OFF by default, maps to the
`METRICS_MULTI_THREADED` compile definition, and **does not mean**
concurrent registry/reset handling is validated yet.

## Install and use from another CMake project

```sh
cmake --install build/thread-only --prefix /tmp/vex-prefix
cmake -S tests/package_consumer -B build/consumer \
  -DCMAKE_PREFIX_PATH=/tmp/vex-prefix
cmake --build build/consumer
```

For an external project:

```cmake
find_package(Vex CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE Vex::thread_pool)
```

Installable targets export their transitive dependencies; an installed
Monitoring/ObjectPool package additionally requires a discoverable
`prometheus-cpp` package. More build cases and validation limitations are
documented in [the test matrix](.codex/TEST_MATRIX.md).

## Development

Read [AGENTS.md](AGENTS.md) and [.codex/README.md](.codex/README.md).
Initial CTest smoke tests are **not** a substitute for sanitizer, stress,
benchmark or comprehensive Monitoring tests. Other implementation fixes
remain deferred until explicitly approved by the project owner.

License: [MIT](LICENSE).
