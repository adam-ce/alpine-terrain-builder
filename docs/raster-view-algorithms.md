# Raster views and view algorithms

Status: design agreed on 2026-09-21 and extended and implemented on 2026-09-22,
including overloads that allocate and return rasters.
The scaling refactor also implements rectangular kernels and per-axis strides.

## Scope

Add rectangular views of `radix::Raster<T>` and operations over caller-supplied
input and output views or whole rasters:

- `copy`: copy pixels from one view into another.
- `transform`: invoke a callable on each input pixel to produce an output pixel.
- `zip_transform`: invoke a callable with one pixel from each of two or more
  input views to produce an output pixel. Input pixel types may differ.
- `window_transform`: invoke a callable with a rectangular, kernel-sized input
  view to produce one output pixel.

Inputs may also be read-only clamped views that replicate source-edge pixels.
Destinations must be writable ordinary views or rasters.

Pointwise operations require matching view widths and heights. Source and
destination pixel types may differ for transforms. The windowed operation
uses positive `glm::uvec2` kernel sizes and strides with no border treatment.
Each output dimension is `1 + (input_size - kernel_size) / stride`, using
integer division independently on each axis. Omitted stride means `{1, 1}`.
There are no scalar kernel-size or scalar-stride overloads.

## Agreed view model

Ordinary views are statically typed as `raster::View<T>` or
`raster::View<const T>`, defined in `terrainlib/raster/View.h`. Clamped views
are defined in `terrainlib/raster/ClampedView.h`. Operations belong to
`raster::algorithm`, with implementations in `terrainlib/raster/algorithm/copy.h`,
`transform.h`, `zip_transform.h`, and `window_transform.h`. The existing
`terrainlib/raster/algorithm.h` includes those headers.
There is no type erasure or runtime datatype field. A pixel type denotes the
complete stored pixel, including all components of a vector pixel.

Support rectangular regions of `radix::Raster<T>` and subviews of those
regions. Pixels within a row remain contiguous, and a subview inherits its
backing raster's row stride. Offset and row stride are measured in pixels.
External buffers, flipped rows, and every-nth-pixel sampling are outside scope.

Views borrow storage without extending its lifetime. The caller must keep the
backing storage alive and stable; destroying or replacing it invalidates its
views. Algorithm inputs expose read-only pixels. An ordinary descriptor carries
the backing storage pointer, pixel offset, row stride, width, and height; its
template argument supplies the pixel type. Representation details remain an
implementation choice within this model.

## Construction and dimensions

Use the free factory `raster::make_view` for raster and ordinary view sources;
there is no `subview` member method. Whole-raster or whole-view construction
returns a view directly. Construction with an origin and size returns
`Expected<View<T>>`, with pixel constness preserved. Coordinates are relative
to the supplied raster or view. A requested region must fit completely;
construction does not clip it.

Mutable views can convert to read-only views, but not the reverse.
Both view factories reject temporary rasters. A temporary view is only a
borrowed descriptor, and its destruction does not destroy the backing raster.

Zero-width or zero-height views are valid. Pointwise operations with matching
empty dimensions succeed without invoking the callable. Window transforms
require positive kernel dimensions that fit the corresponding input dimensions
and positive strides on both axes. A `{1, 1}` kernel is valid; an oversized
kernel is an error.

## Clamped views

Provide a separate, read-only `raster::ClampedView<T>` type, constructed by
`make_clamped_view(source, origin, size)`. The source is a raster or ordinary
view. The signed origin and logical dimensions may describe a region extending
beyond the source. Reads clamp each source coordinate to the supplied source
region's edges, repeating edge and corner pixels without allocating padding.
An ordinary source view defines its own clamping bounds, rather than those of
its entire backing raster. A nonempty clamped view requires a nonempty source.

Clamped views expose the same `pixel`, `width`, `height`, and `size` interface
as ordinary views, but pixel access is always read-only. Callers must still
address within the clamped view's logical dimensions. Clamping applies to the
mapping into the source; it does not permit arbitrary out-of-view coordinates.
Ordinary views retain their direct addressing without a runtime clamping mode.

`make_view(clamped_view)` preserves the clamped view type and mapping. Creating
a region of a clamped view through `make_view` requires that region to fit its
logical dimensions and preserves the original clamping bounds. In particular,
a window-transform kernel must not clamp independently at its own edges.

The window transform does not add border treatment.
For example, extending an input by three pixels on each side allows a 7x7
window transform at stride `{1, 1}` to produce output with the original input's dimensions.

### Agreed window-transform extension

The [scaling refactor](raster-store/scaling.md#agreed-window-transform-reuse)
uses rectangular kernels for separable passes. Both `kernel_size` and `stride`
are `glm::uvec2`; explicit-stride forms place the stride after the kernel size:

```cpp
window_transform(source, kernel_size, stride, function, destination);
auto filtered = window_transform(source, kernel_size, stride, function);
```

For each input dimension `n`, kernel dimension `k`, and stride `s`, the output
dimension is `1 + (n - k) / s`, using integer division. Validate that the kernel
fits and that the stride is positive before calculating dimensions. Windows
start at `(x * stride.x, y * stride.y)`; trailing pixels that cannot form a complete window
are omitted. Both overload forms use the same rule, and supplied destinations
must already have the resulting dimensions. No destination view is resized.
A 2x2 kernel with stride two maps an 8x6 input to a 4x3 output. Larger kernels
consume more border pixels; stride two alone does not guarantee half-size output.

Box and Lanczos downscaling use horizontal and vertical passes with an intermediate
raster of working pixels. Encoding happens only after both passes. Overlap
and clamped-view semantics are unchanged.

## Access and call syntax

Expose `pixel({x, y})`, `width()`, `height()`, and `size()` in the style of
`radix::Raster`. Pixel coordinates are relative to the view. Individual pixel
access has an in-bounds precondition checked by debug assertions; it does not
return `Expected`.

For ordinary views, `pixel(...) const` returns `T&`: constness of the descriptor
does not determine pixel writability. `View<const T>` exposes read-only pixels, whereas even a
const `View<T>` descriptor can expose writable pixels. Internal algorithm view
parameters are const references; public wrappers accept rasters or views as
described below. The output pixel type must be non-const. The algorithms do
not change the supplied descriptors.

Use these call shapes:

```cpp
copy(source, destination);
transform(source, function, destination);
zip_transform(a, b, function, destination);
zip_transform(a, b, c, function, destination);
zip_transform(std::tuple{a, b, c, d}, function, destination);
window_transform(source, kernel_size, function, destination);
```

Each call also has an overload omitting the destination, returning
`Expected<radix::Raster<T>>`. Copy preserves the source pixel type; transforms
infer `T` from the callable's exact return type. Pointwise output dimensions
match the inputs; window output dimensions follow the kernel-and-stride rule.
Validate input dimensions and kernel parameters before allocating. Invalid
geometry returns an error; unrepresentable storage sizes or allocation failure
return `ResourceExhausted`. Callable exceptions retain their existing contract.

```cpp
auto copied = copy(source);
auto transformed = transform(source, function);
auto combined = zip_transform(a, b, function);
auto filtered = window_transform(source, kernel_size, function);
```

Zip exposes direct overloads for exactly two and three inputs. Four or
more inputs use the tuple overload, accepting the tuple by const
reference. Input order is the callable's pixel-argument order. All overloads
share validation and processing logic internally.

When the tuple contains views, constructing it copies only view descriptors,
not raster pixels. For raster inputs, use a tuple of references such as
`std::tie(a, b, c, d)`, or explicitly create views before constructing the
tuple. `std::tuple{...}` containing raster objects by value would copy their
pixel buffers at the call site.
A bare braced list cannot deduce the heterogeneous input types for this
function-template interface; `std::tuple{...}` provides that deduction.
This deduction behavior was verified with a standalone C++23 compiler probe
and follows the language's
[initializer-list argument deduction rules](https://eel.is/c++draft/temp.deduct.call).

## Raster and view argument wrappers

Each public operation accepts a raster, ordinary view, or clamped view
independently for each input. The destination accepts a writable raster or
ordinary view. Normalize these arguments through `make_view`
and delegate to one internal implementation operating on views. A raster
becomes a view covering the whole raster; an existing view becomes an
equivalent view. Normalization does not copy raster buffers. Overloads with
an explicit destination do not allocate output storage; destination-free
overloads allocate a raster and use the same processing implementation.

Internal implementations template their input-view types and read through
`pixel`, so ordinary and clamped inputs use the same processing logic without
inheritance or runtime dispatch. Zip inputs may mix both view types. Both
types provide backing-storage information internally for overlap validation.

Constrain the normalized destination to `View<T>` with non-const `T`, rejecting
read-only and clamped destinations at compile time. For example:

```cpp
template<typename SourceView, typename T>
    requires (!std::is_const_v<T>)
Expected<void> copy_views(const SourceView& source, const View<T>& destination);
```

For example, the public copy wrapper has this structure, with the supported
argument and pixel-type constraints applied in the implementation:

```cpp
template<typename Source, typename Destination>
Expected<void> copy(Source&& source, Destination&& destination)
{
    auto source_view = make_view(source);
    auto destination_view = make_view(destination);

    return detail::copy_views(source_view, destination_view);
}
```

The single public template supports all four combinations:

```cpp
copy(source_raster, destination_raster);
copy(source_raster, destination_view);
copy(source_view, destination_raster);
copy(source_view, destination_view);
```

`detail::copy_views` owns dimension and overlap validation and the copying
loop. Apply the same wrapper structure to `transform`, `window_transform`,
and the three `zip_transform` overloads. Normalize each zip input, including
tuple elements, independently, preserving pixel constness. Temporary raster
arguments are valid for these synchronous operations.

There are twelve public function-template overloads: six taking a destination
and six returning a raster. Each group has one copy, one transform, one window
transform, and three zip transforms. Internal implementation helpers are
additional. Do not multiply public overloads for
raster/view combinations or introduce a converting constructor to make
algorithm calls work. The compiler can instantiate the templates for the
argument combinations actually used.

## Types and errors

Pixel-type and callable compatibility are checked at compile time.
Dimensions, region bounds, kernel parameters, and prohibited input/output
overlap are checked at runtime. Runtime validation errors follow terrainlib's
`Expected` / `Error` conventions; operations return `Expected<void>`.

All argument validation completes before writing any output pixels or invoking
the transform callable. Invalid arguments leave the output unchanged.

Copy requires identical pixel types, ignoring source constness. Each transform
callable must return exactly the destination pixel type by value. There is
no implicit output conversion; callers express conversions inside the callable.

## Callables

The callable is a template parameter, with no type erasure or machinery to
inspect its state. Immutable configuration, including captured values, is
permitted. Callbacks must not change state between invocations or depend on
invocation order. This is a caller contract; const invocation does not prove
the absence of external side effects.

Pointwise callbacks receive stored input pixels through const references.
Window callbacks receive a read-only input view with local coordinates
starting at `(0, 0)`. The kernel for output pixel `(x, y)` starts at input-view
coordinate `(x * stride.x, y * stride.y)` and covers `kernel_size` pixels.
The kernel preserves the source's ordinary or clamped view type; a callback
such as `[](const auto& window) { ... }` can handle either.

Execution order is not part of the public contract. The implementation visits
pixels in row-major order internally. Exceptions have no specified API
guarantees or special treatment; no catching, translation, or rollback is
introduced for callback exceptions.

## Overlap

Input views may overlap each other. Reject partial overlap between any input
and the output. For copy and pointwise transforms, permit an identical
input/output region when pixel types match. Other inputs to a zip transform
must independently satisfy the same rule relative to the output.

For clamped inputs, overlap checks account for the backing pixels read through
clamping. Identical input/output regions require corresponding logical pixels
to address the same stored pixels, not merely equal logical dimensions.

Reject all input/output overlap for windowed transforms. These operations do
not promise snapshot behavior through temporary buffers.

## Pixel interpretation

These are raw pixel operations. They do not interpret attribution, NoData,
colour encoding, or numeric conversion policy. Callables receive stored
pixels unchanged and control interpretation and conversion. Copy preserves
pixels exactly.

The [scaling refactor agreed on 2026-09-22](raster-store/scaling.md) adopts
the same raster/view normalization, clamped sources, destination and allocating
overloads, and header placement. Generic scaling has no attribution, validity
mask, or value-mapping enum. A supplied decoder/encoder tuple determines the
working type and conversion; linear numeric conversion is the default for
`scale`, and identity is the default for `reduce`. Reducers receive exactly
four decoded working pixels through a read-only span. Numerical paths require
finite, encodable values as a caller precondition; NN/crop bypass conversions
and preserve pixels exactly. Empty inputs and interiors remain errors for
scaling. Paired `raster_store::scaler::scale` accepts the shared
`raster_store::pixel::Mapping` and selects the data conversion tuple; paired
`reduce` accepts a custom conversion tuple with identity as its default.
Both compose independent data and attribution operations.

## Verification

The refactor's focused scaler/view suite passes 61 cases (1,347 assertions).
The terrainlib, RF builder, and DAG builder regression suites pass. Eleven
public headers compile independently with warnings as errors. GCC 16 Release
compilation of the scaler tests at `-O3 -Werror` passes, including median
instantiations that previously triggered an array-bounds diagnostic.
Changed C++ is formatted with `clang-format-21`; Qt lint reports only an
existing warning on unchanged `storage.h` code. See the
[scaler verification record](raster-store/scaling.md#refactor-verification--2026-09-22).
