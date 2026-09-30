# Tiles with halo

A tile with halo carries extra pixels around its interior so that filters can
be evaluated near tile edges. Halos are either persisted in a snapshot or
assembled on read from neighbouring tiles. The implementation is
`src/terrainlib/raster_store/read_tile_with_halo.h`.

## Geometry

Let `N` be the interior side, `H` the stored halo width, and `h` the requested
halo width. The interior is square with a power-of-two side of at least 64
pixels. Stored rasters have side `N + 2H`; results have side `N + 2h`, with
`0 <= h <= N`, including the corners. A halo extends the sampling region
beyond the tile ID's geographic extent at the interior's pixel spacing.

RF snapshots always have `H = 0`. TB snapshots may have `0 <= H <= N`. The
[storage format](storage-format.md#index-and-metadata) records the nominal
size, stored size and halo width in snapshot metadata.

The requested centre must be a physical tile (`Leaf` or `Inner`). Read and
decode errors of indexed payloads are propagated, not treated as missing
coverage.

## Stores with a stored halo

For `H > 0`, fail if `h > H`. Otherwise crop both stored rasters to the
requested halo width, preserving payload and attribution exactly. Neighbours
are not fetched and the stored halo is not repaired.

## Stores without a stored halo

For `H = 0`:

1. Copy the central tile's data and attribution into the interior exactly,
   including payload values with attribution zero.
2. Fill the halo from spatial neighbours, including diagonal neighbours.
   Prefer a physical tile at the requested zoom.
3. If that tile is not physical, descend into intersecting child regions,
   stopping at the first physical tile on each branch, and downscale with a
   box filter. Descent is limited to four levels.
4. For regions without a selected physical descendant, use the nearest
   physical ancestor and upscale its region with the requested interpolation.
   Descendant coverage in one region does not suppress fallback in another.
   Ancestor gaps up to 30 levels are supported.
5. Regions without any physical source replicate the central interior's edge
   through a clamped view and receive attribution zero, independently of the
   central pixel's attribution.

Wrap horizontally at the antimeridian. Do not wrap vertically; beyond the
vertical world limits, use the same replication.

Source regions are resolved from the index before reading payloads: the halo
is partitioned into disjoint regions assigned to source tiles, and each
source is read once and fills only its assigned regions. Ancestor samples
therefore never overwrite same-zoom or descendant data.

With a minimum interior side of 64 and at most four levels of descent,
descendant tile boundaries align with output-pixel boundaries, so an output
pixel never combines different source resolutions.

Selection is based on physical coverage, not attribution. Pixels with
attribution zero in a selected physical tile are valid inputs to scaling and
do not trigger a search at other levels. Attribution follows the
[representative attribution](../adr/0004-representative-attribution-for-halo-samples.md)
rule; complete provenance is not retained.

Ancestor interpolation support is clamped at the complete supplying
ancestor's edges, even if another tile exists beyond it. Neighbouring tiles
are not fetched to extend interpolation support. This avoids recursive
support dependencies and preserves once-per-source reads, but it does not
provide the seam-free invariant described in
[sampling and generation](sampling-and-generation.md#no-duplicated-height-borders-in-the-store).

The snapshot's value mapping selects linear or sRGB conversion for
resampling. Native and nearest-neighbour copies bypass conversion and are
bit-preserving.
