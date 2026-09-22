# RF builder design

Status: implementation approved on 2026-09-10. Progress and verification are
tracked in [implementation status](implementation-status.md).

The [architecture](architecture.md), [storage format](storage-format.md),
and [implementation status](implementation-status.md) describe the implemented
storage API. The [domain glossary](terminology.md) records the terms sharpened
during this design session.

## Accepted decisions

### Inputs

The builder consumes one prepared GDAL dataset, a vector validity mask, and
an existing source-attribution entry, and produces a new RF snapshot.
Prepared datasets include VRT mosaics. The builder does not assemble mosaics
from file lists or select their overlap priorities.

Support continuous scalar raster data and RGB imagery. Extend the existing
GDAL reading capability to RGB. Inputs must have an explicit CRS and an affine
geotransform, including rotated or skewed grids. GCP, RPC, and geolocation-array
georeferencing are outside the initial scope; unsupported inputs must fail
explicitly.

Initial output modes are float32 scalar tiles and RGB8 colour tiles. Convert
the selected source bands to those output representations.

### Adaptive resolution

An import may use different RF zoom levels in different regions, but any
coordinate is represented at only one physical tile level. Select disjoint
leaf tiles: write a candidate tile or its descendants, never both. Ancestors
needed for traversal are virtual. This is an RF-builder output policy; it does
not change the storage format's ability to hold physical ancestors and
descendants.

The local sampling ratio is the largest directional stretch of the local
transformation from RF pixel coordinates into source-pixel coordinates.
Its limit is 1.25. Thus limited downsampling is permitted. Pure rotation alone
does not increase the ratio. Area alone is insufficient because it can hide
excessive reduction along one direction.

Estimate the ratio at locations across each candidate tile, increasing the
sampling where estimates vary or approach the limit. Local approximations are
accepted; this is not a proven bound over every point between samples. The numerical sampling scheme is described under implementation details below.

### Filtering

Use GDAL's filtering rather than implementing custom supersampling when GDAL
provides the required resampling. RGB filtering operates directly on the
supplied nonlinear channel values: no conversion to linear light is required
for this builder. This is a deliberate builder policy; the linear-light
discussion in [sampling and generation](sampling-and-generation.md) must not
be read as an additional requirement for this import stage.

Do not add tests of GDAL's filter mathematics merely to duplicate upstream
coverage. Tests of our configuration, reading, coverage, tile selection, and
boundary handling are still needed.

### Mask and coverage

Reuse the mesh-mask reader in `src/sf_merger/mask.h`, generalizing it where
necessary rather than introducing a separate vector-mask reader. Its
referenced-polygon loading stage is the relevant reusable part; RF does not
need the subsequent projection onto a sphere or mesh construction. Keep its
existing 0.1-metre simplification for RF as well; do not introduce an RF-specific
option to disable it. Pixel-centre acceptance therefore uses the loaded,
simplified mask.

An output pixel is selected when its centre lies in the mask. It also needs
valid source data to receive the import's attribution index. RGB output
requires valid results for all three selected channels; otherwise attribution
is zero. Other output
pixels have attribution zero. Keep a tile when it contains at least one
accepted valid output pixel. A narrow region containing no output pixel
centres can disappear under this rule.

The mask selects output pixels; it is not a filter cutline. Otherwise valid
source samples outside the mask may contribute to a selected output pixel.
Source NoData and validity information still apply to filtering.

These are source-import rules, not a validity convention for stored rasters.
Stored attribution zero means unattributed and does not make the payload
invalid. Under the [single-raster scaling contract](scaling.md), every supplied
data sample participates regardless of attribution. A caller reusing imported
data must prepare usable values before invoking those generic algorithms;
neither the scaler nor its paired store wrappers fills or masks source gaps.
This distinction does not change GDAL's source NoData handling or the mask's
selection of attributed output pixels.

Planning must not prune a region solely because a coarse candidate tile has
no selected pixel centres: finer descendants can have centres inside the mask.
Use conservative spatial intersection during candidate traversal and apply
the pixel-centre rule at the selected output resolution.

### World boundaries

Antimeridian-crossing sources must cover both sides of the canonical
longitude seam without duplicate spatial coverage. Unsupported polar regions
must be clipped to the Web Mercator tile world.

The caller must split mask polygons at the antimeridian. Do not offer
continuous, out-of-range longitudes as an alternative mask convention or
automatically reinterpret a long polygon edge as a shorter seam crossing.

Source raster coordinates are a separate matter: GDAL maps pixel columns and
rows into the source CRS through its geotransform. That source footprint can
cross the antimeridian. Output RF tile IDs and pixel coordinates remain
canonical; this requirement does not introduce noncanonical RF tile IDs.
Source addressing and filter-neighbour handling are described below.

### Cache inputs and write ordering

Input-record creation, comparison, removal, and the resulting cache eligibility
rules belong exclusively to `rf_builder`. Do not put this policy in shared
`store`, `raster_store`, or their publication APIs. `tb_builder` and other
tools may define different cache policies, including reuse of published RF
snapshots without `inputs.tmp`.

Record the build inputs, including dataset URL, mask input, and relevant
processing settings, in `<snapshot>.part/inputs.tmp`, serialized through
`io::envelope`. Read and compare the recorded inputs before reusing cache
tiles. Tile key or output pixel type alone is not sufficient evidence of
compatibility.

Add a tile to the index only after its payload has been successfully written
to disk. Apply the same ordering to reusable tiles: complete the hard link
before adding its key. Checkpoints must not advertise unfinished tile writes.
This preserves the architecture's normal-operation guarantee and does not
add a power-failure durability requirement.

A requested cache with a missing, corrupt, or mismatching `inputs.tmp` aborts
the run before any tile production, with a diagnostic explaining the failure.
Do not silently ignore an explicitly requested incompatible cache.

Remove `inputs.tmp` as part of publication finalization so it is absent from
the published snapshot. Together with mandatory input-record validation, this
makes only incomplete `.part` snapshots eligible as caches for this builder.
`rf_builder` rejects published snapshots as caches because their input record
is absent. This is not a restriction on the snapshot format or other tools.

Comparing input identifiers does not prove that content at an unchanged path
or URL has remained unchanged. The exact compared fields are proposed below.

Write the input record once before producing any tiles. Proposed comparison
fields are the dataset and mask identifiers, selected bands, output mode,
attribution index, tile dimensions, sampling limit, mask-processing options,
resampling settings, and processing-version identifier. Operational settings
that cannot affect tile content, such as output path or logging verbosity,
must not make an otherwise compatible cache unusable.

### Empty results and reporting

A valid run producing no accepted valid pixels succeeds with an empty
snapshot, including when coverage lies entirely beyond the polar cutoff.
Invalid inputs and processing failures remain errors.

After successful publication, print/log the number of tiles and their size.
The proposed size measure is the total stored payload-file bytes, including
reused tiles once per output tile. This is distinct from additional physical
disk allocation, since reused tiles are hard-linked.

## Approved implementation choices

- Choose the coarsest candidate tile satisfying the estimated sampling limit;
  otherwise refine it. Retain the existing default of 4096 pixels per side
  unless the remaining interface discussion changes it.
- Use GDAL Lanczos resampling and original-resolution source data, avoiding
  automatically selected overviews whose filtering may differ. This follows
  GDAL's [resampling and overview documentation](https://gdal.org/en/stable/programs/gdalwarp.html#resampling-method-to-use).
- Estimate local stretch from a small finite-difference Jacobian, using its
  largest singular value. Compute pixel spacing from the actual RF tile
  dimensions, not an assumed 256-pixel delivery tile.
- Remove `inputs.tmp` immediately before the final publication operation.
  A failure or interruption after removal can leave a `.part` folder without
  its record; mandatory cache validation will reject it.

These choices are included in the approved implementation plan below.

## Existing lifecycle requirements

The architecture already requires a new immutable snapshot for every run,
abort-on-error behavior, index checkpoints at least every few minutes, and an
optional earlier or aborted snapshot as a reuse cache. Reusable tiles are
hard-linked. Finish outstanding writes before checkpointing or publication.
The builder decides semantic cache compatibility; matching tile keys and
payload dimensions alone do not establish that source data, masks, and
sampling settings match.

The storage API already supports publishing an empty snapshot.

## Existing implementation and reference input

- `src/tile_builder/DatasetReader.cpp` warps a selected band to requested
  bounds and dimensions and returns floats. It currently chooses cubic
  resampling and an approximate coordinate transformer. It needs multiband
  support and explicit validity handling for this task.
- `src/terrainlib/Dataset.cpp` estimates resolution from transformed whole
  bounds divided by source dimensions. This is not the local estimate above;
  its bounds helper also rejects rotated/skewed geotransforms.
- `src/terrainlib/store/Index.h` supports sparse virtual ancestors and disjoint
  leaves. It also permits physical ancestors, so the builder must enforce its
  own nonoverlapping output policy.
- `src/terrainlib/store/Storage.h` already completes `save()` and `copy_from()`
  payload operations before indexing. The byte writer checks writing,
  flushing, and closing. Preserve this order and coordinate concurrent
  writes with checkpoints; plain `Storage` does not provide synchronization.
- `src/sf_merger/mask.h` provides mesh-oriented vector-mask reading. Its
  referenced-polygon stage will be reused. It currently simplifies at a
  0.1-metre tolerance and falls back to WGS84 when the CRS is absent. Keep
  existing mesh callers working when generalizing this stage for RF.
- Existing `ctb::Grid` bounds arithmetic has unsigned intermediate limits;
  do not assume it safely handles all raster-store zooms and tile dimensions.

The [Swissimage reference file](https://data.geo.admin.ch/ch.swisstopo.swissimage-dop10/swissimage-dop10_2024_2682-1199/swissimage-dop10_2024_2682-1199_0.1_2056.tif)
was inspected remotely with GDAL on 2026-09-10. It is a 10000-by-10000
EPSG:2056 raster with 0.1-metre affine pixel spacing, three byte RGB bands,
256-by-256 blocks, YCbCr JPEG compression, and five overview levels. Its bands
report no numeric NoData value and all-valid masks.

At its centre, source pixel edges project to approximately 0.146 metres in
Web Mercator. A 4096-pixel RF tile at zoom 16 has 0.149291 metres per pixel.
This is a local illustration, not a resolution decision for all Switzerland.
A 4096-pixel tile at zoom z has the pixel spacing of a 256-pixel tile at z+4.

Use a small crop or a structurally similar generated fixture for unit tests,
rather than including the full source image. A prepared VRT can expose adjacent
files as one source and let filters read across their boundaries, as described
in GDAL's [mosaic documentation](https://gdal.org/en/stable/programs/gdalwarp.html#multiple-input-files).

## Approved implementation plan

1. Generalize the mesh-mask reader's referenced-polygon loading stage just
   enough to share it with RF. Preserve the existing simplification, CRS
   fallback, and mesh behavior. Evaluate output centres in the mask's CRS;
   include polygon boundary points and exclude hole interiors. This avoids
   treating a straight approximation of a reprojected edge as authoritative.
2. Extend the existing GDAL reader with scalar/RGB output and explicit
   resampling and validity results. Preserve existing clients' defaults.
   For RF, use Lanczos, base-resolution source data, and initially exact
   coordinate transformations. Default scalar input to band 1; identify RGB
   by band colour interpretation, with explicit band selection when needed.
   Use GDAL's declared NoData/mask handling rather than inventing a numeric
   sentinel or treating black imagery as missing.
3. Implement adaptive tile planning using full affine source footprints,
   conservative spatial intersection, and sampled local stretch. Choose
   disjoint leaves at the coarsest estimated acceptable scale. Sample tile
   interiors and edges, increasing samples where variation or proximity to
   1.25 warrants it. Sampling density and convergence tolerances are numerical
   implementation choices to exercise with synthetic transforms; the
   accepted contract remains approximate. Reject inability to meet the limit
   at the maximum supported zoom rather than silently accepting coarser data.
4. Handle antimeridian-crossing source footprints and source neighbourhoods
   without changing canonical RF keys. Clip coverage to the square Web
   Mercator tile world, whose latitude limits are approximately
   +/-85.05112878 degrees. Preserve GDAL filtering across ordinary input-file
   and output-tile boundaries. Do not wrap latitude or connect opposite polar
   edges. Distinguish excluded coverage from actual transformation failures.
5. Add the RF-builder command using the repository's CLI11 conventions:
   dataset, mask, output, attribution index, scalar/RGB mode, optional band
   selection, and optional cache. Retain 4096-pixel tiles as the default and
   allow smaller dimensions for fixtures and practical runs. Validate the
   attribution entry and requested cache before tile production. Write the
   envelope input record once before any tile payloads.
6. Read or hard-link only the selected leaf tiles, preserving write-before-
   index ordering. Coordinate index mutation and checkpoints with writes;
   target checkpoints every two minutes, after outstanding writes complete.
   Omit tiles with no accepted valid pixels. A readable cache uses its saved
   index and ignores unindexed payload files.
7. In `rf_builder`, remove the input record during finalization, publish through
   the existing storage API, then report tile count, pixel dimensions, and total tile-file
   bytes. Empty snapshots follow the same successful publication path.
8. Verify our integration with small fixtures: scalar and Swissimage-like
   RGB inputs; a prepared mosaic spanning a source-file boundary; rotated and
   skewed affine grids; disjoint mixed-zoom leaves around the sampling limit;
   mask holes, simplification, and centre selection; source NoData; both
   antimeridian sides; polar clipping; input-record mismatch/corruption;
   write failures and cache reuse; and empty publication/reporting. Check
   source-neighbour handling at seams without retesting GDAL's filter
   mathematics. Run affected existing reader, mask, and storage regressions.

Implementation also includes moving DatasetReader and its header into terrainlib,
retaining existing callers, and enabling GDAL CURL support for HTTP inputs.
Numerical implementation choices are verified with synthetic fixtures.

## Command usage

GDAL input uses the explicit `gdal` subcommand. The former invocation without
a subcommand is replaced. For JPEG source pyramids, use the separate
[`tiles` subcommand](rf-builder-downloader-design.md).

Build the `rf-builder` target, enabled by `ALP_BUILD_RF_BUILDER`. The selected
`source_attribution_table.json` must already exist beside the future index or
in one of its two ancestor directories. Attribution index 0 is reserved;
imports accept existing entries 1 through 65534.

```sh
build/Desktop_Debug/src/rf_builder/rf-builder gdal \
    --dataset /data/prepared.vrt --mask /data/validity.gpkg \
    --output /data/rf/new-snapshot --attribution-index 1 --mode rgb
```

`--mode scalar` is the default and reads band 1. RGB band selection follows
colour interpretations; use `--bands 3 2 1` to supply an explicit RGB order.
`--tile-size 4096` is the default; other positive power-of-two sides are accepted.
HTTP(S) dataset identifiers are opened through GDAL `/vsicurl/`.

Use `--cache /data/rf/aborted-snapshot.part` to reuse an incomplete compatible
snapshot. Every run writes to a new output path. Cache compatibility compares
normalized dataset/mask identifiers, resolved bands, mode, attribution index
and selected entry, dimensions, sampling and mask settings, resampling policy,
processing version, and GDAL version. Output paths do not affect compatibility.
Published snapshots have no input record and are rejected as RF-builder caches.
Unchanged identifiers do not detect changes to the content of a source or VRT.

On success, the command reports the published path, tile count, tile dimensions,
total payload-file bytes, and reused tile count. It reports empty results in
the same way. Exit status is nonzero on invalid inputs or processing failures.

Runtime messages go to stderr and an appended `<output>.log` beside the snapshot,
flushed after every info, warning, or error message. The log includes startup
inputs, progress, checkpoints, GDAL diagnostics, failures, and publication.
CLI parse errors and failures opening the log itself are console-only.
A count-only planning pass determines the number of candidate leaves without
retaining their keys. Progress counts processed candidates, including empty and
reused tiles, and reports a percentage, estimated remaining time, and UTC
expected finish timestamp. Updates occur after the first tile, then at most
every ten seconds between completed tiles, and after the last tile. Estimates
use the average time per completed candidate; planning and final publication
are separate phases, and variable tile costs can change the estimate.

## Implementation details

- `DatasetReader.h/.cpp` now live in `terrainlib`. Legacy instance methods retain
  cubic/approximate behavior; RF uses the explicit scalar/colour methods.
- `RasterTransform` applies the complete affine grid. Coverage uses the affine
  footprint's enclosing source rectangle and GDAL's 257-point-per-edge CRS
  bounds transformation, padded by one sampling interval to retain curved-edge
  extrema between samples. Geographic coverage is split at the canonical seam and
  clipped to the Mercator latitude range before tile traversal. Mask polygon
  bounding boxes only prune candidates; final centre tests use simplified
  polygons in their original CRS, including boundaries and excluding holes.
- Mask selection builds one exact polygon union, a persistent CGAL trapezoidal
  point-location index, and a static BVH over conservative union-boundary edge
  boxes. After transforming a row's centres into the original mask CRS, a span
  whose enclosing box intersects no boundary is classified with one point
  query. Boundary-adjacent spans subdivide and eventually use individual
  indexed queries. Enclosures use the actual transformed, source-valid centres,
  preserving curved-projection behavior, holes, and boundary inclusion.
  Source-invalid pixels remain ineligible for the import's attribution.
  There is no pre-transform whole-tile shortcut.
- Sampling uses quarter-output-pixel finite differences and the largest
  singular value, starting with 3x3 points over each candidate's coverage.
  It increases to 9x9 and 17x17 for variation above 5% or ratios within 10% of
  the limit. A sampled ratio above 1.25 immediately requires child tiles.
  Later grids can stop when the maximum changes by at most 0.1% and stays
  below 99% of the limit. Failure to meet the limit at zoom 32 is an error.
- Regional seam-crossing affine grids are addressed continuously in source
  coordinates. Axis-aligned periodic global grids expose an eight-source-pixel
  wrapped halo through a VRT, so GDAL can filter across the longitude boundary.
  No latitude wrapping is performed. GDAL still performs the filtering.
- `--jobs N` selects a fixed tile-worker count, defaulting to one. Each worker
  owns its dataset, coordinate transformations and mask query state for the run;
  individual GDAL warps remain single-threaded. Planning streams keys in spatial
  traversal order. At most twice the worker count is queued, active or awaiting
  consumption, and completed tiles may be consumed out of order.
- One coordinator owns cache lookup/linking, payload compression and writes,
  index mutation, progress and checkpoints. It checkpoints approximately every
  two minutes between completed writes while workers may continue computing.
  All workers are joined before publication. Worker count is an execution
  setting and does not affect cache compatibility or the intended output bytes.
- SIGINT/SIGTERM stops new scheduling and discards queued jobs. Active jobs
  finish; successful results are saved and checkpointed. The process exits
  without publication and retains `inputs.tmp` for reuse of the incomplete
  snapshot. Processing failures stop queued work and join active workers;
  failed results never enter the index. The first recorded worker error is
  preserved even when failures are consumed out of order.
- Memory is bounded by worker scratch buffers, outstanding tile payloads,
  GDAL's cache, per-worker mask geometry, and the snapshot's sparse index.
  Default-size scalar payloads occupy 96 MiB each before compression.

## Stored value mapping

Both `rf-builder gdal` and `rf-builder tiles` accept
`--value-mapping linear|srgba`. RGB8/RGBA8 output defaults to `srgba`; all other
pixel representations default to `linear`. Alpha remains linear under SRGBA.
An explicit override takes precedence. The resolved mapping is written to
snapshot metadata and the incomplete-cache input record; an explicit option
matching the default remains cache-compatible. RF stores always have zero
halo, and cache metadata must match nominal/stored sizes, halo, mapping, and
codec before any reuse or production.

This option declares the stored values' interpretation. Existing GDAL sampling
and online JPEG linear-light fallback interpolation retain their existing
kernels; the option does not perform source-profile conversion.
