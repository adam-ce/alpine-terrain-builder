# 2D/3D hierarchical store refactor plan

Status: proposal for review. This document is an implementation plan, not a
record of completed work.

## Decisions already made

- The shared implementation will live in `src/terrainlib/store` and use the
  `store` namespace.
- The 2D implementation will live in `src/terrainlib/raster_store` and use the
  `raster_store` namespace.
- Existing 3D Structura Fundamentalis datasets must remain readable and
  writable without changing their on-disk contract.
- 3D compatibility includes both existing path layouts:
  `flat` and `level_and_coordinate_directories`.
- The 2D raster-fundamentalis format is a separate format. Requirements in
  this directory apply to that 2D format and must not be retrofitted onto
  existing 3D datasets.
- A path layout strategy should contain a stable identifier and two
  operations: key to relative path, and relative path to key. It should not
  require an inheritance hierarchy, RTTI, global self-registration, or heap
  allocation.
- 3D geometry, ECEF bounds, mesh codecs, mesh reconstruction, mask geometry,
  and raster-specific processing remain outside the shared store.

## Goals

1. Use one sparse hierarchy implementation for `octree::Id` and
   `radix::tile::Id`.
2. Use one traversal, storage, cache, codec boundary, and unchanged-subtree
   copier for 2D and 3D.
3. Make paired-tree walking reusable without putting mesh or raster merge
   policy into the shared layer.
4. Replace the current layout-strategy class hierarchy with small path-mapping
   values backed by function pairs.
5. Preserve all valid existing 3D index files and payload paths.
6. Introduce the 2D storage adapter without inventing unspecified raster file
   details.
7. Land the refactor in small, testable steps. Every phase should build and
   pass tests before the next phase begins.

## Non-goals

- Changing `octree::Id`, `octree::Space`, `IdRect`, `OddLevelShifted`, or
  other 3D spatial calculations.
- Defining or implementing GDAL ingestion, raster resampling, filtering,
  source selection, or mask rasterisation.
- Defining the `.arft` payload or source-attribution-table serialization
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
| Copy with equal extensions | hard link, or an explicit error |
| Copy with different extensions | decode and encode through the codec |

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
    └── store_layout/
        └── Zxy.h
```

The exact file grouping may be collapsed if a file would only contain a few
lines. The important boundaries are:

- `store` contains dimension- and payload-neutral mechanisms;
- `octree` contains the legacy 3D format and key adapters;
- `raster_store` contains the new 2D format and key adapters; and
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

### Path mappings

Replace `octree::disk::layout::Strategy` and
`octree::disk::layout::StrategyRegister` with a value similar to:

```cpp
template<typename Key>
struct store::PathMapping {
    std::string_view id;
    std::filesystem::path (*key_to_path)(
        const Key&, std::string_view extension_with_dot);
    std::optional<Key> (*path_to_key)(
        const std::filesystem::path& relative_path);
};
```

`store::Layout<Key>` owns the base directory, preferred extension, and one
`PathMapping<Key>`. It only adds/removes the base directory around the two
mapping functions.

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

Path parsers must validate the complete relative path and the expected file
extension at the `Layout` boundary. They must return an error or `nullopt`;
they must not assert on input read from disk.

### Storage and format adapters

Generalize these mechanisms over traits, payload, and codec:

```cpp
store::RawStorage<Traits, Payload, Codec>
store::Storage<Traits, Payload, Codec>
store::IndexedStorage<Traits, Payload, Codec>
store::cache::Interface<Traits, Payload>
```

The payload codec remains path-based and key-neutral. Mesh and raster codecs
remain with their payload domains.

Index serialization is not a responsibility of `store::Index`. Opening and
saving a dataset must receive a dimension-specific format adapter which
provides:

- the index filename;
- index read/write conversion;
- mapping lookup by stable ID;
- the default mapping; and
- the mappings considered during legacy directory scanning.

This adapter may be a compile-time policy or a small value of function
pointers. Choose the smaller implementation after the Phase 0 tests exist.
It must not reintroduce a class hierarchy or global registration.

For 3D, the adapter reads and writes the current `octree` index DTO unchanged.
For 2D, it reads and writes a separately versioned
`raster_store::v1` DTO using the serialization envelope required by
[storage-format.md](storage-format.md).

Automatic dirty-index saving currently happens in the 3D storage destructor.
Preserve that behaviour for existing 3D entry points during the migration.
The new 2D snapshot API should require an explicit finalization/publication
step; a destructor must not make an incomplete snapshot authoritative.

### Unchanged-subtree reuse

Move payload-neutral subtree copying out of `sf_merger::NodeWriter`. The
shared operation should:

1. traverse an indexed source subtree;
2. skip `Virtual` nodes;
3. copy physical payloads for both `Leaf` and `Inner`;
4. use `Storage::copy_from` so equal-format payloads are hard-linked;
5. update the target index incrementally; and
6. return an error instead of asserting or terminating.

This fixes the current incorrect assumption that every non-virtual visited
node is a `Leaf`.

Hard-link rules:

- never modify an existing linked payload in place;
- matching extensions require hard-link creation;
- different extensions retain the current decode/re-encode path;
- hard-link failure is explicit;
- 2D snapshot tools must preflight that source and destination support hard
  links before a long operation starts; and
- no silent copy fallback is introduced by this refactor.

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

1. Add `store::PathMapping<Key>` and `store::Layout<Key>`.
2. Port the two existing 3D layouts to ordinary function pairs without
   changing paths or stable IDs.
3. Replace the singleton strategy registry with explicit `from_id()` and
   `all()` functions in the 3D adapter.
4. Port layout guessing to consume a span of mappings supplied by the format
   adapter.
5. Add the proposed 2D `z/x/y.arft` mapping only after the review decision
   listed below is resolved.
6. Switch path and layout-detection tests to the new implementation.
7. Delete the old strategy base class, registration machinery, and concrete
   strategy classes once no call site uses them.

Exit criterion: both legacy fixtures resolve to identical payload paths;
there is no layout inheritance, RTTI lookup, static registrar, or owning
strategy pointer.

### Phase 3 — Generalize storage and index lifecycle

1. Move the codec concept, copy error, raw storage, caches, logical storage,
   and indexed storage into `store`.
2. Replace every embedded `octree::Id` with `Traits::Key`.
3. Keep payload codecs outside the shared module:
   `octree::MeshCodec` remains 3D, and the future `.arft` codec remains under
   `raster_store`.
4. Split generic directory scanning from 3D index serialization.
5. Implement the small format-adapter boundary described above.
6. Keep the current 3D `terrain.index` DTO and open functions as compatibility
   adapters over the shared storage.
7. Migrate the existing octree storage aliases and all application callers.
8. Preserve the current 3D destructor-save behaviour until all callers have
   explicit index finalization.

Exit criterion: all existing applications build and all Phase 0 fixtures pass
through the shared storage implementation. No second storage implementation
remains under `octree`.

### Phase 4 — Generalize subtree reuse and paired walking

1. Add the shared unchanged-subtree copier.
2. Test copies containing `Leaf`, `Virtual`, and `Inner` nodes.
3. Add the paired hierarchy walker and typed actions.
4. Cover all 16 status pairs with table-driven tests.
5. Adapt the 3D merger to the shared walker while keeping mesh policy in
   `sf_merger`.
6. Remove generic recursion and copy logic from `sf_merger::Merger` and
   `NodeWriter`.
7. Add a 3D integration test proving an unchanged subtree is hard-linked and
   a changed boundary node is newly written.

Exit criterion: the existing 3D merger behaviour is preserved, `Inner` no
longer reaches `UNREACHABLE()`, and the shared walker contains no mesh, ECEF,
GDAL, OpenCV, or raster dependencies.

### Phase 5 — Add the 2D raster-fundamentalis adapter

This phase starts only after the unresolved 2D format decisions below are
edited into decisions in this document.

1. Add the checked 2D persistent-key conversion around `radix::tile::Id`.
2. Add the chosen 2D path mapping and stable ID.
3. Define the versioned 2D index DTO and index filename.
4. Use the required magic/version/checksum/compression serialization envelope.
5. Add the 2D format adapter and storage aliases under
   `raster_store`.
6. Exercise storage with a small test codec if the final `.arft` codec is not
   implemented yet; do not make `.arft` claims from a placeholder codec.
7. Test:
   - invalid and boundary tile IDs;
   - index serialization and validation;
   - `Leaf`/`Inner` coexistence;
   - sparse traversal and ancestor lookup;
   - path round trips;
   - snapshot hard-link reuse; and
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
| `octree/disk/Layout.h` | `store/Layout.h` |
| `octree/disk/layout/Strategy.h` | `store/PathMapping.h` |
| `StrategyRegister.h` | explicit dimension-adapter lookup functions |
| `strategy/Flat.h` | `octree/store_layout/Flat.h` |
| `strategy/LevelAndCoordinateDirectories.h` | `octree/store_layout/LevelAndCoordinateDirectories.h` |
| `octree/storage/cache/*` | `store/cache/*` |
| `octree/storage/codec/Codec.h` | `store/Codec.h` |
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
| Hard-link failure appears late | 2D operation preflight and explicit errors |
| Shared code accumulates mesh/raster policy | Dependency tests/review against the source boundary |
| Generic index accidentally dictates both disk formats | Separate 3D and 2D format adapters |

## Decisions required before Phase 5

These are intentionally unresolved because the current raster-store documents
mark them as unclear:

1. **2D index filename:** choose the filename used inside a
   raster-fundamentalis snapshot.
2. **2D payload path:** confirm `z/x/y.arft`, including whether chunks live
   directly under the snapshot or below a `chunks/` directory.
3. **2D layout ID:** choose the stable string serialized in the index.
4. **Maximum zoom:** choose the supported persistent range and integer widths
   for zoom, x, and y.
5. **Index contents:** decide whether v1 stores only sparse status entries or
   also derived aggregate metadata. The first implementation should omit
   derivable metadata unless a concrete query requires it.
6. **Index envelope constants:** assign the 2D index magic number, compression
   choice, and version according to the common serialization rules.
7. **Publication:** define whether snapshot completion uses an atomic rename,
   a manifest marker, or an external store-level operation. The generic
   storage layer should expose finalization but not invent store-level
   lifecycle policy.

Until these decisions are made, Phases 0–4 can complete and the shared
implementation can be proven with `radix::tile::Id` in memory and with
temporary-directory tests. No provisional 2D disk format should escape into
production data.
