# External package validation

Configure, build, test and install Vex to a throwaway directory, then build
this **independent** consumer using only installed CMake exports/headers:

```sh
cmake -S . -B build/check -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=ON \
  -DBUILD_MONITORING=OFF -DBUILD_OBJECT_POOL=OFF -DBUILD_THREADPOOL=ON \
  -DVEX_ALLOW_DEPENDENCY_FETCH=OFF
cmake --build build/check
ctest --test-dir build/check --output-on-failure
cmake --install build/check --prefix /tmp/vex-test-prefix
cmake -S tests/package_consumer -B build/consumer \
  -DCMAKE_PREFIX_PATH=/tmp/vex-test-prefix
cmake --build build/consumer
./build/consumer/vex_external_consumer
```

For a full build, install prometheus-cpp 1.3.x with pull support (or allow
CPM to fetch it) before configuring, and repeat the consumer test against the
full installed package. The installed Vex package discovers prometheus-cpp via
`find_dependency` rather than relying on source-tree include paths.

The legacy `tests/pool_tsan_test.cpp` and
`tests/prometheus_metrics_cpp/` are not part of this smoke test. Their
known problems are tracked in `.codex/AUDIT.md` as VEX-018. A complete
regression/sanitizer suite is a separate deliverable.
