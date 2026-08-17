# Versioned storage-envelope migration plan

Status: implemented in the `raster-store` worktree; awaiting commit.

## Purpose

This plan replaces the repository's remaining private raw ZPP Bits disk
formats with the generic versioned envelope in `terrainlib/io`. It also moves
dataset-wide format selection out of the octree index and into an independent
metadata file, splits DAG data from its cheaply readable metadata, and removes
domain transformations and validation from ZPP serialization callbacks.

The migration intentionally breaks compatibility with the pre-envelope
`terrain.index`, `.terrain`, and `.bin` formats. Every changed format receives
a new filename ending. Once an enveloped format is introduced, later readers
must continue to read its older envelope payload versions and upgrade them to
the latest in-memory type.

## Decisions already made

- The generic `io::envelope` magic, class name, class version, bounded
  decompression, checksum, and compression fields are used for every new
  private binary format.
- Default zstd compression with its embedded checksum is used for indexes,
  metadata, SF meshes, DAG data, and DAG metadata, including already encoded
  meshoptimizer and JPEG byte streams.
- The new persistent names are:
  - `octree.storemeta` for dataset-wide metadata;
  - `octree.storeindex` for the sparse hierarchy index;
  - `.sfmesh` for enveloped SF mesh nodes;
  - `.dag` for enveloped DAG clustering data; and
  - `.dagmeta` for enveloped DAG node metadata.
- Standard `.glb` and `.gltf` files remain unchanged. They are debugging
  formats and are not wrapped in the private envelope.
- Filename endings continue to identify formats. A format change receives a
  new ending; versions that remain readable by the same schema retain the
  ending.
- The dataset metadata file, not the index, is authoritative for the layout
  ID, payload class, and codec selector. It remains readable when the index is
  missing or malformed.
- The index continues to persist the complete `store::Index` state, including
  `Leaf`, `Inner`, and `Virtual`. Deterministic byte order is not required.
- A DAG logical node consists of two files. The writable batch codec writes
  both `.dag` and `.dagmeta`; the read-only metadata codec reads only
  `.dagmeta`.
- Hard-link behavior remains path-list based. Identical new formats use the
  same endings and can be linked; changing format or compression policy uses
  a different ending and therefore decodes and rewrites.
- Domain-specific ZPP serialization callbacks are removed. ZPP serializes
  versioned aggregate wire types mechanically. Explicit free functions encode,
  validate, and decode those wire types.
- The old `io/serialize.h` and `io/serialize.inl` API is removed after all
  production and test callers migrate.
- Avoidable copies are removed with spans and moves. Compression necessarily
  requires owned uncompressed and compressed buffers; zero-copy decoding is
  not a goal.

## Goals

1. Give every private binary file envelope identity, versioning, checksum,
   compression, bounded allocation, and complete-input validation.
2. Keep persistence schemas explicit and versioned without putting disk
   concerns into processing types.
3. Make validation independently callable and testable.
4. Preserve cheap DAG metadata reads without reading or decompressing `.dag`.
5. Remove layout/codec guessing from directory contents.
6. Keep standard glTF behavior unchanged.
7. Preserve current application semantics, topology rules, hard-link reuse,
   and error propagation apart from the deliberate format break.

## Non-goals

- Reading pre-envelope `terrain.index`, `.terrain`, or `.bin` files.
- Changing GLB or JSON glTF serialization.
- Defining raster-fundamentalis `.amort`, source-attribution, or raster index
  payloads.
- Changing hierarchy algorithms, SF merge policy, DAG construction, mesh
  processing, or snapshot publication.
- Making index serialization deterministic.
- Adding a hard-link compatibility registry or implicit transcoding between
  formats with the same ending.
- Refactoring unrelated image, tile-builder, or downloader formats.

## Format boundary

```text
octree dataset root
├── octree.storemeta                 envelope: octree.StoreMetadata
├── octree.storeindex                envelope: octree.StoreIndex
└── <layout-specific node path>
    ├── <node>.sfmesh                envelope: mesh.SfMesh
    ├── <node>.dag                   envelope: dag.Clustering
    ├── <node>.dagmeta               envelope: dag.NodeMetadata
    ├── <node>.glb                   unchanged standard debug output
    └── <node>.gltf                  unchanged standard debug output
```

The files shown under one node are alternatives except that `.dag` and
`.dagmeta` form one logical DAG node.

## Versioned payloads

Each format defines `v1` aggregate payloads, a schema alias, a latest-type
alias, and small read/write wrappers. Future `v2` types add
`from_previous(v1::Type)` as demonstrated by the envelope tests.

### Dataset metadata

`octree::storage::v1::StoreMetadata` contains:

- layout ID;
- payload class ID; and
- codec selector/format ending.

Its validator rejects empty identifiers and unknown layouts at the adapter
boundary. Payload-specific open functions additionally require the expected
payload class and codec.

### Sparse index

The versioned index payload contains the complete runtime
`store::Index<octree::StoreTraits>`. To remove the custom `octree::Id` ZPP
callback, the disk representation uses aggregate key/status entries and
explicit conversion to and from the runtime index. Conversion validates IDs,
status values, duplicates, and topology consistency.

The conversion is allowed to retain the runtime index's semantic state and
non-deterministic iteration order. It must not silently normalize malformed
serialized status combinations.

### SF mesh

`mesh::sf::v1::Payload` contains fixed-width counts and encoded byte buffers
for triangles, positions, UVs, and the optional texture. The envelope version
replaces `mesh::Encoded::Header::version`. Dimension and component-type fields
that are invariant for `mesh::Simple3d` are not used for runtime dispatch.

Free functions perform:

```text
mesh::Simple
  -> encode meshoptimizer buffers and image
  -> validate v1 payload
  -> envelope write

envelope read
  -> validate v1 payload
  -> decode meshoptimizer buffers and image
  -> mesh::Simple
```

### DAG clustering and metadata

The `.dag` payload contains the encoded clustering only. Its aggregate DTOs
use fixed-width counts, primitive coordinate aggregates, encoded
meshoptimizer buffers, encoded JPEG byte vectors, IDs, texture references,
and errors. The `.dagmeta` payload independently contains group assignment,
groups, child IDs, bounds, and errors.

The writable `Codec<dag::ClusterBatch>` returns both paths, encodes the two
payloads, and writes both envelopes. Its read path reads both envelopes and
combines their decoded latest types. `Codec<dag::NodeMetadata>` returns and
reads only `.dagmeta`, with writes unsupported.

Validation rejects malformed encoded buffers, invalid octree IDs, invalid
cluster/group/texture references, inconsistent counts, and dimensions that
would overflow allocations before constructing processing objects.

## Serialization and I/O cleanup

One bounded ZPP byte API supports the envelope implementation. Public path
helpers read and write `io::envelope::Bytes`; path wrappers compose byte I/O
with `io::envelope::serialize` and `deserialize`. All error conversion occurs
at the format/codec boundary.

The migration removes:

- raw `io::write_to_bytes`, `read_from_bytes`, `write_to_path`, and
  `read_from_path` serialization;
- the `octree::Id` custom serializer and its embedded `try_make` validation;
- `store::Index` and `NodeStatus` persistence hooks;
- `mesh::Encoded` and `ImageAndExt` persistence hooks;
- DAG GLM, ID, metadata, texture, cluster, clustering, and batch serializers;
  and
- unused standalone clustering persistence helpers.

`io::bytes` remains the filesystem byte layer. Envelope path helpers expose
its `uint8_t` buffers as `std::byte` spans without copying.

## Opening and creation lifecycle

Opening an existing private dataset requires both fixed root files:

1. Read and validate `octree.storemeta`.
2. Resolve the layout and payload codec from metadata.
3. Read and validate `octree.storeindex`.
4. Construct indexed storage.

An unreadable metadata or index file returns a typed open error. There is no
directory scan fallback.

Creating a dataset receives an explicit layout and codec, creates an indexed
in-memory store immediately, and writes node payloads while updating that
index. Finalization writes dataset metadata and the index. The implementation
does not infer layout or codec from existing node filenames.

## Implementation phases

### Phase 0 — Plan and semantic baseline

- Record this approved design.
- Retain semantic round-trip, store, merge, and builder tests rather than raw
  legacy bytes.
- Confirm the clean raster-store worktree and current local commits.

Exit criterion: format names, compatibility boundary, split DAG layout, and
metadata authority are explicit.

### Phase 1 — Consolidate envelope I/O

- Add envelope path read/write helpers and zero-copy byte-span bridging to the
  filesystem I/O layer.
- Provide a latest-version serialization overload so ordinary writers do not
  repeat the latest version number.
- Remove avoidable intermediate copies.
- Add path I/O and latest-version tests.

Exit criterion: production codecs can use the envelope without the legacy raw
serialization API.

### Phase 2 — Version dataset metadata and index

- Add versioned dataset metadata and index DTOs and schemas.
- Add free validation/conversion functions.
- Write `octree.storemeta` and `octree.storeindex` through the envelope.
- Separate dataset metadata persistence from index persistence in the shared
  storage plumbing.
- Remove directory discovery and index reconstruction by extension scanning.
- Start new outputs indexed in memory.

Exit criterion: a new empty or populated store opens only through its metadata
and index, and corrupt/mismatched files return typed failures.

### Phase 3 — Migrate SF mesh payloads

- Add the versioned `.sfmesh` wire payload.
- Move meshoptimizer and texture work into explicit encode/decode functions.
- Add free validation for counts, buffers, texture data, and mesh semantics.
- Update the SF mesh codec resolver and CLI defaults to `.sfmesh`.
- Retain `.glb`/`.gltf` unchanged.

Exit criterion: SF builder/merger/store tests round-trip `.sfmesh`; standard
debug codecs still behave as before; no `.terrain` production path remains.

### Phase 4 — Split and migrate DAG payloads

- Add versioned `.dag` clustering and `.dagmeta` metadata payloads.
- Move all DAG encoding/decoding out of ZPP callbacks.
- Add a two-file writable/readable batch codec.
- Add a separate read-only `.dagmeta` codec.
- Update DAG builder and debug converter callers.

Exit criterion: full batches round-trip through two files, metadata reads touch
only `.dagmeta`, and malformed encoded data returns errors rather than
asserting.

### Phase 5 — Remove legacy serialization and inference

- Delete domain-specific serializers and `io/serialize.h/.inl`.
- Delete unused standalone clustering persistence.
- Remove `.terrain`, `.bin`, and `terrain.index` private-format selectors and
  tests.
- Remove unindexed directory discovery and suffix-probe index scans.
- Update documentation, golden scripts, test expectations, and error text.

Exit criterion: repository search finds no production raw ZPP persistence or
legacy private extension and no format inference from directory contents.

### Phase 6 — Verification

- Run focused envelope, index, codec, storage, SF, and DAG tests after their
  phases.
- Run the complete terrainlib, DAG builder, SF builder finalization, and SF
  merger suites.
- Build `sf-builder`, `sf-merger`, `sf-index-browser`, `dag-builder`, and
  `dag-convert-debug` in the repository build configuration.
- Run the golden end-to-end workflow with new expected endings where its data
  prerequisites are available.
- Inspect `git diff --check` and the final worktree.

## Required tests

- Every production schema round-trips v1 through default zstd.
- Each schema rejects the wrong envelope class and unsupported versions.
- Corrupt compressed data and malformed aggregate payloads return typed
  errors.
- Dataset metadata remains readable when `octree.storeindex` is corrupt.
- Payload class, codec, and layout mismatches fail before node processing.
- Invalid octree IDs, node statuses, duplicates, and inconsistent topology are
  rejected by free validation/conversion functions.
- SF mesh encode/decode preserves current semantic mesh equality and rejects
  malformed meshoptimizer/image data.
- DAG full-batch reads require `.dag` and `.dagmeta`.
- DAG metadata reads succeed with only `.dagmeta` present and do not access
  `.dag`.
- Same-format node copies hard-link both DAG files; differing formats
  decode/re-encode.
- GLB/GLTF tests remain unchanged.
- New dataset construction never scans extensions to decide its format.

## Risks and controls

| Risk | Control |
|---|---|
| A future reader cannot load v1 | Versioned namespaces, schema conversion trail, and retained v1 tests |
| Dataset metadata and index disagree | Metadata is authoritative; index contains topology only |
| Broken index hides format information | Independent `octree.storemeta` is read and tested separately |
| DAG metadata reads regress to full data reads | Separate `.dagmeta` file and read-only codec tests |
| Custom ZPP callbacks continue to hide failures | Aggregate DTOs plus explicit expected-returning validation/decode |
| Meshoptimizer corruption aborts through assertions | Checked decode wrappers return typed errors |
| New compression adds excessive copies | Shared byte type, spans, moves, and copy-focused review |
| New format is accidentally linked to an old one | Every changed format uses a new ending |
| Standard debug formats are accidentally wrapped | Dedicated unchanged GLB/GLTF codecs and existing tests |
| Incomplete output becomes authoritative | Keep explicit finalization boundaries and write root metadata/index last |

## Expected migration map

| Current | Target |
|---|---|
| `terrain.index` raw ZPP DTO | `octree.storeindex` envelope schema |
| layout and preferred extension inside index | `octree.storemeta` envelope schema |
| `.terrain` raw encoded mesh | `.sfmesh` envelope schema |
| `.bin` DAG batch | `.dag` clustering plus `.dagmeta` metadata envelopes |
| `.bin` metadata-prefix view | read-only `.dagmeta` codec |
| extension/layout directory discovery | explicit metadata-driven open |
| unindexed save then recursive scan | indexed in-memory creation and final write |
| domain `serialize(Archive&, T&)` callbacks | aggregate DTO plus free encode/validate/decode |
| `io/serialize.h/.inl` | envelope path helpers |
| `.glb` / `.gltf` | unchanged |

## Verification result

The implementation builds the SF builder/merger, DAG builder/debug converter,
and SF index browser targets. The terrainlib, DAG builder, SF finalization, SF
merger, and SF builder/merger integration tests pass with the new formats. The
golden end-to-end script has been migrated to the new filenames and passes
shell syntax validation; its external-data workflow was not run as part of
this implementation.
