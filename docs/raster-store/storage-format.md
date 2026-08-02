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

## alpine maps raster store format
this is the binary format used to store rf and tb tiles. both share the same basic tile format, but use it in different ways.
- The hierarchy is a Web Mercator (EPSG:3857) quadtree keyed by tile IDs (radix::tile::ID, https://docs.maptiler.com/google-maps-coordinates-tile-bounds-projection/).
- stored tiles have a resolution of 4096x4096 pixels
- Every stored pixel has one data value (can be vector type) and one source attribution index.
- the source attribution index is stored as uint16, and indexes into a global source attribution table (see above)
- data is stored in radix::Raster<Type> objects (one for source attribution index, one for the actual data), i.e. template <typename PixelType> struct raster_store::Tile { radix::Raster<PixelType> data; radix::Raster<uint16> source_attribution; };
- the file ending is .amort (AlpineMapsOrg raster tile), it is serialised used the principles outlined below.
- each snapshot stores its index as `raster_store.index`.
- the default `zoom/x/y_google` layout stores a tile as
  `<zoom>/<x>/<y>.amort`, directly below the snapshot root.

## raster-fundamentalis (rf) format
raster-fundamentalis is our authoritative raster-store, containing only the data and no overviews / downsampled version.
- unlike a tile pyramid, not every level is occupied (there is no downsampled versions of the data).
- Coarse physical tiles (e.g. zoom level 10) may coexist with more accurate descendants (e.g. zoom level 15).
- the raster-fundamentalis builder is implemented in src/rf-builder/*, it consumes raw gdal data.

## Tile-base Format (tb)
tile-base is a hierarchy build from raster-fundamentalis, containing all data and its overviews / downsampled versions. it is used directly by the tile-server to generate tiles at the requested resolution and format.
- every level is occupied, and every level selects an adequate data source
- the tile-base builder is implemented in src/tb-builder, it consumes rf

### tile-server
- should generate tiles of requested resolution and pixel type (vertex|area) on the fly
- requests by url, e.g.: layer/vertex|area/resolution/z/x/y.ending
- live in src/tile-server/*
- the delivery tile format is not yet defined 

## serialization / deserialization envelope and versioning 
-zpp::bits is a library that takes a cpp object and (de)serialises it (from)into a byte stream, 
- we serialise objects with zpp::bits in two levels
 - first level (the envelope) contains:
  - a c++ class identifier (string like type)
  - an uint64 long file type specific magic number, F5FBD3EF919428CA, as an definitive file format identifier
  - a version number (uint32)
  - an enum for the checksum, default to HandledByCompressionLib, other option None
  - a string checksum for the payload, computed from the uncompressed data (default empty, this is unused for now)
  - an enum for the compression algorithm, default to to ZStd_BestCompression, other option None.
  - a payload (byte vector), the second level
 - the second level is a compressed byte vector. the compressed payload is deserialised directly into the respective versioned data classes
- data structs are stored in versioned namespaces, e.g.:
  `raster_store::v1::Tile`
- outside the versioned namespace, there is a using declaration for the newest version
- outside the versioned namespace, there is a using declaration for the serialization function, taking only the newest version
- outside the versioned namespace, there is a deserialization function, taking a byte stream, and returning the newest version (convert to the newest version, if the payload encodes an older version)
- conversion to newer versions is done by the constructor, e.g. the v2::Tile constructor shall take a v1::Tile, and convert it to v2. once we have a v3, it would take a v2. this way we would have a conversion trail from v1 to v3.
- serialization/deserialization should be done in a templated function, receiving the class type, version, and name (string like type) as template parameters, and the object as function parameter. the functions above should be implemented in terms of the templated function, e.g. by try reading with V3, then V2, then v1. a more elegant design with a few more functions may be proposed, if it generalises nicely. I can imagine, that something with a variadic template (versioned classes) could work.
- we have clear fails if
 - the classname or magic is wrong, or the version is unsupported
 - if the checksum check fails
 - if the compression algorithm is missing or unsupported.
 - deserialization fails
- we fail by returning an unexpected in these cases
- compression: use libzstd with best compression. libzstd must be imported via the projects cmake install facility from https://github.com/AlpineMapsOrgDependencies/zstd. the context must be configured to compute and check a checksum.
- an API for compression and decompression should be created. data should be passed as std:: vector<std::byte>, error handling via std expected, dispatch between compression algorithms is done via a simple switch case.
- checksum: verify the enum is either None or HandledByCompressionLib.


## to be defined
- semantic layer kind (height scalars, linear colour, gamma-encoded colour, categorical values), should be used for filtering
- human readable description?
- enumeration of layers etc?

For now, these things will be defined in code, we will have one terrain-elevation, one surface-elevation and one ortho-photo store. later probably also a percentage store (for the snow layer)
