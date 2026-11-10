# Vex Monitoring — current handoff (2026-11-10)

**Branch:** `feature/monitoring-correctness-tests`; [Draft PR #3](https://github.com/majiddarvishan/vex/pull/3) remains stacked on foundation PR #2 and **unmerged**.

## Latest verified evidence

Owner ran the indexed-ledger version successfully:
**10/10** baseline/threaded/ASan+UBSan/Clang TSan;
**900/900** stress; **13/13** foundation and installed package consumer.
No failed tests or sanitizer reports observed; exact local Git SHA not printed in logs.

Owner subsequently supplied the after-index Release CSV: 37 lines,
36 rows across four paired Vex/raw shapes and 10/100/1000 samples.
At 1000 independent Families:
- Vex median **17.151 → 11.169 ms** (**34.9% decrease**).
- Raw prometheus-cpp median **9.568 → 9.575 ms** (~stable).
- Vex-vs-raw overhead difference **7.583 → 1.594 ms**.
At 1000 series under one Family, Vex median **3.070 → 3.257 ms**
(+6.1%), with overlapping three-run ranges; follow up in MON-OPEN-06.
No RSS/allocation or multi-thread benchmark was yet measured.

**MON-OPEN-01 targeted improvement is complete for measured workload.**
Tasks MON-OPEN-02/03/04 are also verified within supported contracts.
Open: MON-OPEN-06 (more performance/resource measurements), MON-OPEN-05
(static/portability matrix), MON-OPEN-07 (owner review/merge decision).

Evidence:
- [Before/after report](MONITORING_PROFILE_BEFORE_AFTER_2026-11-10.md)
- [After CSV](benchmarks/2026-11-10-monitoring-managed-vs-raw-after.csv)
- [Indexed acceptance](MONITORING_INDEXED_ACCEPTANCE_2026-11-10.md)
- [Task board](MONITORING_TASKS.md)

Next: work on MON-OPEN-06 and MON-OPEN-05 in Monitoring scope only.
No other module behavioral changes. Do not merge the Draft PR without
explicit owner approval. The documents are updated after owner test
and CSV; no Monitoring source change was made in the reporting commits.
