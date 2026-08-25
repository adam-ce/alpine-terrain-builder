# Terminology
** attribution raster **
: A square data matrix (image), containing indices into the source-attribution table

** source-attribution table **
: A table of data sources, including meta data like resolution, dates etc.

**raster-fundamentalis (rf)**
: The authoritative raster store

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
