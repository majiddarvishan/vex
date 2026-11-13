# VEX Monitoring — MON-OPEN-07 final technical review and PR handoff (2026-11-14)

**Disposition:** **MONITORING TECHNICAL REVIEW COMPLETE — HANDOFF READY; OWNER PR STATE / MERGE DECISION PENDING.** This is not a GitHub review approval, CI pass, merge authorization or an assertion that every possible source defect is absent.

## Identity and bounds

- Repository: `majiddarvishan/vex`; review scope: Monitoring implementation, its public headers, test/benchmark harnesses, build/package integration and associated docs.
- Branch: `feature/monitoring-correctness-tests` only; reviewed prior HEAD `4198661c2ad5813bad2978f1c0e956842a6d35cc` (a documentation-only follow-up after the successfully tested Monitoring source).
- PR #3: **Open, Draft, not merged**; base `feature/cmake-ctest-foundation`; **84 changed files, 86 commits at the pre-report review snapshot**. PR #2 (foundation) is itself **Draft and unmerged**, based on `audit/codex-review-2026-11-10`. Preserve the stack order; never silently retarget.
- No new Monitor Runtime change, API signature change, dependency install, deploy, logger/object_pool/threadpool behavior edit, Draft conversion, base retarget or merge was performed for this final review.
- GitHub connector reported **no review submissions, inline review threads or issue/PR discussion comments** at the review snapshot. GitHub Actions listing and commit checks returned **zero runs/checks** for the checked review head. Do not represent owner-run local results as hosted CI.

## Focused reviewer inspection

Checked source-backed semantics of:
- `src/monitoring/metrics_manager.cpp` and `include/vex/monitoring/metrics_manager.hpp`: manager mutex, registry shared ownership, owner-aware `weak_ptr` keyed tracking, quota/HELP/bucket rollback, expired-key sweep on each 256th tracked registration attempt (including failures), quiescent `Reset`, and identity distinct from raw pointer reuse.
- `src/monitoring/health_check.cpp` and public header: weak Registry ownership, temporary strong snapshot during pointer use, idempotent registration, error recovery on metric-family collision, guarded mutators.
- `src/monitoring/raii_guards.cpp` and public header: immutable merged-label snapshot for `Has` and `Add`, external-series rejection, guard refcount and non-throwing release; Registry/Family lifetime and external mutation remain caller responsibilities.
- `src/monitoring/decorators.cpp`, `metrics.cpp`, `metrics.h`, selected contract and lifecycle test sources.
- `CMakeLists.txt`, `modules.cmake`, `modules/monitoring/CMakeLists.txt`, `tests/CMakeLists.txt`, `tools/verify_monitoring.sh`, `tools/verify_monitoring_portability.sh`, soak runner source/script, public `README.md` and `API.md`.
- No newly *demonstrated* source bug in those inspected paths requiring Runtime changes. This review is focused, **not a claim that all 84 PR files were independently line-by-line inspected**. Legacy limitations are documented, not hidden.

## Owner-verifiable executed acceptance

| Gate | Owner-provided result | Attribution / limits |
|---|---|---|
| Monitoring baseline / threaded / ASan+UBSan / Clang 20 TSan | **10/10 PASS each** | Linux host; TSan selected Clang, GCC TSan host/runtime issue is historical |
| Monitoring stress | **900/900 PASS** | Repeated exercised C++ contract tests only |
| Foundation + external installed consumer | **13/13 PASS** | Combined CTest package gate |
| GCC/Clang × static/shared packaging | **11/11 PASS each of four modes** | Linux, tested dependency provisioning paths |
| Focused clang-tidy 20.1.2 | **checked=5 failed=0** | Selected checks with five narrowly justified suppressions, not universal lint clean |
| Lifetime Managed 8×509 | **4072/4072 expirations PASS** | Phase-aligned 256-attempt sweep, no infinite-lifetime guarantee |
| Managed 300s | **599999/599999 expirations; 2374 quota checks; 2371 actual sweeps; PASS** | 300.001s, same-phase RSS 5016KiB / live heap 90352B flat after 30s |
| Raw 300s | **599999/599999 expirations; PASS** | 300.000s, RSS 4500KiB / live heap 81504B flat after 30s, no manager Sweep |
| GitHub-hosted CI on reviewed head | **NONE OBSERVED** | No hosted green check may be inferred |

See `MONITORING_MEMORY_SOAK_2026-11-14.md`, `MONITORING_ACCEPTANCE_2026-11-10_HTTP.md`, `MONITORING_PORTABILITY_LOAD_RESULTS_2026-11-10.md`, `MONITORING_TASKS.md`, and `TEST_MATRIX.md` for detailed evidence. Latest review commit contains **documentation-only changes** after the owner-run tests. It does not establish a newly executed full matrix at the documentation SHA.

## Reviewer residual risks and explicit contracts

1. `MetricsManager::Reset()` is **quiescent-only**. External raw metric references and RAII objects must not outlive their Registry/Family; bypassing Vex ownership contracts can cause invalid references.
2. Concurrent *uncoordinated raw prometheus-cpp* Family Add/Remove alongside scoped guards is unsupported; no atomic cross-API guarantee exists.
3. Direct Prometheus Family/Registry mutations bypass managed quota/ledger enforcement; default quota is unlimited unless configured.
4. Sweep cleans expired tracking metadata on the **next multiple of 256 tracked registration attempts**; if no more registrations occur, stale weak-key bookkeeping can remain until `Reset()`. The observed pre/post glibc heap sawtooth is bounded under the tested churn, not proof of leak freedom.
5. Results are Linux and workload-specific; no Windows/macOS certification, no longer soak/multithreaded-soak proof, and no GitHub-hosted CI gate. The historical old/new 1000-series noisy ~6.1% signal remains **unconfirmed research**, not a blocker or a declared regression.
6. The PR is **stacked** on an unmerged Draft foundation PR #2. Keep explicit reviewer awareness of parent/base; merge order and any conversion of PR status require owner authorization.

## Documentation corrections in final review

- Public Monitoring `README.md` now highlights measured acceptance and replaces stale 'not yet tested' language in legacy benchmark, shared packaging, static and lifetime sections.
- Public `API.md` fixes the ASCII DEL validation wording and the obsolete 'HTTP test awaiting verification' line.
- `.codex/AGENTS.md`, `MONITORING_TASKS.md`, `TEST_MATRIX.md`, `MONITORING_REVIEW.md`, `HANDOFF.md` have updated authoritative final status. Older dated progress text is preserved as historical evidence, explicitly superseded.

## Final decision and handoff

**MON-OPEN-01..06:** ACCEPTED/CLOSED within stated test scope.

**MON-OPEN-07 technical review/documentation package:** **COMPLETE**; no new confirmed blocking defect in the focused audited paths; no additional owner-run test needed for these documentation-only changes.

**MON-OPEN-07 GitHub PR transition / merge / final approval:** **PENDING OWNER EXPLICIT DECISION**. Remain **Draft**, preserve original base, do not merge; the owner may later request specific Ready/merge actions after reviewing the stack and any current checks/reviews. Release/merge are not implicitly authorized by 'finish the Monitoring work.'

Next module work is out of scope of this Monitoring closure.
