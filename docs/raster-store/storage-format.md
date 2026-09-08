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
 └── store-snapshot-id/
      ├── raster_store.metadata
      ├── raster_store.index
      ├── build_log.txt
      └── <zoom>/<x>/<y>.amort
```

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
(`Table`, containing `Entity` objects). Each entry has these fields:

- Spatial resolution in pixel-width at the equator (EPSG:3857).
- Date of data acquisition, stored as a string.
- Date of ingestion, stored as a string.
- Copyright string.
- Copyright link string.
- License string.

Date strings are preserved verbatim without date parsing or interpretation by
the storage library. Every slot, including slot 0, contains a complete object
with all fields; slots are never null or represented by deletion markers.
Entries the user considers removed remain ordinary objects to the library.
Removing an entry and shifting later indices is forbidden. See
[attribution management](architecture.md#dataset-organization) for ownership
and reuse policy.

Attribution indices must be strictly less than 65535; indices at or above
65535 produce an `Unsupported` error. Pixel validity and the reserved meaning of
index 0 are defined under [NoData](#nodata-and-payload-preservation).

Table lookup checks beside the index first, then the index directory's parent,
then its grandparent. It stops at the first table found and does not search
farther. Tile attribution indices refer to this selected table. Delivered
tiles use an abbreviated form of attribution information.

## Index and metadata

`raster_store.index` and `raster_store.metadata` use separate versioned
`io::envelope` payloads. The index contains the sparse hierarchy. Metadata
contains the layout ID, codec selector, exact payload-type identifier, and
tile dimensions. Tile dimensions default to 4096x4096 pixels and apply to all
tiles in that snapshot. Any positive square dimensions are permitted; side
lengths need not be powers of two. Both tile rasters must match the metadata
dimensions.

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

### NoData and payload preservation

Attribution rasters default to index 0, the sole indicator of NoData. The
fields of attribution entry 0 do not determine pixel validity. A NaN with a
nonzero attribution index remains an attributed value, while an ordinary
numeric value with index 0 is NoData.
Storage preserves payloads losslessly, including NaNs and values underneath
NoData pixels. Consumers are responsible for interpreting these values.
