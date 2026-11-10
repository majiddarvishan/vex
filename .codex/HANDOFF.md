# Audit handoff — 2026-11-10

## Latest owner-run validation (2026-11-10)

The owner executed both scripts on the real checkout: **thread 2/2 PASS**, **full 4/4 PASS** including package-consumer; full build on GNU 13.3.0. See [FOUNDATION_RUNS.md](FOUNDATION_RUNS.md). Baseline CMake/CTest gate met; extended build matrix not complete. **Next approved implementation: Monitoring only.** Other behavioral fixes remain deferred.

## Foundation implementation update (2026-11-10)

- **Branch:** `feature/cmake-ctest-foundation` (stacked on the audit documentation branch).
- **Repeatable helper:** `tools/verify_foundation.sh thread` (offline CMake/CTest + package consumer); `tools/verify_foundation.sh full` (needs full Prometheus dependency resolution). Its thread-mode logic was verified on the reconstructed subset, but it was not executed on an actual repository clone.
- **Scope implemented:** consolidated CMake flags; conditional modules/examples; explicit missing logger integration; optional system-first Prometheus dependency resolution; transitive includes/links; package export/import; CTest smoke targets for ThreadPool, ObjectPool, Monitoring; automatic CTest external-install consumer.
- **Verification achieved:** GCC 14.2.0 + CMake 3.31.6 + Ninja 1.12.1, reconstructed **ThreadPool-only subset** assembled from reviewed source (not a clone of the GitHub repository): configure/build succeeded, 2/2 CTest smoke/package-consumer passed, package install and separate consumer compile/run passed. All-OFF configure succeeded (0 tests expected).
- **NOT RUN / BLOCKED:** full original repository checkout, monitoring/object_pool builds and tests, ASan/UBSan/TSan, all external package modes and all-platform matrix; GitHub DNS unavailable in shell and no installed prometheus-cpp.
- **Next:** verify the actual branch checkout in a network-capable runner, then proceed with Monitoring-only fixes and tests. **Other behavioral defects remain deferred.**
- **Important:** test infrastructure is partially verified, not yet certified as reliable across full dependency configurations.


## Owner decision (2026-11-10)
Implement CMake and trustworthy testing infrastructure first. Monitoring is the next explicitly prioritized subsystem after this gate. Keep all other audit findings in the backlog; do not implement them until owner authorizes. A foundational build/test PR may change CMake wiring for other modules without changing their implementation.


**Repository:** `majiddarvishan/vex`
**Reviewed branch/commit:** `main` / `71e74d221f182f0567f43531f7f933104cb58c68`.
**Audit documentation branch:** `audit/codex-review-2026-11-10`.
**Scope:** static first-party C++/CMake/examples/tests review, production code unchanged.

## Completed
- Read repository metadata and recursive tracked tree; inspected first-party module headers/sources and test/example/CMake structure.
- Recorded code-backed issues with severity, paths/lines, impact, proposed fixes and regressions (`AUDIT.md`).
- Prepared agent instructions, architecture map, prioritized engineering backlog, reproducibility test matrix and focused scenarios.
- All changes in this branch are documentation only.

## Not completed / limitations
- **NOT RUN:** configuration, build, unit tests, CTest, sanitizer tests, code analysis tools, benchmark, end-to-end package consumer.
- Direct shell access to GitHub was unavailable due to DNS failure (`Could not resolve host: github.com`); connected GitHub source reads were used instead.
- Vendored CPM implementation and binary image not source-reviewed.
- Some hypothesized consequences (deadlock due to external macros, exact prometheus-cpp family conflict semantics, cross-device path behavior) require dynamic/contextual verification.
- No production behavior fix was made, and no claim of shipped reliability or full executable coverage is warranted.

## Recommended next change
**Owner superseded this older recommendation:** CMake/CTest baseline has been implemented and owner-run smoke checks passed. Work on Monitoring next. Keep VEX-001, VEX-002, and all non-Monitoring behavior changes deferred until explicitly authorized.

## Continuation prompt for an AI coding agent
```text
Open majiddarvishan/vex. Read root AGENTS.md and all .codex files. Rebase/compare audit baseline with HEAD. Build a reproducible CMake/CTest test harness first, preserving API compatibility. Focus only on Monitoring regression tests and fixes (registry lifetime, reset, label validation, thread safety), with ASan/UBSan/TSan as applicable; do not implement deferred logger, ObjectPool or ThreadPool fixes until owner authorization. Do not claim tests without logs. Never merge or release automatically. Keep .codex/AUDIT.md and .codex/HANDOFF.md current with actual results and SHAs.
```
