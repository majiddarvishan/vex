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

## Independent TSan investigation, owner-run 2026-11-10

Environment: c++ Ubuntu 13.3.0-6ubuntu2~24.04.1; Linux 6.8.0-147-generic.
The independent program used std::thread and std::atomic, with TSan
instrumentation, and **does not import Vex or prometheus-cpp**.

Result of 10 launches:
- PASS: **3/10** (runs 3, 6, 8).
- RUNTIME_MAPPING: **6/10** (runs 1, 4, 5, 7, 9, 10), each exit 66 and
  ThreadSanitizer: unexpected memory mapping.
- OTHER_FAILURE: **1/10** (run 2), exit 139, shell reports Segmentation
  fault (core dumped). Full contents of probe-2.log were not provided.
- Explicit TSan data-race reports: **0/10**.

The single Monitoring TSan smoke rerun also **FAILED** with CTest
SEGFAULT, 0/1 tests, without a stack trace. Therefore this host's
GCC TSan runtime is unreliable **even without Vex**, but the origin of
the Monitoring segfault is not proven. TSan is **BLOCKED/NOT PASS**.
This does not certify Monitoring as race-free.

Safe next gates:
1. Run ./tools/verify_monitoring.sh stress (100 fail-fast repetitions each of the Monitoring smoke and
   concurrent contract tests; not a replacement for TSan).
2. If clang++ is already installed, use an isolated clang runtime probe:
   CXX=clang++ VEX_TSAN_DIAG_DIR="$PWD/build/tsan-probe-clang" ./tools/diagnose_tsan.sh 10
3. Only after that independent Clang probe passes reliably, verify
   Monitoring in a *fresh* CMake directory (compiler cache is sticky):
   CXX=clang++ VEX_BUILD_DIR="$PWD/build/monitoring-tsan-clang" ./tools/verify_monitoring.sh tsan
4. Otherwise run TSan on a compatible independent CI runner.
   Do not disable ASLR or change kernel/system-wide sysctl settings.
   
LLVM TSan limitations: https://clang.llvm.org/docs/ThreadSanitizer.html
Ubuntu ASLR security context:
https://documentation.ubuntu.com/security/security-features/process-memory/aslr/

Keep the Monitoring PR as Draft; non-Monitoring runtime fixes remain deferred.

## Monitoring stress PASS and versioned Clang discovery (owner, 2026-11-10)

Owner ran `git pull --ff-only origin feature/monitoring-correctness-tests`
and the transcript showed fast-forward `f6c385e..65e4abc` followed by
`./tools/verify_monitoring.sh stress` in the real local checkout.
The run configured/built with **GNU 13.3.0**, passed the initial CTest
Monitoring **2/2**, then repeated **both** `vex.monitoring.smoke`
and `vex.monitoring.contract` **100 successful times each** with CTest
`--repeat until-fail:100`. Total repeat CTest time **7.01 seconds**;
summary printed `VEX MONITORING STRESS PASS repetitions=100
each_test=smoke,contract`. This run passes **non-TSan stress only**;
it does **not** establish absence of data races.

The same pasted transcript executed `command -v clang++` (no
executable resolved) followed by an attempted
`CXX=clang++ ./tools/diagnose_tsan.sh 10` that **did not execute**:
`clang++: command not found`. The owner confirmed their installation
uses the name **`clang++-20`**. This is a portability issue in the
verification instructions, **not a Monitoring source failure**.

### Version-independent Clang verification (new code, NOT YET RUN)

`tools/find_clangxx.sh` selects the explicit `CXX` first, otherwise
the unversioned `clang++` executable if present, otherwise the highest
numeric available `clang++-N` in `PATH` (e.g. 18 or 20).
It fails explicitly when no compiler is available. CTest adds the
independent `vex.monitoring.compiler-selection` fixture that verifies
these selection rules without needing real Clang to be installed.

- `./tools/diagnose_tsan.sh --clang 10`: auto-resolves Clang and
  runs an independent TSan probe, with separate per-compiler probe logs.
- `./tools/verify_monitoring.sh tsan-clang`: auto-resolves Clang,
  validates its banner, configures a **separate compiler-specific
  CMake build directory** and runs the Monitoring tests under TSan.
  It refuses to reuse a build cache whose compiler differs.
- `CXX=clang++-20 ...` is an explicit portable override mechanism
  when the selected compiler must be pinned on this machine.

The new Clang code/CTest compiler-discovery test was committed *after*
the successful stress run. **Do not mark either Clang test PASS** until
real logs from the new branch head are supplied. TSan remains **BLOCKED**
pending a stable, independently verified toolchain; do not change ASLR
or system-wide security configuration.

Non-Monitoring behavior remains frozen per owner decision.

## Clang 20 ThreadSanitizer PASS — owner-provided logs (2026-11-10)

Host: Ubuntu 24.04 series, Linux `6.8.0-147-generic`.
Compiler: `/usr/bin/clang++-20`, Ubuntu Clang `20.1.2 (0ubuntu1~24.04.3)`.

The owner executed on `feature/monitoring-correctness-tests`:

| Command | Result |
|---|---|
| `./tools/diagnose_tsan.sh --clang 10` | **10/10 PASS**, mapping=0, sanitizer_report=0, other=0 |
| `./tools/verify_monitoring.sh tsan-clang` | **3/3 CTest PASS**, including `smoke`, concurrent `contract`, and compiler resolver fixture |
| `CXX=clang++-20 ./tools/diagnose_tsan.sh --clang 10` | **10/10 PASS**, mapping=0, sanitizer_report=0, other=0 |

Clang compiled the Monitoring target and test binaries successfully using
`-fsanitize=thread`. No data-race report appeared in the supplied
test output. Therefore **the TSan gate for these exercised tests is PASS
with Clang 20**. This does not prove all possible interleavings, registry
teardown races, metric-family ownership, or unrelated modules are safe.

The original GCC 13.3.0 TSan runtime remains intermittently unusable,
as independently demonstrated by a standalone probe (3/10 PASS, 6/10
mapping fatal, 1/10 SegFault). **Do not reinterpret GCC TSan as PASS.**
The independent Clang TSan outcome supersedes the earlier
host-wide inability to run a reliable TSan check.

### Clang warning follow-up

The Clang 20 build emitted a non-fatal compiler warning:
`-Wunused-value` in `SAFE_CREATE_GAUGE`,
from the left operand of the validation expression's comma operator,
`include/vex/monitoring/type_safety.hpp` line 55.

Remediation committed *after* these successful logs:
- Cast the `MetricNameChecker<ValidateName(...)>` expression to
  `void` in **all three SAFE_CREATE macros** so the compile-time
  `static_assert` still occurs without an unused-expression warning.
- Exercise `SAFE_CREATE_COUNTER`, `SAFE_CREATE_GAUGE`,
  `SAFE_CREATE_HISTOGRAM` in the Monitoring contract test.
- Enable `-Werror=unused-value` for the Monitoring contract test with
  GNU/Clang, preventing regression of this exact warning.

**Status of the warning fix:** Code committed; **not yet rebuilt by
the owner**. The previous Clang TSan PASS belongs to the earlier
checkout code. Re-run full Clang TSan to validate the new commit.
A tiny equivalent standalone C++17 expression was syntax-tested under
Clang and GCC with `-Werror=unused-value`; this is not an actual Vex build.

Owner-provided terminal output did not contain `git rev-parse HEAD`
for these Clang runs. Do not attach an unverified exact commit SHA.

**Still open:** broader coverage, build matrix/portability, and review
of Monitoring API contracts/registry & family lifetime. No merge yet.
All non-Monitoring behavioral changes remain deferred by owner order.


## Post-warning-fix owner validation: PASS (2026-11-10)

User-pasted terminal output on branch `feature/monitoring-correctness-tests`:
`./tools/verify_monitoring.sh tsan-clang` and `./tools/verify_foundation.sh full`.

**TSan with Clang 20** (using `/usr/bin/clang++-20`; previous independent
probe 10/10 PASS, twice):
- Configure/build: PASS; compiled Monitoring and contract test
- `vex.monitoring.smoke`: PASS (0.01 s)
- `vex.monitoring.contract`: PASS (0.13 s), including all three
  `SAFE_CREATE_*` macros under `-Werror=unused-value`
- `vex.monitoring.compiler-selection`: PASS (0.03 s)
- **CTest 3/3 PASS, total 0.17 s; no sanitizer report in supplied output**
- Prior `-Wunused-value` compiler warning: **not observed** after fix

**Full CMake/CTest** (existing local GCC build):
- Build: PASS for enabled compiled Monitoring, ObjectPool/ThreadPool
  tests and examples; no compiler warning in supplied output
- `vex.thread_pool.smoke`: PASS
- `vex.object_pool.smoke`: PASS
- `vex.monitoring.smoke`: PASS
- `vex.monitoring.contract`: PASS
- `vex.monitoring.compiler-selection`: PASS
- `vex.package.consumer`: PASS
- **CTest 6/6 PASS, total 1.30 s**

These user-supplied commands verify the earlier Clang warning remediation
in the local checkout. The output identifies the branch but not the
exact `git rev-parse HEAD` commit SHA; **do not claim a cryptographically
confirmed local commit ID**. The connected GitHub branch currently contains
the remediation and is reviewed separately.

**Current verified acceptance matrix for exercised configurations:**
baseline Monitoring 2/2 PASS; threaded 2/2 PASS; ASan/UBSan 2/2 PASS;
100 repeats per Monitoring smoke and contract PASS; Clang 20 TSan
3/3 PASS; full CMake/CTest and installed package consumer 6/6 PASS.

**Remaining Monitoring work (do not conflate with passed gates):**
- RAII `ScopedCounter` semantics (counter deletion on scope exit), metric
  lifetime and metric family ownership; no owner decision yet about
  changing cumulative-counter semantics
- adversarial concurrent reset/register/scrape and external registry
  lifetimes, including post-reset user-held raw metric references
- invalid Prometheus registration handling, series cardinality and
  exception paths; negative tests and benchmarks
- toolchain compatibility (shared libraries, clean dependency resolution,
  compiler/platform matrix)

GCC 13 TSan remains unstable on this host **independently of Vex**;
Clang 20 has a passing TSan execution for the tests above. No proof of
global race freedom, production certification, or merge approval.
All non-Monitoring behavior changes remain deferred by owner instruction.
