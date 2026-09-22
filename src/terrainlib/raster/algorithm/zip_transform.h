#pragma once

#include <tuple>

#include "detail.h"

namespace raster::algorithm {

namespace detail {
    template <typename... Inputs, typename Function, typename T>
    requires(!std::is_const_v<T>) && PixelFunction<Function, T, typename Inputs::value_type...>
    Expected<void> zip_transform_views(const std::tuple<Inputs...>& inputs, const Function& function, const View<T>& destination)
    {
        return std::apply(
            [&](const auto&... sources) -> Expected<void> {
                Expected<void> valid;
                const auto validate = [&](const auto& source) {
                    if (valid) {
                        valid = validate_pointwise_views(source, destination);
                    }
                };
                (validate(sources), ...);
                if (!valid) {
                    return valid;
                }
                if (destination.width() == 0 || destination.height() == 0) {
                    return {};
                }
                for (unsigned y = 0; y < destination.height(); ++y) {
                    for (unsigned x = 0; x < destination.width(); ++x) {
                        destination.pixel({ x, y }) = std::invoke(function, std::as_const(sources.pixel({ x, y }))...);
                    }
                }
                return {};
            },
            inputs);
    }

    template <typename... Inputs, typename Function>
    requires RasterFunction<Function, typename Inputs::value_type...>
    auto zip_transform_new(const std::tuple<Inputs...>& inputs, const Function& function)
        -> Expected<radix::Raster<FunctionOutput<Function, typename Inputs::value_type...>>>
    {
        const auto size = std::get<0>(inputs).size();
        if (!std::apply([&](const auto&... sources) { return ((sources.size() == size) && ...); }, inputs)) {
            return Error::fail(Error::Code::InvalidInput, "zip input view dimensions differ");
        }
        return produce_raster<FunctionOutput<Function, typename Inputs::value_type...>>(
            size, [&](const auto& destination) { return zip_transform_views(inputs, function, destination); });
    }
} // namespace detail

template <detail::ViewSource First, detail::ViewSource Second, typename Function, detail::WritableViewDestination Destination>
requires detail::PixelFunction<Function, detail::SourcePixel<Destination>, detail::SourcePixel<First>, detail::SourcePixel<Second>>
[[nodiscard]] Expected<void> zip_transform(First&& first, Second&& second, const Function& function, Destination&& destination)
{
    return detail::zip_transform_views(std::tuple { raster::make_view(first), raster::make_view(second) }, function, raster::make_view(destination));
}

template <detail::ViewSource First, detail::ViewSource Second, detail::ViewSource Third, typename Function, detail::WritableViewDestination Destination>
requires detail::PixelFunction<Function, detail::SourcePixel<Destination>, detail::SourcePixel<First>, detail::SourcePixel<Second>, detail::SourcePixel<Third>>
[[nodiscard]] Expected<void> zip_transform(First&& first, Second&& second, Third&& third, const Function& function, Destination&& destination)
{
    return detail::zip_transform_views(
        std::tuple { raster::make_view(first), raster::make_view(second), raster::make_view(third) }, function, raster::make_view(destination));
}

/// Four or more heterogeneous inputs; tuples of raster references avoid copies.
template <typename... Sources, typename Function, detail::WritableViewDestination Destination>
requires(sizeof...(Sources) >= 4) && (detail::ViewSource<Sources> && ...)
    && detail::PixelFunction<Function, detail::SourcePixel<Destination>, detail::SourcePixel<Sources>...>
[[nodiscard]] Expected<void> zip_transform(const std::tuple<Sources...>& sources, const Function& function, Destination&& destination)
{
    return std::apply(
        [&](const auto&... inputs) {
            return detail::zip_transform_views(std::tuple { raster::make_view(inputs)... }, function, raster::make_view(destination));
        },
        sources);
}

template <detail::ViewSource First, detail::ViewSource Second, typename Function>
requires detail::RasterFunction<Function, detail::SourcePixel<First>, detail::SourcePixel<Second>>
[[nodiscard]] auto zip_transform(First&& first, Second&& second, const Function& function)
    -> Expected<radix::Raster<detail::FunctionOutput<Function, detail::SourcePixel<First>, detail::SourcePixel<Second>>>>
{
    return detail::zip_transform_new(std::tuple { raster::make_view(first), raster::make_view(second) }, function);
}

template <detail::ViewSource First, detail::ViewSource Second, detail::ViewSource Third, typename Function>
requires detail::RasterFunction<Function, detail::SourcePixel<First>, detail::SourcePixel<Second>, detail::SourcePixel<Third>>
[[nodiscard]] auto zip_transform(First&& first, Second&& second, Third&& third, const Function& function)
    -> Expected<radix::Raster<detail::FunctionOutput<Function, detail::SourcePixel<First>, detail::SourcePixel<Second>, detail::SourcePixel<Third>>>>
{
    return detail::zip_transform_new(std::tuple { raster::make_view(first), raster::make_view(second), raster::make_view(third) }, function);
}

template <typename... Sources, typename Function>
requires(sizeof...(Sources) >= 4) && (detail::ViewSource<Sources> && ...) && detail::RasterFunction<Function, detail::SourcePixel<Sources>...>
[[nodiscard]] auto zip_transform(const std::tuple<Sources...>& sources, const Function& function)
    -> Expected<radix::Raster<detail::FunctionOutput<Function, detail::SourcePixel<Sources>...>>>
{
    return std::apply([&](const auto&... inputs) { return detail::zip_transform_new(std::tuple { raster::make_view(inputs)... }, function); }, sources);
}

} // namespace raster::algorithm
