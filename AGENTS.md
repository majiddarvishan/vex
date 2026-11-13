# Agent entrypoint for Vex

**Owner implementation scope, decided 2026-11-10:** Work on CMake and reproducible test infrastructure first; prioritize Monitoring next. Every other behavioral fix is tracked but deferred until the owner explicitly says to start it. Do not confuse audit severity with implementation authorization.


Before editing this repository, read `.codex/README.md`, `.codex/AGENTS.md`, `.codex/ARCHITECTURE.md`, `.codex/AUDIT.md`, and `.codex/TEST_MATRIX.md`.

The audit baseline is commit `71e74d221f182f0567f43531f7f933104cb58c68` on `main` (2026-11-10 audit date). Recheck against the current HEAD; findings are not proof that later commits remain affected.

Do not assume the README or CMake options are accurate. Treat concurrency, memory ownership, CMake packaging, and test coverage as blocking areas. Document build/test evidence rather than claiming tests passed without executing them.

For each change: identify an issue ID in `.codex/AUDIT.md`, write a failing regression test, implement the smallest safe fix, run relevant CMake/CTest + sanitizers, record commands/results, and provide a handoff. Work on a branch and do not merge without review.
