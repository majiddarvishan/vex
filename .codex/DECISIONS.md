# Owner decisions — Vex

## DEC-2026-11-10-01: Priority and change authorization

**Status:** Active.

1. **Approved now:** Fix CMake/module selection, dependency and packaging integration, establish reliable automated test infrastructure and evidence.
2. **Next after the foundational gates:** Monitoring. Focus on correctness, lifecycle, registry/health metric ownership, thread safety, semantics, and tests. Do not represent basic smoke tests as a full monitoring review.
3. **Deferred:** All other known findings—including critical logging queue and ObjectPool memory safety, ThreadPool behavior, and performance tuning—are to be kept documented in the audit/backlog but **must not be implemented** before the owner explicitly authorizes the relevant work.
4. Preserve the default branch; work in reviewable, isolated branches. Do not merge automatically.

**Interpretation:** Static code issues remain open and may carry serious risk. “Deferred” is not “fixed” or “safe.” CMake/test scaffolding may include other components for smoke coverage but must not silently alter their runtime implementation.

**Recording:** User confirmed the sequence in chat on 2026-11-10. Update this record when the owner changes priorities.

## DEC-2026-11-10-02: Foundation verified and Monitoring in progress

Owner provided actual checkout logs on `feature/cmake-ctest-foundation`: `verify_foundation.sh thread` PASS (2/2), `verify_foundation.sh full` PASS (4/4), with installed package-consumer tests passing. This satisfies the baseline prerequisite to begin Monitoring work. An extended build/sanitizer/toolchain matrix is still open. Monitoring work lives on `feature/monitoring-correctness-tests`; no other module behavioral changes authorized.

## DEC-2026-11-10-03 — Complete Monitoring, preserve other freeze

Owner expressly requested finishing Monitoring. Authorized implementation covers Monitoring behavior, API, tests, lifecycle, cardinality, performance benchmark setup and documentation. Logger/ObjectPool/ThreadPool behavioral work remains deferred. No branch merge without owner approval.
