# Vex Monitoring handoff — 2026-11-10

Repository: `github.com/majiddarvishan/vex`
Branch: `feature/monitoring-correctness-tests`
Draft PR: https://github.com/majiddarvishan/vex/pull/3
Stacked on `feature/cmake-ctest-foundation`. Do NOT merge to main
without an explicit owner instruction.

## Current evidence boundary

The owner's last complete Monitoring verification on the earlier
candidate passed: baseline, threaded, ASan/UBSan and Clang 20 TSan
**9/9 each**; stress **800/800**; full package consumer **12/12**.
The real Release benchmark passed. Full report:
[MONITORING_FINAL_REPORT_2026-11-10.md](MONITORING_FINAL_REPORT_2026-11-10.md).

**New code after that owner run is NOT YET owner-validated:**
- MON-OPEN-02: One immutable validated label snapshot is now passed to
  both `Family::Has` and `Family::Add`; a concurrent
  `SetDefaultLabels`/persistent-series collision regression was added.
  Raw concurrent external `Family::Add/Remove` and `Registry::Remove`
  remain unsupported by contract; Vex cannot synchronize uncoordinated
  third-party calls.
- MON-OPEN-03: Failed Create*/quota registration now removes newly
  created empty ledger entries. Manager explicitly rejects conflicting
  HELP text for previously tracked families (unlike underlying
  prometheus-cpp Merge). Regression includes strict registry collision,
  repeated failed registration and per-family quota preservation.
  Direct external mutations remain outside managed quota accounting.

New code is a review candidate. **Never claim tests passed after these
changes until a new owner-run transcript confirms them.**

## HTTP integration candidate added after last verified run

`vex.monitoring.http` is a new UNIX-only CTest. It creates an
Exposer bound to `127.0.0.1:0`, reads its actual listening port
and scrapes /metrics via a small standard POSIX socket client. It
covers main/subsystem/external Registries, metric serialization,
Registry expiry and Exposer cleanup. It has NOT been run yet.
Expected Linux CTest counts are now Monitoring 10, 9 C++ stress
tests, full package 13. Do not report these as PASS.

## Next test gate

```sh
git pull --ff-only origin feature/monitoring-correctness-tests
git rev-parse HEAD
git status --short
./tools/verify_monitoring.sh all
```

After PASS, update [MONITORING_TASKS.md](MONITORING_TASKS.md):
MON-OPEN-02 and MON-OPEN-03 can be closed only for the
**supported wrapper API contract**, with the third-party raw
mutation limitation explicit.

## Scope

Only Monitoring/runtime code, its test/build helpers, and
documentation are authorized. Logger, ObjectPool, ThreadPool
behavioral changes remain deferred. PR #3 stays Draft/unmerged.
No changes to host ASLR or kernel settings.

Historical handoffs: [HANDOFF_HISTORY.md](HANDOFF_HISTORY.md).
Current task register: [MONITORING_TASKS.md](MONITORING_TASKS.md).
