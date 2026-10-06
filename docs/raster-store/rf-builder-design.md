# RF builder design

`rf-builder` imports raster data into a new RF snapshot. It has two input
modules sharing one production lifecycle:

- `rf-builder gdal` imports one prepared GDAL dataset, including a VRT mosaic.
- `rf-builder tiles` imports an online JPEG/PNG tile pyramid.

The [storage format](storage-format.md), the snapshot lifecycle in
[architecture](architecture.md), and the [terminology](terminology.md) apply.

## Common policy

### Inputs and output

An import consumes one data source, a vector validity mask, and an existing
source-attribution entry, and produces a new RF snapshot. The selected
`source_attribution_table.json` must already exist beside the future index or
in one of its two ancestor directories. Attribution index 0 is reserved;
imports accept existing entries 1 through 65534. `--tile-size` defaults to
4096; RF snapshots always have zero stored halo.

### Disjoint leaves

An import may use different RF zoom levels in different regions, but any
coordinate is represented at only one physical tile level. Select disjoint
leaf tiles: write a candidate tile or its descendants, never both. Ancestors
needed for traversal are virtual. This is an RF-builder output policy; it does
not change the storage format's ability to hold physical ancestors and
descendants.

### Mask and attribution

An output pixel is selected when its centre lies in the mask. It also needs
valid source data to receive the import's attribution index; other pixels
have attribution zero. RGB output requires valid results for all three
channels. The mask uses the shared vector-mask reader with its 0.1-metre
simplification; centre tests use the simplified polygons in their original
CRS, including boundaries and excluding holes.

The mask selects output pixels; it is not a filter cutline. Otherwise valid
source samples outside the mask may contribute to a selected output pixel.
Source NoData and validity information still apply to filtering.

These are source-import rules, not a validity convention for stored rasters.
Stored attribution zero means unattributed and does not make the payload
invalid. Under the [scaling contract](sampling-and-generation.md#implemented-scaling-rules),
every supplied data sample participates regardless of attribution.

Planning must not prune a region solely because a coarse candidate tile has
no selected pixel centres: finer descendants can have centres inside the mask.
Use conservative spatial intersection during candidate traversal and apply
the pixel-centre rule at the selected output resolution. A narrow region
containing no output pixel centres can disappear.

### World boundaries

The caller must split mask polygons at the antimeridian. Continuous,
out-of-range longitudes are not accepted as a mask convention.

Source footprints may cross the antimeridian; output RF tile IDs remain
canonical, and filters see neighbours across the longitude seam. Coverage is
clipped to the square Web Mercator world (about ±85.05112878°). Latitude never
wraps.

### Stored value mapping

Both modules accept `--value-mapping linear|srgba`. RGB8/RGBA8 output defaults
to `srgba`; all other pixel representations default to `linear`. Alpha remains
linear under SRGBA. The resolved mapping is written to snapshot metadata. The
option declares the stored values' interpretation; it does not change import
filtering or perform source-profile conversion.

### Cache inputs and write ordering

Input-record creation, comparison, removal, and the resulting cache
eligibility rules belong exclusively to `rf_builder`, not to shared `store` or
`raster_store`. Other tools may define different cache policies.

Record the build inputs and processing settings that affect tile content in
`<snapshot>.part/inputs.tmp`, written once before producing any tiles.
Operational settings such as output path, job count or logging do not affect
compatibility. A requested cache with a missing, corrupt, or mismatching
record aborts the run before any tile production. Cache metadata must also
match nominal/stored sizes, halo, mapping, and codec.

`inputs.tmp` is removed immediately before publication, so only incomplete
`.part` snapshots are eligible as caches for this builder. A failure after
removal leaves a `.part` without its record, which cache validation rejects.
Comparing identifiers does not prove that content at an unchanged path or URL
has remained unchanged.

Add a tile to the index only after its payload has been written or
hard-linked. Checkpoints (about every two minutes) must not advertise
unfinished tile writes.

### Execution, cancellation and reporting

`--jobs N` selects a fixed worker count, defaulting to one. Workers own their
source, transformation and mask state. At most twice the worker count of tiles
are queued, active or awaiting consumption; completed tiles may be consumed
out of order. One coordinator owns cache linking, writes, index mutation,
progress and checkpoints. Worker count does not affect output bytes.

SIGINT/SIGTERM stops new scheduling and discards queued work. Active tiles
finish, are saved and checkpointed, and the process exits without publication,
retaining the `.part` snapshot and its input record for reuse. Processing
failures stop the run; the first recorded worker error is reported.

A valid run producing no accepted pixels succeeds with an empty snapshot,
including when coverage lies entirely beyond the polar cutoff. On success,
the command reports the published path, tile count, tile dimensions, total
payload-file bytes (reused tiles counted once per output tile), and reused
tile count.

Runtime messages go to stderr and an appended `<output>.log` beside the
snapshot. Progress reports a percentage, estimated remaining time and UTC
finish time.

## GDAL module

```sh
rf-builder gdal \
    --dataset /data/prepared.vrt --mask /data/validity.gpkg \
    --output /data/rf/new-snapshot --attribution-index 1 --mode rgb
```

`--mode scalar` is the default and reads band 1. RGB band selection follows
colour interpretations; `--bands 3 2 1` supplies an explicit order. HTTP(S)
dataset identifiers are opened through GDAL `/vsicurl/`. Use `--cache
<aborted-snapshot>.part` to reuse an incomplete compatible snapshot.

### Inputs

The builder does not assemble mosaics from file lists or select their overlap
priorities; that is the job of the prepared dataset. A VRT can expose adjacent
files as one source and let filters read across their boundaries.

Inputs must have an explicit CRS and an affine geotransform, including rotated
or skewed grids. GCP, RPC, and geolocation-array georeferencing are
unsupported and fail explicitly. Output modes are float32 scalar and RGB8.
GDAL's declared NoData and mask handling decide source validity; black imagery
is not treated as missing.

### Filtering

Use GDAL Lanczos resampling on original-resolution source data, avoiding
automatically selected overviews whose filtering may differ. RGB filtering
operates directly on the supplied nonlinear channel values, without
linear-light conversion. This is a deliberate import policy.

### NoData filling

Missing source pixels are filled after the warp, in RF pixel coordinates, so
that stored payloads are usable independently of attribution.

- Read an expanded window, then fill and smooth in memory without further
  source reads. Overlapping reads for neighbouring windows are acceptable.
  Fill extents may differ between adjacent RF zooms.
- Fill with `GDALFillNodata` (inverse-distance interpolation, no built-in
  smoothing). `--nodata-search-radius` defaults to five RF pixels; zero
  disables filling.
- Pixels beyond the fill's reach receive `--nodata-default-value`, default
  zero. It is a payload value, not a validity sentinel, and participates in
  smoothing. RGB accepts one value for all channels or an `R,G,B` triple.
- Smooth originally missing pixels with a separable Gaussian kernel of size
  `--nodata-smoothing-kernel-size`, default 5, with sigma = (K - 1) / 4. Size
  one disables smoothing. Originally valid values are restored after both
  passes.
- For fill radius R and kernel size K the temporary halo is
  H = R + (K - 1) / 2; seven pixels by default. It is not persisted.
- Keep a tile when its expanded window contains at least one originally
  valid, mask-selected pixel, even if its stored interior has entirely zero
  attribution. Synthetic pixels do not qualify additional tiles.
- RGB uses one validity mask requiring all three channels and fills directly
  on the encoded channel values. Originally valid RGB pixels are preserved.
- Filled and smoothed pixels remain unattributed. Valid values outside the
  mask remain usable donors.
- At the north/south world limits, replicate the border in RF coordinates.
- Nonfinite warped values are treated as missing. An undeclared NaN in the
  source can still affect neighbouring Lanczos results during the warp.

```sh
# Scalar fallback -100, with the default fill radius and Gaussian kernel.
rf-builder gdal ... --nodata-default-value -100

# RGB fallback, radius 3, and no smoothing.
rf-builder gdal ... --mode rgb --nodata-default-value 128,64,32 \
    --nodata-search-radius 3 --nodata-smoothing-kernel-size 1
```

## Tiles module

```sh
rf-builder tiles --provider providers/basemap.json \
    --mask validity.gpkg --output new-rf --attribution-index 1
```

The module imports JPEG/PNG imagery on the regular Web Mercator tile grid as
RGB8. All source settings come from one provider JSON file; there is no
built-in provider lookup or CLI override for them. Region selection uses the
required vector mask.

### Provider JSON

```json
{
  "url_pattern": "https://example.org/tiles/{zoom}/{x}/{y}.jpeg",
  "y_direction": "down",
  "min_zoom": 7,
  "max_zoom": 20,
  "tile_size": 256
}
```

All fields are required; unknown fields are rejected. `url_pattern` is an
HTTP(S) template containing `{zoom}`, `{x}`, and `{y}`. `y_direction` is
`down` for XYZ or `up` for TMS addressing. `min_zoom` and `max_zoom` are
source zoom limits. `tile_size` is the square source side in pixels, a power
of two; decoded images with other dimensions are rejected. The RF tile size
must equal the source side times a power of two.

`providers/basemap.json` and `providers/gataki.json` are shipped with the
repository; both configure zooms 4..20 and 256-pixel tiles. Basemap's minimum
deliberately excludes its available zooms 1..3 so the default 4096-pixel RF
side has a valid starting RF zoom of zero.

### Source discovery and RF selection

Let source side be S, RF side be R, and k = log2(R/S). RF zoom r has the pixel
spacing of source zoom r+k. Discovery starts at RF zoom `min_zoom - k`; a
negative value is rejected.

Within the configured range, a 404 terminates that branch; descendants below
it are never searched. The maximum zoom is a ceiling, not a uniform target:
the deepest available imagery is preserved in each region, so Vienna can
retain finer imagery than the rest of Austria. Provider overzooming is not
detected.

For each candidate RF tile:

1. Consult the RF cache before any source work.
2. Follow source ancestry, respecting 404 pruning, and check whether any
   relevant region has imagery finer than the candidate's matching source
   level. One successful next-level probe is enough to refine.
3. If finer imagery exists, replace the candidate by its relevant RF children
   without writing its payload.
4. Otherwise assemble the candidate from imagery at the matching level plus
   coarser ancestor fallback, apply the mask and attribution, and omit it if
   no pixel is accepted.

Source discovery accounts for every relevant quadrant. Fixed RF dimensions can
require substantial upscaling around a small fine patch; preserving fine data
takes priority over avoiding that expansion. No source pyramid is persisted;
intermediate images are only cached in bounded memory.

### Pixels and fallback

Native-resolution regions copy decoded source values exactly. Enlarged
ancestor fallback uses fixed Lanczos-3 in linear light: decoded RGB is treated
as sRGB without ICC-profile conversion, linearized, interpolated, and encoded
back to sRGB8. Fallback supports zoom gaps up to 30 levels.

Interpolation neighbours are resolved consistently across source and RF tile
edges and may lie outside the mask. At a missing neighbour, ancestor data is
used where possible; at a true coverage edge, available edge samples are
extended. Pixels whose centre has no source coverage do not receive the
import's attribution. Longitude wraps; latitude does not.

All successfully decoded pixels are valid before the mask, including black
pixels.

### Cache and restart

Online input records contain the parsed provider settings, not the file path
or JSON text: moving or reformatting a provider file keeps caches reusable,
changing a setting at the same path does not.

- An indexed RF leaf in the cache is final and linked without rediscovering
  or downloading its source imagery.
- A virtual cache ancestor with cached descendants is refined around them;
  only unfinished regions are planned.
- Missing entries are unfinished or empty and are recomputed.

Cached and newly fetched imagery may therefore reflect different provider
updates; a fresh run without a cache refreshes everything. See the
[cache decision](../adr/0003-online-rf-cache-reuse.md).

### Progress and network errors

Progress reports estimated geographic completion weighted by the area of the
mask's conservative bounds, with remaining time estimated from non-cached
throughput. There is no advance discovery pass, so estimates can change
substantially.

Each failing tile request has a one-hour total retry deadline. Waits start at
500 ms and double; retryable responses (408, 429, selected 5xx) and transient
transport errors are retried, respecting Retry-After. Only 404 means absence.
Timeouts, authentication failures, server errors and undecodable images never
silently select coarser data; exhausted retries abort. Cancellation lets an
active tile finish its current retry deadline.
