# Halo extraction implementation plan

Status: implementation approved and completed, 2026-09-22; final verification
is recorded below. Commit and push were explicitly authorized for halo work;
GDAL NoData filling and driver research are excluded.
Conversion tuples stay in the scaling interface. The accepted behavior is
defined by [Tiles with halo](tiles-with-halo.md) and
[Raster scaling](scaling.md), including its windowed-scaling extension.

## Scope and baseline

Implement windowed scaling, snapshot halo/value-mapping metadata, halo
extraction, and RF CLI propagation of the mapping. Reuse the implemented
views, copies, conversion tuples, and paired scaler. `radix::Raster` and
`raster_store::Tile` retain their current members. Work stays in terrain-builder;
the earlier proposed radix refactor has been superseded by the existing
`terrainlib/raster/algorithm` implementation.

The current storage API returns storage and metadata together from `open`
and `create`. Preserve that interface. Keep format version 1 and change its
metadata representation directly, as agreed. Tile buffers still describe
their complete stored raster; no new tile payload fields are needed.

The working tree already contains edits to the raster-store README, scaling
contract, and halo design, plus the untracked GDAL NoData-filling plan. Recheck
and report status before implementation, preserve existing work, and stay on
the current branch. GDAL NoData filling belongs to the separate plan.

## 1. Add windowed scaling

Primary files: `src/terrainlib/raster/algorithm/scale.h`,
`scaling_detail.h`, `src/terrainlib/raster_store/scaler.h`, and
`unittests/terrainlib/raster_algorithm.cpp`.

1. Add the agreed `output_offset` overloads to generic and paired `scale`.
   Allocating overloads also take `output_size`; both are `glm::uvec2`.
   Keep conversion-tuple overloads and destination-last call conventions.
2. Separate validation of a requested window from validation of a full
   output allocation. Calls without an offset still require the exact full
   destination size. Validate both paired destinations and overlap before
   either is written. Use unsigned, overflow-safe bounds and shift checks;
   do not require construction of the full conceptual output dimensions
   when only a small window is requested.
3. Extend the existing interpolation implementation to use the conceptual
   output offset. Preserve integer quotient/remainder phase calculation,
   exact nearest-neighbour copies, and zero-level copying. Compute only the
   requested pixels. Keep whole-output calls on the same implementation.
4. For reduction, derive the required source/intermediate rectangles
   backwards from the requested output. Preserve global block alignment,
   the complete Lanczos support, separable pass ordering, and encoding after
   every factor-two step. Restrict intermediates to the necessary rectangles.
   Reuse `window_transform`, ordinary views, and clamped-view semantics.
5. Window attribution with the same output coordinates: nearest neighbour
   for upscaling and repeated mode reduction for downscaling. Keep zero as
   an ordinary attribution value. Reuse reduction internals where needed;
   a new public windowed custom-reducer interface is not required.

Stage validation:

- Compare allocated and destination-view windows with full-scale-then-crop,
  including non-aligned offsets, rectangular and strided views, zero levels,
  nearest neighbour, bilinear, box, and every Lanczos support radius.
- Check multi-level integer rounding, SRGBA encoding, and mode attribution
  against the existing full-output behavior, including zero attribution.
- Check invalid offsets/extents, insufficient halo, overlap, and paired
  destination mismatches before writes. Without an offset, reject both
  undersized and oversized destinations.
- Request a tiny window across a 20-level upscale, including a case whose
  conceptual full result cannot be allocated as an ordinary raster. Verify
  the expected samples without allocating or visiting the complete result.
- Retain existing exact-copy/conversion-bypass and independent kernel tests.

## 2. Extend snapshot metadata and creation

Primary files: `src/terrainlib/raster_store/io/manifest.h/.cpp`,
`pixel.h`, `storage.h`, codec construction call sites, and
`unittests/terrainlib/raster_store.cpp`.

1. Replace metadata `width`/`height` with `stored_tile_size`,
   `nominal_tile_size`, and `halo_width`; add the existing `pixel::Mapping`
   as `value_mapping`. Nominal/stored defaults are 4096 and halo defaults to
   zero. Keep one shared mapping enum.
2. Replace `CreateOptions::tile_dimensions` with
   `unsigned nominal_tile_size = 4096` and add `unsigned halo_width = 0`.
   Represent the optional explicit mapping choice with
   `std::optional<pixel::Mapping> value_mapping`; resolve an omitted choice
   in `create<PixelType>()`. RGB8/RGBA8 default to SRGBA; other types default
   to Linear. Metadata always contains the resolved enum, not an optional.
3. Validate positive power-of-two nominal size, `halo_width <= nominal_tile_size`,
   and the checked equality
   `stored_tile_size = nominal_tile_size + 2 * halo_width`. Use the existing
   metadata validation/error conventions. The extractor asserts its
   minimum nominal side of 64; this does not require making small standalone
   codec/view test fixtures larger.
4. Construct codecs using square dimensions derived from stored size. Update
   affected callers and metadata fixtures. Preserve the existing open/create
   ownership model, explicit checkpoints, and publication behavior.
5. Validate metadata representation and known enum values, but do not reject
   a valid mapping merely because a downstream scaler cannot process that
   mapping/pixel-type combination. Such rejection stays in that operation.

Stage validation:

- Round-trip metadata and tile payloads with zero and nonzero halos; include
  a power-of-two interior whose stored side is not a power of two.
- Verify type-based defaults, explicit overrides, and persistence of the
  resolved mapping, including overrides unsupported by numerical scaling.
- Reject inconsistent metadata sizes, invalid nominal sizes/halos, arithmetic
  overflow, and unknown mapping enumerators through the appropriate API.
- Preserve payload bits, read/write dimension checks, storage ownership,
  checkpoint, hard-link, and publication regressions. No legacy upgrade tests.

## 3. Implement physical-source halo extraction

Add `src/terrainlib/raster_store/read_tile_with_halo.h`, exposing
`raster_store::read_tile_with_halo`, with implementation helpers kept private.
Register the header in `src/terrainlib/CMakeLists.txt`. Add focused tests in
`unittests/terrainlib/raster_halo.cpp`, registered in `unittests/CMakeLists.txt`.

The function takes storage and metadata by const reference, a tile ID, the
requested halo width, and `raster::algorithm::Interpolation`. It returns
`Expected<Tile<PixelType>>`. Callers supply metadata associated with the store;
perform structural/dimension validation without reopening metadata per read.

1. Validate the ID, physical centre, metadata, and requested width. Require
   `Leaf` or `Inner`; reuse existing missing/invalid-ID error handling.
   Assert nominal side at least 64. Propagate indexed-payload read failures.
2. For a stored halo, reject requests exceeding it and copy the exact
   requested crop from both rasters. For a zero-width request, return the
   exact interior. These paths do not fetch neighbours or invoke scaling.
3. Otherwise allocate the output once and copy the centre through views.
   Partition the surrounding strips/corners into disjoint output rectangles.
   Use tile-local addressing, horizontal wrap, and explicit vertical limits.
4. Resolve each rectangle from the index before loading its payload:
   same-zoom physical tile first; otherwise first physical descendants per
   branch, at most four levels; nearest physical ancestor for unresolved
   coverage, including branches stopped by the limit. Zero attribution does
   not affect source selection.
5. Group assigned rectangles by physical tile ID. Load each supplying tile
   once, process all its assignments, then release it. Reuse the already
   loaded central tile if wrapping selects it. The grouping is per extraction;
   no persistent cache or cache configuration is added.
6. Use view copies for equal-resolution regions. Use paired windowed scaling
   with Box for descendants and the requested interpolation for ancestors.
   Clamp bilinear support to the complete supplying ancestor interior; do not
   load support neighbours or clamp independently at processing cutouts.
7. For missing physical coverage only, copy through a clamped view of the
   central interior and leave synthesized attribution zero. Supplied or
   resampled zero-attribution payloads remain untouched by this fallback.

Stage validation uses synthetic stores and a counting/failing test codec at
the existing storage seam, without adding production test hooks:

- Exact centre/crop results, physical inner centres, all edges/corners,
  zero/full permitted widths, invalid requests, and physical read errors.
- Same-zoom preference, stopping at physical inner descendants, mixed-depth
  sibling branches, four-level cutoff, ancestor fallback, and missing coverage.
- One read per supplying tile across multiple assigned regions, no reads
  beyond the descent cutoff, and no neighbour reads for stored-halo crops.
- Horizontal seam/root wrapping, vertical limits, and corners crossing them.
- Zero-attribution numerical contributions and preservation, synthesized
  zero attribution independent of the central pixel's attribution, and
  exact nonfinite copies where conversion is bypassed.
- Both ancestor interpolation choices, full-ancestor clamping, distant
  ancestor windows, and mapping errors only when an unsupported operation
  is actually used. Verify scaler integration without duplicating its kernel
  mathematics in extractor tests.

## 4. Propagate value mapping through RF creation and CLI

Primary files: `src/rf_builder/run.h/.cpp`, `gdal/build.h/.cpp`,
`gdal/cli.cpp`, `tiles/build.cpp`, `tiles/cli.cpp`, and the GDAL/tiles
`inputs.h` records. Extend existing tests in
`unittests/rf_builder/import.cpp` and `online.cpp`.

1. Add `--value-mapping linear|srgba` to both commands and carry the optional
   override through their existing options, without restructuring the options
   hierarchy. Resolve the default from the produced pixel type. Document
   RGB8/RGBA8 defaults, Linear otherwise, override precedence, and linear alpha.
2. Pass nominal size, explicit zero halo, and the selected mapping into store
   creation. Update RF metadata checks to the revised fields.
3. Include the resolved mapping in both input records and cache checks. Compare
   nominal/stored dimensions, zero halo, mapping, and codec before hard-link
   reuse. An omitted default and an explicit equivalent mapping remain
   compatible; a different effective mapping fails before production/HTTP.
   Revise current version-1 records directly, consistent with the agreed
   absence of compatibility requirements, and update aggregate fixtures.
4. This stage declares the mapping of stored payloads. It retains existing
   import filtering: GDAL's current raster sampling and the online JPEG
   worker's current linear-light fallback interpolation. The override does
   not add source-profile conversion or change these import kernels. This
   scope is explicit because changing import filtering would be additional
   behavior beyond metadata propagation and halo extraction.

Stage validation: scalar/RGB defaults, explicit overrides, zero RF halo,
persisted metadata, CLI help and invalid values, equivalent/mismatched cache
records, inconsistent cache metadata, and online rejection before HTTP.
Retain current import pixel-equivalence, cancellation, and cache regressions.

## 5. Integration, documentation, and verification

1. Update storage-format fields and examples, architecture, sampling/halo
   references, RF CLI documentation, and implementation status to describe the
   completed behavior. Keep this plan as the stage checklist and record actual
   validation results rather than treating historical results as new evidence.
2. Run focused Catch2 tests after each stage. Once stages are integrated, run
   the terrainlib, RF-builder, and DAG-builder regression suites; include
   dependent build targets affected by the metadata API rename. Run affected
   tests under existing sanitizer configurations when available and compile
   the changed template code in an existing optimized configuration.
3. Use the Qt C++ skill for lint/review and `clang-format-21` on changed C++
   sections. Check standalone public-header compilation, `git diff --check`,
   and changed-file line endings with
   `git ls-files --eol --cached --others --exclude-standard`.

The verified local starting configuration is `build/Desktop_Debug` (GCC,
tests enabled). The relevant targets are `rf-builder`,
`unittests_terrainlib`, `unittests_rfbuilder`, and `unittests_dagbuilder`.
Reuse this configuration and the documented PROJ/GDAL test environment.
The renderer-only `dev_driver.py` workflow does not apply to this project.

Build/test entry points:

```sh
cmake --build build/Desktop_Debug --target rf-builder unittests_terrainlib unittests_rfbuilder unittests_dagbuilder --parallel 6
build/Desktop_Debug/unittests/unittests_terrainlib '[raster-algorithm],[raster-store],[raster-halo]'
build/Desktop_Debug/unittests/unittests_rfbuilder
```

Use `[raster-halo]` for the new extraction fixtures. Full regressions follow
the focused tests, using the existing exclusion for the unrelated mesh clipping
benchmark. No runtime timing threshold is required: tiny-window large-gap
tests and exact source-read counts establish the key bounded-work properties.

## Completion criteria

- Each stage's behavior and focused checks pass; integrated regressions and
  required formatting/lint checks are reported with their actual outcomes.
- Halo extraction uses the shared algorithms, preserves the central/cropped
  payload exactly, obeys physical-source selection, and avoids full upscaled
  intermediates and duplicate source reads.
- Metadata and CLI defaults/overrides round-trip and cache reuse checks the
  effective mapping and halo geometry.
- Documentation matches the implemented format and interface. Report any
  remaining limitations, final Git status, and a reminder to commit; do not
  commit, push, or change branches without the corresponding authorization.

## Completion record — 2026-09-22

All five implementation stages are complete. Window reduction uses a
factor-aligned source view with the cumulative kernel halo, then the existing
staged reducer; this limits intermediates while preserving the full result's
stage alignment and encoding. Ancestor windows rebase into source coordinates
before scaling, retaining unsigned phase arithmetic and full-source clamping.

The current validation results are recorded in
[implementation status](implementation-status.md#halo-extraction--2026-09-22).
The user authorized committing and pushing halo extraction and related docs,
while leaving GDAL NoData handling and driver research uncommitted.
