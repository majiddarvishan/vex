## Completion implementation checkpoint; validation open

Monitoring now has candidate fixes for Decorators, RAII lifetime,
health registration, cardinality quotas, histogram shape, Unicode
label validation, and the previously no-op record_metric helper.
8 C++ test executables plus one compiler-discovery test are wired;
100-repeat stress now covers all 8 C++ tests.
These changes are NOT YET RUN; prior Clang and GCC results apply to
older code only. See MONITORING_RUNS.md.

---

## 2026-11-10 — warning fix validation complete

Owner's actual Clang 20 TSan run after SAFE_* macro update:
**3/3 CTest PASS**, no `-Wunused-value` warning, and
`-Werror=unused-value` active on the contract test.
Full CMake/CTest: **6/6 PASS** including installed-package consumer.
The previous 100-repeat smoke/contract stress PASS is unchanged.
Detailed evidence: [MONITORING_RUNS.md](MONITORING_RUNS.md).

Next Monitoring-only work: examine metric family/RAII counter lifetime,
registration-failure and high-cardinality behavior, plus separate
adversarial registry-reset and concurrent usage tests. Do not expand to
Logger, ObjectPool or ThreadPool behavioral fixes without approval.

---

## Owner-run Clang 20 TSan checkpoint (2026-11-10)

The independent Clang TSan test passed **10/10 twice** (auto-detection
and explicit `CXX=clang++-20`); Monitoring TSan build/CTest passed
**3/3** (smoke, contract, compiler selection). This validates TSan
for the exercised tests on Clang 20.1.2; GCC TSan still has separate
runtime instability. Clang exposed a `-Wunused-value` warning in the
SAFE_CREATE_GAUGE macro. A code fix now applies void-casts to all SAFE_*
validation expressions, with all three macros exercised and
`-Werror=unused-value` on the contract-test target. **Owner rerun on
this new commit is still pending.** See [MONITORING_RUNS.md](MONITORING_RUNS.md).

Other module implementations remain excluded by owner instruction.

---

## Latest checkpoint: Stress PASS; numbered Clang supported (2026-11-10)

Owner-run GNU 13.3.0 stress test passed **100 consecutive smoke and
100 consecutive contract checks** (after 2/2 initial tests).
Clang binary is **`clang++-20`** on owner's machine rather than
`clang++`; unversioned lookup failed. Added an automatic Clang resolver
supporting numbered versions and `CXX` overrides, a deterministic CTest
fixture, and independent `--clang` / `tsan-clang` verification modes.
These new modes are **not executed yet**. GCC TSan still blocked and
non-Monitoring behavioral work still deferred.

---

## Environment diagnostic checkpoint (2026-11-10)

Independent TSan runtime probe failed 7/10 launches (six mapping
fatalities, one SegFault), while three runs passed. Monitoring smoke
TSan rerun also SegFault. Thus TSan is BLOCKED on the present host.
An optional Clang probe or compatible CI host is the next TSan gate.
A non-sanitizer 100 repeats of each Monitoring smoke/contract test has been
added, but it cannot replace TSan. Full evidence: MONITORING_RUNS.md.

---

# Monitoring reliability work plan

**Branch:** `feature/monitoring-correctness-tests`, based on verified CMake/CTest foundation.
**Owner scope:** Monitoring only, after baseline `thread` 2/2 and `full` 4/4 checkout smoke tests passed (2026-11-10).

## Planned regression
- Monitor initialization, subsystem registry and invalid argument behavior.
- Label merge precedence and compile-time SAFE_* macro qualified namespace.
- Re-register HealthCheck on same registry without duplicate family registration.
- Concurrent control-plane reads/health gauge updates and uptime.
- Reset/re-init and expiration of metrics handles after no owners remain.

## Safety design
- Guard shared MetricsManager state under one unconditional mutex, returning shared_ptr snapshots to avoid stale registry reads across Reset.
- HealthCheck stores a **weak_ptr** to its registry; every metric access must lock the weak_ptr and retain a strong local reference, guarded by an independent mutex, before dereferencing gauges. It must not own registry lifetime forever. Re-registration in the same registry is idempotent.
- `MetricsManager::Reset` is for tests / quiescent teardown; callers must not retain/use raw metric references across registry removal. Externally held shared registry owners may intentionally keep HealthCheck usable after manager Reset.
- Do **not** modify ObjectPool, ThreadPool, Logger or other tracked runtime modules.
- `METRICS_MULTI_THREADED` and runtime `Init(enable_threading)` remain API-compatible but may still select compatibility/reporting semantics; manager's own state access is synchronized regardless of flag.

## Actual checkout evidence (2026-11-10)

Owner's reported results: baseline **2/2 PASS**, threaded **2/2 PASS**, ASan/UBSan **2/2 PASS**, full integration **5/5 PASS**. TSan compiled, `contract` PASSED but `smoke` FAILED with `ThreadSanitizer: unexpected memory mapping`. Record: [MONITORING_RUNS.md](MONITORING_RUNS.md). TSan is **NOT PASSED** and cannot be used to certify race freedom. The next diagnostic is `./tools/diagnose_tsan.sh 10` (no privileged machine changes).

## Historical pre-run plan
Implementation (manager synchronization, weak-registry HealthCheck access and namespace-qualified type-safe macros) is prepared as a separate commit after the contract-test commit. These changes need execution on the owner's real checkout. The existing `full` smoke PASS was from the owner's original checkout before this branch. The new contract test and fixes are **NOT RUN** at creation time. Run `./tools/verify_foundation.sh full`, and independently ASan/UBSan + TSan after implementation.

## Verification helper

`tools/verify_monitoring.sh baseline`: Monitoring-only build, compile-time threading disabled, both monitoring tests.
`tools/verify_monitoring.sh threaded`: `ENABLE_METRICS_THREADING=ON`, same tests.
`tools/verify_monitoring.sh asan`: AddressSanitizer + UndefinedBehaviorSanitizer, isolated binary.
`tools/verify_monitoring.sh tsan`: ThreadSanitizer, isolated binary (do not combine with ASan).

The helper intentionally filters to monitoring tests so the separately installed package consumer is not linked with sanitizer-instrumented static objects but absent sanitizer flags. The `full` foundation run already validated installation. These monitoring helper commands are **NOT RUN** until owner reports execution.
