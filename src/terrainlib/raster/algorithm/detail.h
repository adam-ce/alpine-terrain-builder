#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

#include "raster/ClampedView.h"

namespace raster::algorithm::detail {

template <typename Source>
concept ViewSource = requires(Source& source) { raster::make_view(source); };

template <ViewSource Source>
using SourceView = decltype(raster::make_view(std::declval<Source&>()));

template <ViewSource Source>
using SourcePixel = typename SourceView<Source>::value_type;

template <typename Destination>
concept WritableViewDestination
    = ViewSource<Destination> && view_details::is_ordinary_view<SourceView<Destination>> && (!std::is_const_v<typename SourceView<Destination>::element_type>);

template <typename Function, typename Output, typename... Inputs>
concept PixelFunction = std::invocable<const Function&, const Inputs&...> && std::same_as<std::invoke_result_t<const Function&, const Inputs&...>, Output>;

template <typename T>
concept RasterPixel = std::is_object_v<T> && (!std::is_array_v<T>) && std::same_as<T, std::remove_cv_t<T>> && (!std::same_as<T, bool>)
    && std::default_initializable<T> && std::movable<T>;

template <typename Function, typename... Inputs>
using FunctionOutput = std::invoke_result_t<const Function&, const Inputs&...>;

template <typename Function, typename... Inputs>
concept RasterFunction = std::invocable<const Function&, const Inputs&...> && RasterPixel<FunctionOutput<Function, Inputs...>>;

template <RasterPixel T>
Expected<radix::Raster<T>> allocate_output(glm::uvec2 size)
{
    if ((size.y != 0 && size.x > (std::numeric_limits<std::size_t>::max)() / size.y) || std::size_t(size.x) * size.y > std::vector<T>().max_size()) {
        return Error::fail(Error::Code::ResourceExhausted, "raster output dimensions exceed storage capacity");
    }
    try {
        return radix::Raster<T>(size);
    } catch (const std::bad_alloc&) {
        return Error::fail(Error::Code::ResourceExhausted, "allocate raster output");
    }
}

template <RasterPixel T, typename Operation>
Expected<radix::Raster<T>> produce_raster(glm::uvec2 size, const Operation& operation)
{
    auto output = allocate_output<T>(size);
    if (!output) {
        return output;
    }
    if (auto result = std::invoke(operation, raster::make_view(*output)); !result) {
        return Error::propagate(std::move(result));
    }
    return output;
}

template <typename T>
View<const std::remove_const_t<T>> read_only_view(const View<T>& source)
{
    return source;
}

template <typename T>
ClampedView<T> read_only_view(const ClampedView<T>& source)
{
    return source;
}

template <ViewSource Source>
using ReadOnlyView = decltype(read_only_view(std::declval<SourceView<Source>>()));

template <typename T>
view_details::Footprint view_footprint(const View<T>& source)
{
    return view_details::Access::footprint(source);
}

template <typename T>
view_details::Footprint view_footprint(const ClampedView<T>& source)
{
    return view_details::ClampedAccess::footprint(source);
}

inline bool footprints_overlap(const view_details::Footprint& left, const view_details::Footprint& right)
{
    return left.storage == right.storage && left.size.x != 0 && left.size.y != 0 && right.size.x != 0 && right.size.y != 0
        && std::uint64_t(left.origin.x) < std::uint64_t(right.origin.x) + right.size.x
        && std::uint64_t(right.origin.x) < std::uint64_t(left.origin.x) + left.size.x
        && std::uint64_t(left.origin.y) < std::uint64_t(right.origin.y) + right.size.y
        && std::uint64_t(right.origin.y) < std::uint64_t(left.origin.y) + left.size.y;
}

template <typename InputView, typename T>
bool identical_view_mapping(const InputView& source, const View<T>& destination)
{
    if constexpr (!std::same_as<typename InputView::value_type, std::remove_const_t<T>>) {
        return false;
    } else {
        const auto input = view_footprint(source);
        const auto output = view_footprint(destination);
        // Clamping is monotone: a footprint as large as the logical view has
        // no replicated coordinates, so matching rectangles imply identity.
        return source.size() == destination.size() && input.size == source.size() && input.storage == output.storage && input.origin == output.origin;
    }
}

template <typename InputView, typename T>
Expected<void> validate_view_overlap(const InputView& source, const View<T>& destination, bool allow_identical)
{
    if (footprints_overlap(view_footprint(source), view_footprint(destination)) && !(allow_identical && identical_view_mapping(source, destination))) {
        return Error::fail(Error::Code::InvalidInput, "raster input and output views overlap");
    }
    return {};
}

template <typename InputView, typename T>
Expected<void> validate_pointwise_views(const InputView& source, const View<T>& destination)
{
    if (source.size() != destination.size()) {
        return Error::fail(Error::Code::InvalidInput, "raster input and output view dimensions differ");
    }
    return validate_view_overlap(source, destination, true);
}

template <typename T>
auto window_view(const View<T>& source, glm::uvec2 origin, glm::uvec2 size)
{
    return view_details::Access::region(source, origin, size);
}

template <typename T>
auto window_view(const ClampedView<T>& source, glm::uvec2 origin, glm::uvec2 size)
{
    return view_details::ClampedAccess::region(source, origin, size);
}

} // namespace raster::algorithm::detail
