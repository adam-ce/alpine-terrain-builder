#pragma once

#include <functional>
#include "RasterTransform.h"

namespace rf_builder::planning {

inline constexpr double sampling_limit = 1.25;
using Bounds = RasterTransform::Bounds;
using Transform = std::function<Expected<glm::dvec2>(glm::dvec2)>;
using Visit = std::function<Expected<void>(const radix::tile::Id&)>;

// Largest singular value of the Jacobian whose columns are the two vectors.
double directional_stretch(glm::dvec2 column, glm::dvec2 row);
Expected<double> estimate(const Bounds& region, double pixel_spacing, const Transform& transform);
Expected<void> traverse(unsigned side, const std::vector<Bounds>& source_bounds,
    const std::vector<Bounds>& mask_bounds, const Transform& transform, const Visit& visit,
    const std::function<Expected<void>()>& checkpoint = {});

} // namespace rf_builder::planning
