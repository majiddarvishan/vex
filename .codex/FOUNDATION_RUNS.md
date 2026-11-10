# Actual checkout foundation verification — 2026-11-10

**Provenance:** Output pasted directly by the repository owner from the local checkout:
`/home/darvishan/workspace/mine/vex` on branch
`feature/cmake-ctest-foundation`. At the time the branch head was
`e215f16a0052e18718615b6ec5407dd7c18e7bf1`; the local HEAD SHA was **not printed by the command**, so exact commit identity remains **unverified**. This is user-provided run output, not an independently replayed execution by the assistant.

## Run 1 — ThreadPool-only
Command: `./tools/verify_foundation.sh thread`

- Configure and generate: **PASS**
- Build: **PASS**, target `vex_thread_pool_smoke`
- `vex.thread_pool.smoke`: **PASS**
- `vex.package.consumer`: **PASS** (install + external CMake package consumer)
- CTest: **2/2 PASS**, total **0.07 s**
- Script: `VEX FOUNDATION PASS mode=thread`

## Run 2 — all currently supported modules
Command: `./tools/verify_foundation.sh full`

- Compiler reported: **GNU C++ 13.3.0**
- Configure and generate: **PASS**
- Build: **PASS** for compiled `monitoring`, `thread_pool` and `object_pool` consumer examples, monitoring examples and smoke test executables
- `vex.thread_pool.smoke`: **PASS**
- `vex.object_pool.smoke`: **PASS**
- `vex.monitoring.smoke`: **PASS**
- `vex.package.consumer`: **PASS**
- CTest: **4/4 PASS**, total **1.39 s**
- Script: `VEX FOUNDATION PASS mode=full`

## What this proves
CMake configures, builds and discovers tests on the owner's actual checkout for the supplied `thread` and `full` settings. The package-consumer integration test passed for both. These are meaningful Phase-0 **baseline** gates.

## What it does not prove
Neither basic smoke tests nor this command set verifies memory safety, high-concurrency behavior, sanitizers, registry lifetime/reset, complete correctness or performance. Tests with Clang, shared libraries, alternate module combinations, clean dependency installation, and other OS/toolchains remain to be done. The exact local commit SHA and version of the externally resolved Prometheus dependency were not printed. In particular, **Monitoring behavioral reliability is not yet established**.

**Decision:** Sufficient foundation baseline to start the next owner-authorized phase, Monitoring; keep remaining optional build-matrix verification open as a follow-up gate. Do not touch deferred module behavior.
