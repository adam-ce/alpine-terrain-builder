# Raster store design

This directory describes authoritative raster tile storage and the planned
generation of delivery tile pyramids from it. Storage format version 1 is
documented alongside the implementation status; builders and generators
remain future work.

## Documents

- [Terminology](terminology.md)
- [Architecture](architecture.md)
- [Storage format](storage-format.md)
- [Sampling and pyramid generation](sampling-and-generation.md)
- [Implementation status](implementation-status.md)

## Plans

- [Tile-storage implementation plan](implementation-plan.md)
- [Raster store TODO](todo.md)
- [DRAFT RF builder plan archive](rf_builder.md)
- [DRAFT RF merger plan archive](rf_merger.md)

## Scope

The documents mostly hold format information. The `rf_builder` and
`rf_merger` documents are explicitly non-authoritative idea parking lots, not
tool specifications or implementation plans.
