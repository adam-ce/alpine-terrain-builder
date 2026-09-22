#pragma once

#include "copy.h"
#include "window_transform.h"

namespace raster::algorithm {

enum class Interpolation {
    NearestNeighbour,
    Bilinear,
};
enum class Filter {
    Box,
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

    inline Expected<unsigned> filter_radius(Filter filter)
    {
        switch (filter) {
        case Filter::Box:
            return 0;
        case Filter::Lanczos2:
            return 2;
        case Filter::Lanczos3:
            return 3;
        case Filter::Lanczos4:
            return 4;
        }
        return Error::fail(Error::Code::InvalidInput, "invalid raster reduction filter");
    }

    inline Expected<unsigned> required_upscaling_halo(unsigned n_zoom_levels, Interpolation interpolation)
    {
        if (auto factor = scale_factor(n_zoom_levels); !factor) {
            return Error::propagate(std::move(factor));
        }
        switch (interpolation) {
        case Interpolation::NearestNeighbour:
            return 0;
        case Interpolation::Bilinear:
            return n_zoom_levels == 0 ? 0u : 1u;
        }
        return Error::fail(Error::Code::InvalidInput, "invalid raster interpolation");
    }

    inline Expected<unsigned> required_downscaling_halo(unsigned n_zoom_levels, Filter filter)
    {
        auto factor = scale_factor(n_zoom_levels);
        if (!factor) {
            return Error::propagate(std::move(factor));
        }
        auto radius = filter_radius(filter);
        if (!radius) {
            return Error::propagate(std::move(radius));
        }
        const auto width = *radius == 0 ? 0 : std::uint64_t(2 * *radius - 1) * (*factor - 1);
        if (width > (std::numeric_limits<unsigned>::max)()) {
            return Error::fail(Error::Code::ResourceExhausted, "required raster halo exceeds representable dimensions");
        }
        return static_cast<unsigned>(width);
    }

    struct ScalingGeometry {
        glm::uvec2 interior;
        glm::uvec2 output;
        unsigned factor;
    };

    inline Expected<ScalingGeometry> scaling_geometry(glm::uvec2 input, unsigned halo_width, unsigned levels, bool up, unsigned required_width)
    {
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
inline Expected<unsigned> required_halo(int n_zoom_levels, Interpolation interpolation, Filter filter)
{
    if (auto valid = detail::required_upscaling_halo(0, interpolation); !valid) {
        return Error::propagate(std::move(valid));
    }
    if (auto valid = detail::filter_radius(filter); !valid) {
        return Error::propagate(std::move(valid));
    }
    if (n_zoom_levels < 0) {
        return detail::required_downscaling_halo(static_cast<unsigned>(-std::int64_t(n_zoom_levels)), filter);
    }
    return detail::required_upscaling_halo(static_cast<unsigned>(n_zoom_levels), interpolation);
}

namespace detail {
    inline Expected<ScalingGeometry> scale_geometry(glm::uvec2 input, unsigned halo_width, int levels, Interpolation interpolation, Filter filter)
    {
        auto required = required_halo(levels, interpolation, filter);
        if (!required) {
            return Error::propagate(std::move(required));
        }
        return scaling_geometry(input, halo_width, static_cast<unsigned>(levels < 0 ? -std::int64_t(levels) : levels), levels >= 0, *required);
    }
    // Bounds checks use division so a small window does not require representable full dimensions.
    inline Expected<ScalingGeometry> window_geometry(
        glm::uvec2 input, unsigned halo_width, int levels, Interpolation interpolation, Filter filter, glm::uvec2 offset, glm::uvec2 size)
    {
        auto required = required_halo(levels, interpolation, filter);
        if (!required)
            return Error::propagate(std::move(required));
        auto factor = scale_factor(static_cast<unsigned>(levels < 0 ? -std::int64_t(levels) : levels));
        if (!factor)
            return Error::propagate(std::move(factor));
        if (input.x == 0 || input.y == 0 || halo_width > ((std::min)(input.x, input.y) - 1) / 2 || halo_width < *required) {
            return Error::fail(Error::Code::InvalidInput, "invalid raster interior or insufficient halo");
        }
        const auto interior = input - glm::uvec2(2 * halo_width);
        for (unsigned axis = 0; axis < 2; ++axis) {
            if (size[axis] == 0 || offset[axis] > (std::numeric_limits<unsigned>::max)() - (size[axis] - 1)) {
                return Error::fail(Error::Code::InvalidInput, "invalid scaling output window");
            }
            const unsigned last = offset[axis] + size[axis] - 1;
            if (levels >= 0 ? last / *factor >= interior[axis] : interior[axis] % *factor != 0 || last >= interior[axis] / *factor) {
                return Error::fail(Error::Code::InvalidInput, "scaling output window exceeds output bounds");
            }
        }
        return ScalingGeometry { interior, size, *factor };
    }

} // namespace detail
} // namespace raster::algorithm
