# Vex Monitoring handoff — 2026-11-14

**Repository / branch:** `majiddarvishan/vex` /
`feature/monitoring-correctness-tests`. **Draft PR #3** stacked on Foundation, unmerged.

**Known owner-run PASS:** Monitoring 10/10 each baseline/threaded/
ASan+UBSan/Clang20 TSan, stress 900/900, full foundation/package
13/13, GCC/Clang static/shared package consumers 11/11 each.
Counter load `--controls` 60/60 records valid; peak RSS >= current RSS
60/60. At 8 threads Vex sharded vs shared throughput ~223.97M vs
8.73M ops/s; direct prometheus-cpp reproduces contention. No
Vex-specific Counter-update hot-path bug established.

**Implemented since those results (NOT RUN):**
- Linux weak Registry lifetime and RSS/glibc heap trend probe:
  `tools/benchmark_monitoring_lifetime.sh 8 512` (9 CSV lines expected).
- Focused warning-as-error clang-tidy gate:
  `tools/verify_monitoring_static.sh`; tool availability first via
  `check-tools`. No third-party package installation.

[Current task register](MONITORING_TASKS.md);
[acceptance method](MONITORING_FOLLOWUP_PLAN_2026-11-14.md).

**Open:** MON-OPEN-05 optional static analysis, MON-OPEN-06
lifetime/resource evidence and optional allocation call-site
profiling, MON-OPEN-07 owner review and merge decision. Historic
1000-series +6.1% difference remains **unproven** without matching
old/new warmed harness runs. No other module runtime changes allowed.

Owner-run minimal probes:
```sh
git pull --ff-only origin feature/monitoring-correctness-tests
./tools/benchmark_monitoring_lifetime.sh 8 512 > build/monitoring-lifetime.csv
./tools/verify_monitoring_static.sh check-tools
./tools/verify_monitoring_static.sh > build/monitoring-static.log 2>&1
```
If clang-tidy is missing, record NOT RUN; no installation.
After evidence, inspect and correct only reproducible issues.
Keep PR Draft/unmerged until explicit owner approval.
