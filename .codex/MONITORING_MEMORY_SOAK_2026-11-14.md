# MON-OPEN-06: five-minute Monitoring memory soak (2026-11-14)

**Status: tool prepared; Build, Smoke and 300-second runs NOT VERIFIED.**

Branch: feature/monitoring-correctness-tests. Draft PR #3, stacked on feature/cmake-ctest-foundation. Previous HEAD 6d3a9c77a2e5c364fe85580e600903bfbc54ba45. No runtime/API/non-Monitoring modifications.

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

## Post-run acceptance & analysis (pending)

- Build, managed/raw smoke: **NOT RUN**.
- Managed/raw 300s: **NOT RUN**.
- Require final elapsed >=300s, expired_checks == churn_ops, managed quota_checks >0, phase 255 for pre and 0 for post, and VmHWM >= VmRSS.
- Compare post-sweep, post-warmup start/end, RSS and uordblks slopes in the second half of each independent run. Check pre/post deltas for the expected sweep-phase sawtooth.
- Raw control lacks manager quota validation and sweep, so not an identical cost workload. Compare trends cautiously and never absolute RSS as proof of attribution.
- Stable RSS or glibc heap alone does not prove freedom from leaks; allocator caching/fragmentation, peak history and retained weak-key bookkeeping all matter. If persistent unexplained growth exists, check for already-installed heaptrack and investigate before any targeted runtime change.

**Decision:** MON-OPEN-06 time-duration soak gate stays OPEN pending owner-run evidence and report update. MON-OPEN-07 and merge out of scope.
