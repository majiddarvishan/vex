## 2026-11-14 optional Monitoring memory/static probes pending

Historical owner-run tests **PASS**: 10/10 ×4 modes, stress 900/900,
foundation+consumer 13/13, four compiler/linkage package
configurations 11/11 each; raw/shared/sharded Counter load 60 records
with 60/60 consistent VmRSS/VmHWM.

**New tool-only changes, not yet run:**
`benchmark_monitoring_lifetime.sh 8 512` expects 9 CSV lines and
validates 4096 short-lived Registry expirations while sampling
RSS/heap trends; `verify_monitoring_static.sh` requires
preinstalled clang-tidy, emits explicit warning-as-error PASS/FAIL.
No new build, heaptrend, clang-tidy or leak-free result may be
asserted until measured. See
[MONITORING_FOLLOWUP_PLAN_2026-11-14.md](MONITORING_FOLLOWUP_PLAN_2026-11-14.md).

---

## New owner-controlled Counter comparison and peak RSS — 2026-11-13

Opt-in `benchmark_monitoring_load.sh 3 --controls` **BUILD/RUN PASS**,
CSV **61 lines / 60 records**. Compared Vex-managed/Prometheus-raw
shared versus per-thread sharded Counters; direct underlying
Counter::Increment path used for both. Strong shared Counter
contention in both libraries; Vex sharded 8 threads **223.97M**
vs shared **8.73M ops/s** (medians, N=3).
`peak_rss_kib >= rss_kib` **60/60** records after Linux VmHWM
change. Resource checks do not establish leak freedom or allocation
site counts. Concurrent Collect p95 **774.616µs**, HTTP p95
**187.720µs**. [Report](MONITORING_LOAD_CONTROLS_2026-11-13.md).

Previous GCC/Clang Static/Shared matrix **11/11 each PASS**,
full Monitoring tests 10/10 ×4, stress 900/900, foundation 13/13
remain accepted. Optional clang-tidy not run.

---

## New optional raw/sharded Counter load control — acceptance pending

The earlier owner-run load CSV (33 samples) is archived and fully analyzed. 32/33 `peak_rss_kib` rows were internally inconsistent. The benchmark source has since been adjusted to use Linux `VmHWM` and invalidate impossible peaks, and to add a raw/sharded Counter contention control behind `--controls`. **New code NOT YET compiled or run**. Original 34-line default format is unchanged; `--controls` emits 61 lines with 3 rounds. Test only the new tool; existing full sanitizer/stress suites already passed for the production Monitoring source.

---

## Current owner evidence — every GCC/Clang Static/Shared consumer PASS

GCC Static **11/11**, GCC Shared **11/11**, Clang Static **11/11**, Clang Shared **11/11** (including package consumer). Shared RUNPATH issue resolved in the tested staging layout. Previous full 10/10×4, 900/900, 13/13 remain PASS. Controlled CSV 181 lines; resource CSV 34 lines. Peak RSS inconsistent in 32/33 records; cannot use that field. Optional clang-tidy NOT RUN. Complete [report](MONITORING_PORTABILITY_LOAD_RESULTS_2026-11-10.md).

---

## Latest report — Full Monitoring PASS, shared package runtime still failing

Owner's `Pasted text(7).txt` confirms **10/10 each**
baseline/threaded/ASan+UBSan/Clang20 TSan; **900/900** stress;
**13/13** full foundation/package.
GCC Static and Clang Static: **11/11 PASS** each.
GCC Shared and Clang Shared: **10/11** each; ten Monitoring
cases pass, installed consumer cannot load `libprometheus-cpp-pull.so.1.3`.
The previous PIC link blocker is fixed. A Monitoring-only
`$ORIGIN` installed RUNPATH candidate is now committed, but
**NOT YET OWNER-RUN**. See
[MONITORING_PORTABILITY_RUNTIME_2026-11-10.md](MONITORING_PORTABILITY_RUNTIME_2026-11-10.md).


---

## Owner Portability partial (2026-11-10) and pending new macro/PIC rerun

`benchmark_monitoring_controlled.sh 15` and
`benchmark_monitoring_load.sh 3` both built and ran successfully,
but CSV values are missing from the uploaded terminal log.
`verify_monitoring_portability.sh all` reached GCC static
**11/11 PASS** (with 3 warnings), then stopped at GCC Shared
link failure from installed non-PIC prometheus-cpp archives.
Clang cases were **NOT RUN**.
C++17 macro fix, histogram reference warning fix, enforced pedantic
contract test, and shared isolated dependency strategy have been
committed **AFTER** those results. Re-run required.
[Detailed report](MONITORING_PORTABILITY_PARTIAL_2026-11-10.md).

---

## MON-OPEN-05/06 new optional tools — not yet owner-run (2026-11-10)

The last main acceptance **10/10 × four modes, stress 900/900 and
foundation 13/13 PASS** is historical for the previous Monitoring
implementation before optional benchmark and portability tooling.

New tools:
- `benchmark_monitoring_controlled.sh 15` → **expected 181 CSV lines**.
- `benchmark_monitoring_load.sh 3` → **expected 34 CSV lines**.
- `verify_monitoring_portability.sh all` → **expected four
  GCC/Clang × static/shared CMake/CTest/package-consumer builds**.
- `profile_monitoring_allocations.sh` optional only with
  preinstalled heaptrack.

No new source, memory/resource, portability or static analysis PASS
is claimed yet. See [MONITORING_PERFORMANCE_PLAN.md](MONITORING_PERFORMANCE_PLAN.md).

---

## Indexed-ledger follow-up: all automated tests PASS; after CSV pending (2026-11-10)

Owner uploaded full `verify_monitoring.sh all` output after the
weak-Registry-owner indexed-ledger change: baseline/threaded/ASan+UBSan/
Clang TSan **10/10 each**, stress initial **10/10** and 9 C++ tests
×100 = **900/900**, full foundation/package **13/13**.
Final `VEX MONITORING ACCEPTANCE PASS all`.

The Release benchmark binary also rebuilt and the command completed,
but its stdout was redirected to
`build/monitoring-profile-after.csv`, which was **not included**
in the owner-provided file. No measured after-change medians are
available yet. **MON-OPEN-01 remains open until before/after
performance analysis is possible**. Details:
[MONITORING_INDEXED_ACCEPTANCE_2026-11-10.md](MONITORING_INDEXED_ACCEPTANCE_2026-11-10.md).


---

## Performance-follow-up acceptance pending (2026-11-10)

The latest Monitoring source changes Vex tracked-series metadata
from vector to owner-aware ordered map, and adds a 320 Registry churn
regression. Previous full 10/10 per mode, 900/900 stress and
13/13 foundation PASS are for pre-change source.
**Required rerun:** `./tools/verify_monitoring.sh all`, then
`./tools/benchmark_monitoring.sh 3` on owner checkout. No
post-optimization measurement yet. Historical baseline CSV:
[benchmarks/2026-11-10-monitoring-managed-vs-raw.csv](benchmarks/2026-11-10-monitoring-managed-vs-raw.csv).

---

## Most recent full owner-run matrix — 2026-11-10

After MON-OPEN-02/03/04 changes: baseline **10/10**, threaded **10/10**, ASan/UBSan **10/10**, Clang 20 TSan **10/10**, stress **9 × 100 = 900/900**, full foundation + installed consumer **13/13**. Final `VEX MONITORING ACCEPTANCE PASS all`. No local HEAD in pasted log. See [MONITORING_ACCEPTANCE_2026-11-10_HTTP.md](MONITORING_ACCEPTANCE_2026-11-10_HTTP.md). Earlier 9/9 and 'pending' statements below are superseded for this version.

---

## Latest verified Monitoring acceptance — 2026-11-10

All stages passed on owner checkout **after** RAII ownership fix: baseline 9/9, threaded 9/9, ASan/UBSan 9/9, Clang TSan 9/9, stress 800/800, full installed consumer 12/12. Performance baseline 3 repeated runs per 10/100/1000 shape also completed. See [MONITORING_FINAL_REPORT_2026-11-10.md](MONITORING_FINAL_REPORT_2026-11-10.md). Remaining scenarios are tracked in [MONITORING_TASKS.md](MONITORING_TASKS.md).

---

## Verified Monitoring final acceptance (owner-run 2026-11-10)

**Full `verify_monitoring.sh all` PASSED on real checkout**:
baseline 9/9, threaded 9/9, ASan/UBSan 9/9,
Clang 20 TSan 9/9, stress initial 9/9 and 100 repeats of
each of 8 Monitoring C++ tests (800/800), foundation
package consumer/full CTest 12/12. This supersedes the earlier
expanded-suite "NOT RUN" status in historical notes below.
No sanitizer reports appeared in the supplied log.
Owner did not provide exact local commit SHA.

Optional cardinality/scrape benchmark still unmeasured;
no performance claims justified. PR remains Draft/unmerged.
Full evidence: [MONITORING_RUNS.md](MONITORING_RUNS.md).

---

## Expanded Monitoring acceptance gate (2026-11-10)

Current CTest: smoke, contract, decorators, raii, validation,
lifecycle, registration, legacy, compiler-selection.
Stress repeats the eight C++ tests 100 times each.
Run baseline/threaded/ASan/Clang TSan/stress and full installed
package consumer before acceptance. Current new tests NOT RUN.
Older 3/3 TSan + 6/6 foundation PASS were for earlier commits.
Optional benchmark is not executed; no numbers are asserted.

---

## Latest owner-run regression, post-warning-fix (2026-11-10)

- **Clang 20 TSan**: Monitoring build PASS; smoke, contract, compiler
  selection **3/3 CTest PASS**; no TSan data-race reports observed.
  The previously reported SAFE_CREATE_GAUGE warning is absent, with
  `-Werror=unused-value` enabled for the contract test.
- **Full foundation**: CMake/build/CTest **6/6 PASS**, including external
  installed package consumer and all existing module smoke tests.
- Earlier GCC baseline/threaded/ASan/UBSan/stress successes remain valid
  for their exercised test suites. This completes the existing explicit
  warning-fix regression gate, **not** the whole Monitoring audit.
- Exact local SHA not provided. Source: [MONITORING_RUNS.md](MONITORING_RUNS.md).

**Latest Monitoring TSan update (2026-11-10):** On Clang 20.1.2,
the owner's independent TSan probe passed **10/10** both with automatic
discovery and explicit `CXX=clang++-20`. The Monitoring TSan build
and CTest completed **3/3 PASS** (2 C++ tests + compiler discovery);
no TSan races were reported in this exercised test coverage. Previous
GCC 13.3.0 TSan remains unstable (independent repro). One Clang
`-Wunused-value` warning in the SAFE_CREATE_GAUGE macro was
subsequently corrected in all three SAFE_* macros and guarded by
`-Werror=unused-value`; **the fix is not yet owner-run**. See
[MONITORING_RUNS.md](MONITORING_RUNS.md). Further lifetime and
adversarial race coverage is open.

**Owner-run Monitoring Stress (2026-11-10):** Initial smoke/contract
2/2 PASS and **100 repetitions of each** test passed on GNU 13.3.0,
total repeat time 7.01s. New compiler-discovery CTest and `--clang` /
`tsan-clang` support are pending execution. The owner's Clang executable
is `clang++-20`, not `clang++`. GCC TSan remains blocked due to
reproducible instability in an independent program; stress PASS is
not a TSan substitute. See [MONITORING_RUNS.md](MONITORING_RUNS.md).

**Latest owner TSan diagnosis, 2026-11-10:** 10 independent
standalone GCC TSan trials: 3 PASS, 6 fatal unexpected memory mapping,
1 SegFault; a single Monitoring smoke TSan rerun also SEGFAULT (0/1).
No actual code race report was supplied, but this host cannot certify
TSan correctness. New 100-run repeats of smoke and contract (non-TSan) is prepared;
separate compatible TSan runtime still required. See
[MONITORING_RUNS.md](MONITORING_RUNS.md).

# Build, tests, sanitizers and performance acceptance matrix

**Monitoring owner-run checkpoint (2026-11-10):** baseline **2/2 PASS**, threaded **2/2 PASS**, ASan/UBSan **2/2 PASS**, full CMake/CTest + package consumer **5/5 PASS**. **TSan NOT PASS**: monitoring smoke failed with `unexpected memory mapping` while contract passed; see [MONITORING_RUNS.md](MONITORING_RUNS.md). This is a runtime-mapping fatal, not a logged race finding, and still blocks a clean TSan verdict. Do not conflate this newer checkpoint with the earlier historical status below.

**New verified owner-run baseline, 2026-11-10:** On the actual checkout the `thread` verification script passed **2/2** and `full` passed **4/4** CTest tests with GCC 13.3.0 (including external installed-package consumer in both runs). See [FOUNDATION_RUNS.md](FOUNDATION_RUNS.md). Legacy text below describes earlier unverified status and should be read as **historical** for those specific baseline runs; other matrix combinations remain unverified.

**Status update (2026-11-10):** The initial audit had no execution. Subsequently, a **manually reconstructed ThreadPool-only subset** (not the full GitHub checkout) passed configure/build, 2/2 CTest tests including installed consumer, on GCC 14.2.0 / CMake 3.31.6 / Ninja 1.12.1. An all-OFF configuration also succeeded (zero tests expected). **Full source-tree B01–B07 and T01–T09 remain NOT RUN/UNVERIFIED**; Prometheus-dependent builds, sanitizers and comprehensive tests are pending. Preserve this distinction.

## Build and install
| ID | Configuration/command | Required assertion |
|---|---|---|
| B01 | `cmake -S . -B build/min -DBUILD_EXAMPLES=OFF -DBUILD_MONITORING=OFF -DBUILD_OBJECT_POOL=OFF -DBUILD_THREADPOOL=ON` | Configures/builds without network or Prometheus; thread target visible |
| B02 | `cmake -S . -B build/pool -DBUILD_EXAMPLES=OFF -DBUILD_MONITORING=OFF -DBUILD_OBJECT_POOL=ON` | Object pool stand-alone consumer compiles with correctly declared optional dependencies |
| B03 | `cmake -S . -B build/monitor -DBUILD_EXAMPLES=OFF -DBUILD_MONITORING=ON` | Monitoring target exists and builds with explicit dependency resolution |
| B04 | `cmake -S . -B build/examples -DBUILD_EXAMPLES=ON` | Every enabled example builds; examples don't force unrelated modules |
| B05 | `cmake --install build/monitor --prefix /tmp/vex-install` | No source-tree path leakage; downstream `find_package(Vex CONFIG REQUIRED)` works |
| B06 | configure each of static/shared and GCC/Clang on Linux, plus supported macOS/Windows where claimed | Headers, package config, symbol/link interface tested |
| B07 | `cmake -S . -B build/test -DBUILD_TESTING=ON` then `ctest --test-dir build/test -N` | Real tests appear; no missing files or obsolete targets |

## Correctness tests (add when fixing)
| ID | Component | Cases |
|---|---|---|
| T01 | Logger queue | 32 concurrent producers / 1 consumer, same shard; unique IDs; verify exactly-once data on accepted records |
| T02 | Logger saturation | Block, Reject, DropNewest, DropOldest policies; order and dropped counts; bounded wait |
| T03 | Logger boundaries | 0 shards, 0/1 capacity, multiple instances with different shard counts; no UB |
| T04 | Logger I/O | No permission, disk-full injection, cross-device rotation, restart/incomplete recovery; faithful acknowledgment |
| T05 | ObjectPool | reuse/constructor throws, destructor counters, multiple types, shrink cap, pool destruction before object |
| T06 | ObjectPool metrics | registry replacement, destroyed registry, registration failure, thread churn; bounded cardinality |
| T07 | ThreadPool | 0 workers, worker startup fails, task throws, concurrent enqueue/shutdown, no lost accepted tasks |
| T08 | Monitoring | register/reset/reregister, concurrent uptime/labels/registry, HealthCheck lifetime and moves |
| T09 | Packaging | clean external consuming CMake project for each installed module, no repository source include |

## Dynamic tools (independent binaries)
- **ASan + UBSan:** `-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1`. T05/T06/T08/T04; record exit code and stack trace on failure.
- **TSan:** `-fsanitize=thread -fno-omit-frame-pointer -g -O1`. T01/T02/T07/T08; expect actual test assertions, not only “no TSan warnings”.
- **Static:** clang-tidy and compiler warnings `-Wall -Wextra -Wpedantic` on all first-party targets; clang-format check without edits.
- **Stress:** repeat ≥100 runs with seed recorded; random shutdown while producers run; check max resident memory, no hangs.

## Performance after correctness gate
| Benchmark | Metrics |
|---|---|
| Logger 1/8/32 producers; 1/4/16 shards; 64B/1KB payload | accepted/written/durable throughput; p50/p95/p99 enqueue latency; drop count; CPU, allocations |
| ThreadPool 1/4/32 workers, small/heavy tasks | throughput, queue latency, shutdown latency and RSS |
| ObjectPool 1/8/32 threads vs new/delete baseline | allocation/reuse count, p99 acquire/release latency, RSS, contention |
| Monitoring 10/100/1000 metric series | registration/update/scrape cost, memory and series count |

No numerical performance target is assumed; record environment and establish baseline before optimization.
