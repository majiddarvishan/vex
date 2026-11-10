# Foundation validation status — 2026-11-10

## Owner-run validation (2026-11-10)

**Real checkout PASS:** `./tools/verify_foundation.sh thread` — **2/2** tests; `./tools/verify_foundation.sh full` — **4/4** tests with GCC 13.3.0, including the independently built package consumer in both modes. Detailed provenance and scope: [FOUNDATION_RUNS.md](FOUNDATION_RUNS.md). This clears the baseline prerequisite for moving to Monitoring; optional toolchain/configuration matrix remains open.

## Owner-approved sequence
1. CMake/module/dependency/packaging/test infrastructure: **baseline verified by owner; extended matrix pending**.
2. Monitoring correctness/concurrency/lifecycle: **NEXT**, authorized to begin.
3. All other behavioral/performance fixes: **FROZEN**, log only until explicit owner approval.

## Branches and review
- Documentation: `audit/codex-review-2026-11-10`, Draft PR #1.
- Foundation: `feature/cmake-ctest-foundation`, stacked Draft PR #2 targeting documentation branch. Neither is merged.

## Added controls
- `BUILD_MONITORING` gates monitoring target; examples do not force modules on.
- Logger flag defaults OFF and fails explicitly if deliberately enabled, because missing dependencies are still unaddressed (deferred).
- Installed `Vex::` targets have relocatable include paths and declared dependency discovery.
- `BUILD_TESTING` / CTest discovers module smoke tests and performs a real out-of-tree installed-package consumer build/run.
- `tools/verify_foundation.sh thread` verifies the no-network subset; `tools/verify_foundation.sh full` requests all supported modules and depends on prometheus-cpp.

## Evidence actually obtained
Environment: Debian image, GCC 14.2.0, CMake 3.31.6, Ninja 1.12.1.
A manually reconstructed ThreadPool-only subset (matching the inspected CMake/test logic, but **not** a cloned repository) was configured, built and tested:
- `vex.thread_pool.smoke`: PASS.
- `vex.package.consumer` (install + external consumer configure/build/run): PASS.
- All-OFF CMake configure: PASS; 0 tests expected.
- Recreated verification helper logic in thread mode: PASS (2/2 CTest).
The original source checkout and all Prometheus-dependent builds were **NOT RUN**. Do not imply full CMake/test readiness from the isolated checks.

## Remaining gates before calling Phase 0 complete
- Clone actual `feature/cmake-ctest-foundation` and run `./tools/verify_foundation.sh thread`.
- With prometheus-cpp 1.3.x installed or fetch available, run `./tools/verify_foundation.sh full` and the separate package import test.
- Exercise objectpool-only, monitoring-only, all-OFF and examples OFF/ON; compare static vs shared builds, GCC vs Clang.
- Diagnose full compiler/CTest failures, verify CMake package dependency exports and update the audit statuses.
- Record logs, exact commit/versions, and pass/fail/blocked; only then move to Monitoring behavior.

## Explicitly deferred
VEX-001, VEX-002, VEX-003 and remaining Logger, ObjectPool behavior, ThreadPool behavior, and performance/remediation findings continue to be risk-tracked in `AUDIT.md`; no implicit authorization to fix them.
