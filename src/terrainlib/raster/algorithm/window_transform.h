#pragma once

#include "detail.h"

namespace raster::algorithm {

namespace detail {
    inline Expected<glm::uvec2> window_output_size(glm::uvec2 input_size, glm::uvec2 kernel_size, glm::uvec2 stride = glm::uvec2(1))
    {
        if (kernel_size.x == 0 || kernel_size.y == 0 || stride.x == 0 || stride.y == 0 || kernel_size.x > input_size.x || kernel_size.y > input_size.y) {
            return Error::fail(Error::Code::InvalidInput, "window size and stride must be positive, and the window must fit the input view");
        }
        return (input_size - kernel_size) / stride + glm::uvec2(1);
    }

    template <typename InputView, typename Function, typename T>
    requires(!std::is_const_v<T>) && PixelFunction<Function, T, InputView>
    Expected<void> window_transform_views(
        const InputView& source, glm::uvec2 kernel_size, glm::uvec2 stride, const Function& function, const View<T>& destination)
    {
        auto size = window_output_size(source.size(), kernel_size, stride);
        if (!size) {
            return Error::propagate(std::move(size));
        }
        if (destination.size() != *size) {
            return Error::fail(Error::Code::InvalidInput, "window transform output dimensions do not match the input and kernel");
        }
        if (auto valid = validate_view_overlap(source, destination, false); !valid) {
            return valid;
        }
        for (unsigned y = 0; y < destination.height(); ++y) {
            for (unsigned x = 0; x < destination.width(); ++x) {
                const auto window = window_view(source, glm::uvec2(x, y) * stride, kernel_size);
                destination.pixel({ x, y }) = std::invoke(function, window);
            }
        }
        return {};
    }
} // namespace detail

/// Applies complete rectangular windows at the given positive stride, without border handling.
template <detail::ViewSource Source, typename Function, detail::WritableViewDestination Destination>
requires detail::PixelFunction<Function, detail::SourcePixel<Destination>, detail::ReadOnlyView<Source>>
[[nodiscard]] Expected<void> window_transform(Source&& source, glm::uvec2 kernel_size, glm::uvec2 stride, const Function& function, Destination&& destination)
{
    return detail::window_transform_views(detail::read_only_view(raster::make_view(source)), kernel_size, stride, function, raster::make_view(destination));
}

template <detail::ViewSource Source, typename Function>
requires detail::RasterFunction<Function, detail::ReadOnlyView<Source>>
[[nodiscard]] auto window_transform(Source&& source, glm::uvec2 kernel_size, glm::uvec2 stride, const Function& function)
    -> Expected<radix::Raster<detail::FunctionOutput<Function, detail::ReadOnlyView<Source>>>>
{
    const auto input = detail::read_only_view(raster::make_view(source));
    auto size = detail::window_output_size(input.size(), kernel_size, stride);
    if (!size) {
        return Error::propagate(std::move(size));
    }
    return detail::produce_raster<detail::FunctionOutput<Function, detail::ReadOnlyView<Source>>>(
        *size, [&](const auto& destination) { return detail::window_transform_views(input, kernel_size, stride, function, destination); });
}

/// Existing call forms use stride one.
template <detail::ViewSource Source, typename Function, detail::WritableViewDestination Destination>
requires detail::PixelFunction<Function, detail::SourcePixel<Destination>, detail::ReadOnlyView<Source>>
[[nodiscard]] Expected<void> window_transform(Source&& source, glm::uvec2 kernel_size, const Function& function, Destination&& destination)
{
    return window_transform(std::forward<Source>(source), kernel_size, glm::uvec2(1), function, std::forward<Destination>(destination));
}

template <detail::ViewSource Source, typename Function>
requires detail::RasterFunction<Function, detail::ReadOnlyView<Source>>
[[nodiscard]] auto window_transform(Source&& source, glm::uvec2 kernel_size, const Function& function)
{
    return window_transform(std::forward<Source>(source), kernel_size, glm::uvec2(1), function);
}

} // namespace raster::algorithm
