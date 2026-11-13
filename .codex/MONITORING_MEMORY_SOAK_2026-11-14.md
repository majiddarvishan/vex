# MON-OPEN-06: five-minute Monitoring memory soak (2026-11-14)

**Final status: MON-OPEN-06 five-minute acceptance PASS/CLOSED in the measured scope; both 300-second CSVs received and analyzed; no stable post-warmup growth observed.**

Branch: feature/monitoring-correctness-tests. Draft PR #3, stacked on feature/cmake-ctest-foundation. Previous HEAD 6d3a9c77a2e5c364fe85580e600903bfbc54ba45. No runtime/API/non-Monitoring modifications.

## Historical interim evidence (superseded by final result above)

GCC 13.3.0 Release Monitoring-only CMake/build **PASS**, shell syntax **PASS**, and 5-second Managed and Raw smoke invocations **PASS by zero exit and chained shell execution**. Both 300-second commands returned a unique `MONITORING_SOAK_OUTPUT` CSV/log filename; no error was reported. The full **Raw** 300-second CSV and logs for Raw and Managed were pasted, but the full **Managed CSV was not**. Therefore the managed memory trend and quota/phase validation remain **UNVERIFIED**, and the full five-minute MON-OPEN-06 subgate remains **OPEN**.

Raw source: `monitoring-soak-raw-sbHznA.csv`; run ID `monitoring-soak-raw-sbHznA`; 2026-11-13T23:00:30Z, Linux 6.8.0-147-generic, compiler /usr/bin/c++, Release; interval 10s, warmup 30s, rate target 2000 ops/s. The supplied CSV has **63 rows** (31 pre/post pairs + one after_reset), ends at **300.000 s** with **599,999 churn operations, 599,999 expired checks, 0 quota checks**, and average **1999.994 ops/s**. Every pre-sweep equivalent phase is **255**, every post is **0**; every sample satisfies VmHWM >= VmRSS. **Raw has no actual manager Sweep or quota check**; these phases are modeled solely to align sample positions.

Raw RSS: baseline pre/post **4240/4436 KiB**, reaching **4500 KiB by 10 s**, constant **4500 KiB across every post-warmup (30–300 s) and second-half (150–300 s) sample**. Raw glibc `mallinfo2().uordblks`: **80,928 B** at baseline, **81,504 B** at 10 s and constantly **81,504 B** at post-warmup and in the second half; after_reset **81,472 B**. Hence 30→300s and 150→300s post-phase deltas are **0 KiB RSS and 0 B glibc live heap**. These data show a plateau under this synthetic Raw workload, not proof of leak absence or attribution of Managed allocations. No profiling or runtime change is justified by Raw alone.

Managed run: `monitoring-soak-managed-enfJiz.csv`, log `monitoring-soak-managed-enfJiz.log`, started 2026-11-13T22:53:33Z, same Linux/compiler/settings; log includes **metadata only**, no numerical CSV samples. The runner printed the final unique file path; inspect the Managed CSV before judging its expiration, quota, actual elapsed, Sweep effect and memory slope.

**Next evidence:** provide the existing local `build/monitoring-soak-results/monitoring-soak-managed-enfJiz.csv` (not a rerun) plus the two CSVs if publishing raw artifacts to the repo is desired. Preserve separate run IDs. Then compare Managed post-warmup + second half slopes and phase pre/post deltas, update decision and only profile if sustained unexplained growth appears.

## Final verified 300-second result and technical decision (2026-11-14)

**MON-OPEN-06 — five-minute Monitoring memory soak: PASS within the defined synthetic workload and test duration; CLOSED for this scoped acceptance gate.** Both runs used source SHA `63d90c07ae78435f387ece30fac6a43d9f58a90f`, Linux `6.8.0-147-generic`, GCC 13.3.0 via `/usr/bin/c++`, Release, duration 300s, interval 10s, warmup 30s, target 2000 Registry churn ops/s. The owner's CMake Monitoring-only Release Build and `bash -n` succeeded; both 5s Managed/Raw Smoke commands completed; both independent 300s runs completed and both full CSVs were reviewed in the conversation.

| Check | Managed (`monitoring-soak-managed-enfJiz`) | Raw (`monitoring-soak-raw-sbHznA`) |
|---|---:|---:|
| Data rows excluding header | 63 | 63 |
| Final elapsed (post-sweep-equivalent) | 300.001 s | 300.000 s |
| Churn operations / expired weak-owner checks | 599999 / 599999 | 599999 / 599999 |
| Average churn operations per second | 1999.993 | 1999.994 |
| Quota rejection checks | 2374 | 0 (not supported in Raw) |
| Final tracked registration attempts equivalent | 606976 | 606976 (modeled) |
| Final manager Sweep count | 2371 (actual) | N/A (phase modeled) |
| Warmup-end 30s / final 300s post-phase RSS | 5016 / 5016 KiB | 4500 / 4500 KiB |
| Warmup-end / final post-phase glibc `uordblks` | 90352 / 90352 B | 81504 / 81504 B |
| 150s→300s post-phase RSS delta | 0 KiB | 0 KiB |
| 150s→300s post-phase heap delta | 0 B | 0 B |
| After `Reset` glibc `uordblks` | 88864 B | 81472 B |

**CSV validation:** 31 ordered pre/post sample pairs at 0, 10, ..., 300 s plus one after_reset row per mode. For each pair the churn/expiry counts are unchanged, and the phase advances exactly **255 → 0**. Every row has `peak_rss_kib >= rss_kib`. All managed `registration_attempts_equivalent % 256 == phase_mod_256`, and `modeled_sweeps == registration_attempts_equivalent / 256` (integer division). Source audit confirms that in Managed, the last registration at 0 phase actually executes the expiry sweep under the manager's lock. The Managed quota checks total **2374** = `floor(599999/256) = 2343` transient probes + **31** sample-time anchor probes. The benchmark terminates on steady-clock deadline (300.001 s observed), not an unbounded operation-count loop.

**Managed sweep effect and trend:** paired glibc heap at 30s was **168128 → 90352 B** (pre → post); at 150s **169008 → 90352 B**, and at 300s **169008 → 90352 B**. The final one-sweep reduction is **78656 B**. Across all 28 post-warmup pairs (30..300s), the paired pre/post reduction ranges **77776..78688 B** (median **78640 B**). Thus pre-sweep snapshots oscillate around ~168–169 KB, while every post-sweep heap snapshot between 30 and 300 seconds is exactly **90352 B**. RSS and peak RSS reach **5016 KiB** by 20s, then plateau; heap/retention does not exhibit a persistent second-half upward trend. `Reset` removes 1488 B additional reported live glibc heap, though RSS does not decrease; allocator caching and OS page retention are normal limitations of RSS.

**Raw control:** from 30 through 300 s, both phase-equivalent samples plateau at RSS **4500 KiB** and live glibc heap **81504 B**. In Raw, no manager quota or real 256-call Sweep runs: the pre/post positions are artificial alignment only. Managed minus Raw absolute RSS (~516 KiB) and post-phase heap (~8848 B) are *not* reliable Vex overhead estimates because their registration/validation paths differ, their allocations and persistent registry behavior differ, and the runs were separate processes. Comparison supports trend characterization only.

**Interpretation:** the dramatic pre/post Managed heap sawtooth, with perfectly flat aligned post-sweep post-warmup baseline, supports periodic bounded reclamation of expired weak-key ledger metadata (256 registration-attempt cadence). It also explains why unaligned short-run heap comparisons may misleadingly appear to grow. No persistent memory growth requiring `heaptrack` or speculative Runtime changes was detected in this 300s single-thread synthetic workload. **Do NOT claim absence of all memory leaks:** longer horizons, concurrency, other metric kinds, different allocators and production workloads remain outside this result.

**Technical decision:** Mark the MON-OPEN-06 requested **300s soak gate PASS/CLOSED**, leave Monitoring runtime and public API unchanged, no profiler installation, no extra test reruns and no PR merge or Draft/base changes. MON-OPEN-07 is outside scope. Original complete per-run CSVs remain on the owner's host under `build/monitoring-soak-results/`, with independent run IDs; their key validated results are preserved here. The earlier 'Managed CSV missing' status in the historical sections below is superseded.

---

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

## Earlier interim checklist (superseded)

- Build + managed/raw smoke: **PASS from owner command transcript**.
- Raw 300s: **CSV validated, PASS for measured duration/churn/expiry/phase/peak and post-warmup plateau**.
- Managed 300s: **PASS**; full numerical CSV validation documented in the final result above.
- Require final elapsed >=300s, expired_checks == churn_ops, managed quota_checks >0, phase 255 for pre and 0 for post, and VmHWM >= VmRSS.
- Compare post-sweep, post-warmup start/end, RSS and uordblks slopes in the second half of each independent run. Check pre/post deltas for the expected sweep-phase sawtooth.
- Raw control lacks manager quota validation and sweep, so not an identical cost workload. Compare trends cautiously and never absolute RSS as proof of attribution.
- Stable RSS or glibc heap alone does not prove freedom from leaks; allocator caching/fragmentation, peak history and retained weak-key bookkeeping all matter. If persistent unexplained growth exists, check for already-installed heaptrack and investigate before any targeted runtime change.

**Decision (superseded):** the soak gate was open before receiving the Managed CSV; the final decision is PASS/CLOSED above. MON-OPEN-07 and merge out of scope.
