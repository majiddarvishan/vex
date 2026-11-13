# MON-OPEN-06: five-minute Monitoring memory soak (2026-11-14)

**Status: owner-run Build and both 5s Smoke PASS; Raw 300s CSV fully verified; Managed 300s command completed, but numeric CSV not supplied. Full MON-OPEN-06 300s acceptance OPEN.**

Branch: feature/monitoring-correctness-tests. Draft PR #3, stacked on feature/cmake-ctest-foundation. Previous HEAD 6d3a9c77a2e5c364fe85580e600903bfbc54ba45. No runtime/API/non-Monitoring modifications.

## Owner-run evidence received (2026-11-14; source 63d90c07)

GCC 13.3.0 Release Monitoring-only CMake/build **PASS**, shell syntax **PASS**, and 5-second Managed and Raw smoke invocations **PASS by zero exit and chained shell execution**. Both 300-second commands returned a unique `MONITORING_SOAK_OUTPUT` CSV/log filename; no error was reported. The full **Raw** 300-second CSV and logs for Raw and Managed were pasted, but the full **Managed CSV was not**. Therefore the managed memory trend and quota/phase validation remain **UNVERIFIED**, and the full five-minute MON-OPEN-06 subgate remains **OPEN**.

Raw source: `monitoring-soak-raw-sbHznA.csv`; run ID `monitoring-soak-raw-sbHznA`; 2026-11-13T23:00:30Z, Linux 6.8.0-147-generic, compiler /usr/bin/c++, Release; interval 10s, warmup 30s, rate target 2000 ops/s. The supplied CSV has **63 rows** (31 pre/post pairs + one after_reset), ends at **300.000 s** with **599,999 churn operations, 599,999 expired checks, 0 quota checks**, and average **1999.994 ops/s**. Every pre-sweep equivalent phase is **255**, every post is **0**; every sample satisfies VmHWM >= VmRSS. **Raw has no actual manager Sweep or quota check**; these phases are modeled solely to align sample positions.

Raw RSS: baseline pre/post **4240/4436 KiB**, reaching **4500 KiB by 10 s**, constant **4500 KiB across every post-warmup (30–300 s) and second-half (150–300 s) sample**. Raw glibc `mallinfo2().uordblks`: **80,928 B** at baseline, **81,504 B** at 10 s and constantly **81,504 B** at post-warmup and in the second half; after_reset **81,472 B**. Hence 30→300s and 150→300s post-phase deltas are **0 KiB RSS and 0 B glibc live heap**. These data show a plateau under this synthetic Raw workload, not proof of leak absence or attribution of Managed allocations. No profiling or runtime change is justified by Raw alone.

Managed run: `monitoring-soak-managed-enfJiz.csv`, log `monitoring-soak-managed-enfJiz.log`, started 2026-11-13T22:53:33Z, same Linux/compiler/settings; log includes **metadata only**, no numerical CSV samples. The runner printed the final unique file path; inspect the Managed CSV before judging its expiration, quota, actual elapsed, Sweep effect and memory slope.

**Next evidence:** provide the existing local `build/monitoring-soak-results/monitoring-soak-managed-enfJiz.csv` (not a rerun) plus the two CSVs if publishing raw artifacts to the repo is desired. Preserve separate run IDs. Then compare Managed post-warmup + second half slopes and phase pre/post deltas, update decision and only profile if sustained unexplained growth appears.

## Design and validity

Optional, one-thread, synthetic Registry create/destroy workloads with `steady_clock` deadline, 300s default, 2..3600s bounds and configurable sampling, warmup and 1..20000 ops/s rate cap (default 2000). After each transient Registry goes out of scope, its weak_ptr must expire. Managed mode enforces one-series quota, checking rejection on every 256th transient Registry and at samples. Raw mode bypasses Vex quota by design.

Each sample is paired: pad with duplicate persistent-series registration until the equivalent registration attempt counter is phase 255 modulo 256; capture **pre_sweep**; register once more, triggering the verified manager 256th-attempt Sweep; capture **post_sweep**. Failed managed quota attempts count toward the 256-cycle. Raw phase is a synthetic equivalent only, **not** a manager Sweep. Final sample is followed by Registry release, MetricsManager::Reset and an after_reset snapshot. Pre and post snapshots are NOT independent runs.

CSV metadata includes unique run ID, UTC start, Git SHA, compiler, kernel, build type, mode, configured duration/interval/warmup/rate, actual steady elapsed, operations and rate, expired checks, quota checks, registration attempts, padding calls, sweep phase, modeled sweeps, RSS, VmHWM and glibc mallinfo2 uordblks (or -1 if unavailable). One run produces a unique CSV plus log; incomplete runs never publish a final CSV. Build disables network dependency fetching. No production data, external service or network is used by the run.

## Commands on owner Linux checkout

    git pull --ff-only origin feature/monitoring-correctness-tests
    bash -n tools/benchmark_monitoring_soak.sh
    ./tools/benchmark_monitoring_soak.sh 5 managed 1 1 2000
    ./tools/benchmark_monitoring_soak.sh 5 raw 1 1 2000
    ./tools/benchmark_monitoring_soak.sh 300 managed 10 30 2000
    ./tools/benchmark_monitoring_soak.sh 300 raw 10 30 2000

Output directory: build/monitoring-soak-results. Each invocation must complete independently; provide both .csv and .log files. The isolated offline CMake build requires usable installed prometheus-cpp >=1.3 (core+pull) or existing local package configuration. If unavailable, mark **BLOCKED** without installing packages or enabling network fetching.

## Post-run acceptance & analysis (partially verified)

- Build + managed/raw smoke: **PASS from owner command transcript**.
- Raw 300s: **CSV validated, PASS for measured duration/churn/expiry/phase/peak and post-warmup plateau**.
- Managed 300s: **command completed and output produced; CSV numerical validation PENDING**.
- Require final elapsed >=300s, expired_checks == churn_ops, managed quota_checks >0, phase 255 for pre and 0 for post, and VmHWM >= VmRSS.
- Compare post-sweep, post-warmup start/end, RSS and uordblks slopes in the second half of each independent run. Check pre/post deltas for the expected sweep-phase sawtooth.
- Raw control lacks manager quota validation and sweep, so not an identical cost workload. Compare trends cautiously and never absolute RSS as proof of attribution.
- Stable RSS or glibc heap alone does not prove freedom from leaks; allocator caching/fragmentation, peak history and retained weak-key bookkeeping all matter. If persistent unexplained growth exists, check for already-installed heaptrack and investigate before any targeted runtime change.

**Decision:** MON-OPEN-06 time-duration soak gate stays OPEN pending owner-run evidence and report update. MON-OPEN-07 and merge out of scope.
