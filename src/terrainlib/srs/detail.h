// Copyright (C) 2026 Adam Celarek-Litofcenko
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class OGRCoordinateTransformation;

namespace srs::detail {

// Drop-in replacement for OGRCoordinateTransformation::TransformBounds.
int ogr_transform_bounds(OGRCoordinateTransformation* transform,
    double xmin,
    double ymin,
    double xmax,
    double ymax,
    double* out_xmin,
    double* out_ymin,
    double* out_xmax,
    double* out_ymax,
    int densify_pts);

} // namespace srs::detail
