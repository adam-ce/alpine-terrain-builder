# Retain one representative attribution for halo samples

Halo extraction retains one representative attribution per resampled pixel,
following the rules in [Raster scaling](../raster-store/scaling.md).
Attribution indices are categorical and are never averaged. The refactor
agreed on 2026-09-22 delegates paired operations to `raster_store::scaler`,
whose thin wrappers call the generic single-raster algorithms independently
for data and attribution: nearest-neighbour upscaling and repeated scalar
mode reduction for attribution. Zero participates as an ordinary unattributed
category and never controls which data samples contribute to arithmetic.

This preserves the existing single-index attribution raster, including
persisted TB tiles, at the cost of discarding other contributing source IDs.
The retained index is representative attribution, not complete provenance.
A contributing-source set was considered and rejected.
