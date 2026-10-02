# Sampling and pyramid generation

This document defines generator-facing sampling terminology and invariants.
It deliberately separates output sampling from how an original source raster
was measured or produced.

## Terminology

### Vertex pixel

A vertex pixel is a generated value located on a grid vertex. For a tile with
`N` intervals per side, vertex positions are:

```text
x(i) = left + i × tile_width / N,  i = 0 … N
```

The output has `N+1` pixels per side:

```text
tile boundary                         tile boundary
●---------●---------●---------●---------●
```

Height maps used to form mesh vertices require this placement. Adjacent
rendering tiles contain overlapping copies of their shared edge and corner
vertex pixels.

### Area pixel

An area pixel is a generated value associated with one raster cell. For a tile
with `N` cells per side, effective cell-centre positions are:

```text
x(i) = left + (i + 1/2) × tile_width / N,  i = 0 … N-1
```

The output has `N` pixels per side:

```text
tile boundary                         tile boundary
│    ×         ×         ×         ×  │
```

The term describes placement and support in the generated grid. It does not
assert that the input value was a physical area integral.

## Source semantics versus output placement

An input height raster may have been produced from LiDAR points through
gridding, interpolation, fitting, or averaging. An orthophoto may already have
passed through sensor integration, reconstruction, reprojection, and
resampling. Those histories do not decide where a delivery format requires
its output values.

Generation is modelled as:

```text
stored discrete raster
    ↓ reconstruct its implied field
continuous or evaluable field
    ↓ low-pass for target resolution
filtered field
    ↓ evaluate on requested output grid
vertex pixels or area pixels
```

The source interpretation and reconstruction rule are layer/generator policy.
Vertex-pixel and area-pixel placement are output requirements.

## Reduction by two

### Vertex pixels

At fine spacing `Δ`, fine vertex positions are `nΔ`. Coarse positions are
`2mΔ`, coinciding with every second fine location:

```text
fine:    ●---●---●---●---●
coarse:  ●-------●-------●
```

Copying every second value would be unfiltered decimation and is unacceptable
because frequencies above the new Nyquist limit would alias. The generator
must low-pass first, using a kernel centred on each retained vertex position:

```text
coarse[m] = Σ h[k] × fine[2m - k]
```

The spatial centre stays in place; the value generally changes because it is
sampled from the filtered signal.

### Area pixels

Fine area-pixel centres are `(n+1/2)Δ`. A coarse cell spans two fine cells and
has its centre at `(2m+1)Δ`, halfway between two fine centres:

```text
fine cells:   |---- × ----|---- × ----|
coarse cell:  |---------- × ----------|
```

The simplest reduction is the average of each 2x2 fine block. If fine values
are exact equal-area averages, that produces the exact average over the union
of the four cells. A box filter is not an ideal anti-aliasing filter, however,
and may be insufficient for visual imagery or other signals.

A higher-quality area-pixel reduction applies a low-pass filter with the
correct half-sample phase, centred on the coarse cell centre. Its support may
extend beyond the four cells geometrically covered by the coarse cell.

## No duplicated height borders in the store

RF snapshots store non-overlapping interiors. TB snapshots may persist filter
halos, described by metadata, without changing interior geographic extents.
These halos are distinct from overlapping vertex-pixel rendering borders.
Pyramid generation constructs a vertex-pixel output only after reconstruction
and filtering.

The implementation may obtain a requested `(N+1) × (N+1)` output window by
reading non-overlapping store chunks plus the filter halo required on every
side. The generated shared vertices must be computed from the same global
coordinates and source data for both neighbouring output tiles.

The generator must not independently clamp its filter at each tile edge.
Clamping would make an internal tile boundary behave like a data boundary and
could produce seams.

Two implementation strategies can satisfy the invariant:

1. Evaluate shared global vertex coordinates deterministically from a common
   window reader; or
2. Generate a metatile, filter it once, and split it into overlapping output
   tiles.

The first gives execution-order independence. The second may reduce repeated
I/O. They can coexist if tests establish identical results.

## Filter halos and chunk boundaries

Any nontrivial low-pass filter needs samples outside the exact output bounds.
The required halo is determined by the reconstruction and reduction filters,
not by a fixed one-pixel border flag.

The generator's window reader resolves a tile-centred window over the
quadtree. It resolves:

- physical chunks selected for the requested accuracy;
- ancestor fallback where finer data is absent;
- chunk and source-map decoding; and
- neighbouring data needed by the window.

The generator determines the requested halo. Zero attribution within a
physical source is not a boundary and does not trigger replacement of its data.
Callers prepare usable values before invoking the generic scaler; the scaler
neither assembles neighbours nor invents boundary conditions.

## Layer-specific filtering

Sampling placement alone does not determine a correct filter:

| Semantic kind | Relevant considerations |
|---|---|
| Height | low-pass before decimation; terrain error and peak loss |
| Orthophoto | linear-light filtering; no alpha support needed |
| Categorical | mode, coverage, or another categorical policy |
| Probability/coverage | conservative area averaging may be appropriate |
| Vector/normal | component filtering followed by normalization where needed |
| Source mask/NoData | importer validity and coverage rules before generic scaling |

Source NoData handling remains an importer concern. Once a raster is supplied
to the [scaling facility](#implemented-scaling-rules), attribution does not mask
any numerical input. Missing attribution and missing physical coverage are
distinct; neither should be inferred from a numeric sentinel by the scaler.

The current `radix::raster::generate_mipmap` performs a component-wise 2x2
box average. It may be a reference for simple area-pixel aggregation, but it
does not implement these policies or vertex-pixel filtering.

## Coherent coarse-source selection

The generator need not always filter the deepest available descendants. If a
physical chunk at the requested scale is sufficiently accurate, using that
single coherent source may be preferable to composing several finer sources.

The selection process is conceptually:

```text
choose physical representation(s) for the requested output and quality policy
    ↓
read a continuous window with fallback and required halo
    ↓
filter for the target resolution
    ↓
evaluate vertex pixels or area pixels
```

Source-selection/refinement policy precedes filtering. Filtering does not
change the authoritative hierarchy.

## World and dataset boundaries

The generator needs explicit rules for:

- horizontal wrapping at the Web Mercator antimeridian;
- north/south limits of the Web Mercator world;
- areas with no physical ancestor or descendant;
- zero-attribution pixels inside physically covered chunks; and
- filters whose support crosses a layer's coverage boundary.

Physically supplied zero-attribution payloads are preserved. Other
generator-specific boundary policies remain separate design work. Tests must distinguish true coverage boundaries from ordinary
internal chunk and delivery-tile boundaries. Source dataset NoData/mask
boundaries are handled during import, not by generic scaling.

## Required golden tests

Before production filtering is implemented, synthetic fixtures should prove:

1. A constant raster remains constant across chunks and pyramid levels.
2. An impulse or frequency sweep demonstrates the chosen anti-alias response.
3. Two adjacent area-pixel tiles match a single equivalent metatile result.
4. Two adjacent vertex-pixel tiles produce bit-identical shared edges.
5. Filtering is unchanged when a store window is split into different chunks.
6. A source boundary blends supplied payloads independently of attribution.
   Paired wrappers report representative attribution according to the
   [scaling rules](#implemented-scaling-rules): repeated local 2x2 mode reduction, including
   zero in the vote, and nearest-neighbour attribution for upscaling.
7. Existing zero-attribution pixels contribute normally and retain their
   supplied or resampled payloads; missing physical coverage follows the
   caller's explicit clamped-view policy. Import fixtures separately verify
   source NoData and mask handling.
8. A coherent physical parent can be chosen instead of finer descendants.
9. TMS and Slippy input IDs normalize to the same canonical spatial tile.

The [scaling rules](#implemented-scaling-rules) specify coefficients for
the area-pixel operations. Other layer-specific and
vertex-pixel filters remain separate design decisions; tests should fix their
numeric tolerances only after representative evaluation.

A generator needing the seam-free vertex-generation invariant above must
assemble common support or generate metatiles. Halo attribution is
representative and never masks numerical contributions; zero attribution
triggers no resolution fallback.

## Implemented scaling rules

`raster::algorithm` scales and reduces single rasters; `raster_store::scale`
pairs these operations for data and attribution. This section records their
numerical contract; signatures are in the code.

### Scope

Generic scaling operates on one raster without interpreting attribution or
treating any sample as NoData. Every supplied pixel participates according to
the selected operation. Callers must supply usable, finite values, including
where the attribution raster contains zero; there is no runtime validity scan.
Nearest-neighbour sampling and zero-level cropping copy stored pixels exactly,
including nonfinite values.

Only power-of-two scale factors are supported, with area-pixel placement.
Vertex-pixel generation, world wrapping, physical-source selection and
ancestor fallback belong to callers such as the RF merger.

### Methods

| Selection | Upscaling | Downscaling |
|---|---|---|
| `NearestNeighbourAndBox` | Nearest-neighbour | Box |
| `BiliinearAndBox` | Bilinear | Box |
| `Lanczos2` | Lanczos-2 | Lanczos-2 |
| `Lanczos3` | Lanczos-3 | Lanczos-3 |
| `Lanczos4` | Lanczos-4 | Lanczos-4 |

Custom reductions operate on complete 2x2 blocks. Supplied reducers are
component-wise `Min`, `Max` and `Median` (average of the two middle values;
integer halfway cases round away from zero), and scalar `Mode` (greatest
frequency, ties broken by first occurrence in row-major order; zero is an
ordinary value).

### Sampling phase

With interior pixel centres at integer positions, direct upscaling by factor
`F` evaluates output pixel `j` at `(j + 0.5) / F - 0.5` in input coordinates.
A reduction by two evaluates pixel `j` at `2j + 0.5`. Positions are applied
independently on each axis. Halo offsets affect addressing, not phase.

Upscaling goes directly from the input to the final resolution. Downscaling
repeats reduction by two, encoding the result into the stored pixel type after
every step; the next step decodes it again. A reduction by four therefore
equals two reductions by two, but repeated median, mode, rounding and sRGB
encoding need not equal a single larger filter.

### Kernels

Lanczos upscaling with radius `a` uses normalized `sinc(x) * sinc(x/a)`
weights with `2a` taps per axis, where `sinc(x) = sin(pi*x) / (pi*x)`.
Reduction by two evaluates the same kernel at half the input distance:
weights proportional to `sinc(t/2) * sinc(t/(2a))` for `abs(t) < 2a`, with
`4a` taps, low-passing before decimation. Two-dimensional weights are products
of the axis weights, normalized over the complete support, independently of
payloads or attribution. Box reduction averages the complete 2x2 block;
bilinear interpolation uses all four neighbours.

Separable passes decode once, filter horizontally and vertically in the
working type, and encode only after both passes.

### Required halos

Inputs carry a symmetric halo, supplied physically or through a clamped view.
The scaler never clamps implicitly, including at the edges of a processing
window. Minimum halo widths in input pixels:

| Operation | Required halo |
|---|---|
| Zero levels (crop only) | 0 |
| Nearest-neighbour upscaling | 0 |
| Bilinear upscaling | 1 |
| Lanczos upscaling, radius `a` | `a` |
| Box or custom 2x2 reduction, including repeated steps | 0 |
| Lanczos reduction by total factor `F`, radius `a` | `(2a - 1) * (F - 1)` |

For one Lanczos reduction step, the first output centre is `0.5` and input
indices range from `1 - 2a` through `2a`, giving a halo of `2a - 1`. Retaining
output halo `h` needs input halo `2h + (2a - 1)`; repeated application yields
the formula. Lanczos-3 therefore needs 5 input halo pixels for reduction by
two and 15 for reduction by four.

For Lanczos upscaling by factor `F`, a symmetric output halo `h` needs input
halo `ceil(a - 0.5 + (h - 0.5) / F)`. Lanczos-3 at 2x can thus preserve five
halo pixels: a 266x266 source with a 256x256 interior produces a 522x522
window at offset `{-5, -5}`.

### Output windows

A window of the conceptual full output can be requested by offset and size.
The result equals filtering a sufficiently large raster and cropping, while
computing only the necessary samples and preserving the global phase and
reduction-stage alignment. The full conceptual output is never allocated, so
distant ancestors can be upscaled into small windows.

### Attribution and value mapping

Paired operations handle attribution independently of data: nearest-neighbour
for upscaling and repeated 2x2 `Mode` for downscaling. Zero takes part in the
vote, so `[0, 0, 0, 7]` reduces to zero. Attribution never changes data
samples, weights or results; one representative ID is kept per pixel.

The snapshot's `pixel::Mapping` selects the data conversion. `Linear` filters
stored values directly (integers through floating point, rounded and clamped
on encoding). `SRGBA` requires RGB8/RGBA8 for numerical resampling; it
decodes RGB through the sRGB transfer function to linear light, filters, and
encodes back. Alpha is linear, filtered independently and does not weight RGB.
