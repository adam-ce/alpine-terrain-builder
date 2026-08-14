# Raster store refactor status

This file is the resumable implementation log for
[refactor-plan.md](refactor-plan.md).

## Baseline

- Branch: `raster-store`
- Baseline commit: `d1309f54cfcd5756786735c6a2c849419c187ed7`
- Baseline tag: `refactor_before`
- Started: 2026-08-14
- Worktree was clean before implementation began.
- `dev_driver.py doctor` rejects this repository because its root
  `CMakeLists.txt` is not the alpine-renderer project. Builds therefore use
  this repository's own CMake configuration and build directories.

## Current position

- Phase: 0 — Capture current compatibility
- State: in progress
- Next action: run the full Phase 0 verification, inspect the diff, commit,
  and tag the phase boundary.

## Phase log

### Phase 0 — Capture current compatibility

- Added pre-refactor SF fixtures for `flat` and
  `level_and_coordinate_directories` layouts.
- Added a pre-refactor DAG fixture using `.bin`, with `Leaf`, `Virtual`, and
  `Inner` index states and non-trivial metadata.
- Added characterization coverage for fixture opening, metadata-prefix reads,
  payload byte stability, layout/path round trips, hard links,
  decode/re-encode, enabled overwrites, indexed/unindexed opens, and index
  creation by directory scan.
- Recorded pre-refactor public aliases:
  - SF builder, SF merger, and SF index browser use `octree::MeshStorage` and
    `octree::IndexedStorage` (the latter aliases `IndexedMeshStorage`).
  - DAG builder and DAG convert debug use `octree::DagStorage`,
    `octree::IndexedDagStorage`, `octree::DagMetaStorage`, and
    `octree::IndexedDagMetaStorage`.
  - DAG output synchronization uses
    `dag::ThreadSafeStorage<octree::IndexedDagStorage>`.
- State: complete.

## Verification log

- Baseline `unittests_terrainlib`: 23,676 assertions in 391 test cases passed.
- Baseline `unittests_dagbuilder`: 400 assertions in 69 test cases passed.
- Focused Phase 0 terrainlib compatibility tests: 51 assertions in 6 test
  cases passed before warning cleanup.
- Focused Phase 0 DAG compatibility test: 23 assertions passed.
- Phase 0 `unittests_terrainlib`: 23,732 assertions in 397 test cases passed.
- Phase 0 `unittests_dagbuilder`: 423 assertions in 70 test cases passed.
- Phase 0 `git diff --check`: passed.

## Commit and tag log

- `refactor_before` — annotated baseline tag at `d1309f5`.
- `75277f8` — Phase 0 compatibility tests, indexes, DAG payloads, and status
  log.
- Phase 0 fixture-payload follow-up — force-adds the intentionally committed
  `.terrain` golden payloads ignored by the repository's general artifact
  rule.
- `refactor_phase_0` — annotated Phase 0 completion tag.
