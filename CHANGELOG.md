# Changelog

All notable VEX release changes are documented here.

## [0.1.0] - 2026-11-14

### Added
- Integrated the repository Audit and CMake/CTest Foundation work into the default `main` branch.
- Completed and integrated the Monitoring module's registry ownership, lifecycle, RAII, validation, HTTP contract, portability, and performance verification work.
- Added the synthetic Managed/Raw five-minute Monitoring Registry memory-soak harness and its measured analysis.
- Added an installed-package consumer assertion for the public VEX version API.

### Changed
- Set the canonical project version to `0.1.0` in CMake and the public version header.
- Updated release documentation and recorded the mandatory versioning/tagging procedure.

### Scope notes
- Monitoring soak results are workload-specific and do not prove universal memory-leak or race freedom.
- Owner-run test results recorded in `.codex` are not GitHub Actions CI results.
- Logger, ObjectPool, and ThreadPool runtime remediation remains deferred unless separately authorized.
