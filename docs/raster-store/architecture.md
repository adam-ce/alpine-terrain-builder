# Architecture

## System boundary

The design separates authoritative data management from delivery generation:

```text
Input rasters
    │
    │ inspect, transform to Web Mercator, select source per pixel
    ▼
Raster-store builder
    │
    │ writes a new immutable snapshot
    ▼
Authoritative raster store
    │
    ├── payload chunks
    ├── per-pixel source maps
    ├── source catalog
    └── sparse quadtree index
    │
    │ reconstruct, filter, sample, encode
    ▼
Pyramid generators
    ├── area-pixel texture pyramids
    └── vertex-pixel height/geometry pyramids
```

The store resolves which source owns each stored pixel. Generators may blend
those selected payload values but do not modify the authoritative snapshot.

## Dataset organization

### Proposed

A store root contains immutable snapshots. Each snapshot contains a manifest,
a source catalog, and one or more named raster layers:

```text
store/
└── snapshots/
    └── <snapshot-id>/
        ├── manifest
        ├── sources
        └── layers/
            ├── heights/
            │   ├── layer
            │   ├── raster.index
            │   └── chunks/...
            └── orthophoto/
                ├── layer
                ├── raster.index
                └── chunks/...
```

The names and exact hierarchy are illustrative. The important boundaries are:

- a source catalog has snapshot-wide identity and metadata;
- each layer has a homogeneous pixel schema;
- each layer owns a sparse quadtree index; and
- each physical quadtree node maps to one atomic chunk container.

The store manifest records the format version, snapshot identity, parent
snapshot when applicable, publication state, Web Mercator definition, and
layer list.

## Spatial model

### Confirmed

The hierarchy uses `radix::tile::Id` semantics: zoom, `x`, and `y`, with four
children per node. Inputs are transformed into Web Mercator during ingestion.

Physical payloads may occur at several quadtree levels. A physical parent can
coexist with physical descendants so that the parent remains a coherent
fallback representation.

### Proposed

The persistent store uses one canonical `y` convention. Slippy/XYZ is the
current recommendation because it matches common web-map paths, but this is
not yet confirmed. API callers may convert from TMS before lookup.

A layer chooses one fixed chunk pixel dimension. Every physical node in that
layer covers the Web Mercator bounds of its tile ID with that many stored
pixels per side. Deeper nodes therefore provide twice the linear spatial
resolution at each level.

Store chunks are non-overlapping. Any halo required for filtering or any
shared border required by a delivery format is assembled by a generator.

## Sparse quadtree index

The index uses the same four logical states as the octree index:

| State | Chunk exists | Indexed descendants |
|---|---:|---:|
| `Leaf` | yes | no |
| `Inner` | yes | yes |
| `Virtual` | no | yes |
| `Missing` | no | no |

`Missing` is represented by absence from the index, not serialized as an
entry.

The index answers structural questions only. It does not claim that every
child exists and does not mark a virtual subtree as spatially complete.

### Required operations

- Look up a node without probing the filesystem.
- Add and remove physical nodes while maintaining virtual ancestors.
- Traverse only indexed branches.
- Enumerate physical descendants of a subtree.
- Find the nearest physical ancestor for fallback.
- Determine whether descendants may improve a requested output.
- Serialize and validate a versioned 2D topology.

The last two operations may require aggregate metadata beyond
`Leaf/Inner/Virtual`, such as best descendant resolution or coverage. Such
metadata is an optimization and should be derivable from authoritative
entries.

## Chunk model

Each physical node owns one logical chunk:

```text
Chunk
├── identity and schema reference
├── local source table
├── payload raster
└── source-index raster
```

The payload and source raster have identical dimensions. Every source-index
element names exactly one local source entry.

### Why one atomic container is proposed

An atomic container keeps payload and provenance inseparable, makes one
hard-link represent one unchanged chunk, and prevents a snapshot from pairing
a new payload with an old source map after interruption.

The container may internally use independently compressed blocks to support
bounded window reads. Atomicity does not require monolithic decompression.

## Source catalog and local tables

### Proposed

The source catalog assigns a stable `SourceId` to every ingested source and
records metadata such as:

- source URI or durable identity;
- content/version identity where available;
- original CRS and transform;
- original and effective resolution;
- acquisition or publication time;
- explicit priority or quality fields;
- bands, data type, colour interpretation, and NoData information; and
- ingestion software and parameters.

Each chunk stores a compact local source table:

```text
local index 0 → SourceId 918
local index 1 → SourceId 42
local index 2 → SourceId 7001
```

The source map stores local indexes rather than full global IDs. Its integer
width may be selected per chunk if the container records that width.

This arrangement provides exact per-pixel attribution while keeping common
single- or few-source chunks compact.

`SourceId` values referenced by a hard-linked chunk must remain stable across
the complete snapshot lineage in which that chunk is reused. A child snapshot
must preserve every catalog record referenced by inherited chunks; it cannot
renumber the catalog independently.

## Source selection and fallback

Source selection is a builder policy, not a property of the raster container.
Its initial comparison is expected to prioritize effective pixel resolution.

For a candidate input, the builder conceptually performs:

```text
for each affected stored pixel:
    compare candidate source with selected source
    write one winning payload value
    write the winning source ID
```

The physical hierarchy supports a separate generator decision:

```text
physical node is sufficiently accurate and coherent
    → use it directly

physical node is insufficient and better descendants exist
    → refine and combine descendants, falling back to ancestors for gaps
```

These policies must not be conflated. Pixel ownership determines the contents
of one physical chunk. Refinement determines which physical chunks contribute
to a requested derived tile.

## Snapshot lifecycle

### Proposed

A build never mutates a published snapshot:

1. Create a private staging snapshot.
2. Load the parent snapshot's manifest and indexes when updating.
3. Identify affected chunks from candidate source bounds and resolution.
4. Hard-link unchanged chunk containers into staging.
5. Rebuild changed chunk containers.
6. Write new source catalogs, layer metadata, and indexes.
7. Validate references, dimensions, checksums, and index/file agreement.
8. Atomically publish the completed snapshot.

Because hard links share inodes, a linked container must never be opened for
in-place modification. Changed chunks are written to new temporary paths and
renamed into place.

Cross-filesystem hard links cannot be created. The builder must expose this as
a clear configuration or error rather than discovering it after a long build.

## Pyramid generator interface

A generator requests a layer over a target tile and sampling specification.
The store reader supplies selected authoritative values and provenance over a
window large enough for the generator's filter support.

The generator owns:

- target output zoom and dimensions;
- vertex-pixel or area-pixel placement;
- low-pass/reconstruction filter;
- NoData normalization during filtering;
- colour-space and alpha treatment;
- border construction;
- output codec; and
- tile-level contributing-source metadata.

The store owns:

- chunk location and decoding;
- sparse hierarchy and physical fallback;
- exact stored source map;
- source catalog lookup; and
- consistent window access across chunk boundaries.

## Scaling implications

The 10-100 MB chunk target reduces filesystem entry and index counts but makes
whole-chunk rewrites expensive. The builder should therefore:

- determine affected chunks before decoding parent data;
- bound the number of resident payload/source-map pairs;
- stream source windows rather than load complete input datasets;
- use internal block compression if partial reads are frequent; and
- avoid materializing world-scale tile vectors before parallel execution.

The first implementation should prefer correctness and measurable behaviour.
Compression blocks, caches, aggregate index metadata, and scheduling should be
driven by profiles and representative datasets.
