# Decisions and open questions

This register prevents confirmed requirements, recommendations, and unresolved
questions from being mixed together. A future architectural decision record
may replace an entry when implementation requires a durable choice.

## Confirmed decisions

### D1: Separate authoritative and derived layers

The raster store is authoritative. Delivery tile pyramids are derived and may
be regenerated with different sampling, filtering, dimensions, or codecs.

### D2: Canonical Web Mercator ingestion

Inputs are transformed into Web Mercator while building the authoritative
store, analogous to the SF builder transforming geometry into its canonical
ECEF space.

### D3: Use a quadtree and Radix tile identity

The hierarchy is two-dimensional and uses `radix::tile::Id`, not
`octree::Id`.

### D4: Preserve physical coarse fallbacks

A physical chunk may coexist with descendants. Generators may use the parent
as a coherent coarse representation or refine into more accurate descendants.

### D5: Store chunks are non-overlapping

The authoritative store does not duplicate height borders. Overlapping
rendering borders are generated from a consistent filtered signal.

### D6: One selected value and source per stored pixel

Every stored pixel contains exactly one payload value and one source
attribution. The store does not keep alternate candidate values for that
pixel.

### D7: Generated provenance is tile-level

Derived filtering may blend payload values from several sources. Generated
tiles retain provenance per tile, not per output pixel.

### D8: Generator terminology

Use **vertex pixel** for generated values on grid vertices and **area pixel**
for generated values associated with raster cells. The names describe required
output placement, not source measurement history.

### D9: Large authoritative chunks

Store tiles should be approximately 10-100 MB. They are storage chunks, not
ordinary web delivery tiles.

### D10: Radix owns the generic raster primitive

`radix::Raster<T>` is the common in-memory raster representation. Store
georeferencing, provenance, persistence, and snapshot lifecycle remain outside
Radix.

## Current recommendations

These are proposed defaults, not confirmed decisions.

### R1: Immutable snapshots

Publish immutable snapshots, hard-link unchanged chunk containers, and write
changed chunks to new inodes. This avoids accidental mutation through hard
links and supports discardable failed builds.

### R2: One atomic chunk container

Keep payload, local source table, and source map in one container so they
cannot become inconsistent and can be reused with one hard link.

### R3: Fixed power-of-two chunk dimension per layer

Choose a fixed side length from a logical byte budget. Do not split spatial
identities dynamically based on compressed size.

### R4: Snapshot-wide catalog plus local source tables

Store full metadata once in a snapshot catalog. Let each chunk map compact
local source indexes to stable catalog IDs.

### R5: Canonical persistent tile scheme

Persist one scheme and normalize at API boundaries. Slippy/XYZ is the current
candidate, but TMS has not been rejected.

### R6: Explicit versioning and validation

New format files should have magic values, versions, checksums, strict tile-ID
validation, and a rebuildable index. The current octree index binary should
not be reused as an unlabelled quadtree format.

### R7: Global filtering across chunk boundaries

Generators request filter halos and operate on logical global windows. They do
not clamp at internal chunk or delivery-tile boundaries.

## Open questions

### Spatial and layer model

1. Are raster kinds always separate named layers with independent indexes?
2. Which exact Web Mercator definition and world extent are canonical?
3. Is the persistent scheme Slippy/XYZ or TMS?
4. What is the maximum supported zoom?
5. What exact spatial interpretation does the store assign to its own raster
   elements? This is independent of vertex-pixel/area-pixel delivery policy
   but must be defined for georeferencing and reconstruction.
6. Does source accuracy vary only by source, or can it vary spatially within a
   source?

### Chunk size and physical format

7. Does 10-100 MB mean logical bytes, compressed disk bytes, or constraints on
   both?
8. Are units decimal MB or binary MiB?
9. Is chunk dimension fixed globally or selected per layer?
10. Which representative payload schemas must be used to choose between
    2,048², 4,096², or another size?
11. Does the source map use `uint8_t`, `uint16_t`, or adaptive width?
12. Which container serialization, compression, block size, and checksum are
    suitable for representative imagery and source maps?
13. Is `<z>/<x>/<y>` sufficient for filesystem scaling, or is prefix sharding
    required?

### Source metadata and selection

14. What creates a stable `SourceId`: catalog sequence, content hash, external
    ID, or a combination?
15. Which source metadata fields are mandatory?
16. How is a pixel with no valid source represented?
17. What is the first complete source-ranking policy after pixel resolution?
18. How much resolution loss is acceptable to prefer one coherent source over
    a finer mosaic?
19. How are acquisition time, quality flags, explicit priority, and ties
    handled?
20. Can an update remove or invalidate a previously selected source, and if
    so, where does replacement data come from when alternates are not stored?

### Filtering and generation

21. Which reconstruction and low-pass filters are required for heights?
22. Is terrain reduction optimized only for anti-aliasing, or also for
    geometric error and preservation of extrema?
23. Which colour space and alpha convention are authoritative for imagery?
24. Which semantic raster kinds require categorical or conservative
    reduction rather than linear filtering?
25. What are the exact NoData normalization rules?
26. Is tile-level provenance only a source-ID set, or does it include
    contribution fractions?
27. How are Web Mercator horizontal wrap and north/south boundaries filtered?
28. Must shared generated height edges be bit-identical across separate runs
    and execution orders?

### Index and implementation structure

29. Should the octree index/storage be generalized over hierarchical ID type,
    or should the raster store receive a separate 2D implementation?
30. Should `radix::tile::Id` itself gain checked root/zoom/coordinate and
    serialization APIs, or should storage use a checked adapter?
31. Does the index need aggregate coverage or best-descendant-resolution data,
    or can the first version derive it during traversal?
32. How is an existing index verified against chunk files without making every
    open operation scan the filesystem?

### Snapshots and operation

33. What atomically publishes a completed snapshot?
34. Are cross-filesystem updates rejected or allowed with a copy fallback?
35. What is the retention and garbage-collection policy?
36. How does a long build resume, and how are staged chunks validated before
    being trusted?
37. Which corruption and compatibility guarantees are required for long-term
    archival use?

## Suggested order for resolving questions

The questions do not need to be answered all at once. A practical order is:

1. Fix the store's spatial sampling interpretation and layer model.
2. Select representative payload formats and datasets.
3. Prototype chunk dimensions, source-index widths, and block compression.
4. Decide generic versus separate index/storage implementation.
5. Specify source identity, NoData, and the initial resolution ranking.
6. Implement a single-source area-pixel path with golden grid tests.
7. Add multi-source ownership and immutable snapshots.
8. Prototype vertex-pixel height filtering and seam tests.
9. Finalize operational publication, recovery, and compatibility rules.

Each step should produce a small approved plan before code changes begin.
