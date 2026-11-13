# Monitoring — post-fix static PASS and controlled managed/raw Registry lifetime comparison

**Provenance:** owner terminal transcript, checkout
`34587f820e6384ce06c220a8edc12860711bc8e0`, `feature/monitoring-correctness-tests`, Linux
6.8.0-147-generic; GCC 13.3.0 compiler at `/usr/bin/c++`
and Ubuntu LLVM `clang-tidy` 20.1.2. Runtime sample times:
2026-11-13T22:05:22Z (managed), 2026-11-13T22:05:27Z (raw).
Data is user-supplied, not executed inside the assistant environment.

## MON-OPEN-05 — targeted static-analysis gate PASS

`tools/verify_monitoring_static.sh` now ends with:
```text
MONITORING_STATIC_PASS checked=5 failed=0
```

The selected check set was exactly
`-*,clang-analyzer-*,bugprone-*,performance-*` with
warnings-as-errors; five source files checked:
`decorators.cpp`, `health_check.cpp`, `metrics.cpp`,
`metrics_manager.cpp`, `raii_guards.cpp`.
Five **locally justified NOLINTNEXTLINE** annotations for the
previously reported API/exception cleanup findings were present in
the tested source; the tool did not report an unsuppressed finding
under these checks. This is **not** an unrestricted clean analysis of
all third-party code or all Clang-Tidy checks; the
`warnings generated` counts include suppressed dependency warnings.
Raw terminal excerpt:
[monitoring-static-after-2026-11-13.log](benchmarks/2026-11-13-monitoring-static-after.log).

The previously accepted four GCC/Clang × Static/Shared package
modes each passed 11/11 with installed consumer. The focused static
analysis subgate is now also **PASS for the declared scope**.
No further source suppression or public API change is justified by
the new log.

## MON-OPEN-06 — 4096 Registry expirations, managed vs raw

Data:
- [managed raw CSV](benchmarks/2026-11-13-monitoring-lifetime-managed-new.csv)
- [direct Prometheus raw CSV](benchmarks/2026-11-13-monitoring-lifetime-raw.csv)

Both files have exactly **9 lines** (header plus 8 rounds), with
512 short-lived Registry expirations in each round and totals
512→4096. All rows have `VmHWM >= VmRSS`.

| Metric | Managed | Raw prometheus-cpp |
|---|---:|---:|
| Round 1 RSS | 4,760 KiB | 4,252 KiB |
| Round 8 RSS | 4,892 KiB | 4,444 KiB |
| RSS plateau, rounds 2–8 | 4,892 KiB | 4,444 KiB |
| Round 1 glibc live heap | 88,672 B | 81,136 B |
| Round 8 glibc live heap | 104,768 B | 81,136 B |
| Heap rise, rounds 1→8 | **16,096 B** | **0 B** |
| Median 512-Registry round time, 8 rounds | **1.765 ms** | **0.906 ms** |

Managed owner expiry/quota/fresh-HELP checks were successful;
raw mode intentionally bypassed managed quota checks. This difference
makes these non-identical CPU workloads, but still isolates a
meaningful direct Registry/allocator churn control. Neither RSS
plateau nor monotonically increasing `mallinfo2().uordblks` proves
or excludes a memory leak.

### Specific source-level hypothesis: periodic sweep / sample phase

Read the actual Vex source and the Benchmark:
- `MetricsManager::RegisterTracked` increments
  `tracked_registration_calls_` on *every* registration attempt
  and **sweeps expired weak Registry keys whenever the counter is a
  multiple of 256**, before inserting the current key.
- `MetricsManager::Init()` does not register a metric; the Lifetime
  runner registers **one persistent Counter** before sampling.
- Each managed 512-Registry round issues **515** registration
  attempts: 512 transient creations, 1 fresh-owner creation,
  1 expected quota-failing fresh-owner `CreateCounter`, and
  1 expected quota-failing persistent-owner `CreateCounter`.
  **Failed quota attempts also advance the sweep counter.**
- Hence the total registration attempt count at the end of round
  `r` is `1 + 515r`, namely 516, 1031, ..., 4121.
  The last 256-aligned sweep points are 512, 1024, ...,
  4096, meaning the sample advances **4, 7, 10, 13, 16,
  19, 22, 25** registration attempts beyond the last sweep.
  More expired weak-owner map entries can remain in
  `tracked_metric_series_` at each successive snapshot.
  This is consistent with +~2 KiB/round of managed allocator
  live bytes; the source is still insufficient to assign an
  exact byte count to each map entry.

**Diagnostic test WITHOUT any runtime code change:**
Run the existing managed lifetime benchmark with **509** transient
Registries per round. This gives exactly `509 + 3 = 512`
registration attempts per round, keeping sample phase fixed after
a sweep (one post-sweep attempt, given the initial persistent
registration). Expected `rounds × 509 = 4072` expirations
and nine CSV lines. If managed `uordblks` stops climbing after warmup,
the observations support a bounded sweep-phase artifact rather than
a monotonic unbounded leak. If growth persists, investigate other
retained allocations via a heap profiler or longer steady-state
analysis. A single 8-round aligned test is evidence, not a proof
of asymptotic absence of leaks.

```sh
./tools/benchmark_monitoring_lifetime.sh 8 509 managed \
  > build/monitoring-lifetime-managed-aligned.csv
wc -l build/monitoring-lifetime-managed-aligned.csv
cat build/monitoring-lifetime-managed-aligned.csv
```

Do not preemptively change the 256-registration sweep policy,
which was chosen to avoid linear O(N) cleanup on each registration
and whose performance was previously improved/verified. If
confirmed, update test methodology and document the bounded
delayed-reclamation behavior; if not, profile before altering
the ledger or weakening quota/lifetime safety.

## Disposition

- **MON-OPEN-05:** four-mode package + targeted clang-tidy gates
  **PASS**, within selected tool/configuration coverage.
- **MON-OPEN-06:** weak owner lifetime checks and controlled
  Vex-vs-raw baseline **PASS**; significant managed-only heap growth
  requires one fixed-sweep-phase experiment before classifying
  the mechanism. **Not closed**; no leak claim.
- **MON-OPEN-07:** Draft PR owner code review / merge decision
  remains open. No merge authorized; no unrelated Runtime changes.
