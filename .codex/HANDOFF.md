# Vex Monitoring — latest lifetime and static-analysis findings (2026-11-14)

Branch `feature/monitoring-correctness-tests`; [Draft PR #3](https://github.com/majiddarvishan/vex/pull/3)
stacked on Foundation. Do not merge or change Draft without explicit owner approval.

## Latest owner data (checkout 6df93958)

- Linux GCC 13.3 Lifetime: **9 CSV lines, 4096/4096 short-lived
  Registry weak-owner expiration checks**, live owner quota remains
  enforced, 8/8 VmRSS/VmHWM coherent.
- RSS: 4808 KiB at round 1, **4940 KiB rounds 2–8**. glibc
  live heap `uordblks` **88672→104784 B** (monotonic +16112 B).
  **No leak or leak-free conclusion** without further attribution.
- Clang-tidy Ubuntu LLVM 20.1.2: **FAIL, checked=5,
  failed=3 translation units, 5 actionable diagnostics** in
  decorators.cpp (2), health_check.cpp (1), raii_guards.cpp (2);
  metrics.cpp/metrics_manager.cpp surfaced none.
- Post-owner-run changes: add five **narrow NOLINTNEXTLINE**
  documented exception contracts (no runtime or public signature
  change), and an **optional raw prometheus-cpp Lifetime control**
  to isolate wrapper overhead from general Registry/allocator churn.
  **These edits have NOT passed new static analysis or a new build.**

Latest evidence/raw CSV:
[MONITORING_LIFETIME_STATIC_FINDINGS_2026-11-13.md](MONITORING_LIFETIME_STATIC_FINDINGS_2026-11-13.md);
[managed lifetime CSV](benchmarks/2026-11-13-monitoring-lifetime-managed-8x512.csv).
Current [task board](MONITORING_TASKS.md).

Minimal follow-up:
```sh
git pull --ff-only origin feature/monitoring-correctness-tests
./tools/verify_monitoring_static.sh > build/monitoring-static-after.log 2>&1
./tools/benchmark_monitoring_lifetime.sh 8 512 managed > build/monitoring-lifetime-managed-new.csv
./tools/benchmark_monitoring_lifetime.sh 8 512 raw > build/monitoring-lifetime-raw.csv
tail -n 30 build/monitoring-static-after.log
cat build/monitoring-lifetime-managed-new.csv
cat build/monitoring-lifetime-raw.csv
```

Earlier owner acceptance remains: Monitoring 10/10 ×4 modes,
stress 900/900, foundation/package 13/13, and GCC/Clang
static/shared 11/11 each including external consumers; 60
Counter-control samples and consistent memory peaks. These
prior suites were not rerun after the current comment/control tool edits.

MON-OPEN-05 full static analysis **still PENDING rerun**,
MON-OPEN-06 lifetime weak ownership **verified** but heap trend
attribution still OPEN; MON-OPEN-07 owner decision outstanding.
No Logger/ObjectPool/ThreadPool runtime changes.
