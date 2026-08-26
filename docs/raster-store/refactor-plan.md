# 2D/3D hierarchical store refactor plan

Status: proposal for review. This document is an implementation plan, not a
record of completed work.

## Purpose

This plan describes how to extract the existing octree-specific index,
traversal, storage, and codec code into a shared 2D/3D store. The refactor
must preserve existing 3D datasets and prove, using test-only mappings and
codecs where necessary, that the shared mechanisms work with a 2D key.
Persistent raster-fundamentalis formats, adapters, and tools are later work.

## Decisions already made

- The shared implementation will live in `src/terrainlib/store` and use the
  `store` namespace.
- The minimal 2D traits adapter used to exercise the shared hierarchy will
  live in `src/terrainlib/raster_store` and use the `raster_store` namespace.
- Common Structura Fundamentalis validation and errors will live in
  `src/terrainlib/sf` and use the `sf` namespace.
- Existing 3D Structura Fundamentalis datasets must remain readable and
  writable without changing their on-disk contract.
- DAG datasets written by the current code when Phase 0 begins must remain
  readable and writable throughout the refactor without changing their index
  or payload serialization. Older DAG payload schemas are not supported.
- `Inner` is a valid shared topology state and is supported by DAG,
  raster-fundamentalis, and tile-base datasets. It is not valid in Structura
  Fundamentalis datasets.
- SF producer and processing boundaries validate indexed SF data and return a
  typed `sf::InvalidTopology` error containing an offending key when `Inner`
  is present. The diagnostic `sf_index_browser` is exempt. This is an SF data
  invariant, not a generic store or octree-format rule.
- 3D compatibility includes both existing path layouts:
  `flat` and `level_and_coordinate_directories`.
- A persistent 2D raster tile format is not defined by this refactor.
  [architecture.md](architecture.md) and
  [storage-format.md](storage-format.md) describe intended direction and
  provisional requirements that will be finalized in later RF work. They must
  not be retrofitted onto existing 3D datasets.
- A path layout strategy should contain a stable identifier and two
  operations: key to extensionless `NodePath`, and `NodePath` to key. It
  should not require an inheritance hierarchy, RTTI, global
  self-registration, or heap allocation.
- A configured codec owns all filename endings and maps one `NodePath` to one
  or more physical files. `Codec::paths()` must not need a payload.
- Codecs are stateful runtime objects behind a small interface. They may
  support reading, writing, or both. Reading and writing return
  `std::expected`; unsupported operations and other operational failures are
  reported as error values. Reading and writing must be reentrant (callable
  concurrently from different threads).
- The runtime glTF codec catches exceptions from the existing glTF mesh writer
  and converts them to `CodecError`; changing the mesh I/O API is out of scope.
- `store::RawStorage<Traits, NodeData>` exclusively owns the configured
  `std::unique_ptr<store::Codec<NodeData>>`. `store::Storage` owns the raw
  storage and therefore owns the codec transitively. Storage consumers do not
  receive a codec template parameter and application call sites do not manage
  codec objects.
- This refactor does not add synchronization to storage, indexes, or caches
  and does not change their concurrency guarantees. Any concurrency bug fix is
  separate work.
- Application callers retain their existing storage lifecycle,
  synchronization, and failure-handling behaviour unless explicitly changed
  by this plan.
- Preserve `dag::ThreadSafeStorage` as the caller-side synchronization around
  DAG output storage. DAG storage remains cacheless while shared-lock reads are
  used.
- Legacy index metadata selects a codec through an explicit, caller-supplied
  resolver supplied by the payload-domain opening function. The octree format
  adapter does not contain a global codec registry or depend on mesh or DAG
  payload types. Application-level storage consumers do not call the resolver
  or handle the resulting codec object.
- Dimension-specific index persistence uses a small runtime
  `store::IndexFormat<Traits>` value containing ordinary function pointers. It
  is not an inheritance hierarchy and does not use global registration.
- `copy_from()` hard-links every file when the input and output codecs return
  the same path list for a common dummy `NodePath`. Otherwise it decodes with
  the input codec and encodes with the output codec.
- Public store operations reject invalid hierarchy keys through
  `std::expected`; invalid keys are not represented by assertions or generic
  booleans.
- Preserve `StorageSettings::allow_overwrite`. It defaults to `false`;
  rejected overwrites return `AlreadyExists` through `std::expected`, and
  enabling it replaces existing payloads.
- The existing cache implementations are currently non-functional. Port their
  public API where it remains useful, keep application call sites building, and
  provide compile coverage only. Cache behaviour is not a compatibility
  requirement of this refactor.
- The `.png` written beside changed meshes by `sf_merger::NodeWriter` is an
  unmanaged debug artifact. It is not part of a logical node, is not returned
  by `Codec::paths()`, and is not indexed or copied by storage. Its existing
  application-local behaviour is preserved.
- 3D geometry, ECEF bounds, mesh codecs, mesh reconstruction, mask geometry,
  and raster-specific processing remain outside the shared store.

## Goals

1. Use one sparse hierarchy implementation for `octree::Id` and
   `radix::tile::Id`.
2. Make traversal, storage, cache, and the runtime codec boundary
   dimension-neutral while keeping production format adapters 3D-only in this
   refactor.
3. Replace the current layout-strategy class hierarchy with small path-mapping
   values backed by function pairs.
4. Allow one logical node payload to consist of multiple files without making
   layouts aware of those files.
5. Preserve all valid existing 3D index files and payload paths.
6. Prove the shared topology and traversal with `radix::tile::Id` without
   defining a persistent 2D adapter or format.
7. Land the refactor in small, testable steps. Every phase should build and
   pass tests before the next phase begins.

## Non-goals

- Changing `octree::Id`, `octree::Space`, `IdRect`, `OddLevelShifted`, or
  other 3D spatial calculations.
- Defining or implementing GDAL ingestion, raster resampling, filtering,
  source selection, or mask rasterisation.
- Defining or implementing a raster-fundamentalis index, payload format,
  layout ID, codec, publication lifecycle, or persistent storage adapter.
- Implementing an `rf_builder`, `rf_merger`, tile-base generator, tile server,
  snapshot-reuse operation, or other RF tool in this refactor.
- Defining the raster-fundamentalis merge policy or the final paired-hierarchy
  walker/action algebra. That work is deferred until `rf_merger` requirements
  are defined.
- Defining a shared subtree-copy abstraction, `Inner` subtree-copy behaviour,
  or forced re-encoding policy for RF. Those decisions are deferred until
  `rf_merger`.
- Adding merge semantics for `Inner` nodes in SF. Such nodes are invalid SF
  input and must be rejected before merge dispatch.
- Changing the existing 3D hard-link policy by adding a silent file-copy
  fallback.
- Refactoring unrelated octree, DAG, mesh, or tile-builder code.
- Changing the serialized schema of the current DAG `.bin` payloads.

## Compatibility contract

Before moving code, tests must lock down the following 3D behaviour:

| Item | Required compatibility |
|---|---|
| Index filename | `terrain.index` |
| Index field order | layout ID, preferred extension, index map |
| Node-key encoding | existing `octree::Id` level/index serialization |
| Node-status encoding | `Leaf = 0`, `Inner = 1`, `Virtual = 2` |
| Valid SF statuses | `Leaf` and `Virtual`; reject `Inner` with `sf::InvalidTopology` |
| Valid DAG statuses | `Leaf`, `Inner`, and `Virtual` |
| Flat layout ID | `flat` |
| Flat path | `<level>-<index><extension>` |
| Coordinate layout ID | `level_and_coordinate_directories` |
| Coordinate path | `<level>/<x>/<y>/<z><extension>` |
| Default layout | existing level/coordinate layout |
| Layout detection | both existing layouts remain detectable |
| Mesh codec selection | legacy preferred extension selects terrain or configured glTF codec |
| DAG codec selection | legacy `.bin` preferred extension selects the ZPP Bits codec |
| DAG payload encoding | current `dag::ClusterBatch` ZPP Bits serialization: metadata, then clustering |
| Equal codec path lists | hard-link every file, or report an explicit error |
| Different codec path lists | decode with input codec and encode with output codec |
| Overwrite setting | `StorageSettings::allow_overwrite`, default `false`; enabled writes replace existing payloads |

Compatibility means that each refactor phase can open the Phase 0 fixtures
written by the current code. DAG formats older than the Phase 0 baseline and
pre-refactor readers opening post-refactor output are not tested. Exact
byte-for-byte rewriting of an unordered index map is not required, but the
serialized schema and values must remain compatible during the migration.

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
│   ├── InvalidKey.h
│   ├── Index.h
│   ├── traverse.h
│   ├── NodePath.h
│   ├── PathMapping.h
│   ├── Layout.h
│   ├── IndexFormat.h
│   ├── Codec.h
│   ├── CodecError.h
│   ├── OpenError.h
│   ├── CopyError.h
│   ├── StorageSettings.h
│   ├── RawStorage.h
│   ├── Storage.h
│   ├── IndexedStorage.h
│   ├── cache/
│   │   ├── Interface.h
│   │   ├── Dummy.h
│   │   └── Lru.h
│   └── codec/
│       └── ZppBits.h
├── mesh/
│   └── codec/
│       ├── Terrain.h
│       └── Gltf.h
├── sf/
│   ├── InvalidTopology.h
│   └── validate_index.h
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
    └── StoreTraits.h
```

The exact file grouping may be collapsed if a file would only contain a few
lines. The important boundaries are:

- `store` contains dimension- and payload-neutral mechanisms;
- `store::codec::ZppBits` is the reusable concrete codec for payload types
  that provide ZPP Bits serialization;
- `mesh::codec` contains the separately configured terrain and glTF codecs;
- `sf` contains SF-specific topology validation and errors shared by
  `sf_builder`, `sf_merger`, and `dag_builder`;
- `octree` contains the 3D format and key adapters;
- `raster_store` contains only the minimal 2D hierarchy traits adapter in this
  refactor; and
- subdirectory names match their namespaces where a subnamespace is used.

Shared topology and octree-format adapters accept `Inner`. The SF restriction
is enforced by `sf::validate_index()` at SF producer/consumer boundaries and
reported as `sf::InvalidTopology`. It must not be embedded in `store::Index`,
traversal, or the generic 3D disk adapter. `sf_index_browser` is a diagnostic
tool and intentionally does not apply SF validation, so it can display invalid
trees including `Inner`.

`sf::validate_index()` returns
`std::expected<void, sf::InvalidTopology>`. SF application-level error types
must retain this error and `CopyError` when propagating failures; neither is
reduced to a log message, assertion, or generic boolean.

For this refactor, `sf::validate_index()` checks only for `Inner`. It does not
validate other structural or disk/filesystem invariants. Possible future
extensions are recorded in [todo.md](todo.md).

Temporary forwarding headers and aliases under `octree` are allowed during
migration. They must not contain a second implementation.

DAG serialization remains owned by `dag_builder`. Consolidate the serializers
for `dag::Id`, `dag::Group`, `dag::NodeMetadata`, `dag::ClusterBatch`,
`radix::geometry::Aabb3d`, GLM vectors, `Clustering`, `Cluster`, and
`TextureSet` in `src/dag_builder/serialization.h`. The DAG storage adapter
includes that header explicitly so template instantiation does not depend on
caller include order. Preserve the current tuples exactly:

- `ClusterBatch`: metadata, clustering;
- `NodeMetadata`: group assignment, groups; and
- `Group`: children, error, bounds.

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
- accept zoom levels 0 through
  `std::numeric_limits<uint32_t>::digits`, inclusive;
- treat that maximum zoom as terminal because a child cannot be represented
  by the `uint32_t` x/y coordinates;
- validate the maximum zoom without evaluating an overflowing
  `uint32_t{1} << 32`;
- use `radix::tile::Id::Hasher`; and
- return children in the deterministic order produced by
  `radix::tile::Id::children()`.

This traits adapter defines only hierarchy operations. It does not define
persistent coordinates, a path layout, or an RF disk format.

The shared code must obtain roots, parents, children, validation, and hashing
through the traits. It must not use dimension checks or specialize behaviour
on key types internally.

Operations accepting a key validate it through `Traits::is_valid()`. Index
lookup and mutation, traversal with an explicit root, and storage operations
return an `std::expected` retaining an `InvalidKey<Key>` when validation fails.
Keys produced internally by `Traits::root()`, `Traits::parent()`, and
`Traits::children()` are trusted only after trait-specific tests establish that
they preserve validity. The child order affects traversal order and is locked
down by the 2D and 3D trait tests; it is not serialized as separate metadata.

The API result shapes are:

- index lookup returns `expected<optional<NodeStatus>, InvalidKey<Key>>`;
- index mutation and predicates return `expected<bool, InvalidKey<Key>>`;
- traversal returns `expected<void, InvalidKey<Key>>`; and
- storage `load`, `save`, `has`, `remove`, path lookup, and `copy_from` return
  operation-specific `expected` types which retain `InvalidKey<Key>` and any
  applicable codec, filesystem, missing-source, or overwrite error.

During Phase 1, a forwarding `octree::IndexMap` compatibility wrapper preserves
the old optional/bool API for existing 3D callers with already-valid
`octree::Id` values. It delegates to `store::Index<octree::StoreTraits>`, is not
a second implementation, and is removed after callers migrate.

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
```

It does not necessarily name a physical file. Replace
`octree::disk::layout::Strategy` and
`octree::disk::layout::StrategyRegister` with a value similar to:

```cpp
template<typename Key>
struct store::path_layout::Mapping {
    std::string_view id;
    NodePath (*key_to_node_path)(const Key&);
    std::optional<Key> (*node_path_to_key)(const NodePath&);
};
```

`store::path_layout::Resolver<Key>` owns the base directory and one `store::path_layout::Mapping<Key>`. It
does not own a preferred extension. A configured codec expands the
extensionless `NodePath` into the physical file or files.

The stable ID is format metadata, not a third strategy operation. The
dimension adapters provide ordinary lookup functions:

```cpp
octree::store_layout::flat()
octree::store_layout::level_and_coordinate_directories()
octree::store_layout::from_id(id)
octree::store_layout::all()
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

    virtual std::expected<NodeData, CodecError>
    read(const NodePath&) const {
        return std::unexpected(
            CodecError::unsupported_operation("read"));
    }

    virtual std::expected<void, CodecError> write(
        const NodePath&,
        const NodeData&) const {
        return std::unexpected(
            CodecError::unsupported_operation("write"));
    }
};
```

Concrete codecs contain their configuration and are constructed before
storage use. `CodecError` is a payload-neutral operational error that records
the failed operation, an error category, and a diagnostic message. Concrete
codecs convert their domain errors to it. Unsupported read or write operations
may use the base implementation and return its `UnsupportedOperation` error.
Codec writes preserve the current directory-creation behaviour: they create
the parent directories required by their output paths before writing. The
storage hard-link path continues to create its target parent directories before
linking.

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

Multi-file test codec
  12/34/56/78
    -> 12/34/56/78.data
    -> 12/34/56/78.metadata
```

The mesh side has separate terrain and glTF codecs because they use different
format implementations. Binary `.glb` and JSON `.gltf` remain configurations
of one glTF codec because both use the same `cgltf` implementation.

The shared module also provides:

```cpp
template <typename NodeData>
struct store::codec::ZppBits : store::Codec<NodeData> { .. };
```

It uses the existing `io::read_from_path()` and `io::write_to_path()`
functions, maps one node to `<NodePath>.bin`, and converts `io::Error` to
`CodecError`. It contains no DAG-specific serialization logic. DAG payload
serialization remains in `dag_builder/serialization.h`, and its field order
and meshoptimizer/JPEG encoding remain unchanged.

### Storage and format adapters

Generalize storage over traits and NodeData. It owns a configured codec through
the runtime interface:

```cpp
store::RawStorage<Traits, NodeData>
store::Storage<Traits, NodeData>
store::IndexedStorage<Traits, NodeData>
store::cache::Interface<Traits, NodeData>
```

`RawStorage<Traits, NodeData>` owns the
`std::unique_ptr<Codec<NodeData>>`. `Storage` owns `RawStorage`, and
`IndexedStorage` owns or derives from `Storage`; no other layer shares codec
ownership. Moving storage transfers ownership. Caches, layouts, index formats,
resolvers, and application consumers never own the codec.

A move disarms the source so its destructor cannot save moved-out index state.
Move assignment must finalize a displaced dirty destination through the
existing destructor-save policy rather than silently discard it. Tests cover
the DAG builder's move into and release from `dag::ThreadSafeStorage`.

Domain-specific mesh codecs remain under `mesh::codec`. The reusable ZPP Bits
codec remains under `store::codec`. RF codecs are deferred.

Index serialization is not a responsibility of `store::Index`. Opening and
saving a dataset receives the following small runtime values:

```cpp
template<typename Traits>
struct store::IndexMetadata {
    store::Index<Traits> index;
    std::string layout_id;
    std::string codec_selector;
};

template<typename Traits>
struct store::IndexFormat {
    std::string_view index_filename;

    std::expected<IndexMetadata<Traits>, IndexFormatError>
    (*read)(const std::filesystem::path& index_path);

    std::expected<void, IndexFormatError>
    (*write)(
        const std::filesystem::path& index_path,
        const IndexMetadata<Traits>& metadata);

    std::optional<path_layout::Mapping<typename Traits::Key>>
    (*mapping_from_id)(std::string_view id);

    path_layout::Mapping<typename Traits::Key>
    (*default_mapping)();
};
```

The value provides:

- the index filename;
- index read/write conversion;
- mapping lookup by stable ID;
- the default mapping.

Legacy directory discovery is an octree opening helper which composes the
octree index format, the supplied payload-domain codec resolver, and the known
octree mappings. It is not a generic `IndexFormat` operation. Neither the
format value nor discovery may reintroduce a layout class hierarchy or global
registration.

For 3D, the adapter reads and writes the current `octree` index DTO unchanged.
Its `codec_selector` is exactly the legacy `preferred_extension`, including
the leading dot. Storage retains the selected `IndexFormat`, index path,
layout ID, and codec selector as its index-persistence state, so explicit and
destructor-triggered index saves can reproduce the same metadata.

When opening indexed storage or discovering a legacy unindexed directory, it
passes the legacy `preferred_extension` to a caller-supplied codec resolver.
The resolver is an ordinary callable and returns
`std::expected<std::unique_ptr<store::Codec<NodeData>>, CodecError>`. It is not
a global registry.

The payload domains provide ordinary resolver functions:

```text
mesh::codec::from_extension
  .terrain -> terrain codec
  .glb     -> glTF codec with binary container
  .gltf    -> glTF codec with JSON container

dag::codec::from_extension
  .bin     -> store::codec::ZppBits<dag::ClusterBatch>

dag::codec::metadata_from_extension
  .bin     -> read-only dag::codec::MetadataView
```

An unknown extension returns an explicit `UnsupportedCodec` error. Opening a
new empty store receives an explicit legacy codec selector plus an already
constructed codec at the payload-domain opening boundary; the generic storage
does not infer persistent metadata from `Codec::paths()`. Mesh and DAG
convenience functions select or resolve the codec and pass ownership into raw
storage, so application storage consumers do not handle codec objects.
Convenience functions in `src/dag_builder/storage.h` supply the DAG resolvers.
Preserve `DagMetaStorage` and `IndexedDagMetaStorage` as independently readable
views of the metadata prefix in each `ClusterBatch` `.bin` file. Their codec
returns `UnsupportedOperation` from writes instead of terminating or replacing
a full batch with metadata alone. `dag::codec::MetadataView` implements this
domain-specific read-only prefix view over the generic ZPP Bits codec.

Opening functions return their requested storage type through
`std::expected<..., OpenError>`. `OpenError` is a typed sum which retains the
failing path and the underlying error where applicable:

- index I/O or malformed index metadata (`IndexFormatError`);
- filesystem failure;
- unknown layout ID;
- unsupported codec selector or codec construction failure (`CodecError`); and
- invalid hierarchy key (`InvalidKey<Key>`).

Loading and saving likewise return storage-level expected errors which retain
an invalid key, an underlying `CodecError`, and `AlreadyExists` for a rejected
save. `CopyError` retains invalid-key, missing-source, overwrite, filesystem,
and codec failures. The SF call chains changed in Phase 4 retain these errors
to their application boundary.

Legacy unindexed-directory discovery remains in the 3D adapter: it recognizes
candidate endings by asking the supplied resolver, removes an accepted ending
to obtain a `NodePath`, and then invokes the selected layout parser. The
generic layout does not recover keys directly from codec-owned file paths.
Only a missing index triggers this discovery path. An unreadable or malformed
index, unknown layout, or unsupported codec selector returns `OpenError` and
must not silently fall back to directory discovery.

### Copying one node and SF subtree reuse

There are two separate responsibilities in the current implementation:

1. `sf_merger::NodeWriter` traverses a source subtree and
   `sf_merger::cut_leaf_node()` identifies an unchanged leaf.
2. `octree::Storage::copy_from()` delegates to
   `octree::RawStorage::copy_from()`, where
   `std::filesystem::create_hard_link()` performs the actual hard link and the
   target index is updated on success.

The filesystem hard-link implementation is therefore already in terrainlib.
Move the one-node storage operation into the shared store, but keep subtree
selection and traversal in `sf_merger`. There is no current DAG caller, and RF
subtree-copy requirements will be defined with `rf_merger`.

The migrated call chains are:

```text
sf_merger decides to keep a source subtree unchanged
    -> NodeWriter traverses the source index
    -> store::Storage::copy_from()
    -> hard-link every codec path, or decode/encode

sf_merger determines that a cut leaf is unchanged
    -> cut_leaf_node()
    -> store::Storage::copy_from()
    -> hard-link every codec path, or decode/encode
```

`Storage::copy_from()` remains the operation for copying one logical node.
For one key, `copy_from()`:

1. calls the input and output `Codec::paths()` with the same fixed dummy
   `NodePath`;
2. when the lists are equal, calls both codecs again with their actual source
   and target `NodePath` values and hard-links every source path to the
   corresponding target path;
3. when the dummy lists differ, reads the payload with the input codec and
   writes it with the output codec; and
4. updates the target index only after all links or the write complete.

The dummy path must be fixed and collision-free, for example
`__codec_probe__/node`. Path lists are compared exactly, including count,
order, and filename endings.

If overwrite is enabled for an indexed target, remove the target index entry
immediately before the first target file is modified. If linking several files
then fails partway through, return the error without a transactional rollback
guarantee; target links or old files may remain, but the logical node stays
unindexed. The copy operation stops immediately and propagates the failure
until the application aborts the overall operation. There is no journal,
rollback, cleanup guarantee, or silent copy fallback. An unsupported read or
write needed for re-encoding is returned through `CopyError`, retaining the
underlying `CodecError`.

Hard-link rules:

- never modify an existing linked payload in place;
- a matching codec path list hard-links every file;
- a different path list decodes with the input codec and encodes with the
  output codec;
- hard-link failure is explicit;
- no silent file-copy fallback is introduced.

#### SF-local subtree traversal

`sf_merger::NodeWriter::copy_subtree_to_output()` remains in `sf_merger`. It
uses shared traversal and `Storage::copy_from()`, but it is not promoted to a
generic store API during this refactor.

SF validation guarantees that its indexed inputs contain only `Leaf` and
`Virtual` nodes. The SF-local traversal skips `Virtual`, copies `Leaf`, and
does not define behaviour for `Inner`. An `Inner` node is rejected before
merge or cut processing with `sf::InvalidTopology` containing the offending
key.

When `rf_merger` is designed, it can initially compose `store::traverse` and
`Storage::copy_from()`. At that point, the SF and RF implementations provide
enough evidence to decide whether a shared subtree copier is useful and how it
must handle RF `Inner` nodes.

#### Error propagation

The lower storage layer already represents ordinary copy failures, including
missing source files, directory creation failure, hard-link failure, decode
failure, and encode failure. The current `NodeWriter` consumes
`Storage::copy_from()` with `DEBUG_ASSERT_VAL`, as does the unchanged-leaf path
in `cut_leaf_node()`, while some overwrite paths terminate through
`LOG_ERROR_AND_EXIT()`. Their `void` call chains prevent the application from
reporting or handling these failures.

Return failures from the SF-local subtree and cut functions through their
callers until the application boundary can report the affected key and path.
Codec, filesystem, unsupported-conversion, malformed-dataset, and overwrite
failures are propagated with `std::expected`; `CopyError` retains any
underlying `CodecError`. Assertions remain appropriate for internal
invariants, but operational failures are not assertion failures or
intentionally thrown exceptions.

### Paired hierarchy walking is deferred

This refactor does not extract `sf_merger::Merger` into a shared paired-tree
walker. SF only permits `Missing`, `Leaf`, and `Virtual`, and its current
recursion does not provide enough evidence to define the `Inner` behaviour
needed by raster-fundamentalis and tile-base merging.

The previously proposed mutually exclusive actions `Recurse`, `Ignore`,
`KeepLeft`, `KeepRight`, and `Write` cannot express both an action for the
current physical payload and recursion into descendants. `Inner` merging may
require both. Whether the future interface uses a combined `WriteAndRecurse`
action or independent current-node and descendant decisions belongs to the
`rf_merger` design.

For this refactor:

- keep recursion, subtree traversal, and mesh policy in `sf_merger`;
- validate SF inputs and reject `Inner` through `std::expected`;
- move only the one-node `Storage::copy_from()` mechanism into `store`; and
- do not add a shared `store::merge` namespace.

A future `rf_merger` task will define the paired-tree action algebra from the
2D requirements and may migrate `sf_merger` once both use cases are known.

## Implementation phases

Each phase should be one reviewable commit unless the tests and implementation
are clearer as two commits. Do not begin a later phase while the current phase
has failing tests.

### Phase 0 — Capture current compatibility

No production behaviour changes.

1. Add golden SF fixtures created by the current code:
   - one `terrain.index` using `flat`;
   - one using `level_and_coordinate_directories`;
   - across the fixtures, physical payload paths for a root, child, and deeper
     descendant, without placing physical payloads at ancestor and descendant
     keys in the same index; and
   - index entries containing `Leaf` and `Virtual`, but no `Inner`.
2. Add one golden DAG dataset whose index selects `.bin`, whose payload
   contains a valid serialized `dag::ClusterBatch`, and whose index contains
   `Leaf`, `Virtual`, and `Inner`. Use the current metadata-then-clustering
   format, include non-trivial group metadata, and prove that the same file can
   be opened through the read-only `NodeMetadata` view.
3. Test that all fixtures open, resolve the expected IDs and extensions, and
   traverse the expected sparse nodes.
4. Add path round-trip tests for boundary IDs and both layouts.
5. Add storage tests for:
   - matching-extension hard links;
   - different-extension decode/re-encode;
   - overwrite-enabled replacement;
   - indexed and unindexed opens; and
   - final index creation by directory scan.
6. Record the pre-refactor public aliases used by `sf_builder`, `sf_merger`,
   `sf_index_browser`, `dag_builder`, and `dag_convert_debug`, including
   `DagMetaStorage`, `IndexedDagMetaStorage`, and `dag::ThreadSafeStorage`.

Exit criterion: the compatibility tests pass against the untouched
implementation and fail when any stable filename, layout ID, path encoding,
status value, index field order, or DAG payload serialization is deliberately
changed.

### Phase 1 — Extract topology into `store`

1. Move `NodeStatus` and `NodeStatusOrMissing` to `store`, preserving their
   underlying values and serialization.
2. Introduce the hierarchy-traits concept and `octree::StoreTraits`.
3. Convert `IndexMap` into `store::Index<Traits>`.
4. Convert traversal into `store::traverse`.
5. Add `raster_store::StoreTraits` for `radix::tile::Id`.
6. Run the same index-transition and DFS/BFS tests with both trait types,
   including deterministic child order, invalid-key errors through
   `std::expected`, maximum-depth children, and explicit traversal roots.
7. Provide the temporary forwarding `octree::IndexMap` wrapper and aliases so
   downstream migration is separate; otherwise migrate downstream immediately.

Exit criterion: 2D and 3D keys pass the same topology suite; existing 3D
callers still build through aliases; no filesystem code has changed.

### Phase 2 — Introduce path mappings and runtime codecs

This phase introduces the extensionless layout and runtime codec pieces
together. It does not cut production storage over to them yet. The existing
storage, layout strategies, and static codecs remain temporarily as the
working compatibility path until Phase 3 can replace the complete
layout-plus-codec path construction in one step.

1. Add `store::NodePath`, `store::path_layout::Mapping<Key>`, and
   `store::path_layout::Resolver<Key>`.
2. Add the stateful `store::Codec<Payload>` interface with `paths()`, `read()`,
   and `write()`, plus `CodecError`.
3. Add the runtime `store::codec::ZppBits<T>`, preserving the existing `.bin`
   path and serialized payload bytes.
4. Consolidate the DAG serialization functions in
   `src/dag_builder/serialization.h` without changing their serialized field
   order, meshoptimizer encoding, or JPEG texture encoding.
5. Add the terrain codec and one glTF codec configured for binary `.glb` or
   JSON `.gltf`. Keep the current extension-dispatching `octree::MeshCodec`
   only as temporary production compatibility glue until the Phase 3 cutover.
   Test that glTF writer exceptions become `CodecError` values.
6. Port the two existing 3D layouts to ordinary function pairs without
   changing stable IDs. The mappings return `level-index` and
   `level/x/y/z` without file endings.
7. Add explicit `from_id()` and `all()` lookup functions in the 3D adapter.
   Keep the singleton strategy registry only for the old production storage
   path until Phase 3.
8. Compose each new 3D mapping with each applicable runtime codec in tests and
   prove that they resolve all Phase 0 fixtures to identical physical payload
   paths.
9. Add focused codec tests using single-file, multi-file, read/write, and
   write-only test codecs. Test stable path ordering, unsupported operations,
   directory creation, conversion of domain errors to `CodecError`, and
   concurrent reads and writes. Exercise reentrancy of every production codec
   used by the parallel DAG builder.

Exit criterion: the extensionless mappings and runtime codecs together resolve
all Phase 0 fixtures to their existing physical payload paths; generic
`Layout` contains no extension; the new codec tests pass; existing production
storage and all callers still build unchanged through the temporary legacy
path.

### Phase 3 — Generalize storage and index lifecycle

1. Cut the production path construction over to the Phase 2
   `store::path_layout::Resolver<Key>` and runtime codecs. Move the legacy preferred extension
   out of layout state and retain it as the 3D format adapter's codec selector.
2. Port legacy layout discovery so it recognizes codec endings through the
   supplied resolver, strips the accepted ending, and then calls
   `node_path_to_key()`.
3. Move copy error, raw storage, logical storage, and indexed storage into
   `store`. Port the existing cache API where useful for source compatibility,
   but do not require cache behaviour tests.
4. Make `RawStorage<Traits, NodeData>` exclusively own a configured
   `std::unique_ptr<Codec<NodeData>>`. `Storage` owns it transitively through
   raw storage; remove the codec template parameter from every storage type.
5. Replace every embedded `octree::Id` with `Traits::Key`.
6. Make every raw file operation obtain its complete file list through
   `Codec::paths()`. `has()` requires every listed file, and `remove()` removes
   every listed file.
7. Keep domain-specific mesh codecs outside the shared module under
   `mesh::codec`.
8. Add the function-pointer-based `IndexFormat<Traits>`, `IndexMetadata`, and
   typed format/open errors. Split generic index maintenance from 3D index
   serialization and legacy folder discovery. Test that only a missing index
   starts discovery; all other index errors are returned.
9. Keep the current 3D `terrain.index` DTO and open functions as compatibility
   adapters over the shared storage. Retain its exact preferred extension as
   `codec_selector`, resolve it through the payload-domain mesh or DAG
   resolver, and retain the format metadata required by automatic saving.
10. Add DAG storage convenience functions that supply the writable batch and
    read-only metadata resolvers, and mesh storage convenience functions that
    select the configured terrain or glTF codec. Preserve the DAG metadata
    storage aliases. Migrate `dag_builder`, `dag_convert_debug`, and mesh
    storage consumers without exposing codec objects at application call sites.
11. Migrate the existing octree storage aliases and all other application
    callers, including adapting `dag::ThreadSafeStorage` to the new key and
    expected-returning APIs.
12. Preserve the current 3D destructor-save behaviour until all callers have
    explicit index finalization. Test move construction, move assignment over
    dirty state, moved-from destruction, and the DAG move/release path.
13. Preserve `StorageSettings::allow_overwrite`. Replace process termination
    on a rejected overwrite with `AlreadyExists` in the storage-level expected
    error. Test that a rejected save returns `AlreadyExists` and that enabling
    overwrite replaces the existing payload.
14. Add resolver tests for `.terrain`, `.glb`, `.gltf`, writable
    `ClusterBatch` `.bin`, and read-only `NodeMetadata` `.bin` dispatch, plus
    explicit failure for an unknown preferred extension and metadata writes.
15. Instantiate the shared storage tests with `raster_store::StoreTraits`
    using a test-only path mapping and codec. This proves the storage templates
    contain no hidden `octree::Id` dependency without defining a stable RF
    layout, codec, or disk format.
16. Delete the old strategy base class, strategy registry, concrete strategy
    classes, static codec concept and codecs, and their temporary compatibility
    glue once no call site uses them.

Test that the Phase 0 DAG fixture opens through both new resolvers and that a
new deterministic `.bin` payload matches its golden bytes and remains readable
through the unchanged ZPP serialization functions. Test unknown layout IDs,
malformed index metadata, invalid hierarchy keys, and retained underlying
open/codec errors through `std::expected`.

Exit criterion: all existing applications build and all Phase 0 fixtures pass
through the shared runtime codec and storage implementation. Phase 0 DAG
payload bytes and `.bin` paths remain unchanged. No extension-dispatching
mesh codec, layout inheritance, RTTI lookup, static registrar, owning strategy
pointer, or second storage implementation remains under `octree`.

### Phase 4 — Harden node reuse and enforce SF topology

1. Change `Storage::copy_from()` to compare input and output codec path lists
   for the fixed dummy `NodePath`.
2. Hard-link all actual files when the lists match. A partially failed
   multi-file operation returns an error without rolling back links already
   created.
3. Decode with the input codec and encode with the output codec when lists
   differ.
4. Test:
   - one-file hard linking;
   - multi-file hard linking;
   - different path counts and endings;
   - error propagation after a partially failed multi-file hard link;
   - overwrite failure leaving an existing logical node unindexed even when
     old or partial files remain;
   - conversion between terrain and glTF;
   - `copy_from()` overwrite rejection;
   - `copy_from()` overwrite-enabled replacement; and
   - missing-file, hard-link, decode, and encode error propagation.
5. Add `sf::validate_index()`, returning `sf::InvalidTopology` with the
   offending key when it encounters `Inner`. Do not add other validation rules
   in this refactor.
6. Apply the validator to SF-merger merge and cut inputs and the DAG builder's
   SF input before processing. Apply it to SF-builder and SF-merger output
   after the completed `terrain.index` has been written. If output validation
   fails, retain the written index and payloads for diagnosis, propagate
   `sf::InvalidTopology` to the command line, and report that the output is
   invalid. Do not apply the validator when opening DAG datasets, through
   generic octree/store adapters, or in the diagnostic `sf_index_browser`.
   Extract an SF-builder finalization boundary into `sfbuilderlib` which writes
   the index, validates it, and returns the typed result so output validation
   and command-line propagation are directly testable.
7. Keep SF recursion, subtree traversal, and mesh policy in `sf_merger`.
   Change its subtree and cut call chains to propagate validation and
   `copy_from()` failures through `std::expected` to the application boundary.
8. Add integration tests proving:
    - valid `Leaf`/`Virtual` SF merge behaviour is unchanged;
    - an SF input containing `Inner` fails validation before merge dispatch;
    - invalid SF output writes `terrain.index` for diagnosis and returns
      `sf::InvalidTopology` to the application boundary;
    - an unchanged SF subtree is hard-linked and a changed boundary node is
      newly written; and
    - an unchanged leaf in the SF cut path is hard-linked while a clipped leaf
      is newly written.

Exit criterion: one-node copying works through `Codec::paths()`, SF consumers
reject `Inner` with a typed error before processing, existing valid SF merge
and cut behaviour is preserved, invalid SF output remains inspectable with a
written `terrain.index`, and neither a shared subtree copier nor a paired-tree
walker has been introduced.

### Phase 5 — Cleanup and documentation

1. Remove temporary forwarding headers that no repository caller needs.
2. Remove obsolete files under `octree/disk` and the old generic
   implementation under `octree/storage`.
3. Keep only 3D key, format, codec, and compatibility adapters under
   `octree`.
4. Update includes, CMake source lists, and precompiled-header includes.
5. Update [architecture.md](architecture.md),
   [storage-format.md](storage-format.md), [status-quo.md](status-quo.md), and
   the before-refactor report with links to the implemented boundary. State
   clearly that RF formats and tools remain future work and that architecture
   requirements are not acceptance criteria for this refactor. Preserve the
   before-refactor report as history; do not rewrite it as if it described the
   new code.
6. Document the final public names, a legacy 3D opening example, and an
   in-memory `radix::tile::Id` topology example. Do not document a persistent
   RF format or opening API.

Exit criterion: repository search finds no generic implementation tied to
`octree::Id`; all tests pass; the old layout strategy hierarchy is gone.

### Phase 6 — Remove migration-only compatibility fixtures

1. Delete the Phase 0 golden SF and DAG datasets and the tests whose only
   purpose is opening or byte-comparing data written before the completed
   refactor.
2. Keep the format round-trip, path, resolver, topology, storage, error, and
   application integration tests which exercise the final implementation
   without pre-refactor fixture files.

Exit criterion: no pre-refactor dataset fixture remains, and the retained test
suite passes against data produced by the final implementation.

## Test and verification plan

Generic store tests should live in the existing `unittests_terrainlib` target.
Suggested files:

```text
unittests/terrainlib/store_index.cpp
unittests/terrainlib/store_traverse.cpp
unittests/terrainlib/store_layout.cpp
unittests/terrainlib/store_codec.cpp
unittests/terrainlib/store_storage.cpp
unittests/terrainlib/store_compatibility.cpp # temporary through Phase 5
unittests/terrainlib/sf_validate_index.cpp
```

The temporary DAG payload-compatibility fixture and resolver integration test
belong in `unittests_dagbuilder`, because `terrainlib` must not depend on
`dag::ClusterBatch` or its serializers. Phase 6 removes the fixture and its
byte-comparison test while retaining resolver tests built from current data.

The validator's unit tests belong in `unittests_terrainlib`. Boundary tests
belong with their consumers: SF-builder output validation in
`unittests_sfbuilder`, merge and cut validation/error propagation in
`unittests_sfmerger`, and DAG-builder SF-input validation in
`unittests_dagbuilder`. `sf_index_browser` remains unvalidated by design.

Cache migration has compile coverage only. Existing cache application call
sites must continue to build where the API remains meaningful, but no cache
behaviour or compatibility test is required.

During implementation:

1. Build in `$source_dir/build/$config_name`.
2. Run unit tests from that build directory.
3. Run the focused store tests after each edit.
4. Run the full `unittests_terrainlib` target at every phase boundary.
5. Run `unittests_dagbuilder` after the ZPP codec, DAG serialization header, or
   DAG resolver changes.
6. Configure with `ALP_BUILD_SF_BUILDER`, `ALP_BUILD_SF_MERGER`,
   `ALP_BUILD_SF_INDEX_BROWSER`, `ALP_BUILD_DAG_BUILDER`, and
   `ALP_BUILD_DAG_CONVERT_DEBUG` enabled, then build `sf-builder`, `sf-merger`,
   `sf-index-browser`, `dag-builder`, and `dag-convert-debug` after their
   storage aliases move.
7. Run `unittests_sfbuilder`, `unittests_sfmerger`, and
   `unittests_dagbuilder`, plus any existing merger integration fixture, after
   the Phase 4 validation changes.
8. Inspect `git diff --check` and the final worktree before each commit.

No formatting-only pass or unrelated refactor belongs in these commits.

## Expected migration map

| Current code | Target |
|---|---|
| `octree/NodeStatus.h` | `store/NodeStatus.h` plus temporary alias |
| `octree/NodeStatusOrMissing.h` | `store/NodeStatusOrMissing.h` plus temporary alias |
| `octree/IndexMap.*` | `store/Index.h` |
| `octree/traverse.h` | `store/traverse.h` |
| complete node paths embedded in layouts | extensionless `store/NodePath.h` plus codec endings |
| `octree/disk/Layout.h` | `store/path_layout.h` |
| `octree/disk/layout/Strategy.h` | `store/path_layout.h` |
| `StrategyRegister.h` | explicit dimension-adapter lookup functions |
| `strategy/Flat.h` | `octree/store_layout/Flat.h` |
| `strategy/LevelAndCoordinateDirectories.h` | `octree/store_layout/LevelAndCoordinateDirectories.h` |
| `octree/storage/cache/*` | `store/cache/*` where useful; compile compatibility only |
| `octree/storage/codec/Codec.h` | runtime `store/Codec.h` |
| `octree/storage/codec/DefaultCodec.h` | runtime `store/codec/ZppBits.h` |
| `octree/storage/codec/MeshCodec.h` | `mesh/codec/Terrain.h` and configured `mesh/codec/Gltf.h` |
| `octree/storage/codec/ReadOnlyCodec.h` metadata specialization | `dag::codec::MetadataView` |
| `octree/storage/RawStorage.h` | `store/RawStorage.h` |
| `octree/storage/Storage.h` | `store/Storage.h` |
| `octree/storage/IndexedStorage.h` | `store/IndexedStorage.h` |
| `octree::StorageSettings::allow_overwrite` | `store::StorageSettings::allow_overwrite`, preserving default and enabled behaviour |
| `octree/storage/helpers.*` | generic scan helpers plus 3D format adapter |
| `octree/disk/IndexFile.h` | versioned 3D format adapter under `octree` |
| DAG serializers in `dag_node.h`, `dag_id.h`, `metadata.h`, `encoded.h`, and `zpp_bits_glm.h` | `dag_builder/serialization.h` |
| `dag_builder/storage.h` aliases | Batch and read-only metadata storage aliases plus codec resolver convenience functions |
| `dag_builder/thread_safe_storage.h` | Adapted caller-side wrapper over shared storage |
| `sf_merger::NodeWriter` subtree loop | remains in `sf_merger`; return copy failures through `std::expected` |
| `sf_merger::NodeWriter` auxiliary `.png` write | remains an unmanaged, application-local debug artifact |
| `sf_merger::cut_leaf_node()` copy path | remains in `sf_merger`; return copy failures through `std::expected` |
| SF merger `Inner` `UNREACHABLE()` path | `sf::validate_index()` returning `sf::InvalidTopology` |

## Risks and controls

| Risk | Control |
|---|---|
| Existing indexes stop loading | Golden pre-refactor fixtures and unchanged 3D DTO |
| The refactor changes current DAG `.bin` bytes | Temporary golden DAG fixture, unchanged serializers, and explicit `.bin` resolvers |
| Octree format adapter gains DAG dependencies | Caller-supplied resolver owned by `dag_builder` |
| Unknown legacy extension silently selects the wrong codec | Return an explicit `UnsupportedCodec` error |
| Valid legacy paths are parsed differently | Characterization and round-trip tests before replacement |
| Template migration creates a large unreviewable diff | Compatibility aliases and phase-by-phase caller migration |
| `radix::tile::Id` root underflows | Traits intercept root parent lookup |
| Invalid `radix::tile::Id` values enter the shared index | Validate through `raster_store::StoreTraits` and test boundary zooms |
| Invalid `Inner` nodes reach SF merge dispatch | Validate every SF input first and return the offending key in a typed error |
| SF subtree copying is generalized before RF requirements exist | Keep it in `sf_merger`; reconsider extraction with `rf_merger` |
| A paired-walker API is fixed before RF semantics are known | Defer its action algebra until `rf_merger` requirements are defined |
| Multi-file hard linking fails partway through | Remove any existing index entry before modifying target files, stop immediately, leave the node unindexed, propagate the error, and abort the overall operation; old or partial files may remain |
| Incompatible codecs return the same path list | Treat path-list equality as a codec contract and test every concrete codec pairing |
| Output-only codec is selected for required input | Return a clear `UnsupportedOperation` error |
| Shared code accumulates mesh or provisional RF policy | Dependency tests/review against the source boundary |
| The refactor accidentally fixes the future RF disk format | Do not add a persistent 2D format adapter, layout, or codec |

## Deferred raster-fundamentalis work

This refactor is a prerequisite for, not an implementation of, the system
described in [architecture.md](architecture.md). That document and
[storage-format.md](storage-format.md) remain useful design input, but their RF
details are not acceptance criteria for this refactor and are not declared
final here.

A later RF design phase must resolve and test at least:

- the persistent index filename and schema, and how they use the existing
  generic serialization envelope and versioning support;
- persistent tile keys, coordinate convention, layout IDs, and node paths;
- tile and source-attribution payload formats and codecs;
- snapshot construction, validation, publication, and crash expectations;
- hard-link preflight and unchanged-tile reuse;
- `rf_merger` policy, paired-tree walking, and `Inner` copy behaviour; and
- RF builders, converters, debugging outputs, and other tools.

None of those decisions blocks completion of this refactor.
The removed ideas are retained, without implementation-plan status, in the
explicitly draft [rf_builder notes](rf_builder.md) and
[rf_merger notes](rf_merger.md).
