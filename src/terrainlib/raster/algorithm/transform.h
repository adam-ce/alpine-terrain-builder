#pragma once

#include "detail.h"

namespace raster::algorithm {

namespace detail {
    template <typename InputView, typename Function, typename T>
    requires(!std::is_const_v<T>) && PixelFunction<Function, T, typename InputView::value_type>
    Expected<void> transform_views(const InputView& source, const Function& function, const View<T>& destination)
    {
        if (auto valid = validate_pointwise_views(source, destination); !valid) {
            return valid;
        }
        if (destination.width() == 0 || destination.height() == 0) {
            return {};
        }
        for (unsigned y = 0; y < destination.height(); ++y) {
            for (unsigned x = 0; x < destination.width(); ++x) {
                destination.pixel({ x, y }) = std::invoke(function, std::as_const(source.pixel({ x, y })));
            }
        }
        return {};
    }
} // namespace detail

/// The callable receives a read-only pixel and returns exactly the output type.
template <detail::ViewSource Source, typename Function, detail::WritableViewDestination Destination>
requires detail::PixelFunction<Function, detail::SourcePixel<Destination>, detail::SourcePixel<Source>>
[[nodiscard]] Expected<void> transform(Source&& source, const Function& function, Destination&& destination)
{
    return detail::transform_views(raster::make_view(source), function, raster::make_view(destination));
}

template <detail::ViewSource Source, typename Function>
requires detail::RasterFunction<Function, detail::SourcePixel<Source>>
[[nodiscard]] auto transform(Source&& source, const Function& function)
    -> Expected<radix::Raster<detail::FunctionOutput<Function, detail::SourcePixel<Source>>>>
{
    const auto input = raster::make_view(source);
    return detail::produce_raster<detail::FunctionOutput<Function, detail::SourcePixel<Source>>>(
        input.size(), [&](const auto& destination) { return detail::transform_views(input, function, destination); });
}

} // namespace raster::algorithm
