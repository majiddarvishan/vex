# Vex Monitoring — sweep-aligned weak Registry lifetime check (2026-11-13)

**Verified owner run:** `./tools/benchmark_monitoring_lifetime.sh 8 509 managed`
at **2026-11-13T22:13:59Z**, branch
`feature/monitoring-correctness-tests`, exact local SHA
`eee490533c98fc66568d2915ff7bfa45761f9743`. Linux 6.8.0-147-generic; GCC 13.3 C++ Release
through `/usr/bin/c++`. Tool built and exited without visible error.
Raw source: [2026-11-13-monitoring-lifetime-managed-aligned-8x509.csv](benchmarks/2026-11-13-monitoring-lifetime-managed-aligned-8x509.csv).

The CSV contains **9 lines**, one header plus eight fully consistent
rounds, **509 short-lived Registry expirations per round** and
**4072 total**. The source validates weak ownership, fresh-owner
HELP/quota independence, persistent Registry/Counter value and
quota each round. The benchmark would have failed on a violated
assertion. All eight snapshots have `VmHWM >= VmRSS`.

## Observed memory trend

| Metric | Earlier managed, 8×512 | Aligned managed, 8×509 | Direct raw, 8×512 |
|---|---:|---:|---:|
| Round 1 glibc `uordblks` | 88,672 B | 88,432 B | 81,136 B |
| Round 8 glibc `uordblks` | 104,768 B | 89,968 B | 81,136 B |
| Growth from round 1 to 8 | **16,096 B** | **1,536 B** | **0 B** |
| RSS after round 2 | 4,892 KiB | 4,940 KiB | 4,444 KiB |
| Total checked Registry expirations | 4096 | 4072 | 4096 |

The `uordblks` growth between rounds 1 and 8 fell by
**90.5%** under the sweep-aligned workload.
In the aligned run it increased only **352 B from round 5 to round
8** (89,616 → 89,968 B); rounds 5–6 were equal. This is a
much flatter trend, **not a perfectly flat signal**.
Process RSS reached a plateau at 4,940 KiB from round 2 onward
and all eight peak readings were coherent.

The raw and managed runs are distinct processes and the raw path
intentionally skips Vex quota validation; the comparison supports
diagnosis but is not an exactly identical instruction workload.
Absolute RSS levels differ across runs, so focus on within-process
trends and avoid attributing all differences to Vex.

## Audited cause: reclamation policy and sample-phase drift

`MetricsManager::RegisterTracked` maintains weak-ownership-aware
`std::map` metadata. Every registration *attempt*, including a
quota-rejected attempt, increments `tracked_registration_calls_`.
On every 256th call, the manager checks the map and erases
expired weak Registry entries before inserting/looking up the
current key.

One persistent Registry is created first (one tracked registration).
In the lifetime test each round additionally makes:
- one registration for each of the `batch` ephemeral Registries;
- one new fresh Registry registration with different HELP;
- one expected failed fresh-owner quota attempt;
- one expected failed persistent-owner quota attempt.

Thus `batch=512` means **515** tracked attempts/round,
creating a three-call phase shift relative to the 256-call sweeps.
The `batch=509` variant means **512** attempts/round.
For round `r` in the aligned test, total attempts are
`1 + 512r`; its end sample is at the **same phase, one
attempt after a sweep**, in every round.

The measured ~90.5% reduction in apparent retained glibc heap
is **strong evidence** that the former monotonic 8-round
managed-only trend was dominated by variable numbers of
expired weak-key metadata records waiting for a periodic sweep.
The remaining 1.5 KiB rise could include allocator caching,
one or more retained entries, or unrelated effects; precise
allocation attribution is not available. This is a supported
mechanism, not a proof of global absence of leaks.

**Operational nuance:** reclamation occurs only during later
tracked registrations, not on a timer. After a burst of
registrations, expired weak-key bookkeeping can remain in memory
until a subsequent sweep or `MetricsManager::Reset()`.
It does **not hold a strong Registry reference**, so external
Registry lifetime is still correctly released.
For a workload that creates at most one new tracking key per
call, the number of newly expired entries since the most
recent sweep is bounded by the 255 post-sweep calls.
Unchanged live Registry metadata is not expired and therefore
remains by design. This describes the source algorithm, not a
measured fixed RSS bound.

## Task disposition

**MON-OPEN-06 weak-owner reclamation / controlled short-run memory
characterization: PASS within the documented workload.**
The initial observed growth is substantially explained by the
sample/sweep-phase artifact; no runtime policy change is
justified. Do not remove or reduce the amortized sweep
without a separate performance/correctness trade-off study.

**Long-duration memory assurance and exact allocation sites:
not established.** Optional `heaptrack` (only if present)
or substantially longer steady-state soak testing may still
be undertaken as research, particularly if a real
field symptom develops. No memory leak has been demonstrated.

**MON-OPEN-05** GCC/Clang Static/Shared package matrix and
focused LLVM20 clang-tidy **PASS**, including documented local
NOLINT exceptions. **MON-OPEN-07** owner review/merge remains
open. PR #3 stays Draft, unmerged, stacked on Foundation.
No Runtime, Logger, ObjectPool or ThreadPool code was
modified in this evidence-only follow-up.
