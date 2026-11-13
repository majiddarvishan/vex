# Vex Monitoring handoff — Shared consumer RUNPATH rerun pending

**Branch:** `feature/monitoring-correctness-tests`; Draft
[PR #3](https://github.com/majiddarvishan/vex/pull/3) stacked on
foundation PR #2. No merge.

## Latest owner evidence

The post-macro-fix `verify_monitoring.sh all` **PASSED**:
baseline/threaded/ASan+UBSan/Clang 20 TSan 10/10 each,
stress 900/900, foundation+installed consumer 13/13.

Portability:
- GCC 13.3 Static 11/11 PASS
- Clang 20.1.2 Static 11/11 PASS
- GCC and Clang Shared 10/11 each; both compile/link and pass all
  10 Monitoring tests, but the installed consumer cannot load
  `libprometheus-cpp-pull.so.1.3`.
- The previous third-party non-PIC static archive error is
  resolved by isolated pinned shared dependencies. This is
  now an **installed dependency loader** issue, not linker PIC.
- No optional clang-tidy or heaptrack outcome is in the log.

**New candidate since owner evidence:** Monitoring-only
`$ORIGIN` ELF install RUNPATH for Vex Monitoring and fetched
Prometheus `pull`; permits sibling shared dependencies in
the staged install without requiring a global library-path
override. **Not yet tested**.

## Minimal owner execution

```sh
git pull --ff-only origin feature/monitoring-correctness-tests
git rev-parse HEAD
git status --short
./tools/verify_monitoring_portability.sh gcc-shared
./tools/verify_monitoring_portability.sh clang-shared
wc -l build/monitoring-controlled.csv build/monitoring-load.csv
cat build/monitoring-controlled.csv
cat build/monitoring-load.csv
```

MON-OPEN-05 remains **PARTIAL** pending Shared external consumer
PASS. MON-OPEN-06 still needs the two numeric CSV files (benchmarks
built and ran previously). MON-OPEN-07 is the owner's code review
and explicit merge decision. Other modules frozen; no merge.

Evidence:
[MONITORING_PORTABILITY_RUNTIME_2026-11-10.md](MONITORING_PORTABILITY_RUNTIME_2026-11-10.md)
and [MONITORING_TASKS.md](MONITORING_TASKS.md).
