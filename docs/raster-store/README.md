# Raster store design

This directory describes a proposed authoritative raster store and the
generation of delivery tile pyramids from it. The documents are a design
baseline, not a finalized binary-format specification.

## Documents

- [Terminology](terminology.md)
- [Status quo and reuse assessment](status-quo.md)
- [Architecture](architecture.md)
- [Storage format](storage-format.md)
- [Sampling and pyramid generation](sampling-and-generation.md)

## Plans

- [Store refactoring plan](refactor-plan.md)
- [DRAFT RF builder plan archive](rf_builder.md)
- [DRAFT RF merger plan archive](rf_merger.md)

## Scope

The documents mostly hold format information. The `rf_builder` and
`rf_merger` documents are explicitly non-authoritative idea parking lots, not
tool specifications or implementation plans.
