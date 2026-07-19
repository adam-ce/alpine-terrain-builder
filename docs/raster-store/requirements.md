# Requirements and terminology

## Goals

### Confirmed

The system has two distinct layers:

1. An authoritative raster store that retains the best representation
   available for every covered pixel.
2. Derived tile pyramids generated from that store for rendering or other
   delivery formats.

Inputs may use arbitrary supported coordinate reference systems. Ingestion
transforms them into Web Mercator before placing their data in the store.

The store must support multiple raster kinds. Height data is one raster kind;
orthophotos and other image-like data are others. Pixel format, NoData
handling, filtering, and output encoding may differ by kind.

The store must support mixed accuracy over space. A physical payload at a
coarser quadtree node may remain available as a coherent fallback while more
accurate payloads exist in descendants.

### Proposed

Each named raster layer has a homogeneous value schema and its own sparse
quadtree index. Layers share the same Web Mercator tile addressing and source
catalog infrastructure but do not mix incompatible pixel types in one
payload.

The authoritative store is output-format independent. It does not contain
rendering borders or require the sampling placement of a particular delivery
format.

## Stored pixels and provenance

### Confirmed

Every stored pixel has exactly:

- one payload value; and
- one source attribution.

A tile carries a local source table and a source map. Each source-map element
indexes one entry in the local table. A local entry identifies source metadata
held by the store.

The store does not blend source identifiers and does not retain several
candidate payload values for one stored pixel. Source selection is resolved
before the pixel is committed.

Generated tiles do not retain per-pixel provenance. They record provenance at
tile granularity, for example as the set of sources that contributed to the
filtered output tile. Filtering may blend payload values across source
boundaries without changing the authoritative store's provenance model.

### Open

- Whether a source map refers to a dataset-wide source catalog by stable ID,
  or contains complete source metadata in each tile.
- The reserved representation for a pixel with no valid source.
- Whether generated tile provenance needs only a set of source IDs or also
  approximate contribution fractions.

## Tile and chunk requirements

### Confirmed

Store tiles are large data chunks, not ordinary 256-pixel web delivery tiles.
Their target size is approximately 10-100 MB.

Store tiles do not duplicate rendering overlap. In particular, overlapping
height borders are generated later and are not authoritative duplicated data.

Tiles are addressed by a two-dimensional quadtree using
`radix::tile::Id`, not `octree::Id`.

### Proposed

The size target should initially be interpreted as an uncompressed logical
byte budget for payload plus source map. That gives predictable peak memory,
I/O, and rewrite cost. Compressed on-disk size varies too much with content to
be the only sizing invariant.

The pixel side length should be a power of two. Illustrative uncompressed
sizes are:

| Side length | Pixels | RGB8 + uint16 source | float32 + uint16 source |
|---:|---:|---:|---:|
| 2,048 | 4,194,304 | 20 MiB | 24 MiB |
| 4,096 | 16,777,216 | 80 MiB | 96 MiB |

These calculations exclude headers, the local source table, alignment, and
compression. They demonstrate that a fixed 2,048- or 4,096-pixel chunk is in
the intended range for common formats; they do not decide the final chunk
dimension.

### Open

- Whether the 10-100 MB target refers to logical, compressed, or both sizes.
- Whether all layers use one fixed side length or choose it from their bytes
  per pixel and source-index width.
- Whether the source map uses a fixed or per-tile adaptive integer width.
- Whether payload and provenance are one atomic container or coordinated
  files.

## Accuracy and fallback

### Confirmed

Pixel resolution is expected to be the most important source-accuracy signal,
but the final ranking rule is not yet known.

A coarse, coherent source should be usable directly for a coarse output tile
even when more accurate fragments exist below it. Generators must not be
forced to patch every output from the deepest available descendants.

### Proposed

Source metadata records effective ground resolution after reprojection as
well as original resolution where meaningful. Source selection is a policy
interface rather than a fixed comparison embedded in the storage format.

A pyramid generator refines into descendants only when the current physical
node does not satisfy the requested output resolution or another configured
quality criterion.

### Open

- The resolution tolerance that permits use of one coherent source instead
  of a finer mosaic.
- Tie-breaking by acquisition time, explicit priority, quality, or source
  identity.
- Whether source accuracy is scalar per source or may vary spatially.

## Updates and reuse

### Proposed

Published store versions are immutable snapshots. Building a new snapshot
hard-links unchanged tile containers from an earlier snapshot and writes new
containers only for changed tiles. The snapshot owns a new index and manifest.

This is safer than mutating hard-linked files in place and makes a failed build
discardable without damaging an earlier published store.

Hard-link reuse requires source and destination snapshots to reside on the
same filesystem. The implementation must either require that condition or
define an explicit copy fallback.

### Open

- Snapshot publication and atomic rename rules.
- Retention and garbage-collection policy.
- Whether cross-filesystem builds fail or copy unchanged chunks.

## Terminology

**Raster store tile** or **chunk**
: A large authoritative file associated with a quadtree tile ID. It is not a
  delivery tile and is expected to contain roughly 10-100 MB.

**Payload**
: The selected raster value array stored by a chunk.

**Source map**
: A raster aligned one-to-one with the payload. Each element selects exactly
  one entry in the chunk's local source table.

**Physical node**
: A quadtree node with an authoritative chunk on disk.

**Virtual node**
: An index-only quadtree node with descendants but no chunk of its own.

**Vertex pixel**
: A generated value located on a grid vertex. Height tiles for mesh generation
  require vertex pixels, including shared boundary positions.

**Area pixel**
: A generated value associated with a raster cell. Ordinary texture outputs
  use area pixels whose cell boundaries align with tile boundaries.

The terms vertex pixel and area pixel describe generator outputs. They do not
assert how an original sensor or source raster produced its values.

**Derived tile**
: A filtered and encoded output tile generated from the authoritative store.
It may have different dimensions, sampling placement, encoding, and
provenance granularity from a store chunk.
