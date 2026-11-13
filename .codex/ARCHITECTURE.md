# Architecture as observed at audit baseline

## Intended
Vex is advertised as a modular C++17 utility library. The README mentions a logger, thread pool, networking, header-only simplicity, installable targets and examples.

## Actual tracked modules
| Area | Source | CMake integration | Dependencies |
|---|---|---|---|
| Thread pool | `include/vex/thread/threadpool.hpp` | `modules/thread/CMakeLists.txt`, interface target `thread_pool` | C++17 threads, mutexes |
| Object pool | `include/vex/object_pool/{object_pool,thread_local_object_pool}.hpp` | `modules/object_pool/CMakeLists.txt`, interface target `object_pool` | Prometheus headers required unconditionally; target dependency not declared |
| Monitoring | `include/vex/monitoring/*`, `src/monitoring/*.cpp` | `modules/monitoring/CMakeLists.txt`, compiled target `monitoring` | prometheus-cpp core/pull via CPM |
| Segmented logger | `include/vex/logging/segmented_logger.hpp` | `modules/logging/CMakeLists.txt` exists but is not added by `modules.cmake` | missing tracked `logging.hpp` and third-party `pa/config.hpp` |
| Networking | no implementation directory or CMake target in snapshot | not present | README only |

`modules.cmake` declares `BUILD_LOGGER`, `BUILD_THREADPOOL`, `BUILD_OBJECT_POOL` and `BUILD_MONITORING`. It actually adds object_pool and thread modules and gates monitoring on a different variable (`ENABLE_MONITORING`). The examples' monitoring CMake force-enables it. The root does not add `tests/` and does not call `enable_testing()` or `include(CTest)`.

## Lifetimes / invariants to enforce
**ThreadPool:** workers must be joined before pool storage is destroyed; no tasks accepted after shutdown; task exceptions must not escape a worker thread; bounded or explicitly documented admission.

**ObjectPool:** each checked-out object must be valid and constructed exactly once; on reuse constructor failure, free storage *without destroying an unconstructed object*; metadata and subpool ownership must be exception-safe; deleter must remain safe if the pool is gone.

**Logger:** multiple producers may share a shard; reserve a unique queue slot atomically before writing; only one consumer per shard or correctly coordinate consumer indices; preserve record integrity and truthful success/drop/error accounting; shutdown must settle accepted data according to a documented durability policy. Validate shard and capacity bounds and synchronize configuration reloads.

**Metrics:** metric handles belong to a registry generation; reset or reconfiguration must not leave usable stale pointers; health stats and configuration must be consistent across threads; label cardinality must be controlled.

## Packaging boundaries
Exported `Vex::...` targets must expose install-relative include paths and transitive dependencies. `find_package(Vex CONFIG REQUIRED)` on a clean consumer installation is an explicit acceptance gate. Feature options must be well-defined across an all-OFF/minimal build, selective module builds, examples ON/OFF, static/shared builds.

## Security / deployment boundaries
This is a library, not a deployed service. Security review focuses on unsafe memory behavior, untrusted config/path validation, filesystem ownership/rename policy, HTTP exposition in examples, dependency integrity, and avoiding implicit network downloads during offline builds. No production secret or credential inventory is in scope.
