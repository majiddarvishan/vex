# Vex Monitoring handoff — PIC portability fix awaiting rerun

**Branch:** `feature/monitoring-correctness-tests` · [Draft PR #3](https://github.com/majiddarvishan/vex/pull/3).
No merges; Logger/ObjectPool/ThreadPool runtime remains frozen.

## New owner evidence

`Pasted text(6).txt`:
- Controlled benchmark 15 repeats and load/resource benchmark 3
  repeats **built and finished**; their redirected CSV results were
  not included (expected lines 181 and 34).
- GCC static portability: **11/11 CTests PASS**, including installed
  package consumer; warning diagnostics on two variadic macro calls
  and one histogram const-reference binding.
- GCC shared portability: linker **FAIL** on installed non-PIC
  `/usr/local/lib/libprometheus-cpp-{core,pull}.a`.
  This is a dependency compatibility issue, not a failed runtime test.
- Clang static/shared **NOT RUN**, old script stopped at GCC shared.

## Subsequent unverified changes

`type_safety.hpp` C++17 portable SAFE_CREATE_* argument handling,
test const-ref warning cleanup and -Werror=pedantic guard.
The portability matrix now forces an isolated pinned prometheus-cpp
build for Shared (without modifying system packages), continues
through all requested modes and exits nonzero if any mode fails.

**Next:** re-run `./tools/verify_monitoring.sh all` and
`./tools/verify_monitoring_portability.sh all`, then supply
`build/monitoring-controlled.csv` and
`build/monitoring-load.csv` for analysis. See detailed
[MONITORING_PORTABILITY_PARTIAL_2026-11-10.md](MONITORING_PORTABILITY_PARTIAL_2026-11-10.md)
and [MONITORING_TASKS.md](MONITORING_TASKS.md).

Previous indexed-ledger acceptance 10/10 ×4, 900/900 stress,
13/13 foundation, and 34.9% synthetic 1000-family registration
improvement were obtained before the new warning fixes.
Do not mark the new commit PASS or merge until rerun.
