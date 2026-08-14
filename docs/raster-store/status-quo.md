# Status quo and reuse assessment

> **Historical assessment:** this document records the repository before the
> shared-store refactor and is intentionally not rewritten as current code
> documentation. The completed implementation and verification record is in
> [refactor-status.md](refactor-status.md); the resulting public boundary is
> summarized in [architecture.md](architecture.md#implementation-status).

The refactor chose the generalization option discussed below. Sparse topology,
traversal, layouts, runtime codecs, storage, index lifecycle, and cache
interfaces now live under `store` and are parameterized by hierarchy traits.
The legacy 3D format remains an adapter under `octree`, while
`raster_store::StoreTraits` proves the shared in-memory mechanisms with
`radix::tile::Id`. Persistent RF formats and RF tools remain future work.

This document evaluates the current repository after commit `9cf9065`
(`Consolidate raster handling and use std::expected`). It distinguishes
reusable mechanisms from interfaces that encode assumptions unsuitable for
the raster store.

## Summary

The project already contains most low-level ingredients:

- `radix::Raster<T>` for contiguous typed raster memory;
- `radix::tile::Id` for quadtree addressing;
- Web Mercator grid calculations and GDAL reprojection in `tile_builder`;
- sparse topology, indexed traversal, codecs, layouts, and hard-link reuse in
  the octree storage code; and
- the SF merger's snapshot-like reuse of unchanged subtrees.

There is no existing component that should become the raster store unchanged.
The best path is to compose the Radix raster and tile primitives with a new
2D storage layer, while extracting or adapting selected octree-storage ideas.

## Reuse matrix

| Component | Assessment | Intended use |
|---|---|---|
| `radix::Raster<T>` | Reuse directly | In-memory payloads and source maps |
| `radix::RasterMask` | Reuse directly | Temporary validity/selection masks |
| `radix::raster::transform` | Reuse directly | Typed pixel transformations |
| `radix::raster::generate_mipmap` | Do not use as general generator | Only a 2x2 component-wise box average |
| `radix::tile::Id` | Reuse after hardening or through an adapter | Persistent quadtree keys |
| `radix::quad_tree::Node` | Do not reuse for disk index | Dense in-memory ownership model |
| CTB `GlobalMercator` and grid bounds | Reuse initially | Tile bounds and Web Mercator resolution |
| `Dataset` and GDAL setup | Reuse/adapt | Input discovery and reprojection |
| `DatasetReader` | Adapt substantially | Windowed, typed, multi-band ingestion |
| `Tiler` / `ParallelTiler` | Reuse calculations, replace orchestration | Candidate chunk enumeration |
| `ParallelTileGenerator` | Do not use as store builder | Small-file writer with incompatible lifecycle |
| Octree `IndexMap` algorithm | Reuse design; generalize or port | Sparse physical/virtual topology |
| Octree `Storage_` / `RawStorage_` | Reuse design and selected code | Codec boundary, indexing, hard-link copy |
| Octree disk layouts | Do not use as-is | They encode `octree::Id` and 3D paths |
| SF merger visitors and geometry code | Do not reuse | Mesh- and ECEF-specific semantics |
| SF merger unchanged-subtree copy | Reuse design | Snapshot construction and hard links |
| `zpp_bits` serialization helpers | Reuse cautiously | Versioned metadata/index serialization |

## Radix raster

### What can be reused

`extern/radix/src/radix/raster.h` now provides a shared, value-typed raster:

- rectangular `glm::uvec2` dimensions;
- contiguous row-major `std::vector<T>` storage;
- element and byte spans;
- typed pixel access;
- move construction from an existing vector;
- a contiguous byte-valued `RasterMask`;
- dimension-checked concatenation; and
- masked and unmasked transforms.

These properties fit both store arrays:

```cpp
radix::Raster<PayloadPixel> payload;
radix::Raster<SourceIndex> source_map;
```

The class correctly remains independent of Web Mercator, tile IDs, source
metadata, compression, and file I/O. Those belong to higher layers.

### What needs adaptation around it

Large chunks and filtered generation will benefit from operations not
currently supplied by `Raster<T>`:

- non-owning raster views and subwindows;
- explicit row stride where external codecs require it;
- halo/window assembly across neighbouring chunks;
- checked construction that reports allocation/dimension errors without
  relying on assertions; and
- streaming or block processing when a full set of input chunks would exceed
  the memory budget.

These should be introduced only when required. They are not reasons to embed
store concepts into `Raster<T>`.

### What cannot be reused for final filtering

`radix::raster::generate_mipmap` requires a square, power-of-two raster and
reduces each 2x2 group by component-wise averaging. It does not provide:

- a selectable reconstruction or low-pass filter;
- the phase difference between vertex pixels and area pixels;
- halo samples across tile boundaries;
- linear-light colour and premultiplied-alpha handling;
- NoData-aware normalization;
- categorical reduction; or
- source-contribution tracking.

It is therefore a useful simple raster utility, not the raster-store pyramid
generator.

## Radix tile addressing

### What can be reused

`radix::tile::Id` already contains the required 2D identity:

- zoom level;
- `x/y` coordinates;
- parent and four-child relationships;
- TMS/Slippy conversion; and
- hashing and ordering support.

### Required hardening

Persistent storage needs stronger invariants than the current convenience
type supplies:

- Calling `parent()` at zoom zero currently underflows.
- Construction does not reject coordinates outside `[0, 2^z)`.
- Shifting `1u << zoom_level` limits valid conversion at high zooms.
- The scheme participates in identity, so the same spatial tile in TMS and
  Slippy form becomes two keys.
- There is no persistent serialization contract or format version.

The store should choose one canonical scheme and normalize all IDs at its API
boundary. Whether the hardening belongs in Radix or in a checked store adapter
is open.

`radix::quad_tree::Node<T>` is not a replacement for the index. It owns a
fully allocated group of four children whenever refined, represents no
missing child within such a group, and has no persistence or physical/virtual
status. The store requires a sparse map keyed by tile ID.

## Tile builder and GDAL path

### Reusable foundations

The current tile builder already demonstrates:

- opening GDAL raster datasets;
- reading dataset bounds;
- selecting Web Mercator or geodetic CTB grids;
- transforming arbitrary source SRS data during reads;
- calculating tile bounds and resolutions;
- enumerating intersecting `radix::tile::Id` values; and
- parallel per-tile processing.

`ctb::GlobalMercator`, `Tiler::tile_for`, and `ParallelTiler` are useful
references and may be reused initially for grid math.

### Required changes

`DatasetReader` currently reads one band into a float raster, uses cubic GDAL
warping, and constructs a warped VRT for a requested output rectangle. The
store needs typed and multi-band reads, explicit alpha/NoData handling,
controlled resampling, source metadata, and deterministic alignment with the
store chunk grid.

`Tiler` models delivery-oriented dimensions through `Border::Yes/No` and a
south/east extra pixel. Store chunks have no rendering overlap, and generated
vertex pixels require an explicit global sampling/filtering model. The border
boolean should not define store geometry.

`ParallelTileGenerator` writes individual image files for explicitly
enumerated tiles. It is not suitable as the snapshot builder because it lacks:

- source-map generation;
- old-snapshot reuse;
- atomic chunk containers;
- index transactions and publication;
- resumability validation; and
- bounded enumeration/streaming for very large tile sets.

Its parallel work pattern and progress reporting may still inform the new
builder.

## Octree index and storage

### Reusable design

The octree index captures the required topology semantics:

- `Leaf`: physical payload without indexed descendants;
- `Inner`: physical payload with indexed descendants;
- `Virtual`: no physical payload, but indexed descendants; and
- absence from the map: missing node and subtree.

Adding a physical node creates virtual ancestors and promotes a physical
ancestor from leaf to inner. Removing nodes collapses unused virtual chains.
Traversal follows only present index entries.

The storage stack also has useful separation between:

- logical indexed storage;
- raw path-based storage;
- disk layout;
- payload codec; and
- optional cache.

`RawStorage_::copy_from` demonstrates hard-link reuse when source and target
extensions match. The SF merger demonstrates copying unchanged physical
subtrees and writing a new output index.

### Why it cannot be reused unchanged

The entire stack uses concrete `octree::Id` types. `IndexMap`, cache APIs,
layouts, filesystem path parsing, traversal, storage, serialization, and
formatting all embed this type. The coordinate-directory layout is
`level/x/y/z`, and the default codec is a mesh codec.

The existing index file also records no tree kind. Reinterpreting its
serialized `(level, index)` octree IDs as web tiles would be unsafe.

A 2D implementation can either:

1. generalize the hierarchy/storage stack over an ID and layout policy; or
2. create a raster-store-specific 2D implementation using the same algorithms.

Generalization avoids duplicate infrastructure but has a larger blast radius
in mature octree code. A separate implementation is initially safer but risks
long-term duplication. This requires an explicit decision before coding.

### Behaviours that should not be copied blindly

- Hard-link failure currently has no copy fallback.
- Existing indexes are trusted rather than reconciled against filesystem
  contents when opened.
- An unindexed output discovers files through a final recursive directory
  scan.
- Some error paths use assertions or process termination.
- The binary index lacks an explicit magic/version/topology header suitable
  for a new durable format.

The raster store should retain the useful topology and codec boundaries while
specifying stronger snapshot, validation, and error-handling rules.

## SF builder and merger

The SF builder's ECEF octree placement, mesh construction, texture atlases,
mask clipping, and mesh visitors are not reusable for a Web Mercator raster
store.

The reusable ideas are architectural:

- transform source data into a canonical spatial system during build;
- preserve physical ancestors as fallback representations;
- use an index to avoid per-node filesystem probing;
- stop refinement when a coherent representation is sufficient; and
- hard-link unchanged chunks into a new output dataset.

Those ideas should be reimplemented against Radix tile IDs and raster payloads
rather than adapted through mesh abstractions.

## Recommended component boundary

The current components suggest the following dependency direction:

```text
Radix
  Raster<T>, RasterMask, tile::Id, geometry
       ↓
Terrain library
  GDAL dataset access, Web Mercator grid math, filtering primitives
       ↓
Raster store
  source catalog, chunk container, sparse index, snapshots
       ↓
Builders and generators
  ingestion, source selection, texture pyramids, height/geometry pyramids
```

The raster store should consume `radix::Raster<T>`; Radix should not depend on
the store's source catalog, file format, or snapshot lifecycle.
