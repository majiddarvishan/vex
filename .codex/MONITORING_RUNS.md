# Monitoring verification — owner-supplied 2026-11-10

**Source:** User-pasted terminal output from local workspace
`/home/darvishan/workspace/mine/vex`, branch
`feature/monitoring-correctness-tests`. Compiler shown in configure:
**GNU C++ 13.3.0**. The pasted output **does not include `git rev-parse HEAD`**;
the exact checkout commit SHA is not independently evidenced.
This is **owner-provided evidence**, not a remote execution by the assistant.

## Command results

| Command | Configure / Build | CTest | Outcome |
|---|---|---|---|
| `./tools/verify_monitoring.sh baseline` | PASS | **2/2** (smoke + contract) | PASS |
| `./tools/verify_monitoring.sh threaded` | PASS | **2/2** | PASS |
| `./tools/verify_monitoring.sh asan` | PASS | **2/2** | PASS; ASan + UBSan flags |
| `./tools/verify_monitoring.sh tsan` | PASS | **1/2** | **NOT PASS** |
| `./tools/verify_foundation.sh full` | PASS | **5/5** including external package consumer | PASS |

TSan detail:

- `vex.monitoring.smoke`: **FAILED**, exited at startup with
  `FATAL: ThreadSanitizer: unexpected memory mapping 0x62e22a6dc000-0x62e22a6e6000`.
- `vex.monitoring.contract`: **PASSED** in that same run (0.16 s).
- This particular diagnostic does **not show a detected data race**; it indicates a
  TSan runtime or memory-layout failure. Neither pure-environment attribution
  nor bug absence has been established. TSan coverage is **incomplete/blocked**.

## Investigation

GCC and sanitizer maintainers have previously documented
`ThreadSanitizer: unexpected memory mapping` for otherwise minimal programs
when kernel/PIE address-space layouts conflict with TSan runtime assumptions:
- GCC bug report: https://gcc.gnu.org/pipermail/gcc-bugs/2015-August/515597.html
- Google sanitizers issue: https://github.com/google/sanitizers/issues/503

These references justify an **independent runtime probe**, not bypassing TSan.

A safe diagnostic was added: `./tools/diagnose_tsan.sh 10`, which builds a
small data-race-free `std::thread` program with TSan and runs it repeatedly.
It preserves individual logs and exits nonzero if there is *any* error.
This diagnostic **does not modify ASLR, sysctl or kernel settings**.

## Remaining acceptance gates

1. Run `./tools/diagnose_tsan.sh 10` on the same host and provide summary.
2. Re-run TSan just for the failed smoke case and the contract case in a
   non-mutating environment, recording successes/failures and stderr:
   ```sh
   ctest --test-dir build/monitoring-tsan --output-on-failure -R '^vex[.]monitoring[.]smoke$'
   ctest --test-dir build/monitoring-tsan --output-on-failure -R '^vex[.]monitoring[.]contract$'
   ```
3. If even the independent probe fails with the same fatal message, treat
   this machine's TSan runtime coverage as **blocked**, and verify in a
   supported clean toolchain/environment before promoting Monitoring.
   Do **not** globally disable ASLR or relax host security for a workaround.
4. If the independent probe is stable but only Vex tests fail, investigate
   startup/linking/PIE and test-specific dependencies separately.
5. Complete separate adversarial lifetime/concurrency stress tests before
   claiming production-level Monitoring reliability.

**Owner-authorized scope remains Monitoring only.** Non-Monitoring behavioral
findings are recorded and deferred.
