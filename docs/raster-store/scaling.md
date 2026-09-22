# Raster scaling

Status: the single-raster, view-based refactor was implemented on 2026-09-22,
including paired raster-store wrappers, conversion tuples, rectangular strided
windows, and separable box/Lanczos filtering. Halo extraction and its storage
metadata remain separate work.

## Scope and ownership

Generic scaling and reduction operate on one raster, without interpreting
attribution or treating any sample as NoData. Every supplied pixel participates
according to the selected operation. Callers must supply usable payloads,
including where their separate attribution raster contains zero. For numerical
paths, callers must ensure finite decoded values, finite intermediate and
reducer results, and values accepted by their encoder. These are preconditions,
not runtime validity scans or per-pixel errors. Nearest-neighbour sampling and
zero-level cropping bypass conversion and preserve stored pixels exactly,
including nonfinite floating-point values.

Only power-of-two scale factors are supported. Input and output use area-pixel
placement; vertex-pixel generation and area/vertex conversion are outside this
facility. World wrapping, physical-source selection, ancestor fallback, and
preparation of missing coverage belong to callers or the halo extractor.

Implementations live under `src/terrainlib/raster/algorithm/*.h` and are exposed
through the `raster/algorithm.h` umbrella in namespace `raster::algorithm`.
Paired convenience wrappers live in `raster_store/scaler.h`, in namespace
`raster_store::scaler`. They compose generic operations for data and
attribution; they do not implement filtering, interpolation, or pixel loops.

See [Raster views and view algorithms](../raster-view-algorithms.md) for the
view model and [Sampling and pyramid generation](sampling-and-generation.md)
for broader generator requirements.

## Interface and geometry

Accept a raster, ordinary view, or clamped view as a source. Normalize it with
`raster::make_view`, as the existing algorithms do, and expose read-only input
to the implementation. Public operations are synchronous and may accept a
temporary raster for the duration of the call. No borrowed view escapes.

Each operation has an explicit-destination overload returning `Expected<void>`
and an allocating overload returning `Expected<radix::Raster<T>>`. An explicit
destination may be a writable raster or ordinary view, must have exactly the
computed output dimensions, and must use the same stored pixel type as the
source. Read-only and clamped destinations are rejected at compile time.
Normalize source and destination independently; do not multiply overloads for
every raster/view combination. Both overloads share a view-based implementation.

Inputs are rectangular, with a symmetric `halo_width` on all four sides. Both
axes scale by the same factor. The output contains only the scaled interior;
all input halo pixels are excluded from its extent. Reject empty inputs and
empty interiors after removing the halo, even though the view model permits
empty views. Destination overloads avoid allocating the final output, but
separable passes and repeated reductions still allocate intermediate rasters.

Express scaling amounts as `n_zoom_levels`, the exponent of two. Public
`scale` and `required_halo` use a signed count: positive means upscaling,
negative means downscaling, and zero means cropping only. For example, `+2`
upscales by four and `-2` downscales by four. Directional helpers and custom
`reduce` use unsigned exponent counts. Zero levels crop without invoking a
reducer. Keep `upscale`, `downscale`, and directional halo queries in `detail`.

For reduction, both interior dimensions must be divisible by the factor.
Validate geometry, halo bounds, algorithm enum selections, zoom-level counts,
and dimension/allocation overflow. Check pixel and conversion-callable type
compatibility at compile time. Validate exponents before shifting, and obtain
a negative signed count's magnitude without signed overflow.

With interior pixel centres indexed by integers, direct upscaling by factor
`F` evaluates output pixel `j` at `(j + 0.5) / F - 0.5` in input coordinates.
A reduction by two evaluates pixel `j` at `2j + 0.5`. Apply these positions
independently on each axis. Halo offsets affect addressing, not sampling phase.

### Generic API

The algorithm enums remain `Interpolation { NearestNeighbour, Bilinear }` and
`Filter { Box, Lanczos2, Lanczos3, Lanczos4 }`. Remove the scaler's `ValueMapping`
enum and the fixed source-to-working-type policy from generic `scale` and
`reduce`. Instead accept a tuple `(value_decoder, value_encoder)`; the decoder's
return type determines the working pixel type. The encoder returns exactly
the original stored pixel type, so neither operation changes the raster's type.

Expose the following call shapes in `raster::algorithm`. `conversion` is the
tuple. Convenience overloads omit it and supply the operation's default; all
forms delegate to the same processing implementation. Keep destinations last
and constrain conversion tuples and destinations so the forms are unambiguous.

```cpp
scale(source, halo_width, n_zoom_levels, interpolation, filter);
scale(source, halo_width, n_zoom_levels, interpolation, filter, destination);
scale(source, halo_width, n_zoom_levels, interpolation, filter, conversion);
scale(source, halo_width, n_zoom_levels, interpolation, filter, conversion,
    destination);

reduce(source, halo_width, n_zoom_levels, reducer);
reduce(source, halo_width, n_zoom_levels, reducer, destination);
reduce(source, halo_width, n_zoom_levels, conversion, reducer);
reduce(source, halo_width, n_zoom_levels, conversion, reducer, destination);
```

Allocating forms return `Expected<radix::Raster<T>>`, where `T` is the source
pixel type. Explicit-destination forms return `Expected<void>`. The default
tuple is the supplied linear numeric conversion for `scale` and identity for
`reduce`, as specified below. A custom reducer is invoked through `const Reducer&`.
The halo queries are independent of conversion:

```cpp
namespace raster::algorithm {

Expected<unsigned> required_halo(
    int n_zoom_levels, Interpolation interpolation, Filter filter);

namespace detail {
Expected<unsigned> required_upscaling_halo(
    unsigned n_zoom_levels, Interpolation interpolation);
Expected<unsigned> required_downscaling_halo(
    unsigned n_zoom_levels, Filter filter);
} // namespace detail
} // namespace raster::algorithm
```

`scale` and `required_halo` receive both algorithm selections and use the one
corresponding to the requested direction. The filter enum represents the three
Lanczos support radii without a separate parameter for box filtering.

## Algorithms and reducers

Upscaling offers nearest-neighbour and bilinear interpolation. Upscale directly
from the input to the final resolution rather than repeating interpolation by
two. Nearest-neighbour sampling copies the selected stored pixel exactly, without
a decoder/encoder round trip; neither callable is invoked. Bilinear interpolation
uses all four neighbours; there is no attribution-based bypass or exclusion.

Downscaling offers box filtering and Lanczos with lobe count/support radius
2, 3, or 4. Implement reduction by two and repeat it for larger reductions.
Box filtering averages the complete local 2x2 block. Lanczos support is widened
for the reduction ratio to low-pass before decimation.

For Lanczos radius `a`, the one-dimensional kernel is proportional to
`sinc(t / 2) * sinc(t / (2a))` for `abs(t) < 2a` and zero otherwise, where
`sinc(x) = sin(pi*x) / (pi*x)` and `sinc(0) = 1`. Two-dimensional weights are
products of the axis weights. Normalize over the complete kernel support,
independently of payloads or attribution. Zero-weight samples do not contribute
payload arithmetic. The former masked-weight cancellation and empty-block
payload fallbacks do not apply: there is no validity mask changing the support.

Custom reduction also operates on a complete 2x2 block at each step. Accept
lambdas and functors through templates, without type erasure or a required
function pointer. Let `W` be the value type returned by the decoder. The
reducer receives `std::span<const W>` containing exactly four decoded values
in row-major order and returns exactly one `W` by value. Encode that result
into the original stored pixel type after each step. With the default identity
tuple, `W` is the stored type, including integer types. No samples are omitted
and no blocks bypass the callable during a nonzero reduction.

Invoke reducers through `const Reducer&`. Immutable captured configuration is
allowed, but callbacks must not change state between calls or depend on
invocation order. This matches the existing view-algorithm contract and
supersedes the scaler's earlier support for stateful reducers. Const invocation
alone does not prove the absence of external side effects.

Provide these stateless functors with templated const call operators:

- `Min` and `Max`: operate component-wise for scalar and GLM-vector pixels.
- `Median`: operates component-wise and averages the two middle values of the
  four samples. For integer components, calculate without overflow and round
  halfway cases away from zero; this rule belongs to the functor and also
  applies with identity conversion. `Min`, `Max`, and `Mode` select input values.
- `Mode`: operates on scalar-like values. Select the sample with the greatest
  frequency under ordinary equality, breaking ties by the first occurrence in
  row-major order. Each sample is a candidate with one occurrence of its own;
  equal other samples add to its frequency. Zero is an ordinary value. There
  is no NaN special case: NaNs do not compare equal to other samples, including
  other NaNs, and therefore do not form a repeated group. A first NaN can win
  an all-distinct tie; a repeated ordinary value beats it.

`Mode` is a categorical frequency operation, not a numeric median. Its ordinary
equality semantics above also describe direct functor calls with NaNs; they do
not waive the finite-value precondition for numerical `scale`/`reduce` paths.
The scalar-like value support belongs to the functor itself. Generic `reduce`
does not inspect which functor was supplied or add a `Mode`-specific vector
restriction; no explicit vector-exclusion checks or tests are required.

## Paired raster-store wrappers

The paired wrappers accept matching rectangular data and `uint16_t` attribution
sources, each independently supplied as a raster, ordinary view, or clamped
view. Both use the same logical dimensions, halo width, and zoom-level count.
The wrappers do not fill coverage gaps or repair payloads. A caller choosing
clamped inputs is responsible for their source coverage and boundary policy.

Data follows the requested generic operation. Attribution follows:

- Upscaling: generic nearest-neighbour scaling, bypassing conversion.
- Downscaling: generic custom reduction with scalar `Mode` and identity conversion,
  repeated by two, independently of the data filter's larger support.
- Zero levels: crop both sources exactly.

Every attribution value participates, including zero. For example,
`[0, 0, 0, 7]` reduces to zero. Attribution never changes the data samples,
weights, interpolation path, or result. Attribution reduction operates directly
on `uint16_t` IDs without floating-point conversion. Keep one representative ID;
complete provenance and original-source weight totals are not retained.

Provide these call shapes under `raster_store::scaler`. Paired `scale` accepts
an explicit `raster_store::pixel::Mapping` for data interpretation. Paired
`reduce` accepts a decoder/encoder tuple of lambdas or functors, defaulting to
identity conversion, and has no mapping enum parameter:

```cpp
scale(data, attribution, halo_width, n_zoom_levels,
    interpolation, filter, value_mapping);
scale(data, attribution, halo_width, n_zoom_levels,
    interpolation, filter, value_mapping, data_destination, attribution_destination);

reduce(data, attribution, halo_width, n_zoom_levels, reducer);
reduce(data, attribution, halo_width, n_zoom_levels, reducer,
    data_destination, attribution_destination);
reduce(data, attribution, halo_width, n_zoom_levels, conversion, reducer);
reduce(data, attribution, halo_width, n_zoom_levels, conversion, reducer,
    data_destination, attribution_destination);
```

Allocating wrappers return
`Expected<std::pair<radix::Raster<T>, radix::Raster<std::uint16_t>>>`, with data
first. Explicit-destination wrappers return `Expected<void>`. The `scale` wrapper maps
`pixel::Mapping` to a supplied data conversion tuple and invokes the generic
algorithm. Custom scaling tuples remain available through the generic
single-raster API. The `reduce` wrapper forwards its data conversion tuple and
reducer to generic `reduce`. Attribution reduction always uses `Mode` with
identity conversion, independently of the data tuple.
Reuse generic validation and processing helpers rather than duplicate kernels.

`Mapping::Linear` selects the supplied linear conversion for `scale`;
`Mapping::SRGBA` selects sRGB conversion for `scale` and requires RGB/RGBA
unsigned 8-bit pixels. Validate mapping enumerators and compatibility before
writing either output. Callers may explicitly supply the sRGB conversion tuple
to paired `reduce`, just as they can supply custom lambdas.

### Shared pixel description

The former `raster_store/pixel_type.h` is now `raster_store/pixel.h`, with namespace
`raster_store::pixel`. `pixel::identifier<T>()` is public; `Format`
and its specializations live in `pixel::detail::Format`. Existing identifier
strings and pixel-layout requirements are preserved. Storage and codec use
the new header and names.

Define `pixel::Mapping { Linear, SRGBA }` in that same header. It replaces the
former scaler `ValueMapping` name while retaining runtime mapping selection at
the paired `scale` boundary. The planned snapshot metadata and creation options
use this same type for `value_mapping`; do not define a separate enum in the
manifest's versioned namespace. Generic raster algorithms accept conversion
tuples and do not depend on the raster-store pixel header or mapping enum.
Adding the planned metadata field remains part of the separate halo/metadata
implementation, not an implicit schema change in this refactor.

## Conversion tuples and numeric behavior

A conversion is a `std::tuple` of two lambdas or functors, in decoder/encoder
order. Invoke both through const references, without type erasure. Like the
reducers, they may capture immutable configuration but must not change state
or depend on invocation order.

For source pixel type `T`, the decoder accepts `const T&` and returns a plain
working pixel value `W`. The encoder accepts `const W&` and returns exactly
`T` by value. Neither callable returns `Expected`, a reference, or a different
output pixel type. Infer `W` from the decoder; generic algorithms do not choose
it through a `WorkingPixel<T>` mapping. The reducer also returns exactly `W`.

`scale` supports floating-point working components, including floating-point
GLM vectors. Integer source and destination rasters remain supported through
their conversion tuples. `reduce` supports integer and floating-point working
components, including GLM vectors. Operations
work on the decoder's result; callers choose any layer-specific interpretation
or postprocessing through their callables.

For numerical paths, the caller must ensure that decoded values, intermediate
arithmetic, and reducer results remain finite, and that values passed to the
encoder are within its supported domain. Finite decoder output alone is not
sufficient: arithmetic can overflow or a custom reducer can produce infinity.
No runtime validity scanning, per-pixel conversion errors, or special recovery
is promised for precondition violations. This supersedes the former promise
to preserve NaN/Inf through numerical filtering or report nonfinite-to-integer
conversion errors. Nearest-neighbour sampling and zero-level cropping instead
copy stored pixels exactly, never invoke decoder/encoder callables, and have
no finite-value requirement.

### Supplied conversion tuples

Provide reusable tuple factories alongside the algorithms:

- `identity_conversion()`: decoder and encoder return their input value
  unchanged. This is the default for `reduce`, preserving integer working
  values exactly. It can also be used with `scale` when the resulting working
  components are floating point.
- `linear_conversion<T>()`: default for `scale`. The decoder casts stored
  components to the former working precision shown below, without changing
  the interpretation or scale of linear values. Its encoder converts back to
  `T`; for integer components it rounds halfway cases away from zero and
  clamps finite results to the representable range before conversion. For
  floating-point storage it preserves representable finite overshoot. These
  conversions belong to the supplied tuple, not generic scaling/reduction.
- `srgb_conversion<T>()`: supports RGB/RGBA unsigned 8-bit stored pixels. Decode
  RGB through the sRGB transfer function into linear-light float components
  and encode back afterwards. This is sRGB, not an arbitrary gamma exponent.
  Decode alpha, when present, linearly into `[0, 1]` and encode it independently;
  alpha neither weights RGB nor excludes samples. Encoding rounds and clamps
  to the stored channel range. Use this tuple with either operation explicitly.

The default linear tuple selects its decoder component type as follows,
preserving GLM vector length:

| Stored component type | Linear decoder component type |
|---|---|
| 32-bit `float` | `float` |
| Signed or unsigned integers up to 16 bits | `float` |
| `double` | `double` |
| Signed or unsigned 32-bit integers | `double` |
| Signed or unsigned 64-bit integers | `long double` |

For example, `linear_conversion<glm::vec<4, uint16_t>>()` decodes to four
`float` components, whereas identity conversion keeps four `uint16_t`
components for reduction. A caller may supply a different decoder precision.
Numerical results are subject to the chosen working type; exact weighted
64-bit integer arithmetic is not promised. Identity reduction avoids an
unnecessary conversion for integer min/max/mode, and integer median uses its
own overflow-safe rounding rule.

Every nonzero reduction step encodes its result into the original stored
pixel type. Subsequent steps decode that stored result again, including any
rounding or sRGB conversion. Reduction by four therefore has the same stage
semantics as two explicit reductions by two with sufficient halos and the same
tuple. Repeated median and mode need not equal a direct reduction over the
original larger block. Repeated rounding and encoding also prevent a general
promise of agreement with a single larger numeric filter.

The planned snapshot metadata uses `raster_store::pixel::Mapping` to describe
linear or sRGB interpretation. The future halo extractor passes that value to
paired `scale`, which selects the corresponding tuple. Runtime mapping
selection stays at this store boundary; generic `scale` and `reduce` receive
only conversion callables. This refactor does not change the manifest schema.

## Halo and clamped-view contract

Require an explicit `halo_width` for every source representation. It identifies
the interior within the logical input rectangle, whether halo pixels occupy
physical storage or are supplied by a clamped view. Clamped views still have
finite logical bounds; they do not grant arbitrary out-of-bounds access.

For example, bilinearly upscaling a 4x4 interior requires a logical 6x6 source
with a one-pixel halo. A caller choosing edge replication can construct:

```cpp
auto padded = raster::make_clamped_view(source, {-1, -1}, {6, 6});
// After checking padded, pass *padded with halo_width = 1.
// At n_zoom_levels = +1 the output is 8x8, with no output halo.
```

An ordinary 6x6 source view can instead supply physical neighbours. The scaler
does not construct clamped views implicitly. It rejects an insufficient logical
halo for either source kind. Internal tile edges must not change filter support.

Compute the required halo for the complete operation, including all reduction
steps. Retain enough intermediate halo for later steps and crop only the final
result. Minimum widths, in input pixels on each side including corners, are:

| Operation | Required halo |
|---|---|
| Zero levels (crop only) | 0 |
| Nearest-neighbour upscaling | 0 |
| Bilinear upscaling, positive levels | 1 |
| Box or custom 2x2 reduction, including repeated steps | 0 |
| Lanczos reduction by total factor `F`, radius `a` | `(2a - 1) * (F - 1)` |

For one Lanczos step, the first output centre is `0.5` and input indices range
from `1 - 2a` through `2a`, giving a halo of `2a - 1`. Retaining output halo
`h` needs input halo `2h + (2a - 1)`; repeated application yields the formula.
Lanczos-3 therefore requires 5 input halo pixels for reduction by two and 15
for reduction by four. Halo queries validate parameters and arithmetic overflow.

There is no `required_reduction_halo`: custom block reduction always needs zero
halo, though its supplied halo is still cropped and its geometry validated.
Paired wrappers use the data operation's requirement, which also suffices for
nearest-neighbour or mode attribution.

## Overlap and failure behavior

Reject input/output storage overlap for nonzero scaling or reduction, using the
existing view helpers and physical footprints for clamped views. Zero-level
cropping follows `copy`: permit an identical interior/destination mapping,
reject partial overlap. Copying into an overlapping displaced region is not
promised to behave like a snapshot.

Paired wrappers validate both output geometries and input/output relationships
before writing either output. Check cross-overlap as well as each corresponding
pair, including when data is also `uint16_t`; reject overlapping output rasters.
Inputs may overlap each other when they do not violate these output constraints.

Complete argument validation before writing output or invoking reducers or
conversion callables. Invalid arguments leave destinations unchanged. A later
failure, such as an allocation failure while producing paired outputs, has no
rollback guarantee; paired outputs are not transactional. An allocating overload
returns an error without exposing its partial raster. Callback exceptions
receive no special catching, translation, or rollback guarantee, matching the
view algorithms. Numerical precondition violations are not returned as errors.

Use `InvalidInput` for invalid geometry, parameters, overlap, and insufficient
halos, and `ResourceExhausted` for unrepresentable allocation sizes or allocation
failure. Pixel, decoder, encoder, and reducer type incompatibilities are
compile-time errors. Follow the project's `Error::fail`/`Error::propagate` style.

## Relationship to halo extraction

The planned [halo extractor](tiles-with-halo.md) calls the paired store wrappers
with box downscaling; it has no downscaling-algorithm parameter. Upscaling
interpolation remains an extraction parameter. Algorithm details are owned here.

The extractor resolves physical coverage, source windows, world wrapping, and
required halos. It preserves supplied and resampled zero-attribution payloads.
Only missing physical coverage, including vertical world limits, uses a clamped
view of the central interior to replicate data with synthesized attribution zero.
There is no attribution-based replacement after scaling. See the
[attribution ADR](../adr/0004-representative-attribution-for-halo-samples.md) for
the decision to retain one representative ID rather than complete provenance.

## Implementation and verification plan

1. Move the scaler into headers under `raster/algorithm/`, retaining the umbrella
   includes. Reuse view normalization, read-only access, allocation, and overlap
   helpers, and `copy` for zero-level cropping. Extend
   `window_transform` to even kernels and a stride parameter to share
   downscaling traversal, as agreed below. Direct upscaling retains its
   coordinate mapping over the shared view interface.
2. Remove coupled validity/attribution paths, implement both generic overload
   patterns, retain intermediate halos, and add const-callable four-sample
   reduction and scalar `Mode`. Replace `ValueMapping` and the generic fixed
   working-type policy with decoder/encoder tuples; provide linear numeric,
   sRGB, and identity factories with the agreed operation defaults.
3. Add thin paired wrappers with `pixel::Mapping` selection for `scale`,
   identity-default conversion tuples for `reduce`, and validation of
   both destinations before writes. Consolidate the pixel identifier and
   mapping type in `raster_store/pixel.h`. Update CMake registrations and
   existing tests for the revised contracts.
4. Build and run focused and terrainlib regression tests. Verify GCC 16 Release
   compilation with warnings as errors: the existing median reducer produced
   an array-bounds diagnostic in `std::sort`. An explicit `std::min(size, 4)`
   bound did not eliminate it. Report whether the refactor incidentally fixes
   the warning or it still occurs. Do not take additional steps specifically
   to fix or suppress it; any such fix requires the user's separate decision.
   Apply Qt C++ lint and format new C++ with `clang-format-21`.

### Agreed window-transform reuse

The extension accepts `glm::uvec2` kernel sizes and strides, with positive
components and even or odd rectangular kernels. There are no scalar/square
shorthand overloads. Omitting the stride selects `{1, 1}`. Per-axis output size is
`1 + (input_size - kernel_size) / stride`, using integer division after checking
that the kernel fits. Windows start at `output_coordinate * stride`,
component-wise; incomplete trailing strides are not sampled. The scaler retains its stricter divisibility
checks before invoking this primitive.

For a 2x2 kernel and stride two, even input dimensions produce output dimensions
exactly half as large. For example, 8x6 becomes 4x3. Larger kernels also consume
border pixels: an 8x8 input with a 4x4 kernel and stride two produces 3x3 output.
Both the allocated raster and explicit destination use these same dimensions;
the caller supplies a destination of that size, and the operation does not
resize its view.

The extension expresses every factor-two reduction step:

| Operation | Kernel size | Stride |
|---|---|---|
| Custom reduction, including attribution mode | `{2, 2}` | `{2, 2}` |
| Box, horizontal then vertical | `{2, 1}`, then `{1, 2}` | `{2, 1}`, then `{1, 2}` |
| Lanczos radius `a`, horizontal then vertical | `{4*a, 1}`, then `{1, 4*a}` | `{2, 1}`, then `{1, 2}` |

Box and Lanczos use separable passes. The horizontal pass decodes stored pixels
and produces a raster of working pixels; the vertical pass filters those values
and encodes the final output. Encode only after both passes, never between them.
The next zoom level decodes that stored output again. Rectangular windows also
support caller-supplied separable filters such as Gaussian; no Gaussian scaler
mode is introduced.

Custom reduction callbacks decode the 2x2 window, invoke the reducer, and encode
the output pixel. Separable filter callbacks split that work across two passes. The shared operation owns spatial traversal and destination assignment,
including ordinary/clamped input support. The scaler still owns repeated levels,
intermediate rasters, retained halos, and cropping the stage input to the exact
required window. Validate the public source/destination overlap before narrowing
that input; keep paired cross-overlap validation outside the primitive.

Direct upscaling still requires separate coordinate mapping. The code
savings are modest: the reusable traversal replaces output loops and coordinate
setup, but not filter arithmetic or the level driver. Verification covers rectangular/even kernels, per-axis positive strides,
sample origins, dimensions, ordinary/clamped equivalence, and encoding only
after both filter passes.

### Required coverage

- Raster, ordinary subview with row stride, and clamped-view sources; allocating
  and explicit-destination routes; read-only input and writable output constraints.
- Output equivalence across routes and view forms, unchanged pixels outside
  destination subviews, empty-input/interior rejection, dimensions, divisibility,
  enums, overflow, extreme zoom counts, and insufficient logical halos.
- Clamped logical halos versus equivalent materialized padding, no implicit edge
  clamping, and tile/metatile equivalence with sufficient physical halos.
- Overlap rejection, clamped physical-footprint overlap, zero-level identical
  copies, paired cross-overlap, and argument errors before any output writes.
- Independent numeric references, constant preservation, Lanczos impulse and
  frequency response, direct multi-factor upscaling, and repeated-step equality.
- Exactly four decoded reducer samples in row-major order, const-callable lambdas
  and configured functors, decoder-selected working types, and the distinct
  scale/reduce defaults. Check integer identity reduction, including 64-bit
  min/max/mode and overflow-safe, halfway-away-from-zero integer median.
- Supplied and custom tuples, same stored input/output type, and compile-time
  rejection of incompatible types and `Expected`-returning converters. Verify
  that NN/crop bypass both callables and preserve nonfinite payloads exactly.
- Scalar mode, first-occurrence ties and zeros. Direct functor tests may check
  distinct NaNs and repeated ordinary values beating NaNs; numerical pipeline
  tests respect the finite-value
  precondition.
- Paired output matching two independent generic calls; NN attribution on every
  upscaling path, mode at every downscaling step, and all-zero attribution having
  no effect on data arithmetic, including when Lanczos uses outer support.
- Linear-light RGB, independent alpha, integer limits, finite rounding/clamping,
  representable floating overshoot, and repeated sRGB stage semantics.

Vertex shared edges, physical-source selection, world wrapping, and tile-scheme
normalization remain broader generator/extractor tests.

### Refactor verification — 2026-09-22

- Focused scaler/view tests: 61 cases, 1,347 assertions passed.
- Terrainlib: 546 cases, 26,141 assertions; RF builder: 47 cases, 53,812 assertions;
  DAG builder: 76 cases, 479 assertions.
- Eleven affected public headers compile independently with warnings as errors.
- GCC 16 Release compilation of the scaler test translation unit at `-O3`
  with warnings as errors passed. The previous median `std::sort` array-bounds
  diagnostic did not recur; no warning-specific fix or suppression was added.
- New and changed C++ is formatted with `clang-format-21`. Qt lint reports only
  an existing warning on unchanged `storage.h` code about a default-constructed
  `std::optional`.

### Historical verification of the previous implementation

The coupled scaler passed 34 cases/904 assertions and the terrainlib suite
passed 519 cases/25,698 assertions, excluding the existing mesh clipping
benchmark, on 2026-09-19. Its independent Lanczos fixtures, type conversions,
Qt review, formatting, and line-ending checks passed. CI for commit `2a75fb4`
subsequently passed GCC 14, Clang unity, ASan, and TSan; GCC 16 failed on the
median sort diagnostic. These results are a baseline, not verification of the
refactor specified above. Halo extraction remains unimplemented.
