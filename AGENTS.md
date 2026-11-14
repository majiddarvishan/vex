# Current project status (2026-11-14)

The Audit, CMake/CTest Foundation, and Monitoring work is integrated into the default `main` branch through merged PRs #1–#4. Monitoring work items MON-OPEN-01..07 are complete within their documented test scope. Historical owner-run evidence is recorded in `.codex/MONITORING_FINAL_REVIEW_2026-11-14.md` and `.codex/MONITORING_MEMORY_SOAK_2026-11-14.md`; do not describe those local runs as GitHub Actions CI or as proof of universal leak/race freedom.

The current library release version is **0.1.0**. Follow `.codex/VERSIONING.md` for every version change and release. Logger, ObjectPool and ThreadPool runtime remediation remains deferred until the owner explicitly authorizes that scope.

---

# Agent entrypoint for Vex

**Owner implementation scope, updated 2026-11-14:** Audit, CMake/CTest Foundation and Monitoring are integrated into `main`; Monitoring acceptance is closed within the recorded evidence. Logger, ObjectPool and ThreadPool runtime fixes remain deferred until explicit owner direction. Do not confuse audit severity with implementation authorization.


Before editing this repository, read `.codex/README.md`, `.codex/AGENTS.md`, `.codex/ARCHITECTURE.md`, `.codex/AUDIT.md`, `.codex/TEST_MATRIX.md`, and `.codex/VERSIONING.md`.

The audit baseline is commit `71e74d221f182f0567f43531f7f933104cb58c68` on `main` (2026-11-10 audit date). Recheck against the current HEAD; findings are not proof that later commits remain affected.

Do not assume the README or CMake options are accurate. Treat concurrency, memory ownership, CMake packaging, and test coverage as blocking areas. Document build/test evidence rather than claiming tests passed without executing them.

For each change: identify an issue ID in `.codex/AUDIT.md`, write a failing regression test, implement the smallest safe fix, run relevant CMake/CTest + sanitizers, record commands/results, and provide a handoff. Work on a branch and do not merge without review, except when the owner explicitly authorizes a direct commit to a named branch (such as a version bump on `main`).
