# 2D/3D hierarchical store refactor plan

Status: proposal for review. This document is an implementation plan, not a
record of completed work.

## Purpose

This plan describes how to extract the existing octree-specific index, traversal, storage, codec, and subtree-reuse code into a shared 2D/3D store. The refactor must preserve existing 3D datasets while enabling raster-fundamentalis storage without duplicating infrastructure.

## Decisions already made

- The shared implementation will live in `src/terrainlib/store` and use the
  `store` namespace.
- The 2D implementation will live in `src/terrainlib/raster_store` and use the
  `raster_store` namespace.
- Existing 3D Structura Fundamentalis datasets must remain readable and
  writable without changing their on-disk contract.
- 3D compatibility includes both existing path layouts:
  `flat` and `level_and_coordinate_directories`.
- The 2D raster tile format is a separate format. Requirements in
  this directory apply to that 2D format and must not be retrofitted onto
  existing 3D datasets. The storage format for 2d tiles is defined in storage-format.md
- A path layout strategy should contain a stable identifier and two
  operations: key to extensionless `NodePath`, and `NodePath` to key. It
  should not require an inheritance hierarchy, RTTI, global
  self-registration, or heap allocation.
- A configured codec owns all filename endings and maps one `NodePath` to one
  or more physical files. `Codec::paths()` must not need a payload.
- Codecs are stateful runtime objects behind a small interface. They may
  support reading, writing, or both; an unsupported operation throws. reading and writing must be reentrant (callable concurrently from different threads).
- `copy_from()` hard-links every file when the input and output codecs return
  the same path list for a common dummy `NodePath`. Otherwise it decodes with
  the input codec and encodes with the output codec. Callers can force
  re-encoding.
- 3D geometry, ECEF bounds, mesh codecs, mesh reconstruction, mask geometry,
  and raster-specific processing remain outside the shared store.

## Goals

1. Use one sparse hierarchy implementation for `octree::Id` and
   `radix::tile::Id`.
2. Use one traversal, storage, cache, runtime codec boundary, and
   unchanged-subtree copier for 2D and 3D.
3. Make paired-tree walking reusable without putting mesh or raster merge
   policy into the shared layer.
4. Replace the current layout-strategy class hierarchy with small path-mapping
   values backed by function pairs.
5. Allow one logical node payload to consist of multiple files without making
   layouts aware of those files.
6. Preserve all valid existing 3D index files and payload paths.
7. Introduce the 2D storage adapter without inventing unspecified raster file
   details.
8. Land the refactor in small, testable steps. Every phase should build and
   pass tests before the next phase begins.

## Non-goals

- Changing `octree::Id`, `octree::Space`, `IdRect`, `OddLevelShifted`, or
  other 3D spatial calculations.
- Defining or implementing GDAL ingestion, raster resampling, filtering,
  source selection, or mask rasterisation.
- Defining the `.amort` payload or source-attribution-table serialization
  beyond the requirements already in [storage-format.md](storage-format.md).
- Implementing an `rf_builder`, `rf_merger`, tile-base generator, or tile
  server in this refactor.
- Changing the existing 3D hard-link policy by adding a silent file-copy
  fallback.
- Refactoring unrelated octree, DAG, mesh, or tile-builder code.

## Compatibility contract

Before moving code, tests must lock down the following 3D behaviour:

| Item | Required compatibility |
|---|---|
| Index filename | `terrain.index` |
| Index field order | layout ID, preferred extension, index map |
| Node-key encoding | existing `octree::Id` level/index serialization |
| Node-status encoding | `Leaf = 0`, `Inner = 1`, `Virtual = 2` |
| Flat layout ID | `flat` |
| Flat path | `<level>-<index><extension>` |
| Coordinate layout ID | `level_and_coordinate_directories` |
| Coordinate path | `<level>/<x>/<y>/<z><extension>` |
| Default layout | existing level/coordinate layout |
| Layout detection | both existing layouts remain detectable |
| Codec selection | legacy preferred extension selects terrain or configured glTF codec |
| Equal codec path lists | hard-link every file, or report an explicit error |
| Different codec path lists | decode with input codec and encode with output codec |

Compatibility means that the refactored code can open datasets written before
the refactor and produces datasets that the pre-refactor code can open. Exact
byte-for-byte rewriting of an unordered index map is not required, but the
serialized schema and values must remain compatible.

The 3D index disk type should remain a versioned 3D adapter. The shared store
must not add a topology field, new header, checksum, or compression layer to
`terrain.index`.

## Proposed source boundary

```text
src/terrainlib/
├── store/
│   ├── NodeStatus.h
│   ├── NodeStatusOrMissing.h
│   ├── Traits.h
│   ├── Index.h
│   ├── traverse.h
│   ├── NodePath.h
│   ├── PathMapping.h
│   ├── Layout.h
│   ├── Codec.h
│   ├── CopyError.h
│   ├── RawStorage.h
│   ├── Storage.h
│   ├── IndexedStorage.h
│   ├── copy_subtree.h
│   ├── cache/
│   │   ├── Interface.h
│   │   ├── Dummy.h
│   │   └── Lru.h
│   └── merge/
│       ├── Action.h
│       └── walk.h
├── mesh/
│   └── codec/
│       ├── Terrain.h
│       └── Gltf.h
├── octree/
│   ├── Id.h
│   ├── StoreTraits.h
│   ├── store_layout/
│   │   ├── Flat.h
│   │   ├── LevelAndCoordinateDirectories.h
│   │   └── Mappings.h
│   └── storage/
│       ├── IndexFile.h
│       └── open.h
└── raster_store/
    ├── StoreTraits.h
    ├── IndexFile.h
    ├── Storage.h
    ├── codec/
    │   ├── Arft.h
    │   └── Debug.h
    └── store_layout/
        └── Zxy.h
```

The exact file grouping may be collapsed if a file would only contain a few
lines. The important boundaries are:

- `store` contains dimension- and payload-neutral mechanisms;
- `mesh::codec` contains the separately configured terrain and glTF codecs;
- `octree` contains the 3D format and key adapters;
- `raster_store` contains the new 2D format, key adapters, and raster codecs;
  and
- subdirectory names match their namespaces where a subnamespace is used.

Temporary forwarding headers and aliases under `octree` are allowed during
migration. They must not contain a second implementation.

## Shared interfaces

The names below are the intended shape, not signatures that must be copied
verbatim without testing.

### Hierarchy traits

The store should be parameterized by one traits type rather than assuming that
all key classes expose identical member functions:

```cpp
template<typename Traits>
concept HierarchyTraits = requires(typename Traits::Key key) {
    typename Traits::Key;
    typename Traits::Hasher;
    { Traits::root() } -> std::same_as<typename Traits::Key>;
    { Traits::parent(key) };
    { Traits::children(key) };
    { Traits::is_valid(key) } -> std::same_as<bool>;
};
```

The concrete names should be:

```cpp
store::Index<octree::StoreTraits>
store::Index<raster_store::StoreTraits>
```

`octree::StoreTraits` adapts the existing optional parent/children API without
changing `octree::Id`.

`raster_store::StoreTraits` adapts `radix::tile::Id` and must:

- treat zoom zero as the only root;
- never call `radix::tile::Id::parent()` at zoom zero, where it underflows;
- reject coordinates outside `[0, 2^zoom)`;
- define a supported maximum zoom without an overflowing shift;
- use `radix::tile::Id::Hasher`; and
- accept only one canonical XYZ/Slippy interpretation at the persistent
  boundary.

The shared code must obtain roots, parents, children, validation, and hashing
through the traits. It must not use dimension checks or specialize behaviour
on key types internally.

### Sparse index and traversal

Move the existing four-state model to:

```cpp
store::NodeStatus
store::NodeStatusOrMissing
store::Index<Traits>
store::traverse(index, visitor, refine, root, order)
```

The index algorithm remains the current one:

- adding a physical descendant creates virtual ancestors;
- adding a payload to a virtual node makes it `Inner`;
- removing a payload from an `Inner` node makes it `Virtual`;
- removing the final descendant collapses virtual ancestors; and
- a physical parent becomes `Leaf` after its last descendant is removed.

Traversal must follow only indexed nodes and use `Traits::children`. Child
order is the order supplied by the traits and is therefore deterministic per
hierarchy, not universally fixed by `store`.

### Node paths and path mappings

`store::NodePath` is an extensionless logical location for one hierarchy
node. For example:

```text
octree flat          12-123456
octree coordinates   12/34/56/78
raster-store ZXY     12/2200/1400
```

It does not necessarily name a physical file. Replace
`octree::disk::layout::Strategy` and
`octree::disk::layout::StrategyRegister` with a value similar to:

```cpp
template<typename Key>
struct store::PathMapping {
    std::string_view id;
    NodePath (*key_to_node_path)(const Key&);
    std::optional<Key> (*node_path_to_key)(const NodePath&);
};
```

`store::Layout<Key>` owns the base directory and one `PathMapping<Key>`. It
does not own a preferred extension. A configured codec expands the
extensionless `NodePath` into the physical file or files.

The stable ID is format metadata, not a third strategy operation. The
dimension adapters provide ordinary lookup functions:

```cpp
octree::store_layout::flat()
octree::store_layout::level_and_coordinate_directories()
octree::store_layout::from_id(id)
octree::store_layout::all()

raster_store::store_layout::zxy()
raster_store::store_layout::from_id(id)
```

This retains runtime selection from an index file while removing virtual
dispatch, RTTI type-to-ID lookup, static registration, and ownership through
`unique_ptr`.

Path parsers validate the complete logical `NodePath`, not a file ending.
Codec or format-adapter code removes and validates physical file endings
before asking the layout to recover a key. Invalid disk input returns an error
or `nullopt`; it must not trigger an assertion.

### Codec interface

A codec is a configured runtime object for one logical payload type. It owns
all physical filename endings and may map one `NodePath` to several files:

```cpp
template<typename NodeData>
class store::Codec {
public:
    virtual ~Codec() = default;

    virtual std::vector<std::filesystem::path>
    paths(const NodePath& node_path) const = 0;

    virtual NodeData read(const NodePath&) const {
        throw UnsupportedCodecOperation{"read"};
    }

    virtual void write(
        const NodePath&,
        const NodeData&) const {
        throw UnsupportedCodecOperation{"write"};
    }
};
```

Concrete codecs contain their configuration and are constructed before
storage use. Unsupported read or write operations may use the base
implementation and throw at runtime.

`Codec::paths()` has the following contract:

- it needs no NodeData payload and performs no filesystem access;
- it returns every physical file belonging to the logical node;
- results depend only on codec configuration and the supplied `NodePath`;
- result order is stable and pairs corresponding input/output files;
- two codecs returning the same path list for the same `NodePath` must produce
  mutually compatible files; and
- different artifact counts or filename endings produce different lists.

Examples:

```text
Terrain codec
  12/34/56/78
    -> 12/34/56/78.terrain

glTF codec configured for binary output
  12/34/56/78
    -> 12/34/56/78.glb

glTF codec configured for JSON output
  12/34/56/78
    -> 12/34/56/78.gltf

Debug raster codec configured for JPEG data and PNG attribution
  12/2200/1400
    -> 12/2200/1400.data.jpg
    -> 12/2200/1400.attribution.png
```

The mesh side has separate terrain and glTF codecs because they use different
format implementations. Binary `.glb` and JSON `.gltf` remain configurations
of one glTF codec because both use the same `cgltf` implementation.

The raster side has similarly shaped codecs specialized on PixelType:

```cpp
template <typename PixelType> struct raster_store::codec::Amort<PixelType> : store::Codec<raster_store::Tile<PixelType>> { .. };
template <typename PixelType> struct raster_store::codec::Debug<PixelType> : store::Codec<raster_store::Tile<PixelType>> { .. };
```

`Amort` supports reading and writing. `Debug` supports writing only and may
contain runtime options for data format, attribution format, JPEG quality, or
similar debugging choices. It is not template-composed from separate image
codec types.


### Storage and format adapters

Generalize storage over traits and NodeData. It owns a configured codec through
the runtime interface:

```cpp
store::RawStorage<Traits, NodeData>
store::Storage<Traits, NodeData>
store::IndexedStorage<Traits, NodeData>
store::cache::Interface<Traits, NodeData>
```

The NodeData codecs remain with their domains under `mesh::codec` and
`raster_store::codec`.

Index serialization is not a responsibility of `store::Index`. Opening and
saving a dataset receives a dimension-specific format adapter which provides:

- the index filename;
- index read/write conversion;
- mapping lookup by stable ID;
- the default mapping; and
- legacy directory discovery where it is required.

This adapter may be a compile-time policy or a small value of function
pointers. Choose the smaller implementation after the Phase 0 tests exist.
It must not reintroduce a layout class hierarchy or global registration.

For 3D, the adapter reads and writes the current `octree` index DTO unchanged.
Its legacy `preferred_extension` field selects the configured codec:

```text
.terrain -> terrain codec
.glb     -> glTF codec with binary container
.gltf    -> glTF codec with JSON container
```

Legacy unindexed-directory discovery remains in the 3D adapter: it recognizes
the known codec endings, removes them to obtain a `NodePath`, and then invokes
the selected layout parser. The generic layout does not recover keys directly
from codec-owned file paths.

For 2D, the adapter reads and writes a separately versioned
`raster_store::v1` DTO using the serialization envelope required by
[storage-format.md](storage-format.md).

Automatic dirty-index saving currently happens in the 3D storage destructor.
Preserve that behaviour for existing 3D entry points during the migration.
The new 2D snapshot API should require an explicit finalization/publication
step; a destructor must not make an incomplete snapshot authoritative.

### Copying and unchanged-subtree reuse

There are two separate responsibilities in the current implementation:

1. `sf_merger::NodeWriter` traverses a source subtree and decides which
   indexed nodes to reuse.
2. `octree::Storage::copy_from()` delegates to
   `octree::RawStorage::copy_from()`, where
   `std::filesystem::create_hard_link()` performs the actual hard link and the
   target index is updated on success.

The filesystem hard-link implementation is therefore already in terrainlib.
This refactor shall move the payload-neutral subtree traversal and copy
orchestration out of `sf_merger`.

The current call chain is:

```text
sf_merger decides to keep a source subtree unchanged
    -> NodeWriter traverses the source index
    -> octree::Storage::copy_from()
    -> octree::RawStorage::copy_from()
    -> create_hard_link(), or decode/encode when formats differ
```

The target call chain becomes:

```text
merge policy decides to keep a source subtree unchanged
    -> store::copy_subtree()
    -> store::Storage::copy_from()
    -> hard-link every codec path, or decode/encode
```

`Storage::copy_from()` remains the operation for copying one logical node.
Add:

```cpp
struct CopyOptions {
    bool force_reencode = false;
};
```

For one key, `copy_from()`:

1. calls the input and output `Codec::paths()` with the same fixed dummy
   `NodePath`;
2. when the lists are equal and `force_reencode` is false, calls both codecs
   again with their actual source and target `NodePath` values and hard-links
   every source path to the corresponding target path;
3. when the dummy lists differ or re-encoding is forced, reads the payload
   with the input codec and writes it with the output codec; and
4. updates the target index only after all links or the write complete.

The dummy path must be fixed and collision-free, for example
`__codec_probe__/node`. Path lists are compared exactly, including count,
order, and filename endings.

If linking several files fails partway through, remove the target links
created by that call before returning the error. There is no silent copy
fallback. An unsupported read or write needed for re-encoding throws through
the codec interface.

Codec settings that do not change `paths()`, such as compression level or
JPEG quality, do not force re-encoding by default. A caller that needs those
settings applied to every node passes `force_reencode = true`.

Hard-link rules:

- never modify an existing linked payload in place;
- a matching codec path list hard-links every file;
- a different path list decodes with the input codec and encodes with the
  output codec;
- `force_reencode` always selects decode/encode;
- hard-link failure is explicit;
- 2D snapshot tools preflight that source and destination support hard links
  before a long operation starts; and
- no silent file-copy fallback is introduced.

#### Shared subtree copier

Move the payload-neutral traversal in
`sf_merger::NodeWriter::copy_subtree_to_output()` to a shared operation such
as:

```cpp
std::expected<void, CopyError>
store::copy_subtree(
    const IndexedStorage& source,
    Storage& target,
    const Key& root,
    CopyOptions options = {});
```

The shared operation:

1. traverses an indexed source subtree;
2. skips `Virtual` nodes;
3. calls `copy_from()` for physical payloads in both `Leaf` and `Inner`
   states;
4. continues traversal below `Inner` nodes; and
5. returns copy failures to its caller instead of converting them into an
   assertion or immediate process termination.

The operation is payload-neutral because it only interprets hierarchy status
and delegates each physical node to `Storage::copy_from()`. It does not know
about meshes, rasters, masks, attribution, or their encodings.

#### `Leaf`, `Inner`, and `Virtual`

The sparse index distinguishes payload presence from descendant presence:

| Status | Physical payload | Indexed descendants |
|---|---:|---:|
| `Leaf` | yes | no |
| `Inner` | yes | yes |
| `Virtual` | no | yes |

`Missing` is represented by absence from the sparse index and is not visited
by subtree traversal.

The current `NodeWriter` callback effectively does:

```cpp
if (status == NodeStatus::Virtual) {
    return;
}

DEBUG_ASSERT(status == NodeStatus::Leaf);
DEBUG_ASSERT_VAL(target.copy_from(id, source));
```

The `DEBUG_ASSERT(status == Leaf)` is an incorrect assumption. `Inner` is
also a physical state and its payload must be copied. The existing
`IndexMap::add()` creates an `Inner` node whenever a physical `Leaf` gains a
physical descendant. Such a hierarchy is valid in 3D and is explicitly
required for raster-fundamentalis, where a coarse physical tile may coexist
with more accurate descendants.

For example:

```text
zoom 10 physical tile       -> Inner
└── zoom 11 physical tile   -> Leaf
```

Reusing this subtree must preserve both payloads. Skipping the `Inner`
payload would lose the coarse fallback; asserting on `Inner` rejects a valid
hierarchy.

The shared copier shall instead handle status as:

```cpp
switch (status) {
case NodeStatus::Virtual:
    break;
case NodeStatus::Leaf:
case NodeStatus::Inner:
    target.copy_from(id, source, options);
    break;
}
```

Traversal shall continue below the `Inner` node. Copying the parent first adds it
to the target as a `Leaf`; copying its descendant then promotes the parent to
`Inner`, reconstructing the source topology through the normal index
transitions.

The existing paired merger has a related limitation: its dispatcher only
handles `Missing`, `Leaf`, and `Virtual` pairs and sends any pair containing
`Inner` to `UNREACHABLE()`. The paired hierarchy-walking work below must
handle or explicitly reject all 16 status combinations without treating
valid input data as an impossible program state.

#### Error propagation

The lower storage layer already represents ordinary copy failures, including
missing source files, directory creation failure, hard-link failure, decode
failure, and encode failure. The current `NodeWriter` consumes
`Storage::copy_from()` with `DEBUG_ASSERT_VAL`, while some overwrite paths
terminate through `LOG_ERROR_AND_EXIT()`. Because
`copy_subtree_to_output()` returns `void`, the merge caller cannot report or
handle these failures.

The shared copier must return the failure through `copy_subtree()` and the
merge call chain until the application boundary can report it with the
affected key and path. An unsupported codec read or write may propagate as
the codec's runtime exception. Assertions remain appropriate for internal
invariants, but filesystem conditions, unsupported conversions, malformed
datasets, and overwrite conflicts are operational errors rather than
assertion failures.

### Paired hierarchy walking

Extract only the dimension-neutral control flow from `sf_merger::Merger`.
The shared walker obtains the left and right status for a key and asks a
policy for one of:

```cpp
store::merge::Recurse<Context>
store::merge::Ignore
store::merge::KeepLeft
store::merge::KeepRight
store::merge::Write<Payload>
```

The walker owns recursion and unchanged-subtree reuse. The policy owns
selection and payload combination.

All 16 combinations of `Missing`, `Leaf`, `Inner`, and `Virtual` must be
handled. Unsupported combinations may return a typed error, but they must not
fall into `UNREACHABLE()`.

The existing 3D adapter retains:

- `NodeLoader` ancestor mesh reconstruction;
- ECEF node bounds;
- mesh masks and clipping;
- mesh combination and texture atlas generation; and
- mesh validation and auxiliary texture writes.

A future 2D merger can supply a raster policy without changing the shared
walker. Implementing that policy is outside this refactor.

## Implementation phases

Each phase should be one reviewable commit unless the tests and implementation
are clearer as two commits. Do not begin a later phase while the current phase
has failing tests.

### Phase 0 — Capture current compatibility

No production behaviour changes.

1. Add golden 3D fixtures created by the current code:
   - one `terrain.index` using `flat`;
   - one using `level_and_coordinate_directories`;
   - physical payload paths for a root, child, and deeper descendant; and
   - an index containing `Leaf`, `Virtual`, and `Inner`.
2. Test that both fixtures open, resolve the expected IDs and extensions, and
   traverse the expected sparse nodes.
3. Add path round-trip tests for boundary IDs and both layouts.
4. Add storage tests for:
   - matching-extension hard links;
   - different-extension decode/re-encode;
   - `.terrain`, `.glb`, and `.gltf` dispatch;
   - overwrite rejection;
   - indexed and unindexed opens; and
   - final index creation by directory scan.
5. Record the pre-refactor public aliases used by `sf_builder`, `sf_merger`,
   `sf_index_browser`, `dag_builder`, and `dag_convert_debug`.

Exit criterion: the compatibility tests pass against the untouched
implementation and fail when any stable filename, layout ID, path encoding,
status value, or index field order is deliberately changed.

### Phase 1 — Extract topology into `store`

1. Move `NodeStatus` and `NodeStatusOrMissing` to `store`, preserving their
   underlying values and serialization.
2. Introduce the hierarchy-traits concept and `octree::StoreTraits`.
3. Convert `IndexMap` into `store::Index<Traits>`.
4. Convert traversal into `store::traverse`.
5. Add `raster_store::StoreTraits` for `radix::tile::Id`.
6. Run the same index-transition and DFS/BFS tests with both trait types.
7. Provide temporary `octree` aliases so downstream migration is separate
   from the algorithm extraction.

Exit criterion: 2D and 3D keys pass the same topology suite; existing 3D
callers still build through aliases; no filesystem code has changed.

### Phase 2 — Replace disk layout strategies

1. Add `store::NodePath`, `store::PathMapping<Key>`, and
   `store::Layout<Key>`.
2. Port the two existing 3D layouts to ordinary function pairs without
   changing stable IDs. The mappings return `level-index` and
   `level/x/y/z` without file endings.
3. Replace the singleton strategy registry with explicit `from_id()` and
   `all()` functions in the 3D adapter.
4. Move the legacy preferred extension out of generic `Layout` and retain it
   in the 3D format adapter.
5. Port legacy layout discovery so it strips recognized 3D file endings before
   calling `node_path_to_key()`.
6. Add the proposed 2D `z/x/y` mapping only after the review decision listed
   below is resolved. The raster codec, not the mapping, adds `.arft` or debug
   endings.
7. Switch node-path and layout-detection tests to the new implementation.
8. Delete the old strategy base class, registration machinery, and concrete
   strategy classes once no call site uses them.

Exit criterion: the legacy 3D adapter plus codec resolves both fixtures to
identical physical payload paths; generic `Layout` contains no extension;
there is no layout inheritance, RTTI lookup, static registrar, or owning
strategy pointer.

### Phase 3 — Generalize storage and index lifecycle

1. Add the stateful `store::Codec<Payload>` interface with `paths()`, `read()`,
   and `write()`.
2. Split the current extension-dispatching `octree::MeshCodec` into:
   - a terrain codec; and
   - one glTF codec configured for binary `.glb` or JSON `.gltf`.
3. Move copy error, raw storage, caches, logical storage, and indexed storage
   into `store`.
4. Make storage own a configured `std::unique_ptr<Codec<Payload>>`; remove the
   codec template parameter from storage.
5. Replace every embedded `octree::Id` with `Traits::Key`.
6. Make every raw file operation obtain its complete file list through
   `Codec::paths()`. `has()` requires every listed file, and `remove()` removes
   every listed file.
7. Keep payload codecs outside the shared module under `mesh::codec` and
   `raster_store::codec`.
8. Split generic index maintenance from 3D index serialization and legacy
   folder discovery.
9. Keep the current 3D `terrain.index` DTO and open functions as compatibility
   adapters over the shared storage. Map its preferred extension to a
   configured terrain or glTF codec.
10. Migrate the existing octree storage aliases and all application callers.
11. Preserve the current 3D destructor-save behaviour until all callers have
    explicit index finalization.

Add focused codec tests using single-file, multi-file, read/write, and
write-only test codecs before depending on the raster payload implementation.

Exit criterion: all existing applications build and all Phase 0 fixtures pass
through the shared runtime codec and storage implementation. No
extension-dispatching mesh codec or second storage implementation remains
under `octree`.

### Phase 4 — Generalize subtree reuse and paired walking

1. Change `Storage::copy_from()` to compare input and output codec path lists
   for the fixed dummy `NodePath`.
2. Hard-link all actual files when the lists match, with cleanup of links
   created by a partially failed call.
3. Decode with the input codec and encode with the output codec when lists
   differ.
4. Add `CopyOptions::force_reencode`, defaulting to false.
5. Test:
   - one-file hard linking;
   - multi-file hard linking;
   - different path counts and endings;
   - forced re-encoding with otherwise equal paths;
   - conversion between terrain and glTF;
   - conversion into a write-only codec; and
   - runtime failure when a required codec operation is unsupported.
6. Add the shared unchanged-subtree copier.
7. Test copies containing `Leaf`, `Virtual`, and `Inner` nodes.
8. Add the paired hierarchy walker and typed actions.
9. Cover all 16 status pairs with table-driven tests.
10. Adapt the 3D merger to the shared walker while keeping mesh policy in
   `sf_merger`.
11. Remove generic recursion and copy logic from `sf_merger::Merger` and
   `NodeWriter`.
12. Add a 3D integration test proving an unchanged subtree is hard-linked and
   a changed boundary node is newly written.

Exit criterion: the existing 3D merger behaviour is preserved, `Inner` no
longer reaches `UNREACHABLE()`, multi-file reuse works through
`Codec::paths()`, and the shared walker contains no mesh, ECEF, GDAL, OpenCV,
or raster dependencies.

### Phase 5 — Add the 2D raster-fundamentalis adapter

This phase starts only after the unresolved 2D format decisions below are
edited into decisions in this document.

1. Add the checked 2D persistent-key conversion around `radix::tile::Id`.
2. Add the chosen 2D path mapping and stable ID.
3. Define the versioned 2D index DTO and index filename.
4. Use the required magic/version/checksum/compression serialization envelope.
5. Add the 2D format adapter and storage aliases under
   `raster_store`.
6. Add `raster_store::codec::Arft<Payload>` with runtime format options when
   the final `.arft` serialization is available.
7. Add the output-only `raster_store::codec::Debug<Payload>` with runtime
   options for its data and attribution files.
8. If the final `.arft` serialization is not available, exercise storage with
   the Phase 3 test codec and do not make `.arft` claims from it.
9. Test:
   - invalid and boundary tile IDs;
   - index serialization and validation;
   - `Leaf`/`Inner` coexistence;
   - sparse traversal and ancestor lookup;
   - path round trips;
   - snapshot hard-link reuse;
   - ARFT-to-debug output conversion; and
   - explicit cross-filesystem/preflight failure.

Exit criterion: the same shared store can create, open, traverse, and reuse a
minimal 2D raster-fundamentalis fixture without changing the 3D fixtures.

### Phase 6 — Cleanup and documentation

1. Remove temporary forwarding headers that no repository caller needs.
2. Remove obsolete files under `octree/disk` and the old generic
   implementation under `octree/storage`.
3. Keep only 3D key, format, codec, and compatibility adapters under
   `octree`.
4. Update includes, CMake source lists, and precompiled-header includes.
5. Update [architecture.md](architecture.md),
   [status-quo.md](status-quo.md), and the before-refactor report with links to
   the implemented boundary. Preserve the before-refactor report as history;
   do not rewrite it as if it described the new code.
6. Document the final public names and a minimal 2D/3D opening example.

Exit criterion: repository search finds no generic implementation tied to
`octree::Id`; all tests pass; the old layout strategy hierarchy is gone.

## Test and verification plan

Tests should live in the existing `unittests_terrainlib` target. Suggested
files:

```text
unittests/terrainlib/store_index.cpp
unittests/terrainlib/store_traverse.cpp
unittests/terrainlib/store_layout.cpp
unittests/terrainlib/store_codec.cpp
unittests/terrainlib/store_storage.cpp
unittests/terrainlib/store_compatibility.cpp
unittests/terrainlib/store_merge_walk.cpp
```

During implementation:

1. Build in `$source_dir/build/$config_name`.
2. Run unit tests from that build directory.
3. Run the focused store tests after each edit.
4. Run the full `unittests_terrainlib` target at every phase boundary.
5. Build `sf_builder`, `sf_merger`, `sf_index_browser`, `dag_builder`, and
   `dag_convert_debug` after their storage aliases move.
6. Run any existing merger integration fixture after Phase 4.
7. Inspect `git diff --check` and the final worktree before each commit.

No formatting-only pass or unrelated refactor belongs in these commits.

## Expected migration map

| Current code | Target |
|---|---|
| `octree/NodeStatus.h` | `store/NodeStatus.h` plus temporary alias |
| `octree/NodeStatusOrMissing.h` | `store/NodeStatusOrMissing.h` plus temporary alias |
| `octree/IndexMap.*` | `store/Index.h` |
| `octree/traverse.h` | `store/traverse.h` |
| complete node paths embedded in layouts | extensionless `store/NodePath.h` plus codec endings |
| `octree/disk/Layout.h` | `store/Layout.h` |
| `octree/disk/layout/Strategy.h` | `store/PathMapping.h` |
| `StrategyRegister.h` | explicit dimension-adapter lookup functions |
| `strategy/Flat.h` | `octree/store_layout/Flat.h` |
| `strategy/LevelAndCoordinateDirectories.h` | `octree/store_layout/LevelAndCoordinateDirectories.h` |
| `octree/storage/cache/*` | `store/cache/*` |
| `octree/storage/codec/Codec.h` | runtime `store/Codec.h` |
| `octree/storage/codec/MeshCodec.h` | `mesh/codec/Terrain.h` and configured `mesh/codec/Gltf.h` |
| `octree/storage/RawStorage.h` | `store/RawStorage.h` |
| `octree/storage/Storage.h` | `store/Storage.h` |
| `octree/storage/IndexedStorage.h` | `store/IndexedStorage.h` |
| `octree/storage/helpers.*` | generic scan helpers plus 3D format adapter |
| `octree/disk/IndexFile.h` | versioned 3D format adapter under `octree` |
| `sf_merger::NodeWriter` subtree loop | `store/copy_subtree.h` |
| `sf_merger::Merger` recursion | `store/merge/walk.h` |

## Risks and controls

| Risk | Control |
|---|---|
| Existing indexes stop loading | Golden pre-refactor fixtures and unchanged 3D DTO |
| Valid legacy paths are parsed differently | Characterization and round-trip tests before replacement |
| Template migration creates a large unreviewable diff | Compatibility aliases and phase-by-phase caller migration |
| `radix::tile::Id` root underflows | Traits intercept root parent lookup |
| Invalid 2D coordinates become persistent | Validate on every disk/API boundary |
| `Inner` payloads are lost during subtree reuse | Copy every physical status and test mixed-depth fixtures |
| Linked snapshots are modified in place | Immutable snapshot API and overwrite-disabled output |
| Multi-file hard linking fails partway through | Remove links created by the failed `copy_from()` before returning |
| Incompatible codecs return the same path list | Treat path-list equality as a codec contract and test every concrete codec pairing |
| Output-only codec is selected for required input | Throw a clear unsupported-operation error at runtime |
| Hard-link failure appears late | 2D operation preflight and explicit errors |
| Shared code accumulates mesh/raster policy | Dependency tests/review against the source boundary |
| Generic index accidentally dictates both disk formats | Separate 3D and 2D format adapters |

## Decisions required before Phase 5

These are intentionally unresolved because the current raster-store documents
mark them as unclear:

1. **2D index filename:** choose the filename used inside a
   raster-fundamentalis snapshot.
   => call it raster_store.index.
2. **2D node and payload paths:** confirm the extensionless `z/x/y`
   `NodePath`, the ARFT codec's resulting `z/x/y.arft` file, and whether
   chunks live directly under the snapshot or below a `chunks/` directory.
   => I already renamed .arft into .amort (alpine maps org raster tile) in several places. look for places i missed and rename it. the chunks / tiles shall live under a path defined by the store_layout, the default 2d store layout shall put them into z/x/y, not in chunks/z/x/y.
3. **2D layout ID:** choose the stable string serialized in the index.
   => "zoom/x/y_google"
4. **Maximum zoom:** choose the supported persistent range and integer widths
   for zoom, x, and y.
   => use the tile id from radix. x and y are unsigned 32, that would give maximum zoom 31 or 32? that should be well enough.
5. **Index contents:** decide whether v1 stores only sparse status entries or
   also derived aggregate metadata. The first implementation should omit
   derivable metadata unless a concrete query requires it.
   => is this a question?
6. **Index envelope constants:** assign the 2D index magic number, compression
   choice, and version according to the common serialization rules.
   => erm, question?
7. **Publication:** define whether snapshot completion uses an atomic rename,
   a manifest marker, or an external store-level operation. The generic
   storage layer should expose finalization but not invent store-level
   lifecycle policy.
   => use an atomic rename, but don't hide the file. use a .part extension to the new directory name, and then remove it. make sure to flush and close all files before renaming.

Until these decisions are made, Phases 0–4 can complete and the shared
implementation can be proven with `radix::tile::Id` in memory and with
temporary-directory tests. No provisional 2D disk format should escape into
production data.
