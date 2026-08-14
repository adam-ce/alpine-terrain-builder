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

- Phase: 4 — Harden node reuse and enforce SF topology
- State: complete; completion commit pending.
- Next action: commit and tag Phase 4, then begin Phase 5 cleanup and
  documentation.

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

### Phase 1 — Extract topology into `store`

- Added shared node-status types, hierarchy traits, invalid-key errors,
  sparse index, and traversal under `terrainlib/store`.
- Added `octree::StoreTraits` and `raster_store::StoreTraits`.
- Converted `octree::IndexMap` and `octree::traverse` to compatibility
  wrappers over the shared implementation.
- Added common 2D/3D topology and traversal coverage plus 2D boundary and
  invalid-key tests.
- State: complete.

### Phase 2 — Introduce path mappings and runtime codecs

- Added extensionless `NodePath`, `PathMapping`, and `Layout` values.
- Added the stateful runtime `Codec` interface and typed `CodecError`.
- Added runtime ZPP Bits, terrain, binary glTF, and JSON glTF codecs.
- Added ordinary `flat` and `level_and_coordinate_directories` mapping
  function pairs plus explicit lookup.
- Consolidated DAG serialization in `dag_builder/serialization.h` and made
  the legacy DAG storage adapter include it explicitly.
- Added mapping/codec composition, parser validation, single-/multi-file,
  unsupported-operation, write-only, directory-creation, error-conversion,
  and concurrent codec tests.
- State: complete.

### Phase 3 — Generalize storage and index lifecycle

- Added typed storage, filesystem, overwrite, and index-format errors.
- Added shared `RawStorage`, `Storage`, and `IndexedStorage` with exclusive
  runtime-codec ownership, trait-keyed operations, multi-file `has`/`remove`,
  overwrite handling, index persistence, and move finalization.
- Added shared storage instantiations for both 2D and 3D traits plus move,
  overwrite, invalid-key, and multi-file tests.
- Shared storage core focused tests: 48 assertions in 6 test cases passed.
- Shared storage core build and `git diff --check`: passed.
- Added the legacy 3D `terrain.index` format adapter, explicit mesh and DAG
  codec resolvers, folder discovery, and expected-returning opening APIs.
- Preserved independently readable, read-only DAG metadata views and migrated
  DAG output synchronization to the shared storage types.
- Migrated SF builder, SF merger, SF index browser, DAG builder, and DAG
  convert debug without exposing codec ownership at application call sites.
- Ported cache API aliases for compile compatibility.
- Deleted the legacy layout class hierarchy and registry, static codec stack,
  and duplicate octree storage implementation.
- Added resolver, unsupported metadata write, unknown layout/codec, malformed
  index no-fallback, and DAG synchronized move/release coverage.
- State: complete.

### Phase 4 — Harden node reuse and enforce SF topology

- Hardened `copy_from()` around the fixed codec probe path: matching path
  lists hard-link every physical file, differing lists decode/re-encode, and
  actual path-count mismatches return a typed codec error.
- Moved overwrite index removal to the mutation boundary. Missing-source and
  decode failures leave the old logical target indexed; encode and partial
  multi-file hard-link failures leave it unindexed for safety.
- Added one-file and multi-file hard-link, re-encode, overwrite,
  missing-source, decode, encode, and partial-hard-link failure coverage.
- Added `sf::InvalidTopology`, `sf::validate_index()`, typed SF processing and
  finalization errors, and an SF finalization boundary that writes the index
  before validating it.
- Applied SF validation before SF merge/cut and DAG processing and after SF
  builder/merger output finalization. The diagnostic index browser remains
  exempt.
- Propagated SF merger save, copy, open, index, and validation failures through
  `std::expected` to the command-line boundary.
- Added SF builder, SF merger, SF cut, and DAG builder boundary tests,
  including invalid-input rejection, inspectable invalid output, unchanged
  hard links, and newly written changed/clipped nodes.
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
- Phase 1 focused shared-store tests: 67 assertions in 7 test cases passed.
- Phase 1 pre-refactor terrain compatibility tests: 56 assertions in 6 test
  cases passed.
- Phase 1 `unittests_terrainlib`: 23,799 assertions in 404 test cases passed.
- Phase 1 `unittests_dagbuilder`: 423 assertions in 70 test cases passed.
- Phase 1 application builds passed for `sf-builder`, `dag-builder`, and
  `dag-convert-debug`. The current Debug configuration has `sf-merger` and
  `sf-index-browser` disabled.
- The all-target build remains blocked by a pre-existing, unrelated call to
  removed `radix::tile::Scheme` in `unittests/sf_builder/texture.cpp`; the
  affected refactor targets and suites build independently.
- Phase 1 `git diff --check`: passed.
- Phase 2 focused shared-store tests: 160 assertions in 17 test cases passed.
- Phase 2 DAG golden fixture: 30 assertions passed, including runtime codec
  output byte equality.
- Phase 2 concurrent DAG runtime codec: 10 assertions passed.
- Phase 2 `unittests_terrainlib`: 23,892 assertions in 414 test cases passed.
- Phase 2 `unittests_dagbuilder`: 440 assertions in 71 test cases passed.
- Phase 2 application builds passed for `sf-builder`, `dag-builder`, and
  `dag-convert-debug`.
- Phase 2 `git diff --check`: passed.
- Phase 3 focused terrain store/open tests: 233 assertions in 25 test cases
  passed.
- Phase 3 focused DAG store tests: 17 assertions in 2 test cases passed.
- Phase 3 `unittests_terrainlib`: 23,973 assertions in 422 test cases passed.
- Phase 3 `unittests_dagbuilder`: 459 assertions in 73 test cases passed.
- Phase 3 all-application configuration built `sf-builder`, `sf-merger`,
  `sf-index-browser`, `dag-builder`, and `dag-convert-debug` successfully.
- Phase 3 `git diff --check`: passed.
- Phase 4 focused storage-copy tests: 70 assertions in 5 test cases passed.
- Phase 4 SF validator tests: 6 assertions in 2 test cases passed.
- Phase 4 DAG SF-input boundary test: 12 assertions passed.
- Phase 4 SF merger boundary tests: 66 assertions in 4 test cases passed.
- Phase 4 SF builder finalization tests: 19 assertions in 2 test cases passed.
- Phase 4 `unittests_terrainlib`: 24,049 assertions in 429 test cases passed.
- Phase 4 `unittests_dagbuilder`: 471 assertions in 74 test cases passed.
- Phase 4 `unittests_sfmerger`: 121 assertions in 8 test cases passed.
- Phase 4 all-application configuration built `sf-builder`, `sf-merger`,
  `sf-index-browser`, `dag-builder`, and `dag-convert-debug` successfully.
- The pre-existing `unittests_sfbuilder` compile failure involving removed
  `radix::tile::Scheme` remains isolated; the new finalization tests build and
  pass in `unittests_sfbuilder_finalization`.
- Phase 4 `git diff --check`: passed.

## Commit and tag log

- `refactor_before` — annotated baseline tag at `d1309f5`.
- `75277f8` — Phase 0 compatibility tests, indexes, DAG payloads, and status
  log.
- Phase 0 fixture-payload follow-up — force-adds the intentionally committed
  `.terrain` golden payloads ignored by the repository's general artifact
  rule.
- `refactor_phase_0` — annotated Phase 0 completion tag.
- `4c171c8` / `refactor_phase_1` — shared topology extraction and Phase 1
  completion tag.
- `fbc02d0` / `refactor_phase_2` — runtime layouts/codecs and Phase 2
  completion tag.
- `61acb74` — Phase 3 shared runtime-codec storage core.
- `4ffcdea` / `refactor_phase_3` — Phase 3 application migration and
  completion tag.
