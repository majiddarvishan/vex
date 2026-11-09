# Instructions for coding agents

## Ground rules
1. Start by comparing current HEAD to audit baseline `71e74d221f182f0567f43531f7f933104cb58c68`. Do not treat these reports as immutable truth.
2. Separate confirmed source defects, potential bugs pending reproduction, and recommendations. Report file:line references with any code review.
3. Prefer small, reversible commits scoped to one issue ID. Preserve public APIs unless the change explicitly justifies a breaking version.
4. Do not change service configuration, install globally, overwrite system files, publish releases, expose credentials, or merge without authorization. Use out-of-tree builds.
5. Add an automated regression before fixing a correctness or threading bug; test negative/error paths and ownership lifecycle.
6. Evaluate concurrency under real simultaneous producers/consumers, not only sequential demonstrations. Prefer minimal examples in isolated test binaries; repeat with TSan and ASan separately (do not combine TSan and ASan in the same binary).
7. Keep Prometheus metrics bounded in label cardinality. Do not use unbounded raw thread IDs as durable labels.
8. Treat all API pointers/references as having explicit ownership and lifetime contracts; record expected registry/pool shutdown ordering.
9. When network-dependent CMake dependencies are unavailable, document the blocked dependency and do not misreport success.
10. Keep Markdown audit reports updated with tested status, regressions, residual risks, and the relevant commit SHA.

## Code landmarks
- `include/vex/logging/segmented_logger.hpp` — experimental async segmented logger, sharded queue, file rotation.
- `include/vex/thread/threadpool.hpp` — task queue and worker lifecycle.
- `include/vex/object_pool/` — shared and thread-local object pools plus Prometheus adapter.
- `include/vex/monitoring/` + `src/monitoring/` — registry, metric families, RAII, health, decorators.
- `CMakeLists.txt`, `modules.cmake`, `modules/*/CMakeLists.txt` — module toggles, exports/install.
- `examples/`, `tests/` — illustrative programs and incomplete test harness.

## Expected implementation sequence
Follow `BACKLOG.md`: establish build/test gates, fix memory and concurrency defects, repair integration/configuration, then do performance work with baselines. Any workaround to pass a benchmark must not weaken thread safety.

## Evidence in handoff
Include exact SHA/branch; changed paths; commands and stdout outcome; sanitized repro/failure evidence; portability checked; risk; and next items. Mark `NOT RUN` and `BLOCKED` explicitly.
