# Architecture
This document describes the intended raster-store (RS), raster-fundamentalis (RF), tile-base (TB), and tile-server architecture.

Raster-store (RS) is a storage mechanism for geospatial raster data tiles for the alpine maps project.
It is build on top of the store architecture, which is used for 3d meshes as well.
Tile data is made available as templetized radix::Raster<> objects, and referenced with radix::tile::Id (a Google/Mapbox/XYZ tile identifier).
Supported template parameters are in particular basic data types (float, double, all int types etc), and glm vector types.
Every texel contains a data value and attribution reference into a global attribution table.
The attribution table contains meta information like accuracy and date in addition to the copyright information.
[storage-format.md](storage-format.md) notes down the details.

Raster-fundamentalis (RS) is our world-wide authoritative dataset and uses RS for storage.
RF is build from many different sources, some of which contain world wide data, others contain more accurate but only local data.
Any data-source is first transformed into a RF dataset using rf-builder.
The generated RF dataset will have only one attribution.
Two data-sources can be combined using rf-merger, and an attribution priority list. 
The merger selects tiles with the higher priority, or resolves conflicts on a per-texel basis, if necessary.

Tile-base is built from RF by adding overviews (lower resolution versions of the original data).
Several aggregation methods for the lower resolutions must be supported (at least min/max/avg).
Tile-base is consumed by the tile-server, which generates tiles for the alpine-maps renderer client.
The tile-server can combine several tile-base datasets into one tile, e.g. a digital surface and terrain model (dsm and dtm).
The client can ask for different resolutions (e.g. 65px or 256px tiles), for different pixel types (area or vertex pixels with overlapping borders, see [sampling-and-generation.md](sampling-and-generation.md) for details), and for different encodings (e.g. JPEG, PNG, compressed ETC2, compressed DXT1, comrpessed raw, etc).
Raster-store (RF, and TB) always use area pixels.

RS datasets are potentially several TiB in size, so building and merging can take several days or weeks.
In order to prevent premature overengineering, the following policies should be used for rf-builder, rf-merger, and tb-builder:
- every build or merge generates a new dataset
- errors abort the build
- the index is updated at least every few minutes to keep the data readable if there is an abort.
- for builds and merges, it's possible to submit a cache store (e.g. from an earlier or aborted run), which allows to skip processing of already existant nodes.
- if a node can be reused (either from cache, or when merging 2 datasets), a hard link should be set to prevent storage cost explosion.

## Implementation status
So far, a refactor delivered the dimension-neutral mechanisms to index RS:
- `store::Index<Traits>`, `store::traverse`, layouts, runtime codecs, storage,
  typed errors, and cache interfaces;
- `octree::StoreTraits` 3D layout/index/open adapters (for reference only);
- `raster_store::StoreTraits` for in-memory topology keyed by
  `radix::tile::Id`;

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
A store directory is an immutable snapshot. Each snapshot contains an index, a meta-data file (e.g. the used codec), a source attribution table, and the data:

```text
store-snapshot-id/
 ├── raster_store.metadata
 ├── raster_store.index
 ├── source_attribution_table.json
 ├── build_log.txt
 └── <zoom>/<x>/<y>.amort
```

The shown payload path is the default `zoom/x/y_google` layout. Other layouts
may map the same tile IDs differently;

### Sparse quadtree index

The index uses the same four logical states as the octree index:

| State     | Chunk exists | Indexed descendants |
|-----------|-------------:|--------------------:|
| `Leaf`    |     yes      |          no         |
| `Inner`   |     yes      |         yes         |
| `Virtual` |      no      |         yes         |
| `Missing` |      no      |          no         |

`Missing` is represented by absence from the index, not serialized as an
entry.

### Chunk (tile) model

Each Chunk is one radix::Raster<> tile and metadata.
Both are specified by the codec.
Different codecs can be used for writing and reading if they are compatible (e.g. there can be a read-only codec only returning the metadata).

Codecs used for writing should be lossless in order not to degenerate the data during repeated encdoding (e.g. merging, overview generation etc.). 

## Snapshot lifecycle

A snapshot is never mutated, instead, operations build new snapshots, while reducing disk usage by using hard links:

E.g., when adding data, we would:
1. define a validity mask for the new data (in vector format, everything inside is attributed to the dataset).
2. create a new sf from the new data and validity mask using rf-builder
3. define an attribution source merging priority (an ordering of attribution sources)
4. using these two, a new snapshot is generated using rf-merger. most tiles will be hard-linked from one of the datasets, the border tiles will have the higher resolution of the two, and be merged, new chunks/tiles.

Because hard links share inodes, a linked container must never be opened for
in-place modification. Existing snapshots are considered immutable. Obsolete snapshots can be deleted, the data will be preserved if necessary due to reference counting in the inodes.

### Publication

A new snapshot is assembled in a sibling directory named
`<snapshot-id>.part`. Publication follows this protocol:

1. Write all payload and metadata files into the `.part` directory.
2. Write the final index last and validate the completed snapshot.
3. Rename the directory to `<snapshot-id>` on the same filesystem.

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
raster-fundamentalis is our authoritative raster-store.
- unlike a tile pyramid, not every level is occupied (there is no downsampled versions of the data).
- Coarse physical tiles (e.g. zoom level 10) may coexist with more accurate descendants (e.g. zoom level 15).
- it consumes raw gdal data or datasets from the tile downloader.
- the raster-fundamentalis builder is implemented in src/rf-builder/*

### rf_merger

### tb_builder
tile-base is a hierarchy build from raster-fundamentalis, containing all data and its overviews / downsampled versions. 
- lives in src/tb-builder
- see also [sampling-and-generation.md](sampling-and-generation.md)

### tile_server
The tile-server consumes tile-base and generates tiles of requested resolution and pixel type (vertex|area) on the fly.
- requests by url, e.g.: layer/vertex|area/resolution/z/x/y.ending
- some tile layers will want to pack several tile-bases (e.g. dtm+dsm, percentage larger than slope angle (PLaTSA) + phong shading + AO shading)
- extra ECEF bounding sphere tree (later)
- lives in src/tile-server/*
