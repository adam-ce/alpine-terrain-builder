# Storage format

This document specifies the logical format and required invariants. Exact
binary encodings, field widths, compression libraries, and filenames remain
open unless explicitly marked confirmed.

## Format principles

### Confirmed

- The persistent hierarchy is a Web Mercator quadtree keyed by Radix tile IDs.
- Store chunks are non-overlapping and approximately 10-100 MB after compression.
- Every stored pixel has one payload value (can be vector type) and one source attribution.
- A coarse physical chunk may coexist with more accurate descendants.
- Rendering overlap and delivery encodings are not stored authoritatively.

### Proposed

- All files have explicit magic, format version, and byte-order declarations.
- Payload and source map are held in one atomic chunk container.
- Published files are immutable.
- Checksums cover metadata and independently compressed data sections.
- Unknown optional fields can be skipped by length; incompatible required
  fields cause a clear load failure.

## Store manifest

The root manifest should contain:

```text
format magic and version
snapshot ID
optional parent snapshot ID
creation metadata
canonical CRS identifier and definition
canonical tile scheme
source-catalog reference
layer descriptors
```

The CRS must be specified more strongly than a human-readable name. The exact
EPSG/Web Mercator definition and world extent used by tile calculations must
be unambiguous.

## Layer descriptor

A layer descriptor should contain:

```text
layer ID and name
semantic kind
payload pixel format
band/component interpretation
chunk width and height
NoData representation
index reference
chunk layout/codec identifier
```

The descriptor defines stored data. Vertex-pixel versus area-pixel delivery is
generator policy and is not required as a layer field unless the eventual
store permits more than one stored spatial placement.

The semantic kind is still useful because it constrains valid operations. For
example, height scalars, linear colour, gamma-encoded colour, categorical
values, masks, and vectors require different filtering.

## Source catalog

A source-catalog record should contain a stable ID and sufficient provenance
to understand or reproduce ingestion. Candidate fields are:

```text
SourceId
source identity and version/content fingerprint
human-readable name
original URI or path, if durable and safe to persist
original CRS and geotransform
original dimensions, bands, and pixel type
original NoData/alpha information
original nominal resolution
effective Web Mercator resolution
acquisition/publication time
priority and quality metadata
ingestion timestamp
ingestion transform and resampling policy
```

Pathnames may contain deployment-specific or sensitive information. The
format should support opaque source identities without requiring an original
local path.

Source IDs are persistent across related snapshots. A snapshot that hard-links
an older chunk must retain the same meaning for every global source ID in that
chunk's local table. Snapshot-local catalog compaction may not renumber IDs
referenced by inherited containers.

## Quadtree index file

The index should contain:

```text
magic
format version
tree kind = quadtree
canonical scheme
chunk layout identifier
entry count
entries: TileId → NodeStatus
optional aggregate metadata
checksum
```

`TileId` must be validated when decoded:

```text
zoom is supported
x < 2^zoom
y < 2^zoom
scheme is canonical or omitted because the file declares it globally
```

Node status has the following invariant:

- `Leaf` and `Inner` have a chunk file.
- `Virtual` has no chunk file.
- `Inner` and `Virtual` have at least one indexed descendant.
- `Leaf` has no indexed descendant.
- Every non-root entry has all required ancestors represented.

The index must be reconstructible by scanning valid chunk paths. A rebuild
tool should write a new index rather than silently changing a published one.

## Chunk container

### Logical structure

```text
Chunk header
├── magic and version
├── tile ID
├── layer/schema identity
├── raster dimensions
├── payload encoding
├── source-index encoding
└── section directory

Local source table
├── local source index
└── global SourceId

Payload sections
└── typed raster data

Source-map sections
└── local source indexes, one per payload pixel
```

Payload and source-map dimensions must match exactly. A decoder rejects the
container if a source-map value is outside the local source table.

### Source-index width

Possible representations include:

- `uint8_t` for at most 256 local entries;
- `uint16_t` for at most 65,536 local entries; or
- a width selected per chunk and recorded in the header.

Adaptive width is attractive because most chunks are expected to use few
sources, but it adds codec branches. This is an open implementation decision.

A reserved local entry may represent no valid source. If this is adopted, its
payload validity and filtering semantics must be defined rather than inferred
from an arbitrary payload value.

### Compression and random access

The chunk-size target makes whole-file decompression undesirable for small
windows. A proposed container divides payload and source map into matching
independently compressed blocks and records their offsets in the section
directory.

Block dimensions should support:

- bounded memory;
- halo reads across chunks;
- checksumming damaged regions;
- skipping source-map blocks when provenance is not requested; and
- skipping payload blocks during provenance-only inspection.

The payload and source-map blocks need not use the same compression algorithm.
Source maps are categorical and may benefit from run-length, palette, or
general-purpose compression. The initial implementation should benchmark
representative data before fixing a codec.

## Chunk sizing

### Proposed sizing rule

Choose a power-of-two side length from a logical byte budget:

```text
logical bytes ≈ width × height ×
                (payload bytes per pixel + source-index bytes per pixel)
```

The target interval is 10-100 MB, including both large raster arrays but
excluding small metadata. For example:

```text
4096² × (RGB8 + uint16 source)    ≈ 80 MiB
4096² × (float32 + uint16 source) ≈ 96 MiB
2048² × (RGBA8 + uint16 source)   ≈ 24 MiB
```

The examples use binary MiB despite the conversational 10-100 MB requirement;
the final specification must choose units explicitly.

If a layer's pixel type would place 4,096² outside the budget, the layer may
choose another fixed power-of-two dimension. Varying dimensions between nodes
within one layer is not recommended because it complicates resolution and
window calculations.

### Why compressed size should not drive identity

Compressed size depends on terrain, imagery, source-map fragmentation, and
codec settings. Splitting chunks dynamically to hit a compressed-byte target
would make spatial identities content-dependent and destabilize updates.
A fixed spatial grid with a logical size budget gives predictable identity;
compression is an optimization.

## Filesystem layout

A proposed chunk path is:

```text
layers/<layer>/chunks/<z>/<x>/<y>.rst
```

This is familiar and reconstructible, but very large datasets may place too
many entries in intermediate directories. Alternative layouts may shard by
Morton/Hilbert prefix. The layout identifier in layer metadata allows this to
change without changing tile identity.

Layout parsing must accept only canonical chunk files. Auxiliary previews,
temporary files, and generated delivery images must not accidentally become
index entries during a rebuild scan.

## Immutability and hard links

An unchanged chunk in a child snapshot may be a hard link to the parent's
container. The following rules are mandatory if hard links are used:

- Published containers are never modified in place.
- A changed container is written to a distinct temporary inode.
- Completion uses an atomic rename within the target filesystem.
- Snapshot deletion removes names, not shared content still referenced by
  another snapshot.
- Validation may inspect inode/link counts for diagnostics but format
  correctness never depends on them.

The index, manifest, and source catalog are snapshot-specific and normally new
files even when many chunks are linked.

## Consistency and recovery

A snapshot is publishable only if:

- every physical index entry resolves to one valid chunk;
- no virtual entry has a chunk;
- all chunk tile IDs and layer IDs match their paths and index entries;
- source maps reference valid local entries;
- local entries reference valid catalog records;
- dimensions and formats match the layer descriptor; and
- checksums pass.

Temporary and incomplete files use names excluded from layout scanning.
Publication should make the complete snapshot visible in one atomic metadata
operation. The exact mechanism is open.

## Explicitly not finalized

This document does not yet choose:

- binary serialization library;
- integer field widths;
- compression codecs and block dimensions;
- exact source metadata fields;
- checksum algorithm;
- file extensions and directory names;
- snapshot publication mechanism; or
- generic versus raster-specific index implementation.

Those choices require prototypes and representative datasets, and should not
be implied by a first implementation.
