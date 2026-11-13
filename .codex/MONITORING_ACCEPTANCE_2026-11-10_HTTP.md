# Monitoring Acceptance — Extended Ownership + HTTP Integration

**Date:** 2026-11-10  
**Source:** Owner-supplied complete terminal output of `./tools/verify_monitoring.sh all` (`Pasted text(4).txt`, 2233 lines)  
**Branch:** `feature/monitoring-correctness-tests`  
**Recorded GitHub head before this documentation commit:** `bab89053edabf986efb3c06a008c3e037faec761`  
**Exact owner checkout SHA:** Not printed in the supplied output; do not falsely assert a verified local SHA.

## Full result

| Stage | Result | CTest time (real) |
|---|---:|---:|
| Baseline | **10/10 PASS** | 0.49 s |
| Threaded | **10/10 PASS** | 0.49 s |
| ASan/UBSan | **10/10 PASS** | 0.66 s |
| Clang 20 TSan | **10/10 PASS** | 0.60 s |
| Stress initial suite | **10/10 PASS** | 0.42 s |
| Stress repeat (`until-fail:100`) | **9/9 C++ tests × 100 = 900/900 PASS** | 30.90 s |
| Full foundation incl. installed-package consumer | **13/13 PASS** | 2.34 s |

Stress counts checked explicitly against the provided output:
`smoke`, `contract`, `decorators`, `raii`, `validation`,
`lifecycle`, `registration`, `legacy`, `http` — exactly
**100 successful repeated executions each**, 0 failures.
The `compiler-selection` shell CTest was exercised in the normal
suites but deliberately omitted from the repeat selector.

Final lines:
```text
VEX FOUNDATION PASS mode=full
VEX MONITORING ACCEPTANCE PASS all
```

No compile/test failure and no ASan, UBSan or TSan diagnostic appear
in this supplied log. The 10/10 suite explicitly includes
`vex.monitoring.http` (real loopback scrape), while `raii` and
`validation` exercise the new label snapshot, registration rollback
and HELP consistency paths.

## Disposition

**MON-OPEN-02 — VERIFIED for supported Vex API semantics**:
`ScopedCounter` and `ScopedGauge` use one immutable effective-label
snapshot for the ownership check and insertion. Threaded default-label
reconfiguration regression passed. **External concurrent
`prometheus::Family::Add/Remove`/`Registry::Remove` is not an
atomic operation coordinated with Vex guards and remains expressly
unsupported**; callers must not violate lifetime ownership contracts.

**MON-OPEN-03 — VERIFIED for managed Create* API**:
a provisional quota ledger record is removed on failed registration,
HELP conflicts are rejected, repeated strict-registry failures can
recover, and the bounded series limit still holds. Direct external
Prometheus mutations bypass Vex quota bookkeeping; no guarantee is
made of reconciliation for those unsupported calls.

**MON-OPEN-04 — VERIFIED**: real `/metrics` transport, the expected
Counter/Gauge/Histogram sample values across main/subsystem
Registries, transient Registry expiry, manager Reset, and clean Exposer
teardown. Ephemeral `127.0.0.1:0` bind avoided fixed-port collisions.
HTTP stress was repeated 100 times, all PASS.

These test results are strong evidence for the exercised paths, not a
proof of absence of every race, use-after-free under contract
violations or all operating-system/toolchain combinations. No new
Release benchmark measurement was attached in this report; the
previous benchmark baseline remains recorded separately in
[MONITORING_FINAL_REPORT_2026-11-10.md](MONITORING_FINAL_REPORT_2026-11-10.md).

**Remaining open tasks:** MON-OPEN-01 (registration overhead profiling),
MON-OPEN-05 (portability/static analysis), MON-OPEN-06
(resource/concurrency performance), MON-OPEN-07 (owner's final review
and merge/release decision).

The PR is **Draft/unmerged**. Logger, ObjectPool and ThreadPool
behavior changes remain outside scope.
