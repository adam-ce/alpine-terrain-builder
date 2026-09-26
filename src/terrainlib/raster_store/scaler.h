#pragma once

#include "pixel.h"
#include "raster/algorithm/reduce.h"
#include "raster/algorithm/scale.h"

namespace raster_store::scaler {
namespace detail {
    namespace algorithm = raster::algorithm;
    namespace raster_detail = algorithm::detail;

    template <typename Data, typename Attribution>
    concept PairedSources = raster_detail::ViewSource<Data> && raster_detail::ViewSource<Attribution>
        && raster_detail::NumericPixel<raster_detail::SourcePixel<Data>> && std::same_as<raster_detail::SourcePixel<Attribution>, std::uint16_t>;

    template <typename Data, typename Destination, typename AttributionDestination>
    concept PairedDestinations = raster_detail::WritableViewDestination<Destination> && raster_detail::WritableViewDestination<AttributionDestination>
        && std::same_as<raster_detail::SourcePixel<Data>, raster_detail::SourcePixel<Destination>>
        && std::same_as<raster_detail::SourcePixel<AttributionDestination>, std::uint16_t>;

    template <typename T>
    Expected<void> validate_mapping(pixel::Mapping mapping, bool numerical = true)
    {
        switch (mapping) {
        case pixel::Mapping::Linear:
            return {};
        case pixel::Mapping::SRGBA:
            if (!numerical)
                return {};
            if constexpr (raster_detail::srgb_pixel<T>) {
                return {};
            }
            return Error::fail(Error::Code::Unsupported, "sRGB mapping requires RGB or RGBA uint8 pixels");
        }
        return Error::fail(Error::Code::InvalidInput, "invalid raster value mapping");
    }

    template <typename Data, typename Attribution, typename T>
    Expected<void> validate_destinations(const Data& data,
        const Attribution& attribution,
        unsigned halo_width,
        const raster_detail::ScalingGeometry& geometry,
        const raster::View<T>& output,
        const raster::View<std::uint16_t>& output_attribution)
    {
        if (data.size() != attribution.size()) {
            return Error::fail(Error::Code::InvalidInput, "data and attribution dimensions differ");
        }
        if (auto valid = raster_detail::validate_scaling_destination(data, halo_width, geometry, output); !valid) {
            return valid;
        }
        if (auto valid = raster_detail::validate_scaling_destination(attribution, halo_width, geometry, output_attribution); !valid) {
            return valid;
        }
        // Cross-pair aliases must be rejected before either operation writes.
        if (auto valid = raster_detail::validate_view_overlap(data, output_attribution, false); !valid) {
            return valid;
        }
        if (auto valid = raster_detail::validate_view_overlap(attribution, output, false); !valid) {
            return valid;
        }
        return raster_detail::validate_view_overlap(output, output_attribution, false);
    }

    template <typename T, typename Operation>
    Expected<std::pair<radix::Raster<T>, radix::Raster<std::uint16_t>>> produce_pair(glm::uvec2 size, const Operation& operation)
    {
        auto data = raster_detail::allocate_output<T>(size);
        if (!data) {
            return Error::propagate(std::move(data));
        }
        auto attribution = raster_detail::allocate_output<std::uint16_t>(size);
        if (!attribution) {
            return Error::propagate(std::move(attribution));
        }
        if (auto result = operation(*data, *attribution); !result) {
            return Error::propagate(std::move(result));
        }
        return std::pair { std::move(*data), std::move(*attribution) };
    }
} // namespace detail

/// Scale only the selected conceptual output window.
template <typename Data, typename Attribution, typename Destination, typename AttributionDestination>
requires detail::PairedSources<Data, Attribution> && detail::PairedDestinations<Data, Destination, AttributionDestination>
[[nodiscard]] Expected<void> scale(Data&& data,
    Attribution&& attribution,
    unsigned halo_width,
    int levels,
    raster::algorithm::Resampling method,
    glm::ivec2 output_offset,
    pixel::Mapping mapping,
    Destination&& destination,
    AttributionDestination&& attribution_destination)
{
    namespace algorithm = raster::algorithm;
    using T = algorithm::detail::SourcePixel<Data>;
    if (auto valid = detail::validate_mapping<T>(mapping, levels < 0 || (levels > 0 && method != raster::algorithm::Resampling::NearestNeighbourAndBox));
        !valid)
        return valid;
    const auto input = algorithm::detail::read_only_view(raster::make_view(data));
    const auto input_attribution = algorithm::detail::read_only_view(raster::make_view(attribution));
    const auto output = raster::make_view(destination);
    const auto output_attribution = raster::make_view(attribution_destination);
    auto geometry = algorithm::detail::window_geometry(input.size(), halo_width, levels, method, output_offset, output.size());
    if (!geometry)
        return Error::propagate(std::move(geometry));
    if (input.size() != input_attribution.size() || output.size() != output_attribution.size())
        return Error::fail(Error::Code::InvalidInput, "data and attribution dimensions differ");
    const auto check_input = [&](const auto& source, const auto& target) {
        if (levels == 0)
            return algorithm::detail::validate_view_overlap(*raster::make_view(source, geometry->source_origin, output.size()), target, true);
        return algorithm::detail::validate_view_overlap(source, target, false);
    };
    if (auto valid = check_input(input, output); !valid)
        return valid;
    if (auto valid = check_input(input_attribution, output_attribution); !valid)
        return valid;
    if (auto valid = algorithm::detail::validate_view_overlap(input, output_attribution, false); !valid)
        return valid;
    if (auto valid = algorithm::detail::validate_view_overlap(input_attribution, output, false); !valid)
        return valid;
    if (auto valid = algorithm::detail::validate_view_overlap(output, output_attribution, false); !valid)
        return valid;
    const auto scale_data = [&]() -> Expected<void> {
        if constexpr (algorithm::detail::srgb_pixel<T>) {
            if (mapping == pixel::Mapping::SRGBA)
                return algorithm::scale(input, halo_width, levels, method, output_offset, algorithm::srgb_conversion<T>(), output);
        }
        return algorithm::scale(input, halo_width, levels, method, output_offset, output);
    };
    if (auto result = scale_data(); !result)
        return result;
    if (levels < 0) {
        const auto attribution_geometry = *algorithm::detail::window_geometry(
            input_attribution.size(), halo_width, levels, algorithm::Resampling::NearestNeighbourAndBox, output_offset, output.size());
        auto region = raster::make_view(input_attribution, attribution_geometry.source_origin, attribution_geometry.source_size);
        if (!region)
            return Error::propagate(std::move(region));
        return algorithm::reduce(*region, 0, static_cast<unsigned>(-std::int64_t(levels)), algorithm::Mode {}, output_attribution);
    }
    return algorithm::scale(input_attribution, halo_width, levels, algorithm::Resampling::NearestNeighbourAndBox, output_offset, output_attribution);
}

template <typename Data, typename Attribution>
requires detail::PairedSources<Data, Attribution>
[[nodiscard]] Expected<std::pair<radix::Raster<raster::algorithm::detail::SourcePixel<Data>>, radix::Raster<std::uint16_t>>> scale(Data&& data,
    Attribution&& attribution,
    unsigned halo_width,
    int levels,
    raster::algorithm::Resampling method,
    glm::ivec2 output_offset,
    glm::uvec2 output_size,
    pixel::Mapping mapping)
{
    using T = raster::algorithm::detail::SourcePixel<Data>;
    if (auto valid = detail::validate_mapping<T>(mapping, levels < 0 || (levels > 0 && method != raster::algorithm::Resampling::NearestNeighbourAndBox));
        !valid)
        return Error::propagate(std::move(valid));
    auto geometry = raster::algorithm::detail::window_geometry(data.size(), halo_width, levels, method, output_offset, output_size);
    if (!geometry)
        return Error::propagate(std::move(geometry));
    if (data.size() != attribution.size())
        return Error::fail(Error::Code::InvalidInput, "data and attribution dimensions differ");
    return detail::produce_pair<T>(output_size, [&](auto& output, auto& output_attribution) {
        return scale(data, attribution, halo_width, levels, method, output_offset, mapping, output, output_attribution);
    });
}

/// Full-output convenience overloads use the same windowed scaling implementation.
template <typename Data, typename Attribution, typename Destination, typename AttributionDestination>
requires detail::PairedSources<Data, Attribution> && detail::PairedDestinations<Data, Destination, AttributionDestination>
[[nodiscard]] Expected<void> scale(Data&& data,
    Attribution&& attribution,
    unsigned halo_width,
    int levels,
    raster::algorithm::Resampling method,
    pixel::Mapping mapping,
    Destination&& destination,
    AttributionDestination&& attribution_destination)
{
    auto geometry = raster::algorithm::detail::scale_geometry(data.size(), halo_width, levels, method);
    if (!geometry)
        return Error::propagate(std::move(geometry));
    if (destination.size() != geometry->output || attribution_destination.size() != geometry->output)
        return Error::fail(Error::Code::InvalidInput, "raster scaling output dimensions do not match");
    return scale(std::forward<Data>(data),
        std::forward<Attribution>(attribution),
        halo_width,
        levels,
        method,
        glm::ivec2(0),
        mapping,
        std::forward<Destination>(destination),
        std::forward<AttributionDestination>(attribution_destination));
}

template <typename Data, typename Attribution>
requires detail::PairedSources<Data, Attribution>
[[nodiscard]] Expected<std::pair<radix::Raster<raster::algorithm::detail::SourcePixel<Data>>, radix::Raster<std::uint16_t>>> scale(
    Data&& data, Attribution&& attribution, unsigned halo_width, int levels, raster::algorithm::Resampling method, pixel::Mapping mapping)
{
    auto geometry = raster::algorithm::detail::scale_geometry(data.size(), halo_width, levels, method);
    if (!geometry)
        return Error::propagate(std::move(geometry));
    return scale(std::forward<Data>(data), std::forward<Attribution>(attribution), halo_width, levels, method, glm::ivec2(0), geometry->output, mapping);
}

/// Custom data reduction uses the supplied conversion; attribution always uses identity and Mode.
template <typename Data, typename Attribution, typename Conversion, typename Reducer, typename Destination, typename AttributionDestination>
requires detail::PairedSources<Data, Attribution> && detail::PairedDestinations<Data, Destination, AttributionDestination>
    && raster::algorithm::detail::PixelReduction<Conversion, Reducer, raster::algorithm::detail::SourcePixel<Data>>
[[nodiscard]] Expected<void> reduce(Data&& data,
    Attribution&& attribution,
    unsigned halo_width,
    unsigned n_zoom_levels,
    const Conversion& conversion,
    const Reducer& reducer,
    Destination&& destination,
    AttributionDestination&& attribution_destination)
{
    namespace algorithm = raster::algorithm;
    const auto input = algorithm::detail::read_only_view(raster::make_view(data));
    const auto input_attribution = algorithm::detail::read_only_view(raster::make_view(attribution));
    const auto output = raster::make_view(destination);
    const auto output_attribution = raster::make_view(attribution_destination);
    auto geometry = algorithm::detail::scaling_geometry(input.size(), halo_width, n_zoom_levels, false, 0);
    if (!geometry) {
        return Error::propagate(std::move(geometry));
    }
    if (auto valid = detail::validate_destinations(input, input_attribution, halo_width, *geometry, output, output_attribution); !valid) {
        return valid;
    }
    if (auto result = algorithm::reduce(input, halo_width, n_zoom_levels, conversion, reducer, output); !result) {
        return result;
    }
    return algorithm::reduce(input_attribution, halo_width, n_zoom_levels, algorithm::Mode {}, output_attribution);
}

template <typename Data, typename Attribution, typename Conversion, typename Reducer>
requires detail::PairedSources<Data, Attribution>
    && raster::algorithm::detail::PixelReduction<Conversion, Reducer, raster::algorithm::detail::SourcePixel<Data>>
[[nodiscard]] Expected<std::pair<radix::Raster<raster::algorithm::detail::SourcePixel<Data>>, radix::Raster<std::uint16_t>>> reduce(
    Data&& data, Attribution&& attribution, unsigned halo_width, unsigned n_zoom_levels, const Conversion& conversion, const Reducer& reducer)
{
    if (data.size() != attribution.size()) {
        return Error::fail(Error::Code::InvalidInput, "data and attribution dimensions differ");
    }
    auto geometry = raster::algorithm::detail::scaling_geometry(data.size(), halo_width, n_zoom_levels, false, 0);
    if (!geometry) {
        return Error::propagate(std::move(geometry));
    }
    return detail::produce_pair<raster::algorithm::detail::SourcePixel<Data>>(geometry->output, [&](auto& output, auto& output_attribution) {
        return reduce(data, attribution, halo_width, n_zoom_levels, conversion, reducer, output, output_attribution);
    });
}

template <typename Data, typename Attribution, typename Reducer, typename Destination, typename AttributionDestination>
requires detail::PairedSources<Data, Attribution> && detail::PairedDestinations<Data, Destination, AttributionDestination>
    && raster::algorithm::detail::PixelReduction<decltype(raster::algorithm::identity_conversion()), Reducer, raster::algorithm::detail::SourcePixel<Data>>
[[nodiscard]] Expected<void> reduce(Data&& data,
    Attribution&& attribution,
    unsigned halo_width,
    unsigned n_zoom_levels,
    const Reducer& reducer,
    Destination&& destination,
    AttributionDestination&& attribution_destination)
{
    return reduce(std::forward<Data>(data),
        std::forward<Attribution>(attribution),
        halo_width,
        n_zoom_levels,
        raster::algorithm::identity_conversion(),
        reducer,
        std::forward<Destination>(destination),
        std::forward<AttributionDestination>(attribution_destination));
}

template <typename Data, typename Attribution, typename Reducer>
requires detail::PairedSources<Data, Attribution>
    && raster::algorithm::detail::PixelReduction<decltype(raster::algorithm::identity_conversion()), Reducer, raster::algorithm::detail::SourcePixel<Data>>
[[nodiscard]] auto reduce(Data&& data, Attribution&& attribution, unsigned halo_width, unsigned n_zoom_levels, const Reducer& reducer)
    -> Expected<std::pair<radix::Raster<raster::algorithm::detail::SourcePixel<Data>>, radix::Raster<std::uint16_t>>>
{
    return reduce(
        std::forward<Data>(data), std::forward<Attribution>(attribution), halo_width, n_zoom_levels, raster::algorithm::identity_conversion(), reducer);
}
} // namespace raster_store::scaler
