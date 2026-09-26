#pragma once

#include "copy.h"
#include "window_transform.h"

namespace raster::algorithm {

enum class Resampling {
    NearestNeighbourAndBox,
    BiliinearAndBox,
    Lanczos2,
    Lanczos3,
    Lanczos4,
};

namespace detail {
    inline Expected<unsigned> scale_factor(unsigned n_zoom_levels)
    {
        if (n_zoom_levels >= std::numeric_limits<unsigned>::digits) {
            return Error::fail(Error::Code::InvalidInput, "raster zoom-level count exceeds the supported range");
        }
        return 1u << n_zoom_levels;
    }

    inline Expected<unsigned> filter_radius(Resampling method)
    {
        switch (method) {
        case Resampling::NearestNeighbourAndBox:
        case Resampling::BiliinearAndBox:
            return 0;
        case Resampling::Lanczos2:
            return 2;
        case Resampling::Lanczos3:
            return 3;
        case Resampling::Lanczos4:
            return 4;
        }
        return Error::fail(Error::Code::InvalidInput, "invalid raster resampling method");
    }

    // Floor division, including negative output coordinates and factors above INT_MAX.
    inline int source_cell(int position, unsigned factor)
    {
        return position >= 0 ? int(unsigned(position) / factor) : -1 - int(unsigned(-(position + 1)) / factor);
    }

    inline unsigned source_phase(int position, unsigned factor)
    {
        return unsigned(std::int64_t(position) - std::int64_t(source_cell(position, factor)) * factor);
    }

    inline int upscaling_origin(int position, unsigned factor, unsigned radius)
    {
        const int cell = source_cell(position, factor);
        if (radius == 0)
            return cell;
        return cell - (source_phase(position, factor) < factor / 2 ? 1 : 0) - int(radius) + 1;
    }

    inline unsigned upscaling_radius(Resampling method)
    {
        return method == Resampling::NearestNeighbourAndBox ? 0 : method == Resampling::BiliinearAndBox ? 1 : *filter_radius(method);
    }

    struct ScalingGeometry {
        glm::uvec2 interior;
        glm::uvec2 output;
        unsigned factor;
        glm::uvec2 source_origin {};
        glm::uvec2 source_size {};
    };

    inline Expected<ScalingGeometry> scaling_geometry(glm::uvec2 input, unsigned halo_width, unsigned levels, bool up, unsigned required_width)
    {
        assert(input.x <= (1u << 30) && input.y <= (1u << 30));
        auto factor = scale_factor(levels);
        if (!factor) {
            return Error::propagate(std::move(factor));
        }
        const auto padding = std::uint64_t(halo_width) * 2;
        if (input.x <= padding || input.y <= padding) {
            return Error::fail(Error::Code::InvalidInput, "raster must contain a nonempty interior");
        }
        if (halo_width < required_width) {
            return Error::fail(Error::Code::InvalidInput, "insufficient raster halo for scaling kernel");
        }
        const glm::uvec2 interior = input - glm::uvec2(static_cast<unsigned>(padding));
        if (!up && (interior.x % *factor != 0 || interior.y % *factor != 0)) {
            return Error::fail(Error::Code::InvalidInput, "raster interior dimensions must be divisible by the reduction factor");
        }
        const auto width = up ? std::uint64_t(interior.x) * *factor : interior.x / *factor;
        const auto height = up ? std::uint64_t(interior.y) * *factor : interior.y / *factor;
        if (width > (std::numeric_limits<unsigned>::max)() || height > (std::numeric_limits<unsigned>::max)()) {
            return Error::fail(Error::Code::ResourceExhausted, "scaled raster dimensions exceed representable limits");
        }
        return ScalingGeometry { interior, { static_cast<unsigned>(width), static_cast<unsigned>(height) }, *factor };
    }

    template <typename InputView, typename T>
    Expected<void> validate_scaling_destination(const InputView& source, unsigned halo_width, const ScalingGeometry& geometry, const View<T>& destination)
    {
        if (destination.size() != geometry.output) {
            return Error::fail(Error::Code::InvalidInput, "raster scaling output dimensions do not match");
        }
        if (geometry.factor == 1) {
            return validate_view_overlap(*raster::make_view(source, glm::uvec2(halo_width), geometry.interior), destination, true);
        }
        return validate_view_overlap(source, destination, false);
    }

    // Each stage retains just enough halo for the remaining steps. The first
    // source can be clamped; subsequent stages read ordinary intermediate rasters.
    template <typename InputView, typename T, typename Function>
    Expected<void> reduce_levels(
        const InputView& source, unsigned halo_width, const ScalingGeometry& geometry, unsigned base_halo, const Function& function, const View<T>& destination)
    {
        if (geometry.factor == 1) {
            return copy_views(*raster::make_view(source, glm::uvec2(halo_width), geometry.interior), destination);
        }
        auto interior = geometry.interior;
        radix::Raster<T> intermediate;
        bool first = true;
        for (unsigned remaining_factor = geometry.factor / 2;; remaining_factor /= 2) {
            interior /= 2u;
            const unsigned output_halo = base_halo * (remaining_factor - 1);
            const glm::uvec2 output_size = interior + glm::uvec2(2 * output_halo);
            const auto step = [&](const auto& input, const auto& output) {
                const unsigned origin = halo_width - 2 * output_halo - base_halo;
                const auto region = *raster::make_view(input, glm::uvec2(origin), 2u * output_size + glm::uvec2(2 * base_halo));
                return std::invoke(function, region, output);
            };
            const auto process = [&](const auto& output) {
                if (first) {
                    return step(source, output);
                }
                return step(read_only_view(raster::make_view(intermediate)), output);
            };
            if (remaining_factor == 1) {
                return process(destination);
            }
            auto output = produce_raster<T>(output_size, process);
            if (!output) {
                return Error::propagate(std::move(output));
            }
            intermediate = std::move(*output);
            halo_width = output_halo;
            first = false;
        }
    }
} // namespace detail

/// Positive levels upscale, negative levels downscale, and zero only crops.
/// This query covers the full interior; a requested output halo needs additional support.
inline Expected<unsigned> required_halo(int n_zoom_levels, Resampling method)
{
    auto radius = detail::filter_radius(method);
    if (!radius)
        return Error::propagate(std::move(radius));
    auto factor = detail::scale_factor(static_cast<unsigned>(n_zoom_levels < 0 ? -std::int64_t(n_zoom_levels) : n_zoom_levels));
    if (!factor)
        return Error::propagate(std::move(factor));
    if (n_zoom_levels >= 0)
        return n_zoom_levels == 0 ? 0 : detail::upscaling_radius(method);
    const auto width = *radius == 0 ? 0 : std::uint64_t(2 * *radius - 1) * (*factor - 1);
    if (width > (std::numeric_limits<unsigned>::max)())
        return Error::fail(Error::Code::ResourceExhausted, "required raster halo exceeds representable dimensions");
    return static_cast<unsigned>(width);
}

namespace detail {
    inline Expected<ScalingGeometry> scale_geometry(glm::uvec2 input, unsigned halo_width, int levels, Resampling method)
    {
        auto required = required_halo(levels, method);
        if (!required)
            return Error::propagate(std::move(required));
        return scaling_geometry(input, halo_width, static_cast<unsigned>(levels < 0 ? -std::int64_t(levels) : levels), levels >= 0, *required);
    }

    // Validate actual source support, including windows outside the scaled interior.
    inline Expected<ScalingGeometry> window_geometry(glm::uvec2 input, unsigned halo_width, int levels, Resampling method, glm::ivec2 offset, glm::uvec2 size)
    {
        assert(input.x <= (1u << 30) && input.y <= (1u << 30));
        assert(size.x <= (1u << 30) && size.y <= (1u << 30));
        assert(offset.x >= -(1 << 30) && offset.x < (1 << 30) && offset.y >= -(1 << 30) && offset.y < (1 << 30));
        auto required = required_halo(levels, method);
        if (!required)
            return Error::propagate(std::move(required));
        const auto factor = *scale_factor(static_cast<unsigned>(levels < 0 ? -std::int64_t(levels) : levels));
        if (input.x == 0 || input.y == 0 || halo_width > ((std::min)(input.x, input.y) - 1) / 2)
            return Error::fail(Error::Code::InvalidInput, "invalid raster interior");
        ScalingGeometry result { input - glm::uvec2(2 * halo_width), size, factor };
        for (unsigned axis = 0; axis < 2; ++axis) {
            if (size[axis] == 0)
                return Error::fail(Error::Code::InvalidInput, "empty scaling output window");
            const int last = offset[axis] + int(size[axis] - 1);
            std::int64_t first_source;
            std::int64_t last_source;
            if (levels > 0) {
                const unsigned radius = upscaling_radius(method);
                first_source = std::int64_t(halo_width) + upscaling_origin(offset[axis], factor, radius);
                last_source = std::int64_t(halo_width) + upscaling_origin(last, factor, radius) + (radius == 0 ? 0 : 2 * radius - 1);
            } else if (levels < 0) {
                if (result.interior[axis] % factor != 0)
                    return Error::fail(Error::Code::InvalidInput, "raster interior dimensions must be divisible by the reduction factor");
                first_source = std::int64_t(halo_width) + std::int64_t(offset[axis]) * factor - *required;
                last_source = std::int64_t(halo_width) + (std::int64_t(last) + 1) * factor + *required - 1;
            } else {
                first_source = std::int64_t(halo_width) + offset[axis];
                last_source = std::int64_t(halo_width) + last;
            }
            if (first_source < 0 || last_source >= input[axis])
                return Error::fail(Error::Code::InvalidInput, "insufficient raster support for scaling output window");
            result.source_origin[axis] = unsigned(first_source);
            result.source_size[axis] = unsigned(last_source - first_source + 1);
        }
        return result;
    }

} // namespace detail

struct SourceWindow {
    glm::uvec2 origin;
    glm::uvec2 size;
};

/// Query the source rectangle read by scaling an output window, without accessing pixels.
/// The returned origin includes the source halo; output_offset is relative to the scaled interior.
/// Rejects invalid geometry or insufficient source support, just like windowed scale().
[[nodiscard]] inline Expected<SourceWindow> required_source_window(
    glm::uvec2 input_size, unsigned halo_width, int levels, Resampling method, glm::ivec2 output_offset, glm::uvec2 output_size)
{
    auto geometry = detail::window_geometry(input_size, halo_width, levels, method, output_offset, output_size);
    if (!geometry)
        return Error::propagate(std::move(geometry));
    return SourceWindow { geometry->source_origin, geometry->source_size };
}

} // namespace raster::algorithm
