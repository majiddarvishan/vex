# Current Vex Monitoring Handoff — indexed-ledger tests PASS, speedup unmeasured

Branch: `feature/monitoring-correctness-tests`; Draft PR #3 stacked
on Foundation. Only Monitoring runtime/tests/docs remain in scope.

**Latest owner-run acceptance after indexed-ledger changes:**
baseline/threaded/ASan+UBSan/Clang 20 TSan **10/10 each**,
stress 9 C++ tests × 100 = **900/900**, foundation+installed consumer
**13/13**; terminal `VEX MONITORING ACCEPTANCE PASS all`.

Owner also built and ran the after-change Release benchmark at
2026-11-10T14:33:12Z, but it redirected stdout to
`build/monitoring-profile-after.csv` and supplied no contents.
**No measured speedup/regression verdict is supported yet.**

Full new evidence:
[MONITORING_INDEXED_ACCEPTANCE_2026-11-10.md](MONITORING_INDEXED_ACCEPTANCE_2026-11-10.md).
Before-change baseline CSV:
[benchmarks/2026-11-10-monitoring-managed-vs-raw.csv](benchmarks/2026-11-10-monitoring-managed-vs-raw.csv).

**Next:** `wc -l build/monitoring-profile-after.csv` and
`cat build/monitoring-profile-after.csv`, then document
before/after medians and close or continue MON-OPEN-01 as justified.
Remaining MON-OPEN-05 portability/static, MON-OPEN-06 resource/load,
MON-OPEN-07 owner code review and explicit merge/release decision.
Previous HANDOFF entries archived in HANDOFF_HISTORY.md.
Do not merge; non-Monitoring behavior changes remain deferred.
