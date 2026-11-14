# Current VEX scope and release status (2026-11-14)

The Audit, CMake/CTest Foundation, and Monitoring work is integrated into `main` through merged PRs #1–#4. MON-OPEN-01..07 are complete within their documented evidence. Monitoring owner-run results are historical local evidence, not GitHub Actions CI and not proof of universal leak/race freedom.

Current release version: **v0.1.0**. All future version changes must follow `.codex/VERSIONING.md`: update canonical version sources, changelog and package-consumer version assertions; commit to the owner-selected branch; create an annotated `vX.Y.Z` tag on the exact release commit; never overwrite an existing tag.

Logger, ObjectPool, and ThreadPool runtime remediation remains deferred pending explicit owner authorization. For any new runtime work, add regression coverage and run the relevant validation before claiming completion.


Single-command acceptance:
`./tools/verify_monitoring.sh all`

This runs baseline, threaded, ASan/UBSan, Clang TSan, 100x stress
for eight Monitoring C++ tests and the full package-consumer suite.
It fails fast. Historic Clang TSan PASS belonged to older commits.

---

# Instructions for coding agents

## Binding owner priority (2026-11-14)
The Foundation and Monitoring work is complete and merged to `main`. Logger, ObjectPool behavior, ThreadPool behavior, and other runtime implementation improvements remain frozen until explicit owner direction.


## Ground rules
1. Start by comparing current HEAD to audit baseline `71e74d221f182f0567f43531f7f933104cb58c68`. Do not treat these reports as immutable truth.
2. Separate confirmed source defects, potential bugs pending reproduction, and recommendations. Report file:line references with any code review.
3. Prefer small, reversible commits scoped to one issue ID. Preserve public APIs unless the change explicitly justifies a breaking version.
4. Do not change service configuration, install globally, overwrite system files, publish releases, expose credentials, or merge without authorization. Follow `.codex/VERSIONING.md` for explicitly authorized release/version work. Use out-of-tree builds.
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
