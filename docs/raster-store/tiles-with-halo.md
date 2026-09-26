# Tiles with halo

Status: implemented on 2026-09-22. Decisions were recorded during
2026-09-13 through 2026-09-15, with scaling and physical-coverage integration
revised on 2026-09-22. Version 1 is revised directly; no existing snapshots
require compatibility.

## Accepted contract

Use halo and `halo_width`, replacing the proposed border terminology.
Return the existing `Tile<PixelType>`, containing data and attribution rasters.
Keep `radix::Raster` and `raster_store::Tile` unchanged. Do not introduce a
separate `TileWithHalo` type or add halo width to `Tile`:
the caller knows the requested width, and metadata supplies the stored width.
The extraction result is in memory; the storage format must also gain support
for persisted halos in TB snapshots.

Let `N` be the interior side, `H` the stored halo width, and `h` the requested
halo width. The interior is square and has a power-of-two side of at least
64 pixels; assert the minimum-size precondition. Both stored
rasters have side `N + 2H`; both result rasters have side `N + 2h`. A halo
extends the sampling region beyond the tile ID's interior geographic extent.
The requested width satisfies `0 <= h <= N`, including the corners. Reject
larger requests and dimension overflow.

RF snapshots always have `H = 0`. TB snapshots may have `H > 0`, with
`0 <= H <= N`. RF builders enforce zero stored halo width; generic raster
storage does not need an RF/TB discriminator for this restriction.

### Snapshot metadata and creation

The metadata here is `raster_store::io::manifest::Metadata`, persisted in
`raster_store.metadata`, not members of the raster or tile objects.
Replace metadata `width` and `height` with `stored_tile_size`,
`nominal_tile_size`, and `halo_width`, where
`stored_tile_size = nominal_tile_size + 2 * halo_width`.
Nominal tile size defaults to 4096 pixels; halo width defaults to zero.
Both rasters remain square.

Revise the existing version-1 representation directly. No metadata upgrade
or legacy-snapshot compatibility path is required.

Use `raster_store::pixel::Mapping`, with values `Linear` and `SRGBA`, as
`value_mapping` in metadata and `storage::CreateOptions`. Define it in
`raster_store/pixel.h`, alongside the public `pixel::identifier<T>()` and
internal `pixel::detail::Format`; do not define a duplicate mapping enum in
the versioned manifest namespace. The paired wrappers in
`raster_store/scaler.h` use this same type. This is a
value mapping rather than a colour-space label: linear data also includes
elevation. `SRGBA` applies to RGB as well as RGBA: RGB channels use the sRGB
transfer function; alpha, when present, is linear.

Default three- and four-component unsigned 8-bit pixel types to `SRGBA`;
default other pixel types to `Linear`. Allow an explicit mapping override,
which takes precedence over the type-based default. Persist the resolved
mapping in metadata so readers do not infer it again. This convention
supersedes file-format-based selection and mandatory declarations for
untagged TIFF/VRT imagery.

Add an RF CLI override, `--value-mapping linear|srgba`. CLI help must explain
the type-based defaults, the explicit override, and that SRGBA leaves alpha
linear and also applies to three-channel RGB. Both RF commands implement this option. It declares the stored payload mapping;
it does not alter import filtering or perform source-profile conversion.

See [GDAL value-mapping research](gdal-value-mapping-research.md) for the
metadata available from GDAL and the inspected Swissimage reference TIFF.

Keep the `storage::CreateOptions` struct and replace its `tile_dimensions`
member with `unsigned nominal_tile_size = 4096`. Add `halo_width = 0` to
creation options as well. Creation consumes these options to derive
`stored_tile_size` and write metadata, checking for dimension overflow.
This supersedes the earlier decision to specify stored size in the options.

### Extraction API

Use `read_tile_with_halo(storage, metadata, tile_id, halo_width,
interpolation_algorithm)`, returning `Expected<Tile<PixelType>>`.
Downscaling always uses box filtering; there is no downscaling-algorithm
parameter. Upscaling interpolation is caller-supplied and its parameter type
follows the scaling API. Pass metadata explicitly as
`const io::manifest::Metadata&`, allowing callers to read it once and reuse
it across extractions. No metadata member is added to `Raster` or `Tile`.

Perform resolution changes through the implemented paired wrappers in
`terrainlib/raster_store/scaler.h`, which delegate to the generic algorithms
exposed by `terrainlib/raster/algorithm.h`, using these selections.
The [scaling contract](scaling.md) owns algorithm behavior, attribution
selection, conversion-tuple contracts, and required source halos;
do not duplicate those rules here. The extractor
provides the source windows and required halos for the selected operation.

Use `raster::make_view` and `raster::algorithm::copy` for central and
same-zoom copies and for stored-halo cropping. Use a `raster::ClampedView`
of the complete central interior with `copy` for missing-coverage replication;
do not make a narrow source strip its own clamping boundary. Use the scaler's
explicit-destination overloads with output subviews wherever the geometry
matches, keeping fetched source tiles alive until their views are consumed.

Extend the existing `scale` operations with output-window overloads as
specified in [Windowed scaling](scaling.md#agreed-windowed-scaling-extension).
Use `output_offset` and infer window size from the destination; allocating
overloads take `output_size` explicitly. This permits whole source tiles to
feed destination subviews without full scaled intermediates or a separate
crop/copy step. The extension applies to upscaling and downscaling. The
four-level descent limit bounds downscaling from finer tiles; it does not
bound upscaling from ancestors. Ancestor fallback supports gaps up to 30 levels;
larger gaps return an error before calculating local scaling offsets.
Verify bounded work with a 20-level ancestor gap and the 30-level limit.

Bilinear ancestor interpolation requires one source pixel of support;
Lanczos-2/3/4 requires two/three/four respectively. Select the method with
`Resampling`. Finer-source reduction remains Box. Source
support uses a clamped view of the supplying ancestor's complete interior.
Do not fetch neighbouring tiles to extend interpolation support. If processing
a smaller cutout, preserve the full ancestor's clamping bounds rather than
clamping at the cutout edges. Clamping at the supplying ancestor's physical
edge is intentional, even if another tile exists beyond it. This avoids
recursive support dependencies and preserves once-per-source reads.

Reject unsupported mapping/type combinations only in the operation that
does not support them. Do not impose the paired scaler's RGB8/RGBA8 SRGBA
restriction on metadata creation/opening or exact-copy paths. Other metadata
and geometry validation remains applicable.

Validate the requested tile ID and require a physical tile. Both `Leaf` and
`Inner` qualify; `Virtual` and missing tiles do not. Reading or decoding an
indexed payload can fail; propagate those errors rather than treating them
as missing coverage.

### Stores without a stored halo

For `H = 0`:

1. Allocate the two result rasters with side `N + 2h`.
2. Copy the central tile's data and attribution into the interior exactly,
   including payload values with attribution zero. Do not resample the centre.
3. Fill the halo from spatial neighbours, including diagonal neighbours.
   Prefer a physical tile at the requested zoom.
4. If that tile is not physical, descend into intersecting child regions,
   stopping at the first physical tile on each branch. Downscale finer data
   to the requested pixel spacing through the scaling facility. Limit descent
   to four levels. If further
   descent would be required, use the nearest available physical ancestor.
5. For regions without a physical descendant, use the nearest physical
   ancestor and upscale its corresponding region through the scaling facility.
   Descendant coverage in one region does not suppress fallback in another.

Fetch each supplying ancestor at most once per extraction. Ancestor samples
must not overwrite regions supplied by selected same-zoom or descendant
tiles. This protection includes selected pixels with attribution zero:
missing attribution does not trigger fallback to another level.

Resolve the source regions before reading their payloads. Partition the
halo into disjoint regions assigned to source tile IDs using the accepted
selection and depth-limit rules. Group those regions by ID, load each source
once, and fill only its assigned regions. This avoids ancestor overwrites
and the need to retain all decoded ancestors simultaneously. Regions without
a selected physical source use central-tile replication as described below.

With a minimum nominal side of 64 and at most four levels of descent,
descendant tile boundaries align with requested output-pixel boundaries:
even a tile four levels finer spans at least four output pixels per side.
Consequently an output cell's geometric footprint need not combine different
selected tile resolutions. The scaling facility determines any additional
source samples required by the selected operation.

Selection is based on physical coverage, not attribution. Pixels with
attribution zero in a selected physical tile are valid inputs to scaling and
do not trigger a search at other levels. Preserve their supplied or resampled
payloads. The caller is responsible for supplying usable data values;
attribution does not mask numerical contributions. Representative attribution
follows the scaling contract. Complete provenance is not required.

Only regions without a selected physical source use central-tile replication.
Read their data through a `raster::ClampedView` of the central tile's interior,
with the uncovered region's coordinates relative to that interior. The view
replicates the closest central pixel without allocating a padded source.
Set attribution to zero for these synthesized pixels, independently of the
central pixel's attribution. Do not replace a supplied or resampled payload
merely because its attribution is zero.

Wrap horizontally at the antimeridian. Do not wrap vertically. Beyond the
vertical world limits, and wherever no selected physical representation covers
a region, use the same clamped-view replication with attribution
zero. The central interior remains unchanged.

Pass the metadata's `pixel::Mapping` to the paired scaling wrappers, which
select the supplied linear or sRGB decoder/encoder tuple. Do not pass a
value-mapping enum into the generic algorithms. Native and
nearest-neighbour copies bypass conversion and remain bit-preserving.
For numerical resampling, callers must supply finite decoded values and keep
intermediate results finite and encodable. Rescaling follows the facility's
numeric and attribution contract; no separate radix rescaling refactor is
required for extraction.

### Stores with a stored halo

For `H > 0`, fail if `h > H`. Otherwise crop both stored rasters to the
requested halo width, removing `H-h` pixels on each side. Preserve the
retained payload and attribution exactly, including zero-attribution pixels
and stored halo values. Do not fetch neighbours or repair the stored halo.
A request for zero halo width returns the interior; requesting the full
stored width returns all stored pixels.

## Relationship to existing documents

The previously documented allowance for arbitrary positive square dimensions
was intended to accommodate halo-inclusive rasters. The revised model makes
the interior power-of-two and expresses halo width separately. Existing
version-1 snapshots do not need preserving.

The `Mixed sources` section has been removed from
`sampling-and-generation.md` as requested. Halo samples use representative
attribution. Further alignment of the existing storage/sampling documents
with the final halo contract belongs to the implementation plan.

## Implementation

Conversion tuples remain in the scaling interface. The staged
[implementation plan](halo-implementation-plan.md) records the completed stages
and verification results. The public implementation is
`src/terrainlib/raster_store/read_tile_with_halo.h`.

## Required verification

- Invalid, missing, and virtual centre IDs; physical leaf and inner centres.
- Bit-exact central copies, including zero-attribution payloads, and requests
  for zero halo width.
- Same-zoom edges/corners, first-physical descendant selection, four-level
  descent limit with ancestor fallback, and partial coverage.
- At-most-once ancestor fetches and preservation of selected finer samples,
  including zero-attribution regions, when ancestor fallback is also used.
- Bilinear and Lanczos support clamp at the full supplying ancestor's edges, not a
  processing cutout's edges, without reading neighbouring support tiles.
- Numerical contributions from zero-attribution pixels, representative
  attribution, and preservation of supplied or resampled zero-attribution
  payloads even when all source attributions are zero.
- Clamped-view replication only for missing physical coverage, including
  replication from zero-attribution central pixels; synthesized attribution
  remains zero even when the replicated central pixel has nonzero attribution.
- Horizontal wrapping, vertical limits, and uncovered dataset regions.
- Using box downscaling, forwarding the shared `pixel::Mapping` and the
  interpolation parameter to the paired scaling wrappers,
  and exact native/nearest-neighbour copies without invoking conversion.
- Type-based mapping defaults, explicit overrides, persistence of the
  resolved mapping, linear alpha, and CLI help documenting the policy.
- Stored-halo cropping at zero, partial, and full width; rejection of requests
  exceeding the stored halo; preservation of stored zero-attribution samples.
- Power-of-two interiors, the minimum side of 64, halo-width bounds, and
  dimension overflow.
- Revised version-1 metadata and payload round trips.
- Integration with the scaling facility, including sufficient source halos,
  multi-level reductions, and the extractor's missing-coverage replication.
- Windowed upscaling/downscaling into assigned output views, preserving full
  result sampling phase and reduction stages while limiting computed samples.
