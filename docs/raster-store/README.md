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
- [Online tile import extension](rf-builder-downloader-design.md)

- [Tile-storage implementation plan](implementation-plan.md)
- [Raster store TODO](todo.md)
- [DRAFT RF merger plan archive](rf_merger.md)

## Scope

The documents mostly hold format information. The `rf_merger` document is
explicitly a non-authoritative idea parking lot, not a tool specification or
implementation plan.
