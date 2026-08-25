# Architecture

## Implementation status

This document describes the intended raster-fundamentalis and tile-base
architecture. Its RF format, snapshot-publication, builder, merger, pyramid,
and server requirements are future work; they were not acceptance criteria
for the completed 2D/3D hierarchy-store refactor. The implemented boundary is
recorded in [refactor-status.md](refactor-status.md), and the original plan is
kept in [refactor-plan.md](refactor-plan.md).

The refactor delivered the dimension-neutral mechanisms needed by that future
work:

- `store::Index<Traits>`, `store::traverse`, layouts, runtime codecs, storage,
  typed errors, and cache interfaces;
- `octree::StoreTraits` plus legacy 3D layout/index/open adapters;
- `raster_store::StoreTraits` for in-memory topology keyed by
  `radix::tile::Id`; and
- SF-only topology validation under `sf`.

It deliberately did not define a persistent RF index, RF payload codec, RF
opening API, `rf_builder`, `rf_merger`, tile-base generator, or tile server.

### Final public names

The shared API uses `store::NodeStatus`, `store::NodeStatusOrMissing`,
`store::Index<Traits>`, `store::traverse`, `store::RawStorage`,
`store::Storage`, and `store::IndexedStorage`. Mesh storage aliases live under
`mesh::storage`; DAG batch and metadata aliases live under `dag::storage`.
The `octree` namespace retains the 3D key, traits, path mappings, legacy index
DTO, and mesh-opening compatibility functions.

A legacy 3D mesh dataset is opened through the 3D adapter:

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
Input rasters (GDAL)
    │
    │ inspect, transform to Web Mercator, define source, one source per pixel -> rf_builder
    ▼
raster-fundamentalis (one rf per source at the beginning)
    │
    │ rf_merger: merge two rf stores based on (vecrtor) mask, take one tile if no overlap / far from vector border, merge strategy otherwise
    │ at the beginning, have xor strategy, and exact border, later we may implement linear blending
    ▼
Authoritative rf raster store
    │
    │ generate overviews using defined filtering strategy, all area pixels. for now simple averaging, later maybe larger filter sizes / more complex filters
    ▼
tile-base store (one per layer, one per data version. user visible server should only need one version per layer)
    ├── read by tile-server
    ├── area/vertex pixel tile generation
    └── select resolution, type etc by url
```

## Dataset organization

### Proposed

A store root contains immutable snapshots. Each snapshot contains an index, a source attribution table and the data:

```text
store/
└── snapshot-id/
    ├── source_attribution_table.ard
    ├── raster_store.index
    └── <zoom>/<x>/<y>.amort
```

The shown payload path is the default `zoom/x/y_google` layout. Other layouts
may map the same tile IDs differently; there is no mandatory `chunks/`
directory.

## Sparse quadtree index

The index uses the same four logical states as the octree index:

| State     | Chunk exists | Indexed descendants |
|-----------|-------------:|--------------------:|
| `Leaf`    |     yes      |          no         |
| `Inner`   |     yes      |         yes         |
| `Virtual` |      no      |         yes         |
| `Missing` |      no      |          no         |

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

Each physical node owns one logical chunk, see storage-format.md

## Source selection and fallback

Source selection is a policy of the tools, not a property of the raster container.
Its initial comparison is expected to prioritize effective pixel resolution, and can be represented as a per zoom level or global ordered vector of attribution indices.

At the beginning the selection will be binary, later we may introduce blending (over pixels of one zoom level, or several zoom levels).

## Snapshot lifecycle

A snapshot is never mutated, instead, operations build new snapshots, while reducing disk usage by using hard links:

when adding data, we would:
1. create a new sf from the new data
2. define a validity mask for the new data
3. define a source merging priority (an ordering of sources)
4. using these two, a new snapshot is generated from the new data and the existing / authoritative snapshot, hardlinking tiles without change.

Because hard links share inodes, a linked container must never be opened for
in-place modification. Existing snapshots are considered immutable. Obsolete snapshots can be deleted, the data will be preserved if necessary due to reference counting in the inodes.
When merging, we need to create new hardlinks for unchanged rf tiles (taken completely from either snapshot), and we need to create new rf tiles if the new tile shares information from both.

### Publication

A new snapshot is assembled in a sibling directory named
`<snapshot-id>.part`. Publication follows this protocol:

1. Write all payload and metadata files into the `.part` directory.
2. Write the index last and validate the completed snapshot.
3. Flush and close every file.
4. Atomically rename the directory to `<snapshot-id>` on the same filesystem.

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
directory or a final snapshot may be unusable and must be validated and
rebuilt.

## Pyramid generator interface (to be confirmed, LLM, do not use the following without consultation)

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
