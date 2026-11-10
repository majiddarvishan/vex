# Current scope (2026-11-10)

Monitoring is the only owner-approved runtime remediation scope.
The user authorized completing its correctness, lifecycle, concurrency,
RAII, cardinality, testing, benchmarking and documentation.
All other behavioral changes (Logger, ObjectPool, ThreadPool, etc.)
remain deferred. The Monitoring PR is Draft/unmerged and the expanded
test suite must pass on the actual checkout before certification.

---

# Agent entrypoint for Vex

**Owner implementation scope, updated 2026-11-10:** The owner-run CMake/CTest foundation baseline passed (thread 2/2, full 4/4). **Monitoring correctness and tests are now the active implementation scope.** Every other behavioral fix remains tracked but deferred until explicit owner direction. Do not confuse audit severity with implementation authorization.


Before editing this repository, read `.codex/README.md`, `.codex/AGENTS.md`, `.codex/ARCHITECTURE.md`, `.codex/AUDIT.md`, and `.codex/TEST_MATRIX.md`.

The audit baseline is commit `71e74d221f182f0567f43531f7f933104cb58c68` on `main` (2026-11-10 audit date). Recheck against the current HEAD; findings are not proof that later commits remain affected.

Do not assume the README or CMake options are accurate. Treat concurrency, memory ownership, CMake packaging, and test coverage as blocking areas. Document build/test evidence rather than claiming tests passed without executing them.

For each change: identify an issue ID in `.codex/AUDIT.md`, write a failing regression test, implement the smallest safe fix, run relevant CMake/CTest + sanitizers, record commands/results, and provide a handoff. Work on a branch and do not merge without review.
