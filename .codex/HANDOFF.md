# Vex Monitoring Handoff — indexed ledger candidate pending (2026-11-10)

Branch: `feature/monitoring-correctness-tests`; Draft PR #3 stacked on
foundation. Only Monitoring runtime/tests/docs may change.

**Most recent owner-provided results (pre-index optimization):**
baseline 10/10, threaded 10/10, ASan/UBSan 10/10,
Clang 20 TSan 10/10, stress 900/900, full 13/13.
Paired Release benchmark completed 36 CSV rows; median at 1000
`many_series` Vex 3.070ms vs raw 0.930ms, and
`many_families` Vex 17.151ms vs raw 9.568ms.
Raw CSV:
[benchmarks/2026-11-10-monitoring-managed-vs-raw.csv](benchmarks/2026-11-10-monitoring-managed-vs-raw.csv).

**New candidate since that owner run:** replace the per-registration
linear scan of Vex's tracked-family vector with a weak-Registry-owner
ordered map, and sweep expired entries every 256 registrations.
320-registry churn/owner/quota test added. **These changes have not
been compiled or run in the owner's checkout.** An isolated C++17
comparator/map syntax test passed under GCC 14.2.0, only.

Next commands:
```sh
git pull --ff-only origin feature/monitoring-correctness-tests
git rev-parse HEAD
git status --short
./tools/verify_monitoring.sh all
./tools/benchmark_monitoring.sh 3 > build/monitoring-profile-after.csv
```
When logs arrive, investigate any regression and compare measured
before/after medians. No performance improvement has yet been
verified. Then continue MON-OPEN-05/06 (compiler/static/resource
matrix); MON-OPEN-07 is owner's explicit merge/release decision.

Detailed report: [MONITORING_PROFILE_2026-11-10.md](MONITORING_PROFILE_2026-11-10.md).
Task board: [MONITORING_TASKS.md](MONITORING_TASKS.md).
The PR remains Draft/unmerged. Do not alter Logger, ObjectPool,
ThreadPool runtime behavior.
