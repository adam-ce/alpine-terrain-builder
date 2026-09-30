# Raster store design

This directory describes authoritative raster tile storage and the planned
generation of delivery tile pyramids from it. RF import and merging are
implemented; TB generation and the tile server remain future work.

## Documents

- [Terminology](terminology.md)
- [Architecture](architecture.md): components, lifecycle, and the planned
  TB builder and tile server
- [Storage format](storage-format.md)
- [Sampling and pyramid generation](sampling-and-generation.md): sampling
  theory, TB generation requirements, and the implemented scaling rules
- [Tiles with halo](tiles-with-halo.md)
- [RF builder](rf-builder-design.md): GDAL and online tile import
- [RF merger](rf-merger-design.md)
- [TODO](todo.md)

Decisions and their rationale are recorded in the [ADRs](../adr/).
