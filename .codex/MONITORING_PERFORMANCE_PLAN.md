# MON-OPEN-05 / MON-OPEN-06: portability and resource benchmarks

**Date:** 2026-11-10  
**Scope:** `majiddarvishan/vex` → `feature/monitoring-correctness-tests`  
**PR:** https://github.com/majiddarvishan/vex/pull/3 (Draft, stacked on PR #2)

## Status

The benchmark and verification **tools have been implemented and pushed**
after the last owner-run full Monitoring acceptance. They have **NOT
been compiled/run on the owner's current checkout**. No new
benchmark/resource numbers, clang-tidy findings, static/shared PASS,
nor absence of regressions is claimed.

Previous owner-validated baseline was 10/10 × four test modes,
900/900 repeat executions, full foundation/installed consumer 13/13,
and a measured **34.9% lower median registration latency for 1000
independent Families** from the indexed-ledger change. Historic
evidence: [MONITORING_PROFILE_BEFORE_AFTER_2026-11-10.md](MONITORING_PROFILE_BEFORE_AFTER_2026-11-10.md).

## MON-OPEN-06: multi-thread/resource benchmark

```sh
./tools/benchmark_monitoring_load.sh 3 > build/monitoring-load.csv
```

Target: Linux, C++17, Release, opt-in, isolated CMake directory
`build/monitoring-load-release`; CPU-intensive synthetic workload,
no external infrastructure. HTTP uses `127.0.0.1:0` and an
OS-assigned port. The runner prints build information/toolchain
context to **stderr**; CSV to **stdout**.

CSV schema:
```text
phase,implementation,threads,series,operations,elapsed_ms,ops_per_second,p50_us,p95_us,p99_us,rss_kib,rss_delta_kib,peak_rss_kib,heap_live_bytes
```

With default repeats = 3 the expected output has **34 lines**:
1 header + **33 records**, from 3 rounds ×:
- 6 registration workloads: managed and raw 1000 labeled-series
  creation at 1, 4, 8 threads; the worker-start barrier excludes
  thread creation from the wall-clock registration measurement
  (thread join still contributes).
- 3 shared-Counter workloads: 25,000 increments per thread at 1,
  4, 8 threads; observed total is checked against Counter::Value.
- 1 Registry::Collect workload: 100 collection operations while
  another worker updates 1000 Counter series; p50/p95/p99 reported
  in microseconds.
- 1 HTTP /metrics workload: 50 real loopback scrapes of 100 series,
  preceded by a warm-up request; p50/p95/p99 in microseconds.

**Memory interpretation:** `rss_kib` is the process' VmRSS when
available; `rss_delta_kib` is a before/after process snapshot,
not necessarily retained metric memory. `peak_rss_kib` is the
process-wide high-water mark (cumulative across phases).
`heap_live_bytes` uses glibc mallinfo2 when available,
otherwise **-1** (unavailable). It is NOT an allocation count,
and there is no claim of per-object allocations. p50/p95/p99
are **-1** for phases without per-operation samples.
Concurrent HTTP scrapes are not measured by this workload
(the loopback scrapes are sequential); contention specifically
covers registration, shared-Counter updates and Collect
alongside Counter updates.

Optional allocation profiler, only with a *preinstalled* heaptrack:
```sh
./tools/profile_monitoring_allocations.sh
```
No installation is performed. This writes a heaptrack capture
under `build/monitoring-heaptrack`, plus an instrumented CSV;
**never compare its timing data with uninstrumented Release results**.
Use the heaptrack report to inspect allocation sites and peak
allocator behavior, not as a universal memory SLA.

## MON-OPEN-06: 1000-series result stability

```sh
./tools/benchmark_monitoring_controlled.sh 15 > build/monitoring-controlled.csv
```

The historical paired benchmark remains unchanged for 1/3/5 runs.
The new `--controlled` mode adds separate warm-up workloads and
deterministically rotates the ordering of both sizes and managed/raw
shapes. Default 15 controlled runs emit **181 lines**:
header plus 15 × 3 sizes × 4 shapes = **180 samples**.
Supported repetition counts are 9, 15, 21.

This provides more samples of the current implementation and reduces
fixed-order effects, but **does not by itself establish causality
for a before/after regression**, since the historic three-run
baseline used a different ordering and warm-up procedure.
To compare two revisions decisively, use the same harness,
host and toolchain at both revisions; report ranges, medians,
and noise. No p95/p99 production SLA is implied.

## MON-OPEN-05: portability and static analysis

```sh
./tools/verify_monitoring_portability.sh all
```

Runs a monitoring-only, isolated CMake build+CTest+installed
package-consumer matrix (distinct directories under `build/`):
1. GCC static;
2. GCC shared;
3. Clang static;
4. Clang shared.

Each uses `RelWithDebInfo`, C++17, warnings
`-Wall -Wextra -Wpedantic -Werror=return-type -Werror=unused-value`.
Compilers and build directories are reported. No merge, source
formatting, system changes or automatic static fixes are performed.
On Linux, expect Monitoring CTest cases plus the installed consumer;
**do not report PASS until actual output is supplied**.

Optional targeted `clang-tidy` for `src/monitoring/*.cpp`
(no auto-fixes; requires an available compatible binary):
```sh
VEX_RUN_CLANG_TIDY=ON VEX_CLANG_TIDY=/path/to/clang-tidy-20 \
  ./tools/verify_monitoring_portability.sh clang-static
```
There is no requirement to install clang-tidy to run the build
matrix. Tool absence should be reported as **not run**, not PASS.

## Acceptance criteria and boundaries

- Record actual machine/compiler identity and all command exit codes.
- Investigate build/CTest/sanitizer failures before closing tasks;
  re-run `./tools/verify_monitoring.sh all` after any code fix.
- Evaluate 1/4/8 thread throughput, potential lock serialization,
  RSS and per-scrape percentiles. Report unavailable fields honestly.
- Repeat single-family 1000-series registration sufficiently to
  distinguish fixed-order/warm-up effects from a stable regression.
- Record clang-tidy warnings when optional tool exists; confirm
  static/shared installed consumer behavior for supported targets.
- **Do not close MON-OPEN-05 or MON-OPEN-06 without the actual
  owner-run evidence.** The scripts are implementation candidates.
- Logger, ObjectPool, ThreadPool behavior remains explicitly frozen;
  owner decides PR merge/release (MON-OPEN-07).

## Owner partial results and PIC remediation, 2026-11-10

The controlled and load benchmark binaries **built and executed**,
but their CSV output was redirected and not attached. GCC static
portability **11/11 PASS** with three Monitoring test warnings;
GCC shared **FAILED LINK** against existing non-PIC installed
prometheus-cpp static archives. Clang static/shared were not reached.
The portability tool was changed to use an isolated pinned PIC-capable
dependency for Shared and to aggregate results, and C++17 macro/test
warnings were addressed. **Changes NOT YET owner-run.** Full record:
[MONITORING_PORTABILITY_PARTIAL_2026-11-10.md](MONITORING_PORTABILITY_PARTIAL_2026-11-10.md).

## Shared installed-package consumer failure and candidate RUNPATH fix

Owner re-ran the full matrix. Both GCC 13.3 and Clang 20.1
Static builds **11/11 PASS**. Both Shared builds produced
`libvex_monitoring.so` and passed **10/10 Monitoring tests**,
but the installed external consumer failed because
`libprometheus-cpp-pull.so.1.3` could not be located by the ELF
runtime loader. The earlier non-PIC linker problem no longer occurs.

A Monitoring-only `$ORIGIN` installed RUNPATH and a
fetched-Prometheus `pull` sibling RUNPATH were committed
**after** this report. These paths are relative and do not
require environment-variable workarounds. **Unverified
candidate** pending the GCC/Clang Shared package consumer rerun.
Details:
[MONITORING_PORTABILITY_RUNTIME_2026-11-10.md](MONITORING_PORTABILITY_RUNTIME_2026-11-10.md).

## New full owner evidence — 2026-11-10

Compiler/linkage and installed consumer matrix: **4/4 PASS, 11/11 CTests each**. Benchmarks: 180 controlled samples and 33 load samples, raw data archived. Counter contention observed, 1000-series regression remains uncertain without same-harness pre/post A/B; `peak_rss_kib` is invalid in 32/33 rows. Optional clang-tidy/heaptrack not run. See [analysis](MONITORING_PORTABILITY_LOAD_RESULTS_2026-11-10.md) for actual observations and follow-ups; historical “pending” language above is superseded where evidence is now available.
