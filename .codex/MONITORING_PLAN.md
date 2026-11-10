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

## Evidence
Implementation (manager synchronization, weak-registry HealthCheck access and namespace-qualified type-safe macros) is prepared as a separate commit after the contract-test commit. These changes need execution on the owner's real checkout. The existing `full` smoke PASS was from the owner's original checkout before this branch. The new contract test and fixes are **NOT RUN** at creation time. Run `./tools/verify_foundation.sh full`, and independently ASan/UBSan + TSan after implementation.
