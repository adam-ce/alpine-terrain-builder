# Raster scaling

Status: behavioral design agreed on 2026-09-18, with the unified zoom-level
API convention agreed on 2026-09-19. Implementation was authorized and
completed on 2026-09-19. The scaler and its tests are implemented; halo
extraction and its storage metadata remain separate work.

## Scope

Implement scaling in `terrainlib/raster/algorithm.h`, operating on data and
attribution rasters together. Only power-of-two scale factors are supported.
Input and output use area-pixel placement; vertex-pixel generation and
area/vertex conversion are outside this facility.

The caller supplies a raster with halo and an explicit halo width. Return
data and attribution rasters with the halo removed. World wrapping, dataset
boundary handling, source selection, and ancestor fallback belong to the
caller or halo extractor. The scaler does not independently clamp internal
tile edges.

See [Sampling and pyramid generation](sampling-and-generation.md) for the
broader generator requirements, including vertex outputs outside this scope.

## Interface and geometry

Accept matching rectangular data and attribution rasters, with one halo width
on all sides. Scale both axes by the same factor. Expose a unified
`raster::algorithm::scale` entry point. Keep directional `upscale` and
`downscale` helpers in `raster::algorithm::detail`.

Express scaling amounts as `n_zoom_levels`, the exponent of two, rather than
as the factor itself. The public `scale` and `required_halo` functions use
a signed count: positive means upscaling, negative means downscaling, and
zero means cropping only. For example, `+2` upscales by four and `-2`
downscales by four. `required_halo` returns zero for zero levels.

Directional helpers and custom reduction use nonnegative exponent counts:
zero means factor one and two means factor four in the respective direction.

Accept both input rasters by const reference. Return
`Expected<std::pair<radix::Raster<PixelType>, radix::Raster<std::uint16_t>>>`,
with data first and attribution second; do not introduce a custom result
struct. The implementation plan will present exact function declarations.

For downscaling, both interior dimensions must be divisible by the factor.
Factor one simply crops the halo from both rasters. Positive dimensions,
matching raster sizes, halo bounds, representable zoom-level counts, and
dimension overflow need validation. Invalid geometry, zoom-level counts,
and insufficient halos return
errors through `Expected`; concrete error codes follow project conventions.

With input interior pixel centres indexed by integers, direct upscaling by
factor `F` evaluates output pixel `j` at `(j + 0.5) / F - 0.5` in input
coordinates. A reduction by two evaluates pixel `j` at `2j + 0.5`.
Apply these positions independently on each axis, relative to the interior;
the halo offsets buffer addressing rather than changing the sampling phase.

## Algorithms

Upscaling offers nearest-neighbour and bilinear interpolation. Cubic and
Lanczos upscaling have been removed from the proposed scope. Algorithm
selection is caller policy; this facility supplies the implementations.
Upscale directly from the input to the requested final resolution rather than
repeating interpolation by two.

Downscaling offers box filtering and Lanczos with lobe count/support radius
2, 3, or 4. Lanczos support is widened for the reduction ratio to low-pass
before decimation. Implement reduction by two and repeat it for larger
power-of-two reductions.

For Lanczos radius `a`, the one-dimensional reduction kernel is proportional
to `sinc(t / 2) * sinc(t / (2a))` for `abs(t) < 2a` and zero otherwise, where
`sinc(x) = sin(pi*x) / (pi*x)` and `sinc(0) = 1`. Two-dimensional weights
are the products of the axis weights. Normalize over valid contributions
as specified below; a constant kernel multiplier cancels in normalization.

Custom reduction also operates on a 2x2 block at each reduction step. Expose
templated interfaces accepting lambdas and functors, without requiring a
function pointer or type-erased callable. Provide min, max, and median
functors. A reducer receives only valid samples, already decoded to linear
values; the scaler encodes its result as needed. It is never called with an
empty sample range.

The callable accepts `std::span<const WorkingPixel>` containing one to four
valid decoded samples in row-major order, with invalid samples omitted, and
returns one `WorkingPixel`. Its component type follows the working-type
table below; vector length is preserved. The supplied min, max, and median
functors operate component-wise. For an even number of samples, median is
the average of the two middle component values. A NaN in a component
propagates to that result component in these supplied functors. Custom
functors control their own treatment of attributed NaNs.

## Validity and representative attribution

Attribution zero is the sole NoData marker. A nonzero attribution remains
valid regardless of its payload, including a nonfinite floating-point value.
Exclude NoData samples from interpolation, filtering, and custom reduction.
Their payload values must not contribute to arithmetic or normalization.

For every upscaling mode, sample attribution with nearest-neighbour sampling.
Do not vote among neighbouring attribution IDs during upscaling. When data
also uses nearest-neighbour sampling, copy the selected input pixel's payload
and attribution exactly, including NoData, without a colour-conversion round
trip. For bilinear interpolation, if the nearest pixel's attribution is zero,
copy that pixel's payload and skip interpolation, even if other neighbours
are valid. Otherwise interpolate using only nonzero-attribution neighbours
and normalize over their weights. Never mix NoData payloads into a valid
output value.

For reduction by two, the local block is the 2x2 input block covered by the
output cell. Select attribution using this local block, independently of the
larger Lanczos support:

1. Ignore attribution zero.
2. Prefer a nonzero ID occurring at least twice in the local block.
3. Otherwise choose a nonzero ID occurring at least once.
4. Resolve multiple eligible IDs deterministically; the particular tie order
   is not prescribed.

If this reduction block contains no valid pixel, return attribution zero and copy
the payload of one pixel from that block, even if valid samples exist farther
out in the filter support. The payload is unspecified to callers when
attribution is zero, but remains initialized; no C++ undefined behavior is
intended.

Otherwise, reduction filters use valid samples from their filter support and
normalize by the sum of their weights. If that denominator is zero or
numerically unstable, copy a valid pixel from the local block. Attribution
still follows the local voting rule. The implementation treats the denominator
as unstable when `abs(sum(weights)) <= 32 * epsilon * sum(abs(weights))`,
where `epsilon` belongs to the working component type. Negative stable sums
are permitted. Zero-weight samples do not contribute payload arithmetic.

Keep one representative attribution per output pixel. Complete provenance
and contributing-source sets are not required. Apply attribution selection
again at every reduction step; do not retain original-source weight totals.

## Value mapping and numeric behavior

Support the existing arithmetic scalar and GLM-vector pixel types, returning
the same stored pixel type. Numeric filtering is component-wise. Normal
normalization and other layer-specific postprocessing belong to callers.

Accept an explicit value mapping. Linear mapping applies to scalar and vector
data, including elevations. Initially support sRGB mapping only for
three- and four-component unsigned 8-bit pixels. Decode RGB to linear light
before filtering or custom reduction, and encode the resulting RGB values
afterwards. For RGBA, alpha is linear and filtered independently; alpha does
not weight RGB or determine NoData validity.

Choose the working component type from the stored component type:

| Stored component type | Working component type |
|---|---|
| 32-bit `float` | `float` |
| Signed or unsigned integers up to 16 bits | `float` |
| `double` | `double` |
| Signed or unsigned 32-bit integers | `double` |
| Signed or unsigned 64-bit integers | `long double` |

Apply the same mapping to GLM-vector components. For example,
`glm::vec<4, uint16_t>` uses four `float` working components. This mapping
also determines the decoded samples and results used by custom functors.

Round integer results, with half-way cases rounded away from zero, and clamp
them to their representable range before conversion. Preserve floating-point
overshoot, NaN, and infinity results. Return an error if a custom reducer
produces a nonfinite result destined for integer storage.

Filtering uses the specified working type, followed by rounding and
clamping as applicable; results are subject to that type's floating-point
precision. This also applies to weighted 64-bit integer results. Assess
weight cancellation relative to the sum of absolute weights using the
threshold above. Numerical tests use tolerances appropriate to the working
type and fixture, as recorded below.

Each reduction step stores its result in the output pixel type, including
rounding and sRGB re-encoding. A reduction by four therefore follows the
same stage semantics as two explicit reductions by two, with enough halo
available for the second step. This includes custom reduction and attribution.

Repeated validity-aware reduction is deliberately not a direct reduction
over all original valid samples. For example, one 2x2 block containing one
valid zero and another containing four valid values of 100, with the other
blocks empty, produces 50 after two box reductions rather than the direct
five-sample average of 80. Repeated median likewise need not equal the median
of the original larger block.

## Halo contract

Reject an insufficient input halo. Determine the requirement from the
complete operation, including every step of repeated reduction, rather than
only the final kernel. Provide a query for the required input halo.

Retain enough intermediate halo for subsequent reductions and crop only the
final result to the requested interior. Internal tile edges must not change
filter support or results.

The area-grid geometry and compact kernel support imply these minimum input
halo widths, in input pixels on each side, including corners:

| Operation | Required halo |
|---|---|
| Factor one (crop only) | 0 |
| Nearest-neighbour upscaling | 0 |
| Bilinear upscaling, factor greater than one | 1 |
| Box or custom 2x2 reduction, including repeated steps | 0 |
| Lanczos reduction by total factor `F`, fixed radius `a` | `(2a - 1) * (F - 1)` |

For one Lanczos step, the first output centre is `0.5` and its contributing
input indices range from `1 - 2a` through `2a`, giving a halo of `2a - 1`.
If that step must retain an output halo `h`, its input halo must be
`2h + (2a - 1)`. Repeating this relation gives the total-factor formula.
For example, Lanczos-3 needs 5 input halo pixels for reduction by two and
15 for reduction by four. The required-halo query must check arithmetic
overflow as well as the operation's parameters. The public query takes
`n_zoom_levels`; `F` in the formulas above is the derived power-of-two
factor. Validate the exponent before constructing that factor, including
handling the signed count without overflowing when obtaining its magnitude.

There is no `required_reduction_halo` function: custom block reduction
always requires zero halo. Its entry point still validates the zoom-level
count and dimensions.

## Relationship to halo extraction

The [halo extractor](tiles-with-halo.md) uses this facility with box
downscaling only. It has no downscaling-algorithm parameter. The upscaling
interpolation algorithm remains an extraction parameter. Scaling behavior
is defined here rather than repeated in the halo design.

World wrapping and other boundary behavior remain the halo extractor's
responsibility. Its central-tile payload replication for uncovered or invalid
halo pixels is a separate caller policy that must not be silently replaced
by the scaler's local fallback. The
[attribution ADR](../adr/0004-representative-attribution-for-halo-samples.md)
records why one representative source ID is retained rather than complete
provenance, referring here for the selection rules.

## Implementation

The scaler and tests were implemented in the terrain-builder worktree.
Halo extraction and its storage metadata remain a separate implementation
task; their documentation already describes use of this facility.

### Public API

The API is in `src/terrainlib/raster/algorithm.h`, in namespace
`raster::algorithm`. The public
`scale` and `required_halo` use the agreed signed direction convention;
directional helpers and custom reduction use unsigned exponent counts.

```cpp
namespace raster::algorithm {

enum class Interpolation { NearestNeighbour, Bilinear };
enum class Filter { Box, Lanczos2, Lanczos3, Lanczos4 };
enum class ValueMapping { Linear, SRGBA };

template <typename PixelType>
Expected<std::pair<radix::Raster<PixelType>, radix::Raster<std::uint16_t>>>
scale(const radix::Raster<PixelType>& data,
    const radix::Raster<std::uint16_t>& source_attribution,
    unsigned halo_width, int n_zoom_levels,
    Interpolation interpolation, Filter filter, ValueMapping value_mapping);

template <typename PixelType, typename Reducer>
Expected<std::pair<radix::Raster<PixelType>, radix::Raster<std::uint16_t>>>
reduce(const radix::Raster<PixelType>& data,
    const radix::Raster<std::uint16_t>& source_attribution,
    unsigned halo_width, unsigned n_zoom_levels,
    ValueMapping value_mapping, Reducer&& reducer);

Expected<unsigned> required_halo(
    int n_zoom_levels, Interpolation interpolation, Filter filter);

namespace detail {

template <typename PixelType>
Expected<std::pair<radix::Raster<PixelType>, radix::Raster<std::uint16_t>>>
upscale(const radix::Raster<PixelType>& data,
    const radix::Raster<std::uint16_t>& source_attribution,
    unsigned halo_width, unsigned n_zoom_levels,
    Interpolation interpolation, ValueMapping value_mapping);

template <typename PixelType>
Expected<std::pair<radix::Raster<PixelType>, radix::Raster<std::uint16_t>>>
downscale(const radix::Raster<PixelType>& data,
    const radix::Raster<std::uint16_t>& source_attribution,
    unsigned halo_width, unsigned n_zoom_levels,
    Filter filter, ValueMapping value_mapping);

Expected<unsigned> required_upscaling_halo(
    unsigned n_zoom_levels, Interpolation interpolation);
Expected<unsigned> required_downscaling_halo(
    unsigned n_zoom_levels, Filter filter);

} // namespace detail
} // namespace raster::algorithm
```

The filter enum exposes all three Lanczos radius choices without a separate
parameter that would be meaningless for box filtering. The unified functions
receive interpolation and filter selections and use the one corresponding
to the requested direction. Directional halo calculations remain detail
helpers. Custom block reduction has no halo-query function.

Expose `WorkingPixel<PixelType>` as a type alias implementing the agreed
component mapping, and `Min`, `Max`, and `Median` as stateless functors with
templated call operators accepting the agreed spans. Constrain the custom
reduction overload to a callable with that input and working-pixel result.
Reuse the supplied functor through the repeated stages without type erasure.

The scaler `ValueMapping` is independent of snapshot metadata.
The planned metadata enum does not exist in code yet; the future halo
extractor can explicitly map its persisted selection to the scaler's enum.
This implementation does not require changing the manifest schema.

### Implementation sequence followed

1. Add the public declarations, working-pixel traits, parameter validation,
   the unified dispatcher and required-halo query, zero-level cropping, and
   safe numeric conversions.
2. Implement direct upscaling and a shared 2x reduction path for built-in
   filters and templated reducers. Keep validity and attribution handling
   explicit, precompute reusable filter coefficients, and retain required
   intermediate halos. Normalize valid two-dimensional contributions rather
   than independently normalizing rows and columns around NoData holes.
3. Add the supplied reduction functors and focused Catch2 coverage in
   `unittests/terrainlib/raster_algorithm.cpp`. Register the new header and
   tests in the existing terrainlib CMake targets.
4. Build the existing Debug terrainlib test target, run the focused raster
   cases and the terrainlib regression suite, and inspect the numerical
   fixtures before fixing tolerances. Use the Qt C++ skill for linting and
   `clang-format-21` on the new C++ portions. Check whitespace and Git-attribute
   line endings before delivery.

Use `InvalidInput` for invalid geometry, zoom-level counts, enum selections,
insufficient halos, and nonfinite custom results destined for integers. Use `Unsupported`
for a value mapping incompatible with the pixel format, and
`ResourceExhausted` for unrepresentable output/allocation sizes. Validate
counts before constructing raster buffers and use `Error::fail` and
`Error::propagate` according to the project style.

Tests should compare against independent reference calculations and exercise
the behavioral cases below. Include scalar and vector type-mapping checks,
integer boundary conversions, an actual lambda and stateful functor,
different halo widths, and multi-stage tile/metatile equivalence. Numeric
fixtures should measure constant preservation and high-frequency attenuation
with tolerances appropriate to the selected working type.

## Verification to cover

The implementation plan should include:

- Constant preservation, impulse/frequency response for Lanczos reduction,
  and area-tile/metatile equivalence with sufficient halos.
- Invariance under different input chunk partitions and absence of internal
  edge clamping.
- Local reduction attribution voting, deterministic ties, all-invalid local
  blocks with valid outer support, and unstable weighted normalization.
- Nearest-neighbour attribution for every upscaling mode, exact data copies
  for nearest-neighbour interpolation, and direct multi-factor upscaling.
- Bilinear NoData exclusion and nearest-NoData payload copying even when
  other bilinear neighbours contain valid data.
- Excluding NoData from custom functor input and bypassing empty reductions.
- Row-major functor samples, working component types, component-wise
  min/max/median, even-count median, and supplied-functor NaN propagation.
- Repeated reduction semantics for data, attribution, rounding, and sRGB.
- Linear-light RGB filtering and independent linear alpha filtering.
- Integer limits, rounding, floating-point overshoot, and attributed
  nonfinite values under the final numerical contract.
- Exact sufficient halo, insufficient halo errors, and final halo cropping.
- Rectangular inputs, factor-one cropping, divisibility checks, and invalid
  zoom-level counts, dimensions, and overflow under the final API.
- Unified dispatch and halo queries, exponent-to-factor conversion, and
  rejection of extreme counts before signed negation or shifting can overflow.

Vertex shared-edge tests, physical-source selection, world wrapping, and
input tile-scheme normalization remain broader generator/extractor tests.

### Verification results — 2026-09-19

- Existing GCC Debug build: `unittests_terrainlib` builds successfully.
- Focused `[raster-algorithm]` suite: 34 cases, 904 assertions, all passing.
- Terrainlib regression suite, excluding the existing
  `mesh::clip_on_bounds benchmark`: 519 cases, 25698 assertions, all passing.
- The independent double-precision Lanczos reference agrees with the float
  implementation within `3e-6` for the validity-aware fixture. Constant-7
  Lanczos fixtures remain within `3e-5`; the high-frequency fixture at
  0.4 cycles per input pixel has output magnitude below 0.03 for every radius.
- Separately supplied reduction stages and tile/metatile results agree
  exactly in the exercised fixtures. Endpoint, nonfinite, vector-precision,
  sRGB, and invalid-input cases pass.
- Qt C++ deterministic lint reports no findings; all six review missions
  report no confirmed defects. An optional future profiling target is the
  repeated sRGB decoding of overlapping Lanczos samples.
- New C++ is formatted with `clang-format-21`; whitespace and Git-attribute
  line-ending checks pass.
