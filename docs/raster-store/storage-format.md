# Storage format
This document specifies the logical format and required invariants for the following data stores:
- raster-fundamentalis (our authoritative raster store)
- tile-base (the tile-pyramid for our tile-server)
- the source-attribution table (for correct copyright attribution and data selection)

## Implementation status
The 3D octree stores now use the versioned envelope formats. The shared store
still supplies only an in-memory `raster_store::StoreTraits` adapter for
`radix::tile::Id`; there is currently no persistent RF index, `.amort` codec,
snapshot publisher, or RF opening API.


## Source-attribution table
The source attribution table is used in raster-fundamentalis, tile-base, and in abbreviated format in the delivered tiles.
- stores a vector of structs (called Table), each struct describing one source
- the struct (called Entity) contains:
 - the spatial resolution in pixel-width at the equator (EPSG:3857)
 - the date of data acquisition
 - the date of ingestion
 - a copyright string
 - a copyright link string
 - a license string
- the index 0 is reserved for "no-data"
- the index must be checked to be smaller than 2^16-1, and we produce an unsupported error if it becomes larger (this is because of the storage format of tiles).
- the source-attributino table is stored in a file named source_attribution_table.json
- there is one per directory tree
- given a tile or dataset (either rf or tb), the lookup of the source attribution table is first in the same directory and then in all parrent dirs, until a source_attribution_table.json is found (or a failure is thrown).

## alpine maps raster store format
this is the binary format used to store rf and tb tiles. both share the same basic tile format, but use it in different ways.
- The hierarchy is a Web Mercator (EPSG:3857) quadtree keyed by tile IDs (radix::tile::ID, https://docs.maptiler.com/google-maps-coordinates-tile-bounds-projection/).
- stored tiles have a resolution of 4096x4096 pixels
- Every stored pixel has one data value (can be vector type) and one source attribution index.
- the source attribution index is stored as uint16, and indexes into a global source attribution table (see above)
- data is stored in radix::Raster<Type> objects (one for source attribution index, one for the actual data), i.e. template <typename PixelType> struct raster_store::Tile { radix::Raster<PixelType> data; radix::Raster<uint16> source_attribution; };
- the file ending is .amort (AlpineMapsOrg raster tile), it is serialised using io::envelope.

## raster-fundamentalis (rf) format
raster-fundamentalis is our authoritative raster-store.
- unlike a tile pyramid, not every level is occupied (there is no downsampled versions of the data).
- Coarse physical tiles (e.g. zoom level 10) may coexist with more accurate descendants (e.g. zoom level 15).

## Tile-base Format (tb)
tile-base is a hierarchy build from raster-fundamentalis, containing all data and its overviews / downsampled versions.
- every level is occupied, and every level selects an adequate data source

## tile-server
- the delivery tile format is not yet defined
- needs at least support for dxt1 / etc2 compression, jpeg / png, compressed radix::Rasters<>
- also support for metadata (like bounding sphere tree in ECEF)
