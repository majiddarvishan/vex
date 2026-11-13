# Monitoring portability — Static PASS, Shared runtime loader failure

**Owner evidence:** terminal transcript `Pasted text(7).txt`, from
`feature/monitoring-correctness-tests`, dated 2026-11-10;
no exact local `git rev-parse HEAD` was included. The owner's run
preceded commit `11aeffdce076efb023ce4d060e0bcd387b7cf238` and the subsequent documentation commit.

## Full Monitoring acceptance: PASS

`./tools/verify_monitoring.sh all` completed successfully:

| Stage | Result |
|---|---|
| baseline | 10/10 PASS |
| threaded | 10/10 PASS |
| ASan/UBSan | 10/10 PASS |
| clang++-20 TSan | 10/10 PASS |
| stress initial | 10/10 PASS |
| stress repetition | 9 distinct C++ tests × 100 = **900/900 PASS** |
| full foundation/package | **13/13 PASS** |

Terminal ended with `VEX MONITORING ACCEPTANCE PASS all`.
No build failure or sanitizer finding was reported in this portion.

## Four-mode portability: 2 PASS, 2 FAIL

| Mode | Build | Monitoring test cases | Package consumer | Verdict |
|---|---|---:|---|---|
| GCC 13.3 Static | PASS | 10/10 | PASS | 11/11 PASS |
| GCC 13.3 Shared | PASS | 10/10 | FAIL: loader cannot locate `libprometheus-cpp-pull.so.1.3` | 10/11 FAIL |
| Clang 20.1.2 Static | PASS | 10/10 | PASS | 11/11 PASS |
| Clang 20.1.2 Shared | PASS | 10/10 | FAIL: same loader error | 10/11 FAIL |

`PORTABILITY SUMMARY passed=2 failed=2`; the updated
portability runner correctly returned a nonzero result.

The previous non-PIC linking blocker was resolved by the
isolated, pinned prometheus-cpp shared build. This time both
`libprometheus-cpp-{core,pull}.so` and
`libvex_monitoring.so` built and linked.

**Root cause at observed failure:** the external installed-package
consumer could not locate the Prometheus pull shared library at
process startup. It was not a failed Monitoring test and not an
incompatibility during compilation. The shared deployment's
DT_RUNPATH/runtime loader search required repair. Both Shared
cases reached the same installation/loader failure.

The CMake transcript also includes a `CPM: Cache ... is dirty`
warning and several `CivetServer.cpp` warnings from the
third-party prometheus-cpp dependency under Clang. These are
not Vex test failures, but should be tracked separately from a
clean-warning claim.

## Follow-up change (not yet owner-tested)

A Monitoring-only CMake update sets installed ELF `RUNPATH`
to `$ORIGIN` for `libvex_monitoring.so`. When prometheus-cpp is
built locally as a CMake subproject, it also sets `$ORIGIN`
on the upstream `pull` target so it can locate `core`
beside it. The paths are relative to each shared object,
so no `LD_LIBRARY_PATH`, absolute build paths, system
loader configuration or `/usr/local` edits are needed
when the dependencies are deployed into the same library
directory. The upstream pinned project's install rules
install the shared dependencies in that staged prefix.

**This is a candidate fix, not a verified resolution.** No
fresh installed consumer run has yet passed after the RUNPATH
change; keep `MON-OPEN-05` open.

## Minimal next commands (avoid repeating heavy stress)

```sh
git pull --ff-only origin feature/monitoring-correctness-tests
git rev-parse HEAD
git status --short

./tools/verify_monitoring_portability.sh gcc-shared
./tools/verify_monitoring_portability.sh clang-shared

wc -l build/monitoring-controlled.csv build/monitoring-load.csv
cat build/monitoring-controlled.csv
cat build/monitoring-load.csv
```

Only the two Shared modes need new runs for the targeted RUNPATH
change. Earlier GCC/Clang Static 11/11 checks and full
10/10 ×4 + 900/900 + 13/13 remain valid for the preceding
macro fix. No Monitoring C++ runtime semantics changed with
the RUNPATH follow-up.

For diagnosis if still failing:
```sh
readelf -d build/monitoring-portability-gcc-shared/_package_smoke/install/lib/libvex_monitoring.so | grep -E 'RUNPATH|RPATH'
```
The actual install libdir may be `lib64` or architecture-specific;
locate the installed library in the existing `_package_smoke/install`
folder rather than assuming it is `lib`. Keep a failed output
for investigation, and do not mark a configuration PASS prematurely.

## MON-OPEN-06 remains pending numeric CSVs

The earlier controlled and load benchmark executables built and
ran, but stdout was redirected to files on the owner's machine.
Neither numeric CSV is in this transcript. Expect 181 lines
(controlled, 15 repeats) and 34 lines (load, 3 repeats).
Without those files there is no grounded resource,
concurrency scaling or registration variability result.

**PR #3 stays Draft/unmerged.** Other modules' runtime
behavior remains outside the approved Monitoring scope.

## New evidence supersedes prior Shared failure

Owner reran GCC Shared and Clang Shared after the `$ORIGIN` RUNPATH fix: **11/11 PASS in each**, including the external installed package consumer. Previously GCC/Clang Static both passed 11/11. Dirty CPM cache warning persists but does not block these CTests. See [complete evidence and resource results](MONITORING_PORTABILITY_LOAD_RESULTS_2026-11-10.md). Prior Shared failure descriptions above are historical only.
