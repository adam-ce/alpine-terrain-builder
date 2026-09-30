# RF merger design

`rf-merger` combines two RF snapshots into a new one, selecting pixels by an
attribution priority list. [Storage format](storage-format.md) defines the
shared RF/TB format; [architecture](architecture.md#rf_merger) records the
rationale and tradeoffs for disjoint merger output.

```sh
rf-merger --left /data/rf/a --right /data/rf/b \
    --priorities /data/priority.json --output /data/rf/merged \
    --jobs 1
```

`--cache <interrupted>.part` resumes from an incomplete earlier run.
`--compression` selects `zstd` (default), `zstd-best` or `none`. Payload type,
tile dimensions and value mapping are inferred from the inputs.

## Inputs and attribution

Merge two published RF snapshots. Float32 scalar and RGB8 payloads are
supported. Payload types, nominal pixel dimensions, `value_mapping` and codec
must match; RF zoom levels, layout and compression may differ. Inputs have
zero stored halo and must have disjoint physical leaves: physical `Inner`
nodes are rejected.

The caller guarantees that attribution IDs have consistent meaning across
both inputs, the output and any recovery cache. IDs are preserved without
renumbering; the merger does not read source-attribution tables.

The priority table is a JSON array of attribution IDs, highest priority first,
for example `[7, 3, 12]`. Duplicates, index 0 and out-of-range IDs are
rejected. The list may be empty or omit IDs present in the inputs; omitted IDs
share one rank below all listed IDs.

## Pixel selection

Attribution 0 means unattributed. Classify each original input tile by whether
it contains any nonzero attribution. A tile with attribution wins as a whole
over a tile with no attribution, regardless of zoom, including its unattributed
pixels. If neither tile has attribution, select the finer tile, then the right
input at equal zoom.

When both original tiles have attribution, compare pixels using the attribution
at the source pixel containing each comparison centre. Prefer nonzero
attribution, then priority rank, higher original RF zoom, and finally the
right input. Where both pixel attributions are zero, prefer higher zoom, then
the right input. A missing physical supplier contributes no candidate.
Selection is deterministic for fixed ordered inputs and settings.

## Output hierarchy

Produce disjoint physical leaves, partitioned from input topology alone,
regardless of attribution or selection outcome. Wherever either input has a
physical leaf, the output is at least that fine: split a covering coarser
leaf of the other input along the paths to its finer descendants and retain
the needed siblings. Output leaves are therefore never coarser than an input
leaf at the same location, and production never downscales.

A fine tile that wins no pixel still refines the output; its leaf and the
split siblings are filled from the coarser supplier. We accept the larger
output to avoid a payload-reading planning pass. Tile-accurate pruning from
stored per-tile attribution sets could later reduce this without changing
pixel selection, but it requires a storage-format extension and is deferred.

Physical support coverage is preserved, including entirely unattributed tiles
prepared by the [GDAL importer's NoData filling](rf-builder-design.md#nodata-filling).
Support-only tiles take part in partitioning like any other tile.

## Resampling

Coarse suppliers are upscaled with fixed Lanczos-3 through the shared
[scaling rules](sampling-and-generation.md#implemented-scaling-rules), using
the input's value mapping. Support beyond a tile edge comes from the
[halo reader](tiles-with-halo.md). Original input suppliers are scaled
directly to the required grid, never through intermediate results.
Attribution-zero samples contribute numerically. Subsequent merges rank a
resampled value by its stored representative attribution, as specified in the
[attribution decision](../adr/0004-representative-attribution-for-halo-samples.md).

## Whole-tile reuse

An unchanged whole input tile is hard-linked when the output key and grid
permit reuse of its complete data and attribution payload: all-left or
all-right selection at the same native grid qualifies, resampling from one
coarse supplier does not. Input compression may differ from the output's.
All other tiles are newly encoded. Hard-link failure is an error, with no
silent file-copy fallback.

## Recovery

Recovery reuses completed, indexed tiles from an explicitly supplied `.part`
snapshot of an interrupted merge; published snapshots are rejected as caches.
Its record must match the ordered input paths, input fingerprints, ordered
priority IDs and the settings that affect pixels. An input's fingerprint is
its canonical path plus the payload hashes in the envelope headers of its
`raster_store.metadata` and `raster_store.index`; published payloads are
trusted to stay immutable. Output path, compression, job count and logging
do not affect compatibility. Small rounding differences are acceptable; only
semantic changes invalidate otherwise compatible tiles.

## Execution and reporting

The merger shares `raster_store::TilePool` and the bounded scheduling,
checkpointing, cancellation and publication behavior of the
[RF builder](rf-builder-design.md#execution-cancellation-and-reporting).
Cancellation exits with status 128 plus the signal number.

Each checkpoint also writes merge statistics to `statistics.tmp`; recovery
restores them for reused tiles. A missing or corrupt statistics file only
makes the reported statistics incomplete.

On success, the report lists per category the tile count, stored bytes and
their percentages: whole left tiles hard-linked, whole right tiles
hard-linked, and newly encoded tiles with pixels from both inputs, only the
left or only the right. A large single-input count indicates that the other
input caused subdivision without winning.
