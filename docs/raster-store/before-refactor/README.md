# SF storage and merge architecture before the 2D/3D refactor

This report describes the existing Structura Fundamentalis (SF) builder,
sparse octree index, storage stack, and mask-based dataset merger. Its purpose
is to identify the mechanisms that can be generalised from 3D octrees to a
shared 2D/3D hierarchy without carrying mesh- and ECEF-specific behaviour into
the common storage layer.

The central conclusion is:

> Generalise the sparse hierarchy, storage, traversal, merge decisions, and
> unchanged-subtree reuse. Keep GDAL ingestion, raster processing, mesh
> construction, spatial transforms, and mask geometry in dimension-specific
> adapters.

The current storage templates are already payload-generic, but they are not
hierarchy-generic: `octree::Id` is embedded throughout indexing, traversal,
caches, disk paths, and serialization.

## Current architecture

```mermaid
flowchart TB
    subgraph Applications
        SFB[sf-builder]
        SFM[sf-merger]
    end

    subgraph Input
        GDALR[GDAL raster datasource]
        GDALV[GDAL vector mask]
        OLD[Base SF dataset]
        NEW[New SF dataset]
    end

    subgraph Domain3D["3D and mesh policy"]
        SPACE[octree::Space and ECEF bounds]
        MESHBUILD[Height raster to SimpleMesh]
        MESHMASK[Polygon to spherical extruded MeshMask]
        MESHMERGE[Clip, combine, and texture atlas]
        FALLBACK[Reconstruct child by clipping ancestor mesh]
    end

    subgraph GenericCandidate["Mostly generalisable mechanisms"]
        INDEX[IndexMap and NodeStatus]
        WALK[Depth-first and breadth-first traversal]
        DRIVER[Merger and merge result actions]
        STORAGE[Storage and IndexedStorage]
        RAW[RawStorage]
        CODEC[Codec policy]
        LAYOUT[Layout strategy]
        LINK[Hard-link unchanged payload]
    end

    GDALR --> SFB
    SFB --> SPACE
    SFB --> MESHBUILD
    MESHBUILD --> STORAGE

    OLD --> SFM
    NEW --> SFM
    GDALV --> MESHMASK
    MESHMASK --> SFM
    SFM --> DRIVER
    DRIVER --> FALLBACK
    DRIVER --> MESHMERGE
    DRIVER --> STORAGE

    STORAGE --> INDEX
    STORAGE --> RAW
    RAW --> CODEC
    RAW --> LAYOUT
    RAW --> LINK
    WALK --> INDEX
    DRIVER --> WALK
```

The significant code boundaries are:

- hierarchy identity: [`octree::Id`](../../../src/terrainlib/octree/Id.h);
- sparse topology: [`IndexMap`](../../../src/terrainlib/octree/IndexMap.h);
- traversal: [`octree::traverse`](../../../src/terrainlib/octree/traverse.h);
- logical storage:
  [`Storage_`](../../../src/terrainlib/octree/storage/Storage.h);
- raw file and hard-link storage:
  [`RawStorage_`](../../../src/terrainlib/octree/storage/RawStorage.h);
- disk paths: [`disk::Layout`](../../../src/terrainlib/octree/disk/Layout.h);
- merge driver: [`Merger`](../../../src/sf_merger/merge.h).

## Sparse index model

The index has four logical states:

| State | Physical payload | Indexed descendants |
|---|---:|---:|
| `Missing` | no | no |
| `Virtual` | no | yes |
| `Leaf` | yes | no |
| `Inner` | yes | yes |

`Missing` is represented by absence from `IndexMap`. The other states are
defined by
[`NodeStatus`](../../../src/terrainlib/octree/NodeStatus.h).

```mermaid
stateDiagram-v2
    [*] --> Missing

    Missing --> Leaf: add physical root or node
    Missing --> Virtual: add deeper descendant

    Virtual --> Inner: add physical payload here
    Leaf --> Inner: add physical descendant

    Inner --> Leaf: remove final descendant
    Leaf --> Missing: remove payload and no descendants
    Virtual --> Missing: remove final descendant
    Inner --> Virtual: remove payload but keep descendants
```

This state machine is dimension-independent. What is currently 3D-specific is
how a node finds its parent and children: `octree::Id` uses three
Morton-interleaved coordinates and eight children.

Traversal is also reusable in concept. It:

1. looks up the root in the sparse index;
2. visits only entries that exist;
3. enumerates children through the concrete ID type; and
4. supports depth-first or breadth-first order plus a refinement predicate.

The only reason
[`traverse()`](../../../src/terrainlib/octree/traverse.h)
is 3D is its dependency on `octree::Id` and `octree::Id::children()`.

## Creating an SF dataset from a GDAL datasource

The current SF builder creates physical mesh nodes at one requested octree
level.

```mermaid
sequenceDiagram
    participant CLI as sf-builder CLI
    participant DS as Dataset and GDAL
    participant Space as octree::Space
    participant Build as terrainbuilder
    participant Reader as RawDatasetReader
    participant Raster as Raster and RasterMask
    participant Mesh as mesh operations
    participant Store as Storage
    participant Index as IndexMap and terrain.index

    CLI->>DS: Open raster datasource
    DS-->>CLI: SRS, 2D bounds, and height range

    CLI->>Space: Transform bounds to ECEF
    Space-->>CLI: Smallest enclosing octree node

    loop Intersecting children until target level
        CLI->>Space: Node bounds and intersection tests
    end

    loop Each target-level node
        CLI->>Build: build_patch using node bounds
        Build->>Reader: Map SRS bounds to source pixels
        Reader->>DS: RasterIO band 1 as float
        Reader-->>Raster: Height raster
        Raster->>Raster: Build NoData validity mask
        Raster->>Mesh: Generate positions and triangle grid
        Mesh->>Mesh: Clip to node volume
        Mesh->>Mesh: Generate UVs and transform output SRS
        Mesh-->>Store: SimpleMesh with optional texture
        Store->>Store: MeshCodec writes node file
    end

    Store->>Index: Scan output paths
    Index->>Index: Add leaves and virtual ancestors
    Index->>Store: Write terrain.index
```

### Current components

| Responsibility | Current component |
|---|---|
| Own and open a GDAL datasource | [`Dataset`](../../../src/terrainlib/Dataset.h) |
| One-time GDAL registration | [`initialize_gdal_once()`](../../../src/terrainlib/init.cpp) |
| ECEF octree geometry | [`octree::Space`](../../../src/terrainlib/octree/Space.h) |
| Batch enumeration and orchestration | [`build_all_patches()`](../../../src/sf_builder/terrainbuilder.cpp) |
| Direct source-window reading | [`RawDatasetReader`](../../../src/sf_builder/raw_dataset_reader.h) |
| Height-to-mesh conversion | [`build_reference_mesh_patch()`](../../../src/sf_builder/mesh_builder.cpp) |
| Optional imagery | [`TileProvider`](../../../src/sf_builder/tile_provider.h) and [`texture_assembler.h`](../../../src/sf_builder/texture_assembler.h) |
| Mesh persistence | `Storage_<SimpleMesh, MeshCodec>` |
| Sparse topology | `IndexMap` |
| Index creation and serialization | `save_or_create_index()` and `terrain.index` |

For a new output, `build_all_patches()` opens unindexed storage. Individual
saves therefore do not build the index incrementally. The final
`save_or_create_index()` recursively scans the output directory, parses node
paths, and calls `IndexMap::add()`.

### More suitable 2D GDAL path

The separate
[`DatasetReader`](../../../src/tile_builder/DatasetReader.h)
is closer to what a 2D SF or raster-store builder needs:

- it accepts requested target-SRS bounds and output dimensions;
- it creates a GDAL warped VRT;
- it reprojects into the target grid; and
- it returns `radix::Raster<float>`.

[`Tiler`](../../../src/tile_builder/Tiler.h) and
[`ParallelTiler`](../../../src/tile_builder/ParallelTiler.h)
already enumerate `radix::tile::Id` values over dataset bounds.

Their grid calculations are useful, but `ParallelTileGenerator` is a
delivery-file writer rather than an SF snapshot builder.

## Mask-based SF dataset merging

```mermaid
flowchart TD
    START[Open base and new datasets as IndexedStorage]
    MASK[Read vector mask through GDAL and OGR]
    PREP[Polygon repair, sphere projection, triangulation, radial extrusion]
    ROOT[Start at octree root]
    STATUS[Read left and right NodeStatus]
    POLICY[Masked visitor evaluates node]
    RECURSE[Clip mask to node bounds and recurse into 8 children]
    LEFT[Keep base subtree]
    RIGHT[Keep new subtree]
    MERGE[Clip base outside mask and new inside mask]
    ATLAS[Combine meshes and rebuild texture atlas]
    WRITE[Write new physical node]
    COPY[Traverse unchanged source subtree]
    LINK[Hard-link each physical payload]
    INDEX[Scan and save new output index]

    START --> ROOT
    MASK --> PREP --> POLICY
    ROOT --> STATUS --> POLICY

    POLICY -->|virtual or refinement required| RECURSE
    RECURSE --> STATUS

    POLICY -->|unchanged left| LEFT --> COPY
    POLICY -->|unchanged right| RIGHT --> COPY
    COPY --> LINK

    POLICY -->|boundary node| MERGE --> ATLAS --> WRITE
    POLICY -->|no retained data| INDEX

    LINK --> INDEX
    WRITE --> INDEX
```

### Merge components

- [`Merger`](../../../src/sf_merger/merge.h) is the recursive dispatcher.
- [`NodeLoader`](../../../src/sf_merger/NodeLoader.h) reports status and loads
  payloads.
- When an exact payload is missing, `NodeLoader` searches physical ancestors
  and reconstructs the requested child by clipping the ancestor mesh. That
  reconstruction is 3D-specific.
- [`NodeData`](../../../src/sf_merger/merge/NodeData.h) lazily exposes a node
  payload to a visitor.
- [`Result`](../../../src/sf_merger/merge/Result.h) gives the driver four
  actions: recurse, ignore, preserve one source unchanged, or write a merged
  payload.
- [`Masked`](../../../src/sf_merger/merge/visitor/Masked.h) implements
  mesh-and-mask policy.
- [`NodeWriter`](../../../src/sf_merger/NodeWriter.h) writes changed nodes or
  copies unchanged subtrees.

The vector-mask pipeline in
[`mask.h`](../../../src/sf_merger/mask.h)
is almost entirely 3D and Earth-specific after OGR polygon loading:

```text
OGR polygons
  -> referenced 2D polygon mask
  -> ECEF and spherical projection
  -> triangulated surface
  -> extrusion over an Earth-radius interval
  -> closed 3D MeshMask
```

For 2D raster merging, only the OGR polygon loading and CRS transformation
ideas remain relevant. Triangulation, spherical projection, extrusion, 3D
clipping, and texture atlases should not enter the generic core.

## Hard-link behaviour

Hard links are implemented at the lowest storage layer in
[`RawStorage_::copy_from()`](../../../src/terrainlib/octree/storage/RawStorage.h):

```mermaid
flowchart TD
    COPY[copy_from node]
    EXISTS{Source payload exists?}
    EXT{Source and target extensions match?}
    DECODE[Decode source payload]
    ENCODE[Encode into target format]
    REMOVE[Remove existing target]
    DIRS[Create parent directories]
    HARDLINK[Create hard link]
    DONE[Add physical node to target index]

    COPY --> EXISTS
    EXISTS -->|no| ERROR1[FileNotFound]
    EXISTS -->|yes| EXT
    EXT -->|no| DECODE --> ENCODE --> DONE
    EXT -->|yes| REMOVE --> DIRS --> HARDLINK --> DONE
```

Properties:

- Hard linking is payload-agnostic and fully generalisable.
- It happens only when source and target filename extensions match.
- Different formats cause decode and re-encode through the codec.
- There is no copy fallback if hard-link creation fails, including across
  filesystems.
- Snapshot immutability is not enforced by `Storage_`; it is a caller-level
  convention.
- The existing target file is removed before the link is created.
- The new output receives a fresh index; `terrain.index` itself is not linked.

## Generalisation assessment

| Mechanism | Assessment | Required change |
|---|---|---|
| `NodeStatus` state model | Reuse essentially unchanged | Move outside `octree` naming |
| `IndexMap` state transitions | Generalise | Template over node key and tree traits |
| Sparse traversal | Generalise | Obtain children through tree traits |
| `Storage_<T, Codec>` | Good starting point | Also template over node key and layout |
| `RawStorage_` hard-link behaviour | Generalise | Key-neutral paths and explicit fallback policy |
| Codec concept | Reuse | No dimensional dependency |
| Cache interface | Generalise | Key type is currently `octree::Id` |
| Layout strategy | Generalise | Key-neutral path API |
| Index file | Replace or version | Record topology kind, format version, and validated key |
| Merge result algebra | Generalise | `Merged` must hold generic payload, not `SimpleMesh` |
| Recursive merge driver | Generalise | Tree traits, payload, loader/writer, and policy |
| Unchanged-subtree reuse | Reuse | Enumerate every physical state, including `Inner` |
| GDAL datasource ownership | Reuse or adapt | Typed, multi-band reads and explicit NoData |
| Mask loading through OGR | Reuse or adapt | Produce dimension-specific mask representation |
| `octree::Space` | Keep 3D | Add a sibling 2D grid or space policy |
| Mesh clipping, atlas, and UV code | Keep 3D | Do not place in generic storage |
| ECEF and spherical mask conversion | Keep 3D | 2D uses polygon classification or rasterisation |
| `MeshCodec` | Keep 3D | Add a raster chunk codec or container |

## Existing `Inner` limitation

The index supports `Inner`, but the SF merger effectively does not:

- `Merger::call_merge()` dispatches only `Missing`, `Leaf`, and `Virtual`
  combinations.
- `Masked::visit()` declares `Inner` unreachable.
- `NodeWriter::copy_subtree_to_output()` skips `Virtual` and asserts every
  other visited node is `Leaf`.

This matters because a physical coarse node coexisting with physical
descendants is exactly the `Inner` case. A general 2D/3D implementation should
make "has a physical payload" and "has indexed descendants" independent
properties and handle all four states throughout traversal and merging.

## Recommended target architecture

```mermaid
flowchart TB
    subgraph Core["Dimension-neutral hierarchical store"]
        TRAITS["TreeTraits&lt;Key&gt;<br/>root, parent, children, validation"]
        SI["SparseIndex&lt;Key&gt;"]
        TR["Traversal&lt;Key&gt;"]
        ST["Storage&lt;Key, Payload, Codec&gt;"]
        DL["DiskLayout&lt;Key&gt;"]
        ME["MergeEngine&lt;Key, Payload, Policy&gt;"]
        SNAP[Snapshot and subtree reuse]
    end

    subgraph D2["2D adapter"]
        TILE[radix::tile::Id]
        GRID[Web Mercator grid]
        RASTER[RasterChunk]
        GDAL[GDAL warped-window reader]
        MASK2[2D polygon or raster mask policy]
        RC[Raster codec and container]
    end

    subgraph D3["3D adapter"]
        OCT[octree::Id]
        ECEF[ECEF octree space]
        MESH[SimpleMesh]
        MASK3[Extruded mesh-mask policy]
        MC[MeshCodec]
    end

    TILE --> TRAITS
    OCT --> TRAITS

    RASTER --> ST
    MESH --> ST
    RC --> ST
    MC --> ST

    MASK2 --> ME
    MASK3 --> ME

    SI --> TR
    SI --> ST
    DL --> ST
    TR --> ME
    ST --> SNAP
    ME --> SNAP
```

The central abstractions should therefore be:

1. `TreeTraits<Key>`: root, parent, children, maximum depth, and key
   validation.
2. `SparseIndex<Key>`: the current `IndexMap` algorithm.
3. `Traversal<Key, TreeTraits>`: DFS, BFS, and refinement independent of child
   count.
4. `DiskLayout<Key>`: key-to-path and path-to-key conversion.
5. `Storage<Key, Payload, Codec>`: logical storage and index maintenance.
6. `MergeEngine<Key, Payload, Policy>`: paired sparse-tree walking.
7. Generic merge actions such as `Recurse`, `Ignore`, `KeepLeft`, `KeepRight`,
   and `Write<Payload>`.
8. A snapshot copier that hard-links unchanged physical nodes without knowing
   their payload type.
9. Separate 2D and 3D spatial, mask, and fallback policies.

The GDAL builder and mask merger should be clients of this core, not part of
it.
