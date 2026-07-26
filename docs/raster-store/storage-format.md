# Storage format

This document specifies the logical format and required invariants for the following data stores:
- raster-fundamentalis (our authoritative raster store)
- tile-base (the tile-pyramid for our tile-server)
- the source-attribution table (for correct copyright attribution and data selection)

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
- the index must be checked to be smaller than 2^16-1, and we throw unsupported if it becomes larger (this is because of the storage format of tiles).
- the source attribution table is implemented in src/terrainlib/source_attribution.h, in the namespace source_attribution::*
- We need a `struct` declaration and a `using Table = .. ` (both versioned)
- the source-attributino table is stored in a file named source_attribution_table.ard (.alpine raster data)
- there is one per directory tree (it's valid for all tiles stored within the same directory tree, all siblings and children).
- given a tile (either rf or tb), the lookup of the source attribution table is first in the same directory and then in all parrent dirs, until a source_attribution_table.ard is found (or a failure is thrown).

## raster-fundamentalis (rf) format
raster-fundamentalis is our authoritative raster-store, containing only the data and no overviews / downsampled version.
- The hierarchy is a Web Mercator (EPSG:3857) quadtree keyed by tile IDs (radix::tile::ID, https://docs.maptiler.com/google-maps-coordinates-tile-bounds-projection/).
- unlike a tile pyramid, not every level is occupied (there is no downsampled versions of the data).
- stored tiles have a resolution of 4096x4096 pixels
- Every stored pixel has one payload value (can be vector type) and one source attribution index.
- Coarse physical tiles (e.g. zoom level 10) may coexist with more accurate descendants (e.g. zoom level 15).
- the source attribution index is stored as uint16, and indexes into a global source attribution table (see above). 
- data is stored in radix::Raster<type> objects (one for source attribution index, one for the actual data)
- raster-fundamentalis is implemented in src/terrainlib/raster/fundamentalis.h, in the namespace raster::fundamentalis::*
- tiles are stored in .arft files (alpine raster fundamentalis tile)

### unclear
- exact quadtree format / how to reuse structura fundamentalis code
- file names / paths / index file name

## Tile-base Format (tb)
tile-base is a hierarchy build from raster-fundamentalis, containing all data and its overviews / downsampled versions. it is used directly by the tile-server to generate tiles at the requested resolution and format.
- The hierarchy is a Web Mercator (EPSG:3857) quadtree keyed by tile IDs (radix::tile::ID, https://docs.maptiler.com/google-maps-coordinates-tile-bounds-projection/).
- every level is occupied, and every level selects an adequate data source
- stored tiles have a resolution of 4096x4096 pixels (?)
- Every stored pixel has one payload value (can be vector type) and one source attribution index.
- the source attribution index is stored as uint16, and indexes into a global source attribution table (see above)
- data is stored in radix::Raster<type> objects (one for source attribution index, one for the actual data)
- tile-base is implemented in src/terrainlib/raster/tile_base.h, in the namespace raster::tile_base::*
- tiles are stored in .artb files (alpine raster tile base)

### tile-server
- should generate tiles of requested resolution and pixel type (vertex|area) on the fly
- requests by url, e.g.: layer/vertex|area/resolution/z/x/y.ending

### unclear
- how to build tile-base from raster-fundamentalis
- details of the tile server
- file names / index file name

## serialization / deserialization and versioning
- all files are serialised with zpp::bits in two levels
 - first level contains:
  - an uint64 long file type specific random magic number, generated once at coding time, as an definitive file type identifier
  - a version number (uint32)
  - a checksum for the payload, computed from the compressed data
  - an enum for the compression algorithm
  - a payload (byte vector), the second level
 - the second level is a compressed byte array. the compressed payload is deserialised directly into the respective versioned data classes (tile or source attribution table)
- data structs are stored in versioned namespaces, e.g.: raster::fundamentalis::v1::Tile
- outside the versioned namespace, there is a using declaration for the newest version
- outside the versioned namespace, there is serialization function, taking only the newest version
- outside the versioned namespace, there is a deserialization function, taking a byte stream, and returning the newest version (convert to the newest version, if the payload encodes an older version)
- conversion to newer versions is done by the constructor, e.g. the v2::Tile constructor shall take a v1::Tile, and convert it to v2. once we have a v3, it would take a v2. this way we would have a conversion trail from v1 to v3.
- we have clear fails if
 - the magic or version is unsupported
 - if the checksum check fails
 - if the compression algorithm is missing or unsupported.
 - deserialization fails
- we fail by throwing


## to be defined
- semantic layer kind (height scalars, linear colour, gamma-encoded colour, categorical values), should be used for filtering
- human readable description?
- enumeration of layers etc?

For now, these things will be defined in code, we will have one terrain-elevation, one surface-elevation and one ortho-photo store. later probably also a percentage store (for the snow layer)
