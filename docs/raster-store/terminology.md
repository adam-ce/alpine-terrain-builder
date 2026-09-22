# Terminology

The planned extraction and storage contract is in
[Tiles with halo](tiles-with-halo.md).

**Tile interior**:
The square raster region corresponding to a tile ID's geographic extent,
excluding its halo.

**Stored tile size**:
The side length in pixels of a persisted tile raster, including its halo.

**Nominal tile size**:
The side length in pixels of a tile's interior, excluding its halo.

**Value mapping**:
The interpretation of stored numeric values as linear values or sRGB-encoded
colour channels with linear alpha where present. Linear values include
elevation data as well as linear colour data.

**Tile halo**:
Additional raster pixels surrounding a tile interior on all four sides,
including the corners, at the interior's pixel spacing.
_Avoid_: Tile border

**Halo width**:
The number of halo pixels on each side of a tile interior.
_Avoid_: Border width

**Tile with halo**:
A tile whose data and attribution rasters cover both its interior and halo.
A halo width of zero is permitted.
_Avoid_: Tile with borders, tile with border

**Stored halo width**:
The halo width of the tile rasters persisted in a raster-store snapshot.

**Requested halo width**:
The halo width of the tile requested by a consumer of the raster store.

** attribution raster **
: A square data matrix (image), containing indices into the source-attribution table

** source-attribution table **
: The catalog giving meaning to a snapshot's attribution indices, including resolution, dates, copyright, and license information. It may be shared among snapshots or copied into an individual snapshot.

**Attribution entry**
: A source attribution identified by its position in the applicable source-attribution table, kept stable while referenced. Several imports and validity masks may use the same entry.

**Unattributed pixel**
: A stored pixel with attribution index 0. Missing attribution makes no claim
  about the usability of its payload and does not exclude it from arithmetic.
_Avoid_: NoData pixel, invalid pixel (for a stored zero-attribution pixel)

**Source NoData**
: Missing or invalid samples declared by an input dataset, such as through a
  GDAL NoData value or source validity mask. This is an import concern, distinct
  from missing attribution in stored rasters.

**Validity mask**
: A temporary RF-builder input defining the spatial region selecting the centres
  of accepted output pixels. It does not exclude otherwise valid source
  contributions to filtering outside that region. It is not retained in the RF.

**raster-fundamentalis (rf)**
: The authoritative raster store from which delivery data is derived.

**RF import**
: Raster data accepted from one prepared GDAL dataset or one online tile
  pyramid under one validity mask and attributed to one source-attribution entry.

**Source tile**
: An input raster covering one tile-grid region at a particular source zoom.
  It is distinct from an RF tile, which can cover the region of many source tiles.

**Source tile pyramid**
: A hierarchy of source tiles representing a region at different zoom levels,
  potentially with different deepest available levels in different places.

**Source zoom**
: The spatial subdivision level of the source tile grid. It does not express
  pixel spacing without the source tile dimensions and need not equal RF zoom.

**Minimum source zoom**
: The first source level considered by an import; lower levels are outside
  its source search and fallback range.

**Maximum source zoom**
: The ceiling of an import's source search. It does not require all regions
  to have data at that level.

**RF leaf tile**
: A physical RF tile with no physical descendants. A disjoint RF import has
  neither a physical ancestor nor a physical descendant of any retained tile.

**Ancestor fallback**
: Coarser source imagery supplying a region where finer source imagery is
  absent. The supplying source ancestor need not be retained as an RF tile.

**Local sampling ratio**
: The largest directional stretch from an output pixel spacing into source-pixel
  coordinates at a location. It expresses the local reduction in source detail
  without treating a pure rotation as a change in resolution.

**tile-base (tb)**
: basically rf with overviews, used to generate derived tiles

**Vertex pixel**
: A generated value located on a grid vertex. Height tiles for mesh generation
  require vertex pixels, including shared boundary positions.

**Area pixel**
: A generated value associated with a raster cell. Ordinary texture outputs
  use area pixels whose cell boundaries align with tile boundaries.

The terms vertex pixel and area pixel describe generator outputs. They do not
assert how an original sensor or source raster produced its values.


**tile**
: A chunk of raster data with an tile ID. Rf is a store for tiles, tb is a store for tiles, and we generate derived tiles / output tiles for the client.

**Derived / output tile**
: A filtered and encoded output tile generated from the tile base store.
It may have different dimensions, sampling placement, encoding, and
provenance granularity from a store chunk.
