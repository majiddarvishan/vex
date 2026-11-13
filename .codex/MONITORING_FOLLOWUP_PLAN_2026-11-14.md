# Vex Monitoring — focused remaining P2 evidence plan (2026-11-14)

Scope: branch `feature/monitoring-correctness-tests` only; stacked Draft PR #3.
No changes to Monitoring production runtime or unrelated modules in this follow-up.

## 1. Optional Registry churn / memory-trend probe

```sh
./tools/benchmark_monitoring_lifetime.sh 8 512 > build/monitoring-lifetime.csv
```

**What it tests:** eight rounds of 512 short-lived external
`prometheus::Registry` owners (total **4096**), each registering
and incrementing one managed Counter. Each owner is destroyed
before starting the next; its `weak_ptr` must report expired.
A separate persistent Registry survives the entire run and must
keep its Counter value and per-Family quota. A new short-lived
Registry with a different HELP string at the same metric name is
also tested each round to reject cross-owner ledger contamination.
All this exercises lazy cleanup of weak Registry keys on many
registration calls.

**CSV:** one header + eight data rows = **9 lines**. Per round:
`round,registries_per_round,expired_registry_checks,elapsed_ms,
rss_kib,rss_delta_kib,peak_rss_kib,heap_live_bytes,heap_delta_bytes`.
The runner writes host/kernel/Git HEAD and compiler context to
stderr so the source revision can be associated with the report.

**Acceptance:** benchmark compiles/links, emits all expected rows,
completed-check count reaches 4096, no ownership/quota/HELP assertion
fails, and where available `VmHWM >= VmRSS`. Record the memory
**trend** across rounds and flag unexplained sustained growth.
Do not invent a fixed leak threshold without a known allocator/
host baseline; glibc caching and process high-water behavior can
cause RSS growth without a leak. This benchmark cannot establish
absence of a leak or count allocations.

This is independent of the prior 60-record Counter contention/RSS
benchmark; do not merge their CSV rows or report the same sample
counts. Default batch/round counts are bounded (1–20 rounds,
256–2048 registrations per round). No external HTTP port is opened.

## 2. Optional focused clang-tidy static check

```sh
./tools/verify_monitoring_static.sh check-tools
./tools/verify_monitoring_static.sh > build/monitoring-static.log 2>&1
```

`check-tools` detects a present compatible clang-tidy binary,
preferring `clang-tidy-20` and falling back to available versions.
A missing binary is reported as **NOT RUN, exit 2**; it never
installs anything. The full script uses the Clang Static
compilation database, preparing it through the package matrix
only if it is missing; then runs `clang-analyzer-*`,
`bugprone-*` and `performance-*` over Vex
`src/monitoring/*.cpp` with warnings-as-errors and header
filter limited to `include/vex/monitoring/`. All errors are
preserved; no source rewriting or automatic suppression.

**Acceptance:** explicit `MONITORING_STATIC_PASS checked=N failed=0`
with the log and tool/compiler version. Warnings counted as errors
prevent mistakenly green status for an unreviewed finding.
Third-party package warnings and static analysis findings
should be distinguished. If the tool is absent, mark the
optional static-analysis subgate NOT RUN, not PASS.

## 3. Remaining scope and exit criteria

Already proven: all 4 GCC/Clang Static/Shared package modes **11/11**,
full Monitoring 10/10 ×4, 900/900 stress, 13/13 foundation,
Counter controls 60/60 and consistent RSS peak. Do not rerun
heavy sanitizer/stress solely because optional benchmark or
documentation changes were added.

If a new fixture detects a real correctness defect, fix **only
Monitoring** code, add a targeted regression and rerun affected
acceptance modes. Research gaps not affecting correctness can remain
explicit follow-ups. PR #3 remains **Draft/unmerged** until the
owner authorizes MON-OPEN-07.
