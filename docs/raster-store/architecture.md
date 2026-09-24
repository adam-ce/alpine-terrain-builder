# Architecture

This document describes the responsibilities and lifecycle of raster-store
(RS), raster-fundamentalis (RF), tile-base (TB), and the tile-server.
Raster-store provides geospatial raster tile storage for the alpine maps
project on top of the shared store mechanisms also used for 3D meshes.

[Storage format](storage-format.md) defines the directory layout, serialized
data, and format invariants. [Terminology](terminology.md) defines the domain
language, and [sampling and generation](sampling-and-generation.md) covers
sampling and filtering policy.

## Current implementation scope

The raster-store library provides typed tiles, attribution tables, codecs,
persistent index and metadata, snapshot publication, windowed scaling, and
physical-tile halo extraction with neighbour/ancestor fallback. RF importers
are implemented; arbitrary geographic window reads, TB generation, and merging
remain future work.
It remains part of the existing `terrainlib` target.

## Implementation status

So far, a refactor delivered the dimension-neutral mechanisms to index RS:

- `store::Index<Traits>`, `store::traverse`, layouts, runtime codecs, storage,
  typed errors, and cache interfaces;
- `octree::StoreTraits` 3D layout/index/open adapters (for reference only);
- `raster_store::StoreTraits` for in-memory topology keyed by
  `radix::tile::Id`;

The 3D octree stores use versioned envelopes. Raster persistence implementation
and verification progress is tracked in
[implementation-status.md](implementation-status.md).

### Scaling responsibility

The [scaling refactor agreed on 2026-09-22](scaling.md) separates numerical
operations from attribution policy. Generic `raster::algorithm` operations
resample a single raster through ordinary or clamped views; paired
`raster_store::scaler` wrappers compose those operations for data and
representative attribution. Zero attribution means unattributed and does not
mask numerical inputs. Callers must prepare usable payloads before scaling.
Paired `scale` accepts `raster_store::pixel::Mapping` from the shared
`raster_store/pixel.h`; paired `reduce` and generic algorithms receive
decoder/encoder tuples. Paired `reduce` defaults to identity conversion.
The same header provides `pixel::identifier<T>()` with its `Format` machinery
in `pixel::detail`, replacing the former `pixel_type.h` API. Snapshot metadata
uses the shared `pixel::Mapping` type as well.
Source NoData handling stays with importers, while physical-source selection
and missing-coverage replication belong to the halo extractor.

### Raster storage API

Include `raster_store/storage.h`. `raster_store::storage::create<PixelType>()`
takes the final snapshot path and creates its sibling `.part` directory.
The attribution table must already be available through the agreed lookup,
or `CreateOptions::copy_attribution_from_index` can identify an input index
whose selected table is copied unchanged into the output.

```cpp
raster_store::storage::CreateOptions options;
options.nominal_tile_size = 64;
options.halo_width = 0;
auto created = raster_store::storage::create<float>(snapshot_path, options);
if (!created) {
    return Error::propagate(std::move(created));
}
auto [output, metadata] = std::move(*created);
raster_store::Tile<float> tile(metadata->stored_tile_size); // Attribution defaults to zero (unattributed).
if (auto saved = output->save({ 0, { 0, 0 } }, tile); !saved) {
    return saved;
}
return raster_store::storage::publish(std::move(output));
```

Both factories return `Expected<std::pair<...>>` with owning `std::unique_ptr`s
to the storage first and metadata second. `create<PixelType>()` returns writable
`IndexedStorage<PixelType>` and const metadata; `open<PixelType>()` returns const
storage and const metadata. Both pointers are non-null on success and can be
moved independently.

Call the non-const `save_index()` explicitly for intermediate checkpoints on
created storage. Pass `{ .allow_incomplete = true }` to `open<PixelType>()` for
read-only cache access to a `.part` snapshot. `attribution::read_table(index_path)`
provides the selected table separately. `publish()` consumes the writable storage
pointer on success or failure and returns `Expected<void>`; it rejects null
pointers. The metadata pointer remains available after publication.

Raster opening supplies the decoded metadata to the shared `store::open_index`
overload. The shared opener resolves the metadata's codec selector through
the supplied callback; raster codec names and dimensions are handled in
`raster_store`, while mesh callers retain their extension resolver.

Manifest I/O lives in `raster_store/io/manifest.h` under
`raster_store::io::manifest`. `read_metadata(base_path)` and
`write_metadata(metadata, base_path)` read and write the snapshot metadata file.
`raster_store/io/TileCodec.h` defines
`raster_store::io::TileCodec<PixelType>` and its supporting `tile_codec`
namespace. Versioned payload structs belong to `manifest::detail::v1` and
`tile_codec::detail::v1`, respectively. The XYZ layout lives in
`raster_store/path_layout.h` under `raster_store::path_layout::zoom_xy_google`.
Both raster and octree index decoding use `store::Index<Traits>::validate() const`
to validate the decoded hierarchy.

### Halo reads

Include `raster_store/read_tile_with_halo.h` and call:

```cpp
auto tile = raster_store::read_tile_with_halo(*storage, *metadata, tile_id,
    2, raster::algorithm::Interpolation::Bilinear);
```

The result is an ordinary `Tile<T>` with a two-pixel halo. The centre must be
physical. Stored halos are cropped exactly; zero-halo snapshots resolve
same-zoom neighbours, descend at most four levels, and use physical ancestors
for remaining regions. Uncovered data replicates the central edge with zero
attribution. Source assignment precedes payload reads, so each source is read
at most once. See [Tiles with halo](tiles-with-halo.md) for the complete contract.

### Final public names

The shared API uses `store::NodeStatus`, `store::NodeStatusOrMissing`,
`store::Index<Traits>`, `store::traverse`, `store::RawStorage`,
`store::Storage`, and `store::IndexedStorage`. 

A 3D mesh dataset is opened through the 3D adapter (for reference):

```cpp
#include "mesh/storage.h"

auto opened = mesh::storage::open_folder_indexed(dataset_path);
if (!opened.has_value()) {
    return std::unexpected(opened.error());
}
mesh::storage::IndexedStorage storage = std::move(opened.value());
```

The 2D traits adapter can exercise the shared topology without implying a
persistent RF format:

```cpp
#include "raster_store/StoreTraits.h"
#include "store/Index.h"
#include "store/traverse.h"

store::Index<raster_store::StoreTraits> index;
const radix::tile::Id tile{2, {1, 3}};
auto added = index.add(tile);
auto walked = store::traverse(index, [](const auto &id, store::NodeStatus status) {
    // In-memory hierarchy processing only.
});
```

## System boundary

The design separates authoritative data management from delivery generation:

```text
Input rasters (e.g. GDAL)
    │
    │ inspect, transform to Web Mercator, define source, one source per pixel -> rf_builder
    ▼
raster-fundamentalis (one rf per source at the beginning)
    │
    │ rf_merger: merge two rf stores based on attribution priority list
    ▼
Authoritative rf raster store
    │
    │ generate overviews using defined filtering strategy.
    ▼
tile-base store (one per layer, one per data version. user visible server should only need one version per layer)
    ├── read by tile-server
    ├── area/vertex pixel tile generation
    └── select resolution, type etc by url
```

## Dataset organization

RF and tile-base use the same storage format but live under separate roots.
Their different data-selection and hierarchy policies belong to their tools.
Additional metadata can be introduced later using the same envelope mechanism.
See the [directory layout](storage-format.md#directory-layout) for filenames
and the [attribution lookup rules](storage-format.md#source-attribution-table).

RF snapshots normally share an attribution table. TB construction copies that
table unchanged into the output snapshot, preserving indices. This copy is
independent of the mutable RF table, so later edits or RF removal do not
affect it. The complete TB can then be transferred by rsync as one directory.
An RF snapshot using an ancestor table depends on that external table.

The source table is maintained manually. Builders and overview generators
consume entries without editing or renumbering them. A user may correct an
entry or clear its field values while retaining the complete object. Reusing
an index requires the user to establish that no retained snapshot using that
table refers to it; the library does not track this lifecycle. The required
entry structure is specified in the storage format. See also the
[shared-table decision](../adr/0001-shared-raster-attribution-table.md).

## Snapshot lifecycle

Datasets may be several TiB in size, so builds and merges can take days or
weeks. The RF builder, RF merger, and TB builder follow these policies:

- Every build or merge generates a new snapshot; published snapshots are
  never mutated.
- Errors abort the build.
- Builders checkpoint the index at least every few minutes so an aborted
  build remains readable as a cache.
- Callers may supply a cache store from an earlier or aborted run to skip
  processing nodes that can be reused.
- Reusable tiles are hard-linked to limit storage cost.

Because hard links share inodes, a linked container must never be opened for
in-place modification. Obsolete snapshots can be deleted; other hard links
keep the data alive.

Published snapshots are opened through `std::unique_ptr<const IndexedStorage>`;
normal access permits reads but not tile mutations or index checkpoints.
Created snapshots retain writable storage until publication consumes it.

Hard-link reuse may cross RF and TB roots. The calling tool decides whether
a tile can be reused, including payload type, dimensions, encoding, and
attribution-index compatibility.
Storage performs the requested operation and reports operational failures;
it does not add semantic compatibility checks to decide whether reuse is
valid. A TB with different tile dimensions needs newly generated tiles
wherever existing tiles cannot be reused unchanged. Linked TB payloads and
the independent attribution table remain valid after removing the RF.

### Opening and checkpoints

Normal opening rejects `.part` snapshot directories. The explicit
`allow_incomplete` option opens one as const `IndexedStorage` for cache reuse, using
the last saved index and ignoring unindexed files. Checkpoint timing belongs
to the builder, which explicitly saves the index periodically.

Checkpoints replace the index through a temporary file so a failed write does
not truncate the previous checkpoint. The shared storage destructor may also
save a dirty index; destruction never publishes a snapshot. Checkpointing and
publication require the caller to finish outstanding writes first.

Opening and publication do not perform an additional whole-snapshot
validation pass or scan payload files for existence. Errors from envelope
decoding and the normal format-adapter/opening path are propagated when the
index or a tile is read. Generic envelope checks cover the serialized
representation, version, compression, and checksum; index invariants belong
to the format adapter. The attribution table is parsed separately as JSON.

### Publication

A new snapshot is assembled in a sibling directory named
`<snapshot-id>.part`. Publication follows this protocol:

1. Write all payload and metadata files into the `.part` directory.
2. Write the final index last and flush and close the output files.
3. Rename the directory to `<snapshot-id>` on the same filesystem.

Publication returns success after the rename without reopening the snapshot.
The current publication implementation uses Linux `renameat2` with
`RENAME_NOREPLACE` to reject destination collisions at the rename itself.
Other platforms currently report `Unsupported` for publication.

The final destination must not already exist. A `.part` directory is
incomplete and is never considered published. The rename removes the suffix;
there is no separate marker or manifest. During normal operation this gives
readers atomic visibility: they see either no final snapshot or the completed
one.

Cross-filesystem publication is unsupported because the final rename and any
hard links must remain on one filesystem. A builder or merger must reject that
configuration before starting a long operation.

Publication does not guarantee durability or safe recovery across a power
failure, operating-system crash, or storage failure. Flushing and closing
files before the rename is required for normal-operation correctness, but is
not a crash-durability guarantee. The implementation does not require
`fsync()`, `fdatasync()`, `FlushFileBuffers()`, or equivalent
platform-specific synchronization. After such a failure, either a `.part`
directory or a final snapshot may be unusable and must be validated or
rebuilt.

## Tools

### rf_builder

Raster-fundamentalis is the worldwide authoritative dataset, assembled from
sources with different coverage and accuracy. RF has no downsampled overview
levels. Coarse physical tiles, for example at zoom 10, may coexist with more
accurate descendants, for example at zoom 15.

The implemented [RF builder](rf-builder-design.md) consumes one prepared GDAL
dataset, including a VRT mosaic, through `rf-builder gdal`. The
[online module](rf-builder-downloader-design.md) imports a provider-defined JPEG/PNG
tile pyramid through `rf-builder tiles`. Each builder import produces disjoint
physical leaves, without physical parent/descendant overlap. An import
supplies a data source, an existing attribution index, and a vector validity
mask defining the accepted region. The resulting RF has one attribution.
Several imports with different masks may share the same attribution index;
masks are not retained after RF creation.

The RF builder must validate its attribution index against the supported range
and selected table before producing tiles. Tile storage does not scan
attribution rasters on reads or writes to repeat this validation.

Implementation: `src/rf_builder/*`, executable `rf-builder`.

### rf_merger

The merger combines two RF stores using an attribution priority list. It
reuses tiles from the preferred source where possible and resolves conflicts
per texel where necessary. New boundary tiles use the higher resolution of
the inputs.

### tb_builder

Tile-base adds overviews to RF and retains the data. Every level is occupied
and selects an adequate source. It must support at least minimum, maximum, and average
aggregation. A TB may choose different tile dimensions from its RF input.
See [sampling and generation](sampling-and-generation.md) for filtering and
source-selection policy.

Planned location: `src/tb-builder`.

### tile_server

The server consumes tile-base and generates renderer tiles of the requested
dimensions, sampling placement, and encoding on the fly. Requests identify
these choices in the URL, for example `layer/vertex|area/resolution/z/x/y.ending`.
Output dimensions may be 65px or 256px, with area or vertex pixels.

Delivery encoding is not yet finalized. Required options include JPEG, PNG,
compressed ETC2/DXT1, and compressed raw rasters. Some layers combine several
tile-bases, such as DTM and DSM, or percentage larger than slope angle (PLaTSA),
Phong shading, and AO shading. An ECEF bounding-sphere tree is planned later.

Planned location: `src/tile-server/*`.
