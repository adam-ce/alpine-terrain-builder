# DRAFT — `rf_builder`

Status: **draft archive of removed ideas**.

This is not a current implementation plan, accepted format specification, or
statement that the choices below are correct. It is a lightly reformatted
archive of RF-builder and persistent-2D material removed from
[refactor-plan.md](refactor-plan.md). The details are retained so they are not
lost; they require a separate design pass after the shared-store refactor.

## Proposed names and source boundary

The removed proposal placed the 2D implementation in
`src/terrainlib/raster_store` and used the `raster_store` namespace:

```text
src/terrainlib/raster_store/
├── StoreTraits.h
├── IndexFile.h
├── Storage.h
├── codec/
│   ├── Amort.h
│   └── Debug.h
└── store_layout/
    └── ZoomXYGoogle.h
```

The proposed boundary was:

- `store` contains dimension- and payload-neutral mechanisms;
- `raster_store` contains the 2D format, key adapters, and raster codecs; and
- subdirectory names match their namespaces where a subnamespace is used.

The proposed concrete index type was:

```cpp
store::Index<raster_store::StoreTraits>
```

## `raster_store::StoreTraits`

The removed proposal adapted `radix::tile::Id` through
`raster_store::StoreTraits` and specified that it:

- treats zoom zero as the only root;
- never calls `radix::tile::Id::parent()` at zoom zero, where it underflows;
- rejects coordinates outside `[0, 2^zoom)`;
- accepts zoom levels 0 through
  `std::numeric_limits<uint32_t>::digits`, inclusive;
- treats that maximum zoom as terminal because a child cannot be represented
  by the `uint32_t` x/y coordinates;
- validates the maximum zoom without evaluating an overflowing
  `uint32_t{1} << 32`;
- uses `radix::tile::Id::Hasher`; and
- uses the Google/XYZ convention, with the origin at the north-west, at the
  persistent boundary.

The shared code was expected to obtain roots, parents, children, validation,
and hashing through the traits without specializing on the key type.

## Node paths and layout

The removed example mapped a raster node to an extensionless `store::NodePath`:

```text
raster-store ZXY     12/2200/1400
```

The proposed lookup functions were:

```cpp
raster_store::store_layout::zoom_x_y_google()
raster_store::store_layout::from_id(id)
```

The stable layout ID was `zoom/x/y_google`. The default mapping produced
`<zoom>/<x>/<y>` directly below the snapshot root; there was no fixed
`chunks/` directory. A codec, rather than the layout, added `.amort` or debug
file endings.

## Payload and codec sketches

The removed proposal used the shared runtime codec interface and placed
raster codecs in `raster_store::codec`:

```cpp
template <typename PixelType>
struct raster_store::codec::Amort<PixelType>
    : store::Codec<raster_store::Tile<PixelType>> { .. };

template <typename PixelType>
struct raster_store::codec::Debug<PixelType>
    : store::Codec<raster_store::Tile<PixelType>> { .. };
```

`Amort` was proposed as readable and writable. `Debug` was proposed as
write-only, with runtime options for data format, attribution format, JPEG
quality, or similar debugging choices. It was not to be template-composed
from separate image codec types.

The multi-file debug example was:

```text
Debug raster codec configured for JPEG data and PNG attribution
  12/2200/1400
    -> 12/2200/1400.data.jpg
    -> 12/2200/1400.attribution.png
```

The proposed final tile path was `<zoom>/<x>/<y>.amort`. The `.amort` payload
and source-attribution-table serialization were explicitly not defined beyond
the separate storage-format notes.

## Index and format-adapter sketch

The removed proposal used a separately versioned `raster_store::v1` DTO stored
as `raster_store.index`.

The proposed version-1 contents were:

- a layout ID;
- sparse key/status entries;
- serialized `Leaf`, `Inner`, and `Virtual` values;
- `Missing` represented by absence;
- no derived aggregate metadata until a concrete query requires it;
- fixed-width `uint32_t` zoom/x/y fields instead of platform `unsigned`;
- entries ordered lexicographically by `(zoom, x, y)`; and
- duplicate keys rejected while reading.

The proposed serialization envelope contained:

- a fixed, file-type-specific 64-bit magic value generated during
  implementation;
- a 32-bit version;
- zlib CRC-32 stored as `uint32_t` and computed over the compressed payload;
- a compression enum; and
- a zstd-compressed payload using zstd's best-compression setting.

The proposal imported zstd through the project's CMake dependency facility and
used the existing `ZLIB::ZLIB` dependency for CRC-32.

Index serialization was not a responsibility of `store::Index`. The proposed
2D format adapter supplied the index filename, index conversion, mapping
lookup, and default mapping.

## Snapshot publication sketch

The removed proposal required explicit finalization/publication for the 2D
snapshot API; a destructor was not to make an incomplete snapshot
authoritative.

The publication details are now retained in
[architecture.md](architecture.md#publication). The removed proposal used a
sibling `<snapshot-id>.part` directory, wrote the index last, validated and
closed all files, and atomically renamed it to `<snapshot-id>` on the same
filesystem. The destination could not already exist.

The removed text explicitly did not promise durability or safe recovery after
a power failure, operating-system crash, or storage failure, and did not add
`fsync()`, `fdatasync()`, `FlushFileBuffers()`, or equivalent synchronization.

## Removed implementation and verification ideas

The removed 2D-adapter phase contained these items:

- add the checked persistent-key conversion around `radix::tile::Id`;
- add the `<zoom>/<x>/<y>` mapping with stable ID `zoom/x/y_google`;
- define the versioned `raster_store.index` DTO;
- implement the magic/version/checksum/compression envelope;
- add the 2D format adapter and storage aliases under `raster_store`;
- add `raster_store::codec::Amort<Payload>` when final `.amort`
  serialization is available;
- add the output-only `raster_store::codec::Debug<Payload>`;
- use a test codec instead of making `.amort` claims if final serialization is
  unavailable; and
- keep raster-specific processing outside the shared store.

The removed verification list contained:

- invalid and boundary tile IDs, including maximum zoom and rejected children;
- index serialization and validation;
- `Leaf`/`Inner` coexistence;
- sparse traversal and ancestor lookup;
- path round trips;
- snapshot hard-link reuse;
- AMORT-to-debug output conversion;
- explicit cross-filesystem/preflight failure;
- publication from `<snapshot-id>.part` to `<snapshot-id>`; and
- publication rejection when the destination already exists.

## Removed risks and controls

| Removed risk | Removed control |
|---|---|
| `radix::tile::Id` root underflows | Traits intercept root parent lookup |
| Invalid 2D coordinates become persistent | Validate on every disk/API boundary |
| Linked snapshots are modified in place | Immutable snapshot API and overwrite-disabled output |
| Hard-link failure appears late | 2D operation preflight and explicit errors |
| Generic index dictates both disk formats | Separate 3D and 2D format adapters |

All material in this document remains provisional despite the concrete names
preserved above.
