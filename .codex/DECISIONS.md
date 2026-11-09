# Owner decisions — Vex

## DEC-2026-11-10-01: Priority and change authorization

**Status:** Active.

1. **Approved now:** Fix CMake/module selection, dependency and packaging integration, establish reliable automated test infrastructure and evidence.
2. **Next after the foundational gates:** Monitoring. Focus on correctness, lifecycle, registry/health metric ownership, thread safety, semantics, and tests. Do not represent basic smoke tests as a full monitoring review.
3. **Deferred:** All other known findings—including critical logging queue and ObjectPool memory safety, ThreadPool behavior, and performance tuning—are to be kept documented in the audit/backlog but **must not be implemented** before the owner explicitly authorizes the relevant work.
4. Preserve the default branch; work in reviewable, isolated branches. Do not merge automatically.

**Interpretation:** Static code issues remain open and may carry serious risk. “Deferred” is not “fixed” or “safe.” CMake/test scaffolding may include other components for smoke coverage but must not silently alter their runtime implementation.

**Recording:** User confirmed the sequence in chat on 2026-11-10. Update this record when the owner changes priorities.
