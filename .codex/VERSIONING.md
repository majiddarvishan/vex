# VEX versioning and release policy

This file is the durable versioning rule for VEX maintainers and coding agents.

## Canonical version sources

For every release, keep these sources consistent:

1. `CMakeLists.txt`: `project(vex VERSION X.Y.Z LANGUAGES CXX)`. This drives `PROJECT_VERSION` and the generated CMake package version metadata.
2. `include/vex/version.hpp`: `version_major`, `version_minor`, `version_patch`, `version_tag`, `version()`, and `version_full()`.
3. `cmake/version.hpp.in` is the generated-header template. It is intentionally parameterized from the CMake project version; do not hardcode a separate release number there.
4. `README.md` and root `CHANGELOG.md`: current release and user-facing release notes.
5. `tests/package_consumer/main.cpp`: assert the public version API matches the release version so installed-package consumers catch drift.

Do not change unrelated dependency versions or rewrite historical audit records just because they contain old version strings.

## Release procedure (required)

1. Confirm the owner's requested version and target branch. Use that exact target branch; when the owner explicitly says `main`, a direct version-only commit on `main` is authorized. Do not create a feature branch unless requested or needed by repository policy.
2. Inspect the current branch HEAD and check whether the target Git tag already exists **before editing**. Never move, delete, force-update, or overwrite an existing tag. If the requested tag exists, stop and ask the owner how to proceed.
3. Update every canonical version source above in one coherent change. Add a dated changelog entry that describes only work actually included and preserves known scope limitations.
4. Update the package-consumer version assertions and run the available focused configure/build/CTest checks. Record exact commands and outcomes. Never claim a test ran if it did not; explicitly mark unavailable checks `NOT RUN`.
5. Commit the release change to the owner-selected branch. Do not merge unrelated work or alter other branches.
6. Create an **annotated Git tag** named exactly `vX.Y.Z` pointing to the exact release commit on the selected branch. The tag message should name the release and summarize its scope.
7. Verify the local and remote branch SHA, tag object, tag target commit, and remote tag with `git show` and `git ls-remote --tags origin`. Confirm the tag points to the same commit as the release commit.
8. Preserve all prior release tags. Do not publish a GitHub Release or modify deployment/package registries unless separately authorized.

## Version-number convention

Use Semantic Versioning:

- `MAJOR`: incompatible public API/behavior changes.
- `MINOR`: backward-compatible public functionality or a meaningful feature milestone.
- `PATCH`: backward-compatible fixes and maintenance releases.
- Pre-release identifiers, when explicitly approved, belong in `version_tag` (for example `rc.1`); the canonical numeric components remain X.Y.Z.

## Current release record

- Release: **VEX v0.1.0**
- Target branch: `main`
- Previous source version: `0.0.1`
- Scope: Audit integration, CMake/CTest Foundation, and Monitoring integration.
- Existing tags are preserved; the new `v0.1.0` annotated tag must point to the final release commit.
