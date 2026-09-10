# Terminology
** attribution raster **
: A square data matrix (image), containing indices into the source-attribution table

** source-attribution table **
: The catalog giving meaning to a snapshot's attribution indices, including resolution, dates, copyright, and license information. It may be shared among snapshots or copied into an individual snapshot.

**Attribution entry**
: A source attribution identified by its position in the applicable source-attribution table, kept stable while referenced. Several imports and validity masks may use the same entry.

**NoData pixel**
: A stored pixel with attribution index 0, regardless of its payload value. A numeric sentinel such as NaN alone does not make a pixel NoData.

**Validity mask**
: A temporary RF-builder input defining the spatial region selecting the centres
  of accepted output pixels. It does not exclude otherwise valid source
  contributions to filtering outside that region. It is not retained in the RF.

**raster-fundamentalis (rf)**
: The authoritative raster store from which delivery data is derived.

**RF import**
: Raster data accepted from one prepared source dataset under one validity mask
  and attributed to one source-attribution entry.

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
