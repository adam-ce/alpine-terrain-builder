# GDAL import NoData filling

Status: implementation authorized on 2026-09-23. Implemented; verification
and performance measurements are recorded below.

This extends [RF-builder design](rf-builder-design.md) for `rf-builder gdal`.
It prepares usable stored payloads independently of attribution. The online
tile importer and generic scaling semantics are outside this change.

## Agreed decisions

- Fill after GDAL warping, in RF output-pixel coordinates. Differences in the
  geographical extent of the fill at adjacent RF zooms are acceptable.
- Read an expanded window, then perform filling and smoothing in memory.
  Overlapping reads for neighbouring windows are acceptable; filling and
  smoothing must not require another source read.
- Use `GDALFillNodata` with inverse-distance interpolation and no built-in
  smoothing. Make its maximum search distance configurable, defaulting to
  five RF pixels. Accept nonnegative integer radii; zero bypasses filling
  explicitly rather than invoking GDAL's unlimited-search interpretation.
- Make the fallback for missing pixels beyond the fill's reach configurable
  through `--nodata-default-value`, defaulting to zero. The fallback is a
  payload value, not a validity sentinel, and participates in smoothing.
  Scalar imports accept one value. RGB imports accept either one value applied
  to all channels (e.g. `128`) or an explicit R,G,B triple (e.g. `128,64,32`).
  Scalar values must be finite and fit float32; RGB components must be integers
  in 0..255. Equivalent single-value and triple RGB forms share cache settings.
- Smooth originally missing pixels using a Gaussian kernel through the
  facilities exposed by `raster/algorithm.h`. Make the kernel size configurable,
  defaulting to 5x5. Accept positive odd kernel sizes; size one bypasses
  smoothing. For K > 1, derive sigma = (K - 1) / 4 and normalize the sampled
  Gaussian weights. Thus the default sigma is one RF pixel.
- Retain neighbouring tiles based on their expanded window, rather than
  requiring an accepted original pixel in the tile interior. At least one
  pixel in the expanded window must be originally source-valid and have its
  centre inside the vector mask. A retained tile may have entirely zero
  interior attribution. Synthetic pixels do not qualify additional tiles.
- RGB filling and smoothing operate directly on the supplied channel values,
  matching the GDAL import policy, without linear-light conversion. Use one
  common validity mask requiring all three channels. Initialize all components
  of an invalid pixel to the configured fallback. Preserve originally valid
  RGB pixels exactly.
- Attribution remains based on original source validity and vector-mask
  selection in the stored interior. Filled and smoothed replacements remain
  unattributed. Valid values outside the vector mask remain usable donors.
- At the north/south RF world limits, replicate the border in RF coordinates.
  Do not wrap latitude or use samples beyond the vertical world limits as
  source coverage. Longitude continues to wrap.
- Immediately after warping, treat nonfinite floating-point results as missing,
  before RGB byte conversion. The same bounded fill handles these pixels and
  other missing samples. Source-side nonfinite filtering is outside this change.

## Geometry and available mechanisms

For an odd kernel side K, the support radius in pixel-centre coordinates is
(K - 1) / 2. A single Gaussian convolution following a fill of radius R therefore
requires a temporary halo H = R + (K - 1) / 2, including corners. Defaults R = 5
and K = 5 require H = 7. A 4096-pixel interior becomes a 4110-pixel read window,
approximately 0.685% extra raster area, before GDAL source-filter support.

Use two separable `raster::algorithm::window_transform` calls: a Kx1 horizontal
pass followed by a 1xK vertical pass, with floating intermediates. Together
these implement one Gaussian convolution, not two smoothing iterations.
At the default size this uses ten weighted contributions per output pixel
across the two passes, rather than twenty-five for a direct 5x5 evaluation,
apart from halo overhead. Runtime still depends on memory traffic.

For S = (K - 1) / 2 and K > 1, compute one-dimensional weights proportional to
exp(-i*i / (2*sigma*sigma)) for integer i in [-S, S], then divide by their sum.
The two-dimensional kernel is the outer product of these weights. Bypass the
kernel calculation entirely for K = 1.
Put the reusable Gaussian-kernel creation function in
`src/terrainlib/raster/algorithm/window_transform.h`, exposed through
`raster/algorithm.h`. Reuse its one-dimensional weights in both passes.

Use `zip_transform` to select original or smoothed values using original source
validity. Apply this selection after both passes; restoring valid samples
between passes would change the convolution. Window transforms require
separate input and output storage. Use existing numeric conversion facilities
for floating working values and rounded/clamped RGB output. The Gaussian-kernel
creation function is the addition to the generic raster API.

GDAL's fill operation uses an in-memory target and `TEMP_FILE_DRIVER=MEM`
to avoid temporary disk rasters. Its default temporary driver is GeoTIFF.
Preserve a separate original validity mask; zero is an ordinary payload value.

For RGB, preserve the existing reader's byte-valued original pixels and combined
channel validity. Use floating encoded-channel working bands for IDW and both
Gaussian passes, rounding/clamping replacement values only after smoothing.
The current reader performs one scalar warp per RGB channel; this remains one
source-reading stage and requires no further source reads for postprocessing.

The RF reader keeps source columns continuous across periodic global longitude
seams. Its VRT wraps the necessary source edge strips, with padding sized for
the requested window and Lanczos support (at least eight source columns).
This source-pixel padding is separate from the RF-pixel fill halo. Both values
and validity wrap. A counting-dataset regression verifies that a 32-pixel
seam window reads strips narrower than 128 columns from a 4096-column source,
for both -180..180 and 0..360 layouts. Filling makes no further source reads.
Resolution estimates omit source-branch wrapping so the seam cannot inflate
finite-difference derivatives by a full source width.

Successful coordinate transforms may extend beyond the vertical RF world
limits. The worker clips source-reading, fill, and retention regions to the
RF world. For smoothing, replicate the nearest completed in-world row,
including its filled/default values, through a clamped view at the actual
north/south world boundary. This avoids introducing extra fill donors beyond
the world and ensures that the smoothing filter sees a replicated border.
Ordinary internal tile boundaries still use actual neighbouring samples.

Before this change, the planner tested unexpanded tile bounds against source
and mask coverage. Changing only worker retention would therefore miss neighbouring
tiles that need to be considered for the transition. Candidate planning now expands
in both the count pass and production traversal, as do worker read windows.

## Nonfinite samples

The check occurs immediately after GDAL warping, while each band is
still floating-point, before RGB byte conversion and before retention or fill.
Combine the returned GDAL validity with a finite-value check. Initialize newly
invalid payloads to the configured fallback and include them in the same
explicit fill mask as other missing samples. The bounded GDAL search handles
both; no additional source read or separate search is needed.

This is post-warp cleanup. An undeclared NaN or infinity in the source can
already affect neighbouring Lanczos results. Preventing that during the warp
would require source-side validity handling, which is separate work.

## Implementation plan

1. Add configurable search distance, Gaussian kernel size, and fallback value
   to GDAL options and CLI as `--nodata-search-radius`,
   `--nodata-smoothing-kernel-size`, and `--nodata-default-value`, defaulting to
   five, five, and zero respectively. Validate finite fallback values
   representable by the output pixel type, odd/positive kernel size,
   nonnegative integer search radius, GDAL signed dimension limits, and
   allocation/halo arithmetic before planning. Include all three settings in
   the cache input record. Bump the input envelope class version as well as the processing version: the added
   fields change the serialized record layout. Reject previous caches; no
   cache migration is needed. Worker count remains an execution setting.
2. Expand candidate planning and read windows at the candidate's RF spacing.
   Both the count pass and production traversal must consider halo overlap.
   Recompute physical halo width at each candidate zoom against fixed original
   source/mask coverage; never expand coverage from synthesized output tiles.
   Retain the existing approximate 1.25 sampling-limit policy over the expanded
   candidate's intersection with source/mask coverage, using the original RF
   spacing. Canonicalize longitude and clip latitude before estimates.
3. Warp values and original source validity once for the expanded window.
   Support rectangular RF reads to clip polar rows and assemble disjoint
   longitude-seam pieces when needed, preserving pixel phase and spacing.
   Limit reader changes to the RF `read_scalar()` / `read_colour()` path;
   preserve the separate `DatasetReader::read()` / `readWithOverviews()`
   instance API used by `tile_builder`. Classify nonfinite warped values before colour conversion.
   Test retention using a separate mask-selected copy of original validity.
4. Initialize missing values to the configured fallback and run the bounded
   fill with an explicit original-validity mask. Put import-specific preparation
   in a small helper under `src/rf_builder/gdal`, using GDAL memory datasets for the fill and
   existing raster algorithms for conversion and composition.
5. Add the Gaussian-kernel creation function to
   `src/terrainlib/raster/algorithm/window_transform.h`. Smooth the completed
   payload using its weights and raster algorithms; preserve original valid
   values when composing the result.
6. Apply the agreed retention and attribution rules and crop to the original
   RF tile dimensions. Allocate an exact N-by-N output tile and compose/copy
   the cropped views into it; the expanded read cannot be moved directly into
   `Tile::data` as the current worker does. The temporary halo is not persisted.
7. Revise the accepted builder design and implementation status when the full
   plan is agreed.
8. Verify small and large holes, partially invalid RGB, adjacent windows,
   source and mask boundaries, world boundaries, mixed zooms, cache rejection,
   configurable settings, synthetic-only retained neighbours, and memory/runtime
   on representative inputs.

## Buffer ownership and performance verification

Each worker owns its read and postprocessing scratch storage. Reuse scratch
capacity and precomputed kernel weights across tiles where practical. Borrowed
views must not outlive their buffers. Preserve the original pixel buffer and
validity while filling working bands; keep the mask-selected retention buffer
separate. For RGB, process working channels sequentially with the same validity
mask to limit peak scratch memory, then assemble the final RGB tile. No source
channel needs to be warped again for filling or smoothing.

Skip filling and smoothing when the expanded read is fully valid. Skip filling
when R = 0 and smoothing when K = 1. Crop working views to the regions needed
by subsequent passes. Use explicit-destination raster operations where they
avoid extra full-image allocations. Do not overwrite a convolution input while
its output is being computed.

Use Catch2 fixtures and benchmarks to exercise fully valid input, small holes,
large gaps, scalar and RGB data, and serial/parallel workers. Measure per-stage
time and peak process memory, including GDAL scratch rasters. Verify no source
reads occur after the initial read stage and no disk-backed fill scratch files
are created. Measure source-window sizes for periodic global seam fixtures.
Report measured costs rather than promise a runtime before benchmarking.

Correctness checks must include unchanged originally valid values, finite
replacement values, common RGB validity, expected configured-fallback interiors
of large holes (including the default zero and nonzero settings), Gaussian
kernel normalization and symmetry, fill/smoothing disable modes, rejected
invalid configuration, same-grid split-window agreement, replicated completed polar borders, mixed
zoom coverage, and serial/parallel agreement. Retention fixtures must include
a tile with qualifying halo but entirely unattributed interior, and a farther
tile that must be omitted. Update existing tests that assume every retained
tile has nonzero interior attribution. Verify count and production traversal
agree, old caches are rejected, and compatible new caches remain reusable.

Here, unchanged original values means the samples returned by the current
expanded warp. GDAL's window-dependent resampling can differ slightly from a
previous run using unexpanded windows; it is not a bitwise compatibility promise
for old snapshots.

## Accepted concurrency decision (Q12)

The bundled `GDALDriver::Create` writes a shared driver callback on every call
(`extern/gdal/gcore/gdaldriver.cpp`). Before Lanczos warping,
`DatasetReader::read_scalar()` creates a two-band Float32 MEM destination:
one band for warped values and one for validity/alpha. Its local static mutex
covers only `driver->Create()`, not the warp or reads of the result.
`read_colour()` repeats this once per selected channel. This existing lock
serializes destination creation calls for the callback race.
The fill stage exposes its working values and original mask as borrowed
bands in an in-memory dataset, and `GDALFillNodata` creates MEM scratch rasters
through the same API, so private worker datasets alone do not remove the race. Switching temporary file drivers does not fix the shared-create
path either.

Accept this specific callback race and allow concurrent fills on independent
worker datasets. Do not add a shared fill lock, patch GDAL, or introduce
worker-specific drivers. The existing reader creation lock remains unchanged.
For the initialized MEM driver, the getter returns the same callback that
each call writes back. Practical risk on x86-64 is assessed as low, although
the C++ data race remains; no failure has been reproduced. This acceptance
does not extend to other races or shared mutable worker datasets.

The TSan configuration suppresses races through `GDALDriver::Create()` for
this accepted issue. Because suppression matches a stack frame, it also hides
other races through that function; it does not establish general thread safety.

## CLI examples

```sh
# Scalar fallback -100, with the default fill radius and Gaussian kernel.
rf-builder gdal ... --nodata-default-value -100

# RGB fallback, radius 3, and no smoothing.
rf-builder gdal ... --mode rgb --nodata-default-value 128,64,32 \
    --nodata-search-radius 3 --nodata-smoothing-kernel-size 1
```

Input envelope and processing versions are both 2. Older GDAL caches are
rejected; changing any of the three NoData settings also rejects cache reuse.

## Verification and performance — 2026-09-23

The existing `build/Desktop_Debug` configuration builds all targets. Regression
coverage includes RF imports and cache reuse, raster algorithms, the legacy
tile builder, SF builder, DAG builder, and SF merger. The focused fixtures cover
finite replacements, partial RGB invalidity, valid zero/sentinel preservation,
nonzero fallback values, disabled stages, halo retention, mixed zooms, polar
replication, split-window agreement, and concurrent processors. The counting
source verifies narrow global seam reads and no postprocessing source rereads;
a temporary-directory check confirms the fill creates no scratch files there.

Catch2 microbenchmarks used the existing **Debug** build on the development
machine. These are smoke measurements, not Release throughput guarantees.
For a 512x512 output with a seven-pixel halo (three timed samples):

| Input gaps | Scalar fill only | Scalar Gaussian only | Scalar combined | RGB combined |
|---|---:|---:|---:|---:|
| Fully valid | 13.4 ms | 13.4 ms | 13.2 ms | 14.1 ms |
| One-column gaps every 256 columns | 49.9 ms | 118.0 ms | 132.4 ms | 416.3 ms |
| 128-column gaps every 256 columns | 78.6 ms | 118.4 ms | 162.6 ms | 519.2 ms |

The fully valid row measures the copy/validity fast path. Each other column
includes its buffer preparation and output composition, so columns are not
additive stage timers. Peak process RSS for this benchmark was approximately
200.5 MiB, including the test executable and retained scratch allocations.

A separate 4096x4096 scalar benchmark with 128-column gaps measured 10.45 s for
one worker and 10.89 s for a batch of four workers (one timed sample each).
Peak process RSS across these runs was 1.56 GiB. This harness shares one input
raster across workers and measures postprocessing only, including GDAL's MEM
scratch rasters. End-to-end imports also allocate per-worker read buffers,
mask selection, attribution, and GDAL warp/cache storage. RGB channels reuse
the same floating scratch buffers sequentially. Scratch is reused while window
dimensions remain unchanged; clipped polar windows may require reallocation.

Run the hidden Catch2 cases `RF NoData stage benchmarks` and
`RF NoData full tile memory and parallel benchmark` explicitly to repeat these
measurements. Source I/O and warping are excluded from these timings.
