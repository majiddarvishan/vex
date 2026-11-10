## 2026-11-10 paired raw-vs-managed benchmark measured

Owner supplied 36 CSV rows, valid 37-line file including header.
Median (3 samples) registration at 1000:
- one-family-many-series: Vex **3.070ms**, raw **0.930ms**
- many-families: Vex **17.151ms**, raw **9.568ms**

Both upstream Registry and Vex wrapper contribute. Vex's
`tracked_metric_series_` vector is scanned for expired entries and
matching Family on every registration; the upstream
`Registry::Add` also performs linear scans of Family vectors.
The former is an opportunity for indexed manager bookkeeping;
any improvement must be verified, not assumed.

Full analysis: [MONITORING_PROFILE_2026-11-10.md](MONITORING_PROFILE_2026-11-10.md).
Raw CSV: [benchmarks/2026-11-10-monitoring-managed-vs-raw.csv](benchmarks/2026-11-10-monitoring-managed-vs-raw.csv).

---

# Current Vex Monitoring Handoff — 2026-11-10

**Branch:** `feature/monitoring-correctness-tests`  
**Draft PR:** https://github.com/majiddarvishan/vex/pull/3  
**Stacked base:** `feature/cmake-ctest-foundation`  

The owner submitted a full `./tools/verify_monitoring.sh all`
terminal transcript **after** the MON-OPEN-02/03 fixes and HTTP
integration addition. **All PASS:** baseline 10/10, threaded 10/10,
ASan+UBSan 10/10, Clang TSan 10/10, stress 900/900 and full
foundation/installed package consumer 13/13. The exact local SHA was
not included, although the GitHub branch was at `bab89053edabf986efb3c06a008c3e037faec761` before
recording this evidence. See
[MONITORING_ACCEPTANCE_2026-11-10_HTTP.md](MONITORING_ACCEPTANCE_2026-11-10_HTTP.md).

**Current task ledger:** [MONITORING_TASKS.md](MONITORING_TASKS.md)
02/03/04 closed for documented **supported Vex wrapper contracts**;
external uncoordinated raw prometheus-cpp Family mutations remain
unsupported. MON-OPEN-01 has a newly implemented optional paired raw-vs-managed Release benchmark, **NOT YET RUN**; command: `./tools/benchmark_monitoring.sh 3`. Next: MON-OPEN-01 (profiling registration), MON-OPEN-06
(resource/concurrency performance), MON-OPEN-05 (portability), then
MON-OPEN-07 (owner review and merge approval).

Previous Release benchmark measured median 1000-family registration
17.268 ms vs 1000 labeled-series registration 2.812 ms on the user's
GCC 13.3.0 system; this does **not** establish which layer is at fault.
Do not fabricate performance numbers. No further runtime fixes in
other modules are approved.

Historical handoff notes are in [HANDOFF_HISTORY.md](HANDOFF_HISTORY.md).
**Do not merge or mark PR ready automatically.**
