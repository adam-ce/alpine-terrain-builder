# Architecture

This document describes the responsibilities and lifecycle of raster-store
(RS), raster-fundamentalis (RF), tile-base (TB), and the tile-server.
Raster-store provides geospatial raster tile storage for the alpine maps
project on top of the shared store mechanisms also used for 3D meshes. It is
part of the `terrainlib` target.

[Storage format](storage-format.md) defines the directory layout, serialized
data, and format invariants. [Terminology](terminology.md) defines the domain
language, and [sampling and generation](sampling-and-generation.md) covers
sampling and filtering policy.

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
keep the data alive. Published snapshots are opened read-only.

Hard-link reuse may cross RF and TB roots. The calling tool decides whether
a tile can be reused, including payload type, dimensions, encoding, and
attribution-index compatibility. Storage performs the requested operation
and reports operational failures; it does not add semantic compatibility
checks. A TB with different tile dimensions needs newly generated tiles
wherever existing tiles cannot be reused unchanged. Linked TB payloads and
the independent attribution table remain valid after removing the RF.

### Opening and checkpoints

Normal opening rejects `.part` snapshot directories. An explicit option opens
one read-only for cache reuse, using the last saved index and ignoring
unindexed files. Checkpoint timing belongs to the builder, which explicitly
saves the index periodically.

Checkpoints replace the index through a temporary file so a failed write does
not truncate the previous checkpoint. Destruction may save a dirty index but
never publishes a snapshot. Checkpointing and publication require the caller
to finish outstanding writes first.

Opening and publication do not perform an additional whole-snapshot
validation pass or scan payload files for existence. Errors are reported when
the index or a tile is read.

### Publication

A new snapshot is assembled in a sibling directory named
`<snapshot-id>.part`. Publication follows this protocol:

1. Write all payload and metadata files into the `.part` directory.
2. Write the final index last and flush and close the output files.
3. Rename the directory to `<snapshot-id>` on the same filesystem.

Publication returns success after the rename without reopening the snapshot.
The rename uses Linux `renameat2` with `RENAME_NOREPLACE` to reject
destination collisions; other platforms currently report `Unsupported`.

A `.part` directory is incomplete and is never considered published. There is
no separate marker or manifest. During normal operation this gives readers
atomic visibility: they see either no final snapshot or the completed one.

Cross-filesystem publication is unsupported because the final rename and any
hard links must remain on one filesystem. A builder or merger must reject that
configuration before starting a long operation.

Publication does not guarantee durability or safe recovery across a power
failure, operating-system crash, or storage failure; there is no `fsync()`.
After such a failure, either a `.part` directory or a final snapshot may be
unusable and must be validated or rebuilt.

## Tools

### rf_builder

Raster-fundamentalis is the worldwide authoritative dataset, assembled from
sources with different coverage and accuracy. RF has no downsampled overview
levels. Different regions may use different RF zoom levels. RF imports and
merger output use disjoint physical leaves, without physical
parent/descendant overlap. The shared storage format still permits overlapping
physical ancestors and descendants for other uses.

The [RF builder](rf-builder-design.md) imports one prepared GDAL dataset,
including a VRT mosaic, through `rf-builder gdal`, or a provider-defined
JPEG/PNG tile pyramid through `rf-builder tiles`. An import supplies a data
source, an existing attribution index, and a vector validity mask defining
the accepted region. The resulting RF has one attribution. Several imports
with different masks may share the same attribution index; masks are not
retained after RF creation.

Implementation: `src/rf_builder/*`, executable `rf-builder`.

### rf_merger

The merger accepts two disjoint RF snapshots and produces disjoint
physical leaves, selecting attributed pixels by an attribution priority list.
The output partition follows input topology: wherever either input has a
physical leaf, the output is at least that fine, and surviving coarse data is
resampled into the finer leaves. See the [merger design](rf-merger-design.md).

Disjoint output keeps one physical representation over each covered region,
following the RF builders' output policy. Retaining physical parents beneath
finer descendants would preserve coarse values and avoid some resampling,
but would require interpreting overlapping representations. We accept that
repeated merges can resample previous results and that different merge
groupings can produce different numeric values. Partitioning by topology
rather than by selection outcome avoids a payload-reading planning pass, at
the cost of refining regions where a finer input wins no pixel. Discarded
sources cannot be recovered from the output; changing priorities requires
rebuilding from the original imports.

Implementation: `src/rf_merger/*`, executable `rf-merger`.

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
