#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>

#include <cpl_error.h>
#include <fmt/core.h>
#include <gdal.h>
#include <gdal_priv.h>
#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "log.h"
#include <radix/raster.h>

namespace terrainbuilder {

class RawDatasetReader {
public:
    RawDatasetReader(Dataset &target_dataset)
        : RawDatasetReader(target_dataset.gdalDataset()) {}
    RawDatasetReader(GDALDataset *target_dataset)
        : dataset(target_dataset) {
        if (!dataset) {
            throw std::invalid_argument("Invalid GDAL dataset provided");
        }

        if (dataset->GetGeoTransform(this->geo_transform.data()) != CE_None) {
            throw std::runtime_error("Failed to retrieve the GeoTransform");
        }

        if (GDALInvGeoTransform(this->geo_transform.data(), this->inv_geo_transform.data()) != TRUE) {
            throw std::runtime_error("Failed to invert the GeoTransform");
        }
    }

    GDALDataset *gdal_dataset() {
        return this->dataset;
    }

    double get_no_data_value() {
        DEBUG_ASSERT(this->dataset->GetRasterCount() >= 1);
        GDALRasterBand *height_band = this->dataset->GetRasterBand(1);
        return height_band->GetNoDataValue();
    }

    glm::uvec2 dataset_size() {
        GDALRasterBand *heights_band = this->dataset->GetRasterBand(1); // non-owning pointer
        return glm::uvec2(heights_band->GetXSize(), heights_band->GetYSize());
    }

    // TODO: support reading other data types
    std::optional<radix::Raster<float>> read_data_in_pixel_bounds(const radix::geometry::Aabb2i& bounds) {
        DEBUG_ASSERT(glm::all(glm::greaterThanEqual(bounds.min, glm::ivec2(0))));
        DEBUG_ASSERT(glm::all(glm::greaterThanEqual(bounds.max, glm::ivec2(0))));
        DEBUG_ASSERT(glm::all(glm::lessThan(bounds.min, glm::ivec2(this->dataset_size()))));
        DEBUG_ASSERT(glm::all(glm::lessThan(bounds.max, glm::ivec2(this->dataset_size()))));

        DEBUG_ASSERT(this->dataset->GetRasterCount() >= 1);
        GDALRasterBand *height_band = this->dataset->GetRasterBand(1); // non-owning pointer

        // Initialize the HeightData for reading
        radix::Raster<float> height_data(glm::uvec2(bounds.width(), bounds.height()));
        if (bounds.width() == 0 || bounds.height() == 0) {
            LOG_WARN("Target dataset bounds are empty");
            return height_data;
        }

        // Read data from the heights band into heights_data
        const int32_t read_result = height_band->RasterIO(
            GF_Read, bounds.min.x, bounds.min.y, bounds.width(), bounds.height(),
            static_cast<void*>(height_data.buffer().data()), bounds.width(), bounds.height(), GDT_Float32, 0, 0);

        if (read_result != CE_None) {
            const char * message = CPLGetLastErrorMsg();
            LOG_ERROR("Failed to read height data from {}: [{}] {}", dataset->GetDescription(), read_result, message);
            return std::nullopt;
        }

        return height_data;
    }
    std::optional<radix::Raster<float>> read_data_in_pixel_bounds_clamped(radix::geometry::Aabb2i &bounds) {
        const auto original_bounds = bounds;

        const glm::ivec2 max_in_bounds = glm::ivec2(this->dataset_size()) - glm::ivec2(1);
        bounds.min = glm::clamp(bounds.min, glm::ivec2(0), max_in_bounds);
        bounds.max = glm::clamp(bounds.max, bounds.min, max_in_bounds);

        if (original_bounds != bounds) {
            LOG_TRACE("Clamped target dataset bounds from [({}, {})-({}, {})] to [({}, {})-({}, {})]",
                        original_bounds.min.x, original_bounds.min.y, original_bounds.max.x, original_bounds.max.y,
                        bounds.min.x, bounds.min.y, bounds.max.x, bounds.max.y);
        }

        if (bounds.width() == 0 || bounds.height() == 0) {
            LOG_WARN("Target dataset bounds are empty (clamped)");
            return radix::Raster<float>();
        }

        return this->read_data_in_pixel_bounds(bounds);
    }

    std::optional<radix::Raster<float>> read_data_in_srs_bounds(const radix::tile::SrsBounds &bounds) {
        // Transform the SrsBounds to pixel space
        radix::geometry::Aabb2i pixel_bounds = this->transform_srs_bounds_to_pixel_bounds(bounds);

        // Use the transformed pixel bounds to read data
        return this->read_data_in_pixel_bounds(pixel_bounds);
    }

    radix::geometry::Aabb2i transform_srs_bounds_to_pixel_bounds(const radix::tile::SrsBounds &bounds) const {
        radix::geometry::Aabb2d pixel_bounds_exact = this->transform_srs_bounds_to_pixel_bounds_exact(bounds);

        const radix::geometry::Aabb2i pixel_bounds(
            glm::ivec2(static_cast<int32_t>(std::floor(pixel_bounds_exact.min.x)), static_cast<int32_t>(std::floor(pixel_bounds_exact.min.y))),
            glm::ivec2(static_cast<int32_t>(std::ceil(pixel_bounds_exact.max.x)), static_cast<int32_t>(std::ceil(pixel_bounds_exact.max.y))));

        return pixel_bounds;
    }

    radix::tile::SrsBounds transform_srs_bounds_to_pixel_bounds_exact(const radix::tile::SrsBounds &bounds) const {
        radix::tile::SrsBounds transformed_bounds;
        transformed_bounds.min = this->transform_srs_point_to_pixel_exact(bounds.min);
        transformed_bounds.max = this->transform_srs_point_to_pixel_exact(bounds.max);
        transformed_bounds = radix::tile::SrsBounds(glm::min(transformed_bounds.min, transformed_bounds.max),
                                                    glm::max(transformed_bounds.min, transformed_bounds.max));
        return transformed_bounds;
    }

    radix::tile::SrsBounds transform_pixel_bounds_to_srs_bounds(const radix::tile::SrsBounds &bounds) const {
        radix::tile::SrsBounds transformed_bounds;
        transformed_bounds.min = this->transform_pixel_to_srs_point(bounds.min);
        transformed_bounds.max = this->transform_pixel_to_srs_point(bounds.max);
        transformed_bounds = radix::tile::SrsBounds(glm::min(transformed_bounds.min, transformed_bounds.max),
                                                    glm::max(transformed_bounds.min, transformed_bounds.max));
        return transformed_bounds;
    }
    radix::tile::SrsBounds transform_pixel_bounds_to_srs_bounds(const radix::geometry::Aabb2i &bounds) const {
        return this->transform_pixel_bounds_to_srs_bounds(radix::tile::SrsBounds(glm::dvec2(bounds.min), glm::dvec2(bounds.max)));
    }

    template <typename T>
    inline glm::ivec2 transform_srs_point_to_pixel(const glm::tvec2<T> &p) const {
        glm::dvec2 pixel_exact = this->transform_srs_point_to_pixel_exact(p);
        const glm::ivec2 pixel(std::rint(pixel_exact.x), std::rint(pixel_exact.y));
        return pixel;
    }

    template <typename T>
    inline glm::dvec2 transform_srs_point_to_pixel_exact(const glm::tvec2<T> &p) const {
        glm::dvec2 result;
        GDALApplyGeoTransform(this->inv_geo_transform.data(), p.x, p.y, &result.x, &result.y);
        return result;
    }

    template <typename T>
    inline glm::dvec2 transform_pixel_to_srs_point(const glm::tvec2<T> &p) const {
        glm::dvec2 result;
        GDALApplyGeoTransform(this->geo_transform.data(), p.x, p.y, &result.x, &result.y);
        return result;
    }

private:
    GDALDataset *dataset;
    mutable std::array<double, 6> geo_transform;     // transform from pixel space to source srs.
    mutable std::array<double, 6> inv_geo_transform; // transform from source srs to pixel space.
};

}
