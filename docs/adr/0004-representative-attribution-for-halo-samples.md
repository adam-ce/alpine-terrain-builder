# Retain one representative attribution for halo samples

Halo extraction retains one representative attribution per resampled pixel,
following the rules in [Raster scaling](../raster-store/scaling.md).
Attribution indices are categorical and are never averaged.

This preserves the existing single-index attribution raster, including
persisted TB tiles, at the cost of discarding other contributing source IDs.
The retained index is representative attribution, not complete provenance.
A contributing-source set was considered and rejected.
