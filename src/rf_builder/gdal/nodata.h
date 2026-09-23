#pragma once

#include "DatasetReader.h"
#include "raster/ClampedView.h"
#include "raster/algorithm.h"
#include <array>
#include <limits>

namespace rf_builder::gdal::nodata {

// Validate halo/dimension arithmetic before opening inputs or planning tiles.
Expected<unsigned> halo(unsigned tile_side, unsigned search_radius, unsigned kernel_size);

struct Window {
    RasterTransform::Bounds bounds;
    glm::uvec2 size;
    glm::uvec2 interior_offset;
};
Window window(const radix::tile::Id& key, unsigned tile_side, unsigned halo_width);

class Processor {
public:
    static Expected<Processor> create(unsigned search_radius, unsigned kernel_size);

    template <typename PixelType>
    Expected<radix::Raster<PixelType>> process(
        const DatasetReader::Samples<PixelType>& samples, glm::uvec2 offset, unsigned side, const std::array<float, 3>& fallback)
    {
        radix::Raster<PixelType> result(side);
        auto completed = process(samples, offset, fallback, result);
        if (!completed) {
            return Error::propagate(std::move(completed));
        }
        return result;
    }

    template <typename PixelType>
    Expected<void> process(
        const DatasetReader::Samples<PixelType>& samples, glm::uvec2 offset, const std::array<float, 3>& fallback, radix::Raster<PixelType>& result)
    {
        const unsigned side = result.width();
        if (side == 0 || result.height() != side || samples.data.size() != samples.valid.size()) {
            return Error::fail(Error::Code::InvalidInput, "NoData processing requires matching input masks and a square output");
        }
        auto original = raster::make_view(samples.data, offset, glm::uvec2(side));
        auto valid = raster::make_view(samples.valid, offset, glm::uvec2(side));
        if (!original) {
            return Error::propagate(std::move(original));
        }
        if (!valid) {
            return Error::propagate(std::move(valid));
        }
        if (std::ranges::all_of(samples.valid.buffer(), [](auto value) { return value != 0; })) {
            if (auto copied = raster::algorithm::copy(*original, result); !copied) {
                return Error::propagate(std::move(copied));
            }
            return {};
        }
        resize(m_work, samples.data.size());
        constexpr unsigned channels = std::is_same_v<PixelType, float> ? 1 : 3;
        for (unsigned channel = 0; channel < channels; ++channel) {
            auto initialized = raster::algorithm::zip_transform(
                samples.data,
                samples.valid,
                [&](const PixelType& value, std::uint8_t source_valid) -> float {
                    if (!source_valid) {
                        return fallback[channel];
                    }
                    if constexpr (std::is_same_v<PixelType, float>) {
                        return value;
                    } else {
                        return value[channel];
                    }
                },
                m_work);
            if (!initialized) {
                return Error::propagate(std::move(initialized));
            }
            auto completed = complete(samples.valid, offset, side);
            if (!completed) {
                return Error::propagate(std::move(completed));
            }
            const auto output = raster::make_view(result);
            auto composed = raster::algorithm::zip_transform(
                std::tuple { *original, *valid, *completed, output },
                [&](const PixelType& value, std::uint8_t source_valid, float replacement, PixelType previous) -> PixelType {
                    if (source_valid) {
                        return value;
                    }
                    if constexpr (std::is_same_v<PixelType, float>) {
                        return replacement;
                    } else {
                        previous[channel] = std::get<1>(raster::algorithm::linear_conversion<std::uint8_t>())(replacement);
                        return previous;
                    }
                },
                result);
            if (!composed) {
                return Error::propagate(std::move(composed));
            }
        }
        return {};
    }

private:
    Processor(unsigned search_radius, std::vector<double> weights)
        : m_search_radius(search_radius)
        , m_weights(std::move(weights))
    {
    }
    static void resize(radix::Raster<float>& raster, glm::uvec2 size);
    Expected<raster::View<const float>> complete(const radix::Raster<std::uint8_t>& valid, glm::uvec2 offset, unsigned side);

    unsigned m_search_radius;
    std::vector<double> m_weights;
    radix::Raster<float> m_work;
    radix::Raster<float> m_horizontal;
    radix::Raster<float> m_smoothed;
};
} // namespace rf_builder::gdal::nodata
