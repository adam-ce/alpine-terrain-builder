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

The authoritative store does not persist overlapping rendering borders.
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

The store reader should expose a logical raster window over the quadtree. It
resolves:

- physical chunks selected for the requested accuracy;
- ancestor fallback where finer data is absent;
- chunk and source-map decoding; and
- neighbouring data needed by the window.

The generator determines the requested halo and applies boundary conditions
only at true dataset/world boundaries or NoData boundaries.

## Mixed sources

The raster store contains exactly one payload and one source ID for each
stored pixel. A generator filter may span pixels attributed to several
sources:

```text
store pixels:     A  A  A  B  B
filter support:      [-------]
output value:       blend of A and B payloads
```

This is allowed. A generated output pixel does not retain a source ID. The
generated tile records tile-level provenance, at minimum the set of source IDs
whose payload values contributed nonzero filter weight to any output pixel.

Source IDs are categorical and are never averaged. Payload filtering and
provenance collection are parallel operations:

```text
numeric payload samples → weighted filtered value
source IDs              → contributing-source set
```

The exact handling of invalid/NoData samples requires a policy. A common
continuous-raster rule is to normalize by the total weight of valid samples,
but that must not be applied automatically to categorical data.

## Layer-specific filtering

Sampling placement alone does not determine a correct filter:

| Semantic kind | Relevant considerations |
|---|---|
| Height | low-pass before decimation; terrain error and peak loss |
| Orthophoto | linear-light filtering; no alpha support needed |
| Categorical | mode, coverage, or another categorical policy |
| Probability/coverage | conservative area averaging may be appropriate |
| Vector/normal | component filtering followed by normalization where needed |
| Mask/NoData | validity-aware weights and explicit coverage rules |

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
- NoData holes inside otherwise covered chunks; and
- filters whose support crosses a layer's coverage boundary.

These rules are not yet decided. Tests must distinguish true boundaries from
ordinary internal chunk and delivery-tile boundaries.

## Required golden tests

Before production filtering is implemented, synthetic fixtures should prove:

1. A constant raster remains constant across chunks and pyramid levels.
2. An impulse or frequency sweep demonstrates the chosen anti-alias response.
3. Two adjacent area-pixel tiles match a single equivalent metatile result.
4. Two adjacent vertex-pixel tiles produce bit-identical shared edges.
5. Filtering is unchanged when a store window is split into different chunks.
6. A source boundary blends payloads but reports both tile-level sources.
7. A NoData boundary follows the configured validity rule.
8. A coherent physical parent can be chosen instead of finer descendants.
9. TMS and Slippy input IDs normalize to the same canonical spatial tile.

The filter coefficients and acceptable numeric tolerances remain open design
decisions. The tests should lock them only after representative evaluation.
