# Vex engineering review hub

**Snapshot:** `main` at `71e74d221f182f0567f43531f7f933104cb58c68`; assessed 2026-11-10.

**Purpose:** durable context for Codex/other AI agents, maintainers, and reviewers. Documents describe source-observed risks and a validation plan, not certified dynamic test results.

## Navigation
- `AGENTS.md` — agent workflow, safety boundaries, change protocol.
- `ARCHITECTURE.md` — actual modules, dependency graph, ownership and invariants.
- `AUDIT.md` — source-level findings with evidence, severity and fixes.
- `BACKLOG.md` — ordered engineering remediation phases and exit gates.
- `TEST_MATRIX.md` — concrete test/build/sanitizer matrix.
- `REPRO_GUIDE.md` — targeted reproductions and assertions.
- `HANDOFF.md` — status and unresolved validation.

## Audit scope / confidence
Reviewed repository layout and first-party C++ headers/sources, example and test sources, root/module/example/test CMake and helper scripts through the linked GitHub repository. Vendored `cmake/CPM.cmake` and binary `assets/logo.png` were inventoried but not independently audited. No complete local checkout was available in this execution environment (the public GitHub host could not be resolved by shell); accordingly **no configure/build, CTest, ASan, UBSan, TSan, benchmark, fuzzing, or runtime verification was executed**. Findings are static assessments. Distinguish deterministically visible code defects from hypotheses to validate.

## Severity scale
- **Critical:** probable UB/data corruption/process termination in ordinary reachable code paths.
- **High:** significant reliability/integration issue or an exploitable failure path requiring configuration/timing.
- **Medium:** maintainability, performance, edge case, observability, or test-coverage deficiency.
- **Low:** documentation/developer experience/cleanup.

**Safe default:** documentation-only audit branch; no production behavior changed.
