# Monitoring Task Register — 2026-11-10

Scope: Monitoring only, branch
`feature/monitoring-correctness-tests`, stacked Draft PR #3.
The latest expanded automated acceptance passed on the owner's actual
checkout, and the Release benchmark baseline is recorded in
[MONITORING_FINAL_REPORT_2026-11-10.md](MONITORING_FINAL_REPORT_2026-11-10.md).
This task register separates confirmed completion from engineering
follow-ups and formal release decisions.

## Completed and verified

- [x] MON-DONE-01: CMake/CTest modules, install/package consumer tests.
- [x] MON-DONE-02: Core manager metadata concurrency, HealthCheck
  registry expiration, registration recovery tests.
- [x] MON-DONE-03: Decorators and RAII, including rejection of
  externally-owned series on guard creation.
- [x] MON-DONE-04: Name/label/UTF-8 and histogram validation;
  optional managed-series/subsystem quotas and legacy adapter.
- [x] MON-DONE-05: Baseline, threaded, ASan/UBSan, Clang TSan
  9/9 each; 800/800 stress repeat runs; full 12/12.
- [x] MON-DONE-06: Optional Release benchmark built and run,
  18 CSV samples across two series/family shapes.

## Implementation in progress — new candidate after last verified run

- **MON-OPEN-02:** The `Has`/ `Add` label re-merge TOCTOU was
  eliminated by using one validated immutable label snapshot, with a
  concurrent default-label change regression and persistent external
  metric collision. **Awaiting full owner test run.** Uncoordinated
  concurrent raw `Family::Add/Remove` is unsupported; keeping a live
  Registry/Family and not externally mutating its series during active
  guards is a documented caller contract.
- **MON-OPEN-03:** Failed registration no longer leaves empty provisional
  ledger records; managed family HELP-text conflicts reject explicitly;
  tests cover repeated registration failure, quota preservation and
  HELP mismatch. **Awaiting full owner test run.** Direct raw
  Prometheus `Registry::Remove` and `Family::Add/Remove` bypass
  manager quota accounting; they are not supported while relying on
  the manager's strict series limit.
- Tasks `MON-OPEN-01,04,05,06,07` remain open unchanged.
- The owner's earlier 9/9 ×4, 800/800 stress and 12/12 package PASS
  are **historical** evidence before the new code, not a new PASS.


## Remaining technical work

| ID | Priority | Task | Definition of done |
|---|---|---|---|
| MON-OPEN-01 | P1 | Profile 1000-Family registration overhead (~17.268 ms median vs 2.812 ms for 1000 series) | Reproduce on controlled clean Release builds; profile code paths (manager tracking vs prometheus-cpp registration), instrument allocations / CPU, record root cause; only optimize if evidence justifies it; rerun acceptance plus benchmark before/after |
| MON-OPEN-02 | P1 | Harden scoped-series ownership under live configuration changes and raw Prometheus mutation, or fully enforce a documented fail-closed contract | Validate Family::Has → Family::Add effective-label consistency with concurrent SetDefaultLabels and external Add/Remove; remove TOCTOU if supported by public API; write targeted TSan/ASan regressions; keep caller lifetime obligations explicit |
| MON-OPEN-03 | P1 | Review quota metadata across series removal, family reuse, failed registration and external Registry operations | Test duplicate labels, raw Remove/Add, family type/help conflicts, weak Registry expiration and quota rollback; decide which external mutation cases remain expressly unsupported; enforce no unexpected managed-series quota drift |
| MON-OPEN-04 | P2 | End-to-end HTTP /metrics verification with Exposer and real scrape lifecycle | Add loopback integration test for scrape format, histogram/counter/gauge values, multiple subsystems, stopped/expired Registry and clean Exposer teardown; ensure no test port conflicts or external service dependencies |
| MON-OPEN-05 | P2 | Static/portability test matrix | GCC + Clang warnings/clang-tidy/format, clean static/shared CMake install consumer, supported OS targets only where available; record versions and failures explicitly |
| MON-OPEN-06 | P2 | Extend performance/load baselines | Measure RSS, allocations, multi-thread registration/update/Collect, warm/cold startup and repeated scrape; report median plus variability and regressions by toolchain/config; avoid arbitrary production SLA claims |
| MON-OPEN-07 | Process | Owner review and merge/release decision | Re-run tests on final reviewed SHA, resolve substantive PR comments, confirm foundation dependency PR order; owner explicitly approves merge/release. Never auto-merge. |

## Ordering and boundaries

Recommended sequence: **MON-OPEN-02 and 03 (correctness)**, then
**MON-OPEN-04 (integration)**, **MON-OPEN-01 and 06 (profiling and
performance)**, **MON-OPEN-05 (portability)** and finally
**MON-OPEN-07 (owner decision)**. This order prevents optimizing
unstable contracts. Performance findings do not, alone, establish bugs.
The current 9/9, 800/800 and 12/12 results validate only the
already-exercised paths, not the open scenarios.

Logger, ObjectPool, ThreadPool and unrelated runtime behavior remain
explicitly out of scope.
