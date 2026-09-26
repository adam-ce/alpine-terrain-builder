# Raster store design

This directory describes authoritative raster tile storage and the planned
generation of delivery tile pyramids from it. Storage format version 1 is
documented alongside the implementation status. Halo extraction, windowed
scaling, and RF import are implemented;
TB generation and the tile server remain future work.

## Documents

- [Terminology](terminology.md)
- [Architecture](architecture.md)
- [Storage format](storage-format.md)
- [Sampling and pyramid generation](sampling-and-generation.md)
- [Implementation status](implementation-status.md)

## Plans

- [Tiles with halo: design](tiles-with-halo.md)
- [Halo extraction implementation plan](halo-implementation-plan.md)
- [RF-builder design and implementation plan](rf-builder-design.md)
- [GDAL import NoData filling: policy and verification](gdal-nodata-filling.md)
- [Online tile import extension](rf-builder-downloader-design.md)
- [RF-merger design and implementation proposal](rf-merger-design.md)

- [Tile-storage implementation plan](implementation-plan.md)
- [Raster store TODO](todo.md)

## Scope

The documents describe the storage format, implemented import tools, and
proposed merger and delivery-generation behavior.
