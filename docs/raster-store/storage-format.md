# Storage format

This document defines the shared on-disk format for raster-fundamentalis (RF)
and tile-base (TB), including source attribution. [Architecture](architecture.md)
owns system responsibilities, implementation status, attribution management,
and the snapshot lifecycle. [Terminology](terminology.md) defines the domain
language.

## Directory layout

An RF collection typically has this layout:

```text
snapshot-root/
 ├── source_attribution_table.json
 ├── store-snapshot-id.log
 └── store-snapshot-id/
      ├── raster_store.metadata
      ├── raster_store.index
      └── <zoom>/<x>/<y>.amort
```

The RF builder appends runtime messages to `<snapshot-path>.log` beside the
snapshot so startup failures and aborted runs are also recorded.

A TB snapshot has a local attribution table:

```text
tile-base-root/
 └── store-snapshot-id/
      ├── source_attribution_table.json
      ├── raster_store.metadata
      ├── raster_store.index
      ├── build_log.txt
      └── <zoom>/<x>/<y>.amort
```

The default layout ID is `zoom/x/y_google`, mapping tiles to `<zoom>/<x>/<y>`
with the codec adding `.amort`. Other layouts may map the same tile IDs
differently. [Dataset organization](architecture.md#dataset-organization)
explains shared RF tables and independent TB copies.

## Source-attribution table

`source_attribution_table.json` stores a vector of attribution objects
(`Table`, containing `Entity` objects), as a top-level JSON array. Each entry
has these required fields:

- `spatial_resolution`: numeric pixel width in metres at the equator (EPSG:3857).
- `acquisition_date`: string.
- `ingestion_date`: string.
- `copyright`: string.
- `copyright_link`: string.
- `license`: string.

Date strings are preserved verbatim without date parsing or interpretation by
the storage library. Every slot, including slot 0, contains a complete object
with all fields; slots are never null or represented by deletion markers.
Entries the user considers removed remain ordinary objects to the library.
The library implements no clearing operation or special cleared-entry values.
Removing an entry and shifting later indices is forbidden. See
[attribution management](architecture.md#dataset-organization) for ownership
and reuse policy.

Attribution indices must be strictly less than 65535; indices at or above
65535 produce an `Unsupported` error at checked table/API boundaries. The RF
builder validates attribution references before producing tiles, including references outside the selected table. Storage does
not scan attribution rasters to check indices on each tile read or write.
The reserved meaning of index 0 is defined under
[Attribution and payload preservation](#attribution-and-payload-preservation).

Table lookup checks beside the index first, then the index directory's parent,
then its grandparent. It stops at the first table found and does not search
farther. Tile attribution indices refer to this selected table. Delivered
tiles use an abbreviated form of attribution information.

## Index and metadata

`raster_store.index` and `raster_store.metadata` use separate versioned
`io::envelope` payloads. The index contains the sparse hierarchy. Metadata
contains the layout ID, codec selector, exact payload-type identifier, and
nominal/stored tile sizes, halo width, and resolved value mapping. All tiles
in a snapshot share these values. `nominal_tile_size` defaults to 4096 and
must be a positive power of two; `halo_width` defaults to zero and cannot
exceed the nominal size. `stored_tile_size = nominal_tile_size + 2 * halo_width`.
Both square tile rasters must match the stored size, which need not be a
power of two. Halo extraction asserts a nominal size of at least 64.

`raster_store/pixel.h` exposes `pixel::identifier<T>()` and
`pixel::Mapping { Linear, SRGBA }`. Creation defaults RGB8/RGBA8 to SRGBA and
other types to Linear; an explicit override takes precedence. Metadata stores
the resolved mapping. SRGBA treats RGB channels as sRGB and alpha as linear.
Storage validates the enum, while numerical operations validate whether the
pixel type supports the requested mapping. Exact copies do not need conversion.

The codec selector is a codec name string, independent of file extensions.
Opening resolves a reader from this metadata string. A future resolver may
choose among compatible reader implementations; no such registry is required
for the first codec. Existing mesh stores retain extension-based selectors.

### Version 1 representation

The metadata codec selector is `amort`; payload filenames end in `.amort`.
The versioned payloads use the existing `io::envelope` serialization. Version 1
was revised directly for halo metadata; no compatibility with earlier V1
metadata is provided. Size fields are 32-bit unsigned on supported platforms;
mapping enum values are 0 for Linear and 1 for SRGBA:

| Envelope class | Version | Payload fields in serialization order |
|---|---|---|
| `raster_store.Metadata` | 1 | layout ID string, payload-type string, codec-selector string, `unsigned` stored_tile_size, nominal_tile_size, halo_width, `pixel::Mapping` value_mapping |
| `raster_store.Index` | 1 | vector of entries: `uint32_t` zoom, x, y, then `uint8_t` status |
| `raster_store.Tile` | 1 | `uint32_t` width, height, data byte vector, attribution byte vector |

Index entries are written in `(zoom, x, y)` order. Readers reject duplicate
keys, invalid keys/statuses, and inconsistent parent/child topology. Status
values are 0 for `Leaf`, 1 for `Inner`, and 2 for `Virtual`.

Scalar payload identifiers encode representation, such as `float32`,
`float64`, `int16`, and `uint32`. GLM vectors use `vecN<scalar>`, for example
`vec3<float32>` or `vec3<uint8>`. These identifiers are independent of compiler
type names. Supported scalar aliases with the same native representation
share an identifier. Both raster buffers use the stored dimensions and
row-major order, with x varying fastest. No additional raster-specific limit
is imposed on allocation; envelope serialization retains its own limits.

### Sparse quadtree index

The hierarchy is a Web Mercator (EPSG:3857) quadtree keyed by
`radix::tile::Id`, using the [Google/XYZ convention](https://docs.maptiler.com/google-maps-coordinates-tile-bounds-projection/).
The index uses the same four logical states as the octree index:

| State     | Chunk exists | Indexed descendants |
|-----------|-------------:|--------------------:|
| `Leaf`    |     yes      |          no         |
| `Inner`   |     yes      |         yes         |
| `Virtual` |      no      |         yes         |
| `Missing` |      no      |          no         |

`Missing` is represented by absence from the index, not serialized as an
entry. [Opening and checkpoints](architecture.md#opening-and-checkpoints)
defines when these files are read and how incomplete snapshots are opened.

## Tile payload

All stored tiles in one snapshot use one payload type and area pixels. Each
pixel has one data value, which may be a vector, and one `uint16_t` index into
the source-attribution table. A tile is represented by two rasters:

```cpp
namespace raster_store {

template <typename PixelType>
struct Tile {
    radix::Raster<PixelType> data;
    radix::Raster<std::uint16_t> source_attribution;
};

} // namespace raster_store
```

Tile files use the `.amort` extension (AlpineMapsOrg raster tile) and
`io::envelope`. The writer exposes the envelope compression option, defaulting
to standard Zstandard compression with checksum. Readers accept the
algorithms supported by `io::envelope`; each envelope records its selected
algorithm. Writing is lossless to preserve data through repeated merging and
overview generation. Different codecs may be used for writing and reading
when their representations are compatible. The first version stores no
derived tile metadata such as extrema or contributing-attribution lists.

### Pixel serialization scope

Raster serialization remains templated and is instantiated only for pixel
types actually used, including basic scalar types (`float`, `double`, integer
types) and GLM vectors. The first implementation does not explicitly
instantiate a matrix of these types. Only x86 with the project's current
unpadded GLM configuration needs to be supported.

The `.amort` envelope payload stores raster dimensions and the native element
bytes of both rasters, obtained through `radix::Raster<PixelType>::bytes()`.
It does not serialize the object representation of `Raster` or `std::vector`.
One templated codec handles the buffers without per-component GLM serializers.
Compile-time checks require trivially copyable pixel types and unpadded GLM
layouts. Reading allocates typed rasters and verifies buffer byte counts
before copying. The metadata's payload-type identifier still distinguishes
equally sized types such as `float` and `uint32_t`.

See [the native-buffer decision](../adr/0002-native-raster-pixel-buffers.md).

### Attribution and payload preservation

Attribution rasters default to index 0, meaning unattributed. Neither that
index nor the fields of attribution entry 0 determine payload usability.
Attribution is not a validity mask: an ordinary numeric value with index 0
participates in generic scaling just like any other supplied data value.

Storage preserves all payloads losslessly, including NaNs and values with
attribution zero. Callers of numerical algorithms must supply usable values;
storage does not repair or validate payloads for them. Source NoData remains
an importer concern, separate from stored attribution. The single-raster
[scaling refactor](scaling.md) specifies this separation; the design is agreed,
while the existing coupled scaler still awaits replacement.
