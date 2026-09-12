#pragma once

#include "RasterTransform.h"
#include <functional>
#include <optional>

namespace rf_builder::gdal::planning {

inline constexpr double sampling_limit = 1.25;
using Bounds = RasterTransform::Bounds;
using Transform = std::function<Expected<glm::dvec2>(glm::dvec2)>;
using Visit = std::function<Expected<void>(const radix::tile::Id&)>;

class Cursor {
public:
    Cursor(unsigned side, const std::vector<Bounds>& source_bounds, const std::vector<Bounds>& mask_bounds, Transform transform);
    Expected<std::optional<radix::tile::Id>> next(const std::function<Expected<void>()>& poll = {});

private:
    unsigned m_side;
    Transform m_transform;
    std::vector<Bounds> m_coverage;
    std::vector<radix::tile::Id> m_pending;
};

// Largest singular value of the Jacobian whose columns are the two vectors.
double directional_stretch(glm::dvec2 column, glm::dvec2 row);
Expected<double> estimate(const Bounds& region, double pixel_spacing, const Transform& transform);
Expected<void> traverse(unsigned side, const std::vector<Bounds>& source_bounds,
    const std::vector<Bounds>& mask_bounds, const Transform& transform, const Visit& visit,
    const std::function<Expected<void>()>& checkpoint = {});

} // namespace rf_builder::gdal::planning
