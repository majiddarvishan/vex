# Vex Monitoring handoff — Lifetime build PASS; redirected evidence pending

**Branch:** `feature/monitoring-correctness-tests` · [PR #3](https://github.com/majiddarvishan/vex/pull/3)
Draft/unmerged, stacked on `feature/cmake-ctest-foundation`.

**Owner checkout SHA on 2026-11-13:** `6df93958decb1ccc9f68176e0f817af2602e04b9`.

Latest:
- `benchmark_monitoring_lifetime.sh 8 512` configured and **built
  successfully** with GCC 13.3.0. Tool invocation returned to the
  shell with no visible error. **Expected 9 CSV lines**; actual
  values and full 4096 weak Registry expiry checks cannot be
  independently confirmed until redirected CSV is read.
- `verify_monitoring_static.sh check-tools` **PASS**, Ubuntu
  LLVM/clang-tidy **20.1.2** available.
- Full static analyzer was invoked with all output redirected to
  `build/monitoring-static.log`; no log/exit code is in the pasted
  output. Do not mark static analyzer PASS without its final
  `MONITORING_STATIC_PASS checked=5 failed=0` marker.

**Next minimal step (no rerun):**
```sh
wc -l build/monitoring-lifetime.csv build/monitoring-static.log
cat build/monitoring-lifetime.csv
cat build/monitoring-static.log
```

Previous accepted gates unchanged: Monitoring baseline/threaded/
ASan+UBSan/Clang TSan 10/10 each, 900/900 stress,
foundation/package 13/13, GCC/Clang Static/Shared 11/11
each with external consumer. 60/60 peak/current RSS consistency in
prior Counter controls; hot shared-counter contention also reproduced
directly with prometheus-cpp. No production regression indicated by
those data, and no leak-free claim is supported.

[New evidence note](MONITORING_LIFETIME_STATIC_PRELIM_2026-11-13.md);
[current task board](MONITORING_TASKS.md). PR remains Draft/unmerged.
