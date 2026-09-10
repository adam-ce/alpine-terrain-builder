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

RF builder validation must reject unsupported attribution indices and indices
outside the selected table before producing tiles. Storage does not scan tile
attribution rasters on reads or writes. Clearing entries is not a library
operation. RF/TB builders, merging, sampling, window reads, and enforced
read-only access remain separate work.

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
