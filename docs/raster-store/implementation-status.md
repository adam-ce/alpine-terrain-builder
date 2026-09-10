# Raster-store implementation status

The scope and sequence are in [implementation-plan.md](implementation-plan.md).
Tile storage is implemented and verified on Linux/GCC as of 2026-09-08.

| Stage | Status | Implementation and verification evidence |
|---|---|---|
| Shared foundation | Verified | Shared index, storage, codecs, envelopes, and raster traits; mesh/octree regressions pass |
| Accepted decisions and documentation | Verified | Attribution ownership, JSON contract, version-1 encoding, codec selection, API usage, and shared I/O fix recorded |
| Typed tiles and attribution tables | Verified | `raster_store/Tile.h`, `attribution.h/.cpp`; zero attribution, verbatim fields, malformed slots, lookup precedence, and boundary indices |
| XYZ layout, index, and metadata | Verified | `raster_store/path_layout.h`, `raster_store/io/manifest.h/.cpp`; boundary paths, mixed hierarchy, corrupt topology, metadata errors, and checkpoint persistence |
| AMORT codec | Verified | `raster_store/io/TileCodec.h`; native scalar/packed-GLM bytes, NaN bits, NoData payload preservation, dimensions, byte counts, compression, checksum, and version errors |
| Creation, opening, checkpoints, publication | Verified | `raster_store/storage.h`; metadata codec selection, incomplete opening, checkpoint failures, no implicit publication, collision rejection, no payload scan, and `/dev/full` write failure |
| Hard-link integration and regression tests | Verified | `unittests/terrainlib/raster_store.cpp`; cross-root hard links and independent attribution survive RF deletion; all six regression suites pass |

## Deferred work

RF builder validation rejects unsupported attribution indices and indices
outside the selected table before producing tiles. Storage does not scan tile
attribution rasters on reads or writes. Clearing entries is not a library
operation. TB builders, merging, sampling, window reads, and enforced read-only access
remain separate work. RF import is tracked below.

Publication currently uses Linux `renameat2(RENAME_NOREPLACE)`; other platforms
return `Unsupported`. The supported native pixel representation remains x86
with unpadded GLM. Publication provides normal-operation atomic visibility,
without a crash-durability guarantee.

## Verification log

- Initial working tree: clean, branch `main`.
- `dev_driver.py doctor`: rejects this terrain-builder checkout because the
  driver supports only `alpine-renderer`.
- Existing build: `build/Desktop_Debug`, configured for this checkout, GCC,
  with unit tests enabled.
- Build passed:

  ```sh
  cmake --build build/Desktop_Debug --target unittests_terrainlib unittests_tilebuilder unittests_sfbuilder unittests_sfbuilder_finalization unittests_dagbuilder unittests_sfmerger --parallel 6
  ```

- Test executables are under `build/Desktop_Debug/unittests`. Linux test runs
  used `PROJ_DATA` and `PROJ_LIB` pointing to the build's
  `alp_external/proj/share/proj`, and `GDAL_DATA` pointing to
  `alp_external/gdal/share/gdal`.

| Executable and filter | Test cases passed | Assertions passed |
|---|---:|---:|
| `unittests_terrainlib '[raster-store]'` (focused subset) | 24 | 356 |
| `unittests_terrainlib '~mesh::clip_on_bounds benchmark'` | 458 | 24496 |
| `unittests_tilebuilder` | 14 | 389 |
| `unittests_sfbuilder` | 16 | 176102 |
| `unittests_sfbuilder_finalization` | 2 | 16 |
| `unittests_dagbuilder` | 76 | 479 |
| `unittests_sfmerger` (including SF build/merge integration) | 13 | 195 |

- Total across the six suites: 579 test cases and 201677 assertions, excluding
  the separately repeated focused subset.
- Initial tilebuilder and SF-builder runs encountered absent Austrian raster
  fixtures. Missing files were restored from the existing CI archive at
  `https://gataki.cg.tuwien.ac.at/raw/terrain_builder_unittest_data.tar.gz`;
  the affected suites subsequently passed. Existing fixtures were preserved.
- `git diff --check` passed. All changed text files have LF working-tree line
  endings matching `attr/text=auto eol=lf`, checked with
  `git ls-files --eol --cached --others --exclude-standard`.
- No commits, branches, or pushes were made.

### 2026-09-10 code-style follow-up

- Moved manifest I/O to `raster_store::io::manifest`, the AMORT implementation
  to `raster_store::io::TileCodec`, and the layout to
  `raster_store::path_layout::zoom_xy_google`. Versioned payload types now live
  in the corresponding `detail::v1` namespaces; serialized identifiers and
  payload layouts are unchanged.
- Moved shared hierarchy validation into `store::Index<Traits>::validate() const`.
- Missing paths during snapshot rename and attribution copying report
  `NotFound`. `io::utils::create_parent_directories` returns `Expected<void>`;
  byte and glTF writers propagate its errors.
- `cmake --build build/Desktop_Debug --target all --parallel 6` passed.
- The same six regression suites passed: 582 test cases and 201698 assertions.
  Terrainlib now covers 461 cases and 24517 assertions, including missing-path
  rename/copy checks and successful repeated parent-directory creation.
- Qt lint reported no new warnings on the lines changed in this follow-up.
  `git diff --check` and the Git-attribute line-ending check passed.

## RF builder — 2026-09-10

The [RF-builder plan](rf-builder-design.md) is approved. RGB attribution requires
all three selected channels to be valid. Existing documentation edits were
present before implementation and are preserved.

| Stage | Status | Evidence |
|---|---|---|
| Shared referenced mask loading | Verified | `terrainlib/vector_mask.h`, mesh compatibility aliases |
| Shared scalar/RGB reader and validity | Verified | `terrainlib/DatasetReader`, per-band validity, all-channel RGB acceptance |
| Adaptive planning and world boundaries | Verified | `RasterTransform`, `rf_builder/planning`, affine and seam fixtures |
| Command and cache input records | Verified | `rf-builder`, `rf_builder/inputs`, CURL enabled |
| Writes, checkpoints, publication, reporting | Verified | `rf_builder/build`, serialized writes/checkpoints, hard links, empty publication |
| File logging and progress estimates | Verified | stderr plus appended `<output>.log`; candidate percentage, remaining time and UTC finish estimate; CLI checks cover populated, all-invalid and no-candidate results |
| Integration and regression verification | Verified | 21 RF cases plus all six existing regression suites; full build and new-code lint pass |

Baseline existing binaries: raster storage 26 cases/372 assertions; dataset
reading 1 case/72 assertions; mask simplification 2 cases/15 assertions.
The renderer driver rejects this repository; use the existing terrain-builder
CMake configuration and Linux test environment documented above.

### RF-builder verification

- `cmake --build build/Desktop_Debug --target all --parallel 6` passed.
- Tests used the existing Linux PROJ/GDAL data environment described above.

| Executable | Test cases passed | Assertions passed |
|---|---:|---:|
| `unittests_rfbuilder` | 21 | 3768 |
| `unittests_terrainlib` (excluding the existing clip benchmark) | 461 | 24517 |
| `unittests_tilebuilder` | 14 | 389 |
| `unittests_sfbuilder` | 16 | 176102 |
| `unittests_sfbuilder_finalization` | 2 | 16 |
| `unittests_dagbuilder` | 76 | 479 |
| `unittests_sfmerger` | 13 | 195 |
| **Total** | **603** | **205466** |

- RF fixtures cover JPEG/YCbCr tiled RGB, scalar values, per-channel NoData and
  masks, valid black and legacy sentinel values, prepared VRT file boundaries,
  original-resolution reads despite overviews, mask holes/boundaries, affine
  rotation/skew, mixed zooms, narrow masks, maximum zoom rejection, both
  antimeridian sides and filter neighbourhoods, polar-only empty results,
  invalid inputs, missing/corrupt/mismatching cache records, hard-link reuse,
  unindexed payloads, failed links, command reporting, curved projected edges,
  projection-domain exclusions, GDAL threading settings, feature-read errors,
  and small sources inside default-size output tiles.
- The rebuilt GDAL opened the Swissimage reference URL through `/vsicurl/`,
  reported 10000x10000 pixels and three bands, and successfully read a 32x32
  native-resolution window. The source was not added to the repository.
- Qt lint passes for the new and extracted code. Findings in unchanged mesh
  code were left alone. Six read-only review passes covered model/index
  contracts, ownership, threading, C++/API correctness, error handling, and
  performance. Identified issues were corrected and regression-tested.
- `git diff --check` passed. Working-tree text line endings agree with Git
  attributes, verified using `git ls-files --eol --cached --others
  --exclude-standard`.
- Initial user documentation edits were preserved. No commits, branches, or
  pushes were made.

### Logging/progress follow-up and Vienna run

- Rebuilt `rf-builder` and `unittests_rfbuilder`; all 21 RF cases passed with
  3834 assertions after adding persistent success/error and progress checks.
- Six focused review passes found no issues in this follow-up. Qt lint reports
  DEP-10 on `std::chrono::duration::count()`; these calls use the required chrono
  API, so the container `.size()` recommendation does not apply.
- A full-resolution Vienna DSM import is being run under
  `/data/scratch/codex/rf-vienna-20260910`. Inputs are Austria's
  `OeRect_01m_gs_31287.img` and the union of Stadt Wien's official district
  polygons. The user approved a scratch attribution entry with unknown source
  metadata left blank. The boundary source and exact invocation are recorded
  in the scratch directory. Initial planning selected 80 candidate
  4096-square tiles. Partitioning the city polygon on a 2 km grid reduced this
  to 63 candidates; its union differs from the original by only
  1.64e-8 square metres before the builder's normal simplification.
- The initial Debug run exposed costly detailed-boundary centre tests. The
  user stopped the partitioned-mask import and the optimized build at 14:57
  CEST. The last checkpoint contains 4 of 63 candidates (6.3%), with
  221459620 payload bytes. The incomplete snapshot and logs are retained;
  no final Vienna snapshot was published. Whole-tile containment currently
  has no fast path: accepted source pixels still receive individual mask tests.
