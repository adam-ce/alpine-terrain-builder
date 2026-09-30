# RF merger design

Status: implementation proposal, reconciled with shared functionality on
2026-09-26. Shared-function reuse and support-coverage preservation are agreed.
Whole-tile attribution precedence was agreed on 2026-09-27; implementation
is authorized following the documentation commit. Topology-only output
partitioning, output codec checks and recovery fingerprints were agreed on
2026-09-28. Implemented on 2026-09-28; see
[implementation status](implementation-status.md#rf-merger--2026-09-28).

This design uses the code on `main` at `2be4343`.
[Storage format](storage-format.md) defines the shared RF/TB format;
[architecture](architecture.md#rf_merger) records the rationale and tradeoffs
for disjoint merger output.

## Policy

### Inputs and attribution

Merge two published RF snapshots into a new snapshot. Initially support
float32 scalar and RGB8 payloads. Require matching payload types, nominal
pixel dimensions and `value_mapping`; different RF zoom levels are supported.
Each input's codec selector must equal the output codec; initially the output
uses the default `amort` codec, so this cannot fail yet. Input layout and
compression may differ from the output.
RF inputs and output have zero stored halo and follow the shared
[tile-dimension contract](tiles-with-halo.md). Reject unsupported types,
metadata or codec mismatches, and physical `Inner` nodes: inputs must have disjoint
physical leaves. Virtual ancestors remain valid traversal nodes. Accept
`.part` snapshots only through the recovery-cache option.

The caller guarantees that attribution IDs are valid and have consistent
meaning across both inputs, the output and any recovery cache. Preserve IDs
without renumbering. The merger does not look up or compare source-attribution
tables or validate IDs against their entries. Existing storage opening and
creation requirements still apply. Several imports may share an attribution.

The caller supplies a priority table as a JSON array of attribution IDs,
highest priority first, for example `[7, 3, 12]`. Reject duplicates, index 0
and IDs outside the supported range. Each listed ID has a distinct rank.
The list may be empty or omit IDs present in the inputs; omitted IDs share
one rank below all listed IDs.

### Pixel selection and output hierarchy

Attribution 0 means unattributed. Classify each original input tile by whether
it contains any nonzero attribution. A tile with attribution wins as a whole
over a tile with no attribution, regardless of zoom, including its unattributed
pixels. If neither tile has attribution, select the finer tile, then the right
input at equal zoom.

When both original tiles have attribution, compare pixels using the attribution
at the source pixel containing each comparison centre. Prefer nonzero attribution,
then priority rank, higher original RF zoom, and finally the right input.
Equal attribution and two unlisted IDs are equal-rank cases. Where both pixel
attributions are zero, prefer higher zoom, then the right input, on the selected
output grid. A missing physical supplier contributes no candidate. Selection
is deterministic for fixed ordered inputs and settings.

Produce disjoint physical leaves, partitioned from input topology alone,
regardless of attribution or selection outcome. Wherever either input has a
physical leaf, the output is at least that fine: split a covering coarser
leaf of the other input along the paths to its finer descendants and retain
the needed siblings. Never retain a physical parent alongside descendants.
Output leaves are therefore never coarser than an input leaf at the same
location, and production never downscales. Resample surviving coarse data
into finer leaves where needed.

A fine tile that wins no pixel still refines the output; its leaf and the
split siblings are filled from the coarser supplier. We accept the larger
output to avoid a payload-reading planning pass and undoing speculative
subdivision; few entirely unattributed tiles are expected. Tile-accurate
pruning from stored per-tile attribution sets could later reduce this
without changing pixel selection, but it requires a storage-format extension
and is deferred.

Preserve physical support coverage, including entirely unattributed tiles
prepared by the [GDAL importer](gdal-nodata-filling.md#agreed-decisions).
Support-only tiles take part in partitioning like any other tile.
Synthesized halo pixels alone do not create additional output coverage.

### Shared raster operations

Use windowed [`raster_store::scaler::scale`](../../src/terrainlib/raster_store/scaler.h)
with fixed `raster::algorithm::Resampling::Lanczos3`, passing the input metadata's
value mapping and preserving it in output metadata. Obtain support from the
corresponding input snapshot with
[`read_tile_with_halo`](../../src/terrainlib/raster_store/read_tile_with_halo.h).
Delegate filtering, conversion, alignment and representative attribution to
the [scaling contract](scaling.md), and neighbour selection, fallback and
boundary treatment to the [halo contract](tiles-with-halo.md).

Attribution-zero samples contribute numerically; centre eligibility follows
the selection policy above. The shared
[numerical preconditions and exact-copy behavior](scaling.md#conversion-tuples-and-numeric-behavior)
apply; add no merger-specific nonfinite validation or propagation guarantee.
Subsequent merges rank a resampled value by its stored representative
attribution, as specified in the
[attribution decision](../adr/0004-representative-attribution-for-halo-samples.md).

Use the [windowed scaling interface](scaling.md#agreed-windowed-scaling-extension)
and its `required_halo` / `required_source_window` queries for bounded
preparation and evaluation. The merger supplies tile-to-window geometry,
following the halo reader's distant-ancestor preparation for local phase and
preserving extracted support when creating subviews. Observe shared geometry
and allocation limits; do not allocate a full zoom-expanded source tile.
Scale original input suppliers directly to the required grid, not through
intermediate child results. Repeated merges may resample previous outputs;
merge grouping need not preserve numerical results. No original-source pyramid
or exhaustive per-pixel contribution history is retained.

Use [raster views and algorithms](../raster-view-algorithms.md) for pixel
operations: `copy` for native regions within newly assembled tiles,
`transform` / `zip_transform` for pointwise selection, and
[`fold`](../../src/terrainlib/raster/algorithm/fold.h) for winner summaries.
Transform callbacks remain pure; validation and accumulation occur outside
them. Filter kernels remain owned by shared scaling.

### Whole-tile reuse

Hard-link an unchanged whole input tile with
[`Storage::copy_from()`](../../src/terrainlib/store/Storage.h) when the output
key and grid permit reuse of its complete data and attribution payload.
All-left or all-right selection at the same native grid qualifies; resampling
entirely from one coarse supplier does not. Reuse depends only on codec
compatibility, which the input check guarantees; payload path, layout and
envelope compression may differ from the output's. Compatible completed
recovery tiles use the same hard-link mechanism.

All other tiles are newly encoded. A matching extension alone does not
establish compatibility. Never modify a linked payload in place. Hard-link
failure is explicit, with no silent file-copy fallback; report read, write
and link failures with key/path context.

### Recovery compatibility

Recover into a new output using completed, indexed tiles from an explicitly
supplied compatible `.part` snapshot. Do not mutate the old output. Initially
reject published snapshots as recovery caches. Ignore unindexed payloads.

Validate the recovery record against ordered input paths, metadata/index
fingerprints, ordered priority IDs and processing settings. An input's
fingerprint identifies the dataset: its canonical snapshot path and the
payload hashes recorded in the envelope headers of its `raster_store.metadata`
and `raster_store.index` files. Trust published
payloads to remain immutable at their paths; do not hash or scan every payload
for cache validation. Compare priority IDs rather than JSON formatting.
Source-attribution table contents are not part of cache identity.

Record pixel type, nominal/stored dimensions, halo width, resampling method,
value mapping, centre-selection and partitioning policies, and a semantic
processing version covering the shared scaling and halo contracts. Output
path, compression, job count and logging do not affect compatibility. Small rounding
differences are acceptable, including cache reuse and equivalent Lanczos-3
implementations; only semantic changes invalidate otherwise compatible tiles.

## Proposed implementation

### 1. Command and preflight

Add `src/rf_merger` with an `rf-merger` executable and a small library target
for tests, following CLI11 and the existing executable/library structure.
Use `ALP_BUILD_RF_MERGER` and `unittests_rfmerger`. Keep command parsing,
validation/recovery records, planning, source-window preparation, selection
and execution within the merger; do not introduce a generic merge framework.

```sh
rf-merger --left /data/rf/a --right /data/rf/b \
    --priorities /data/priority.json --output /data/rf/merged \
    --jobs 1
```

`--cache /data/rf/interrupted.part` is optional; `--jobs` defaults to one.
Optional `--compression` selects the output envelope compression and
defaults to the storage default, standard Zstandard. Output tiles always use
the storage default hash, XXH3-64; there is no hash option.
Infer payload type and tile dimensions from input metadata. The output uses
the default layout and codec; add layout and codec options only when
alternatives exist. There are no mask, resampling-method, type-conversion or
output-dimension options.

Open inputs through existing storage functions, retaining their read-only
metadata, and validate the [input policy](#inputs-and-attribution). Delegate
metadata/index/codec parsing and validation to existing modules; check
allocation and geometry limits before narrowing dimensions or zoom arithmetic.
Require absent final and `.part` output paths. Preflight hard-link support
from both inputs and any recovery cache to the output filesystem.

Check any recovery index and record against
[recovery compatibility](#recovery-compatibility); missing, corrupt or
mismatching records abort. Then create the new `.part` snapshot and write
`inputs.tmp` through `io::envelope`, with a merger-specific schema/class
identifier, before producing payloads. A builder record is not a merger record.

### 2. Plan the output partition

Walk the two sparse hierarchies in spatial depth-first order, carrying a
covering coarse leaf into descendants while following the other tree. Each
overlap region has at most one physical supplier from each input; an absent
descendant key does not negate coverage from a coarse leaf.

Derive the [partition](#pixel-selection-and-output-hierarchy) from the two
indices without reading payloads. A key is an output leaf when at least one
input supplies it at the same or a coarser zoom and neither input has a
physical tile strictly below it. Where one input has finer physical
descendants, descend towards them and emit the siblings that are still
covered. Emit leaves lazily to production together with their original
suppliers; do not materialize the full partition. This does not require a
uniform-resolution raster or a full quadtree expansion.

Seed planning with compatible completed recovery leaves and exclude their
coverage from new work. A virtual cache ancestor with completed descendants
must prevent an overlapping parent output. Uncached empty regions may be
recomputed.

### 3. Produce tiles

For each planned leaf, locate its original suppliers and apply the selection
policy on the final grid. Take the [whole-tile reuse](#whole-tile-reuse) path
when eligible, without assembling or encoding a replacement. Otherwise use
shared raster operations to prepare candidate windows and select data and
attribution into the new tile. Read each supplier that must be resampled,
together with the halo required by the windowed scaler, from its input
snapshot with
[`raster_store::read_tile_with_halo`](../../src/terrainlib/raster_store/read_tile_with_halo.h);
native suppliers need no halo. Where both suppliers are present, align both
to the leaf grid, using the [paired scaler](#shared-raster-operations) on the
halo tiles when resampling is needed and native rasters/views otherwise, and
apply the [selection policy](#pixel-selection-and-output-hierarchy) with
`zip_transform`. This prepares data and attribution together.

By construction, every original supplier of a leaf covers it at the same or a
coarser zoom. Merger production therefore needs only native copies or
upscaling; internal halo extraction follows its own shared contract. Output
eligibility comes from the partition, not an attributed-pixel count, so
support-only leaves and leaves won entirely by a coarser supplier are
written too.

Use ordinary storage reads. The traversal may favor locality, but do not
implement a bespoke decoded-tile cache. If profiling later identifies disk
traffic as a bottleneck, address it through the external cache work.

The generic store already supplies exact-key reads, sparse topology, traversal
and hard links. Use `store::traverse` and `Storage::copy_from()` directly,
as `sf_merger` does. Do not extract a shared subtree-copy helper now; revisit
that only if both mergers need substantially the same coordination loop.
Keep the mesh-specific merge driver separate.

### 4. Execute and publish

Move the existing [`rf_builder::TilePool`](../../src/rf_builder/TilePool.h)
to `terrainlib/raster_store/TilePool.h` as `raster_store::TilePool`, and use
it from both RF builder and RF merger. Preserve its payload template, raster
tile keys, scheduling, stop and error behavior. Adapt builder includes and
namespace references without changing its behavior. Do not generalize the
builder coordinator or introduce a separate merger worker pool.

Use private read/scaling state per worker and bound queued, active and completed
jobs together to at most twice the worker count. Indices and the traversal
frontier may grow with the hierarchy; payload buffers must not grow with
snapshot size. One coordinator owns writes, hard links, index mutation,
checkpoints and progress, completing each payload operation before indexing it.

Use RF import as the reference for bounded scheduling and publication; error
and interruption handling follow the lifecycle below. Its coordinator assumes one attribution entry;
do not change builder behavior to accommodate merger selection or tile reuse.

Log to stderr and `<output>.log`. An index-only count of output leaves may
precede production to provide totals. Report completed/reused leaves during
production, with elapsed time, periodic updates during long work, and a
labelled ETA when enough production data exists.

On success, report output statistics per category: whole left tiles
hard-linked, whole right tiles hard-linked, and newly encoded tiles with
pixels from both inputs, only the left input or only the right input. A large
single-input count indicates that the other input caused subdivision without
winning. For each category report the tile count, its percentage of all output
tiles, the stored payload bytes and their percentage of all output bytes.
Linked tiles count their full payload size, labelled as shared with the input.
Show all byte values in one binary unit, MiB, GiB or TiB, chosen from the
total. Derive the pixel origin from the selection result, without extra reads.

Checkpoint completed writes at the existing two-minute target. Each checkpoint
writes the statistics totals of the indexed tiles to `statistics.tmp` in the
`.part` snapshot through `io::envelope`, with a merger-specific schema, before
the index. Like the index, write it to a temporary file and rename it into
place. Recovery restores these totals for the reused cache tiles, which keep
their original categories. The two files are not updated atomically together;
a small discrepancy after abnormal termination is acceptable. A missing or
corrupt statistics file does not abort recovery; warn and report the
statistics as incomplete.

On SIGINT or SIGTERM, stop scheduling, finish and save the active tiles,
checkpoint, and throw an `Error::Exception` with code `Cancelled`; the command
exits with 128 plus the signal number and retains the incomplete output and
recovery record. A process killed without this handling keeps the tiles of the
last checkpoint, because the index is replaced atomically and unindexed
payloads are ignored. Errors that the merger
cannot recover from, including a failure to read or hard-link a cached tile,
throw `Error::Exception`: unwinding joins the workers, writes the statistics
and then the index of the completed tiles, and nothing is published. Bugs fail
assertions and abort. On success, finish active work, remove the input record
and statistics file immediately before publication and use existing
final-index writing and no-replace rename. Empty results follow the same
lifecycle. The existing normal-operation guarantee applies; no
crash-durability mechanism is added.

## Verification checklist

Existing scaler and halo suites own filter arithmetic, conversion, alignment,
window phase, allocation behavior and neighbour lookup. Add merger integration
coverage without duplicating those suites:

- **Inputs:** empty/partial priority lists; duplicate, zero and out-of-range
  IDs; payload/dimension/mapping/codec mismatches; physical `Inner` rejection;
  storage-opening failures and destination clashes.
- **Scaling integration:** original-supplier selection; native-value
  preservation; metadata mapping; centre eligibility and representative
  attribution; interior/edge windows, subpixel regions and large zoom gaps;
  bounded output and agreement with direct shared-function calls.
- **Selection and partitioning:** partition derived without payload reads;
  fully overridden fine tiles still refining the output and filled from the
  coarse supplier; a single winning fine pixel; equal-attribution and unlisted
  ties; coarse holes filled by fine data; asymmetric deep branches; split
  siblings filled from the coarse supplier; empty inputs.
- **Support:** entirely unattributed snapshots and ties; support surrounding
  attributed leaves; fine support below an attributed coarse tile refining the
  output without contributing pixels; whole-tile precedence of the attributed
  coarse tile, including its unattributed pixels; retained support available
  to later halo reads and zero-attribution values participating in filtering.
- **Production and reuse:** mixed-source tiles; hard-link identity for unchanged
  attributed and support-only tiles, including with differing input and output
  compression; newly encoded resampled tiles; scalar/RGB
  serial-versus-parallel agreement within the accepted tolerance; bounded
  outstanding work and payload-before-index ordering; statistics categories,
  percentages and unit selection. Run existing RF builder
  parallel/cancellation/error regressions after moving the shared pool.
- **Recovery and lifecycle:** identity changes for input order, metadata/index,
  priorities and semantic/partitioning policies; rounding-only compatibility;
  completed attributed/support leaves, unfinished siblings and unindexed files;
  read/link/write errors; cancellation and recovery after a failure, including restored,
  missing and corrupt statistics; empty publication and
  collisions at publication.

Run the focused merger suite and affected storage/RF regressions using this
terrain-builder repository's existing native CMake configuration. The
renderer-only `dev_driver.py` does not support this repository, as recorded in
[implementation status](implementation-status.md). Run Qt C++ lint on new code,
format new sections with `clang-format-21`, and verify Git-attribute line endings.
Update architecture/status documentation after implementation and verification.
