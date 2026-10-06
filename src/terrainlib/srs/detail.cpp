/*
 * Adapted from GDAL's ogr/ogrct.cpp.
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (C) 2026 Adam Celarek-Litofcenko
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "detail.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include <cpl_error.h>
#include <cpl_string.h>
#include <glm/glm.hpp>
#include <ogr_spatialref.h>

#include "srs.h"

namespace srs::detail {

// Extraction of GDAL 3.10.3's TransformBounds with fixed performance: cache the
// auxiliary transformation used by the projected-output pole and seam checks.
// Geographic output and cases outside the fast path retain GDAL's implementation.
int ogr_transform_bounds(OGRCoordinateTransformation* transform,
    double xmin,
    double ymin,
    double xmax,
    double ymax,
    double* out_xmin,
    double* out_ymin,
    double* out_xmax,
    double* out_ymax,
    int densify_pts)
{
    const auto fallback = [&]() { return transform->TransformBounds(xmin, ymin, xmax, ymax, out_xmin, out_ymin, out_xmax, out_ymax, densify_pts); };
    const auto* source = transform->GetSourceCS();
    const auto* target = transform->GetTargetCS();
    // The cache normalizes axis mapping. Delegate other mappings/units, identity
    // transformations, and invalid arguments to preserve GDAL's own handling.
    if (!source || !target || !target->IsProjected() || source->GetAxisMappingStrategy() != OAMS_TRADITIONAL_GIS_ORDER
        || (!source->IsGeographic() && !source->IsProjected()) || source->IsSame(target)
        || (source->IsGeographic() && (std::abs(source->GetAngularUnits() - glm::radians(1.0)) >= 1e-8 || ymax < ymin)) || densify_pts < 0
        || densify_pts > 10000 || !std::isfinite(xmin) || !std::isfinite(ymin) || !std::isfinite(xmax) || !std::isfinite(ymax)) {
        return fallback();
    }

    CPLErrorReset();
    *out_xmin = *out_ymin = *out_xmax = *out_ymax = HUGE_VAL;
    const int side_points = densify_pts + 1;
    const double delta_x = (xmax - xmin + (source->IsGeographic() && xmax < xmin ? 360.0 : 0.0)) / side_points;
    const double delta_y = (ymax - ymin) / side_points;
    std::vector<glm::dvec2> boundary(4 * side_points);
    for (int i = 0; i < side_points; ++i) {
        boundary[i] = { xmin, ymax - i * delta_y };
        boundary[i + side_points] = { xmin + i * delta_x, ymin };
        boundary[i + 2 * side_points] = { xmax, ymin + i * delta_y };
        boundary[i + 3 * side_points] = { xmax - i * delta_x, ymax };
    }
    {
        CPLErrorHandlerPusher quiet(CPLQuietErrorHandler);
        // GDAL may accept a partially valid boundary; our batch helper requires
        // every point to succeed, so let GDAL handle those exceptional cases.
        if (!transform_points_inplace(transform, boundary)
            || std::ranges::any_of(boundary, [](const auto& point) { return !std::isfinite(point.x) || !std::isfinite(point.y); })) {
            return fallback();
        }
        CPLErrorReset();
    }
    const auto expand = [&](const glm::dvec2& point) {
        *out_xmin = (std::min)(*out_xmin, point.x);
        *out_ymin = (std::min)(*out_ymin, point.y);
        *out_xmax = *out_xmax == HUGE_VAL ? point.x : (std::max)(*out_xmax, point.x);
        *out_ymax = *out_ymax == HUGE_VAL ? point.y : (std::max)(*out_ymax, point.y);
    };
    for (const auto& point : boundary) {
        expand(point);
    }

    // Preserve GDAL's best-effort checks and suppress their transient errors.
    CPLErrorStateBackuper saved_error(CPLQuietErrorHandler);
    std::unique_ptr<OGRSpatialReference> geographic_target(target->CloneGeogCS());
    geographic_target->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    auto to_source = transformation(*geographic_target, *source);
    if (to_source) {
        const auto include = [&](glm::dvec2 point) {
            auto input = transform_point(to_source->get(), point);
            if (!input || !(input->x >= xmin && input->x <= xmax && input->y >= ymin && input->y <= ymax)) {
                return false;
            }
            auto output = transform_point(transform, *input);
            if (!output) {
                return false;
            }
            expand(*output);
            return true;
        };
        const double central_meridian = target->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
        double pole_latitude = 90;
        const char* projection = target->GetAttrValue("PROJECTION");
        if (projection && EQUAL(projection, SRS_PT_MERCATOR_1SP) && central_meridian == 0) {
            pole_latitude = webmercator_latitude_limit;
        }
        constexpr double epsilon = 1e-8;
        bool includes_pole = false;
        double signed_pole_latitude = 0;
        for (int sign : { -1, 1 }) {
            if (include({ central_meridian, sign * (pole_latitude - epsilon) })) {
                includes_pole = true;
                signed_pole_latitude = sign * pole_latitude;
            }
        }
        const auto include_seam = [&](double latitude) {
            for (int sign : { -1, 1 }) {
                include({ std::fmod(central_meridian + sign * (180 - epsilon) + 180, 360) - 180, latitude });
            }
        };
        const double latitude_of_origin = target->GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
        if (central_meridian != 0) {
            include_seam(latitude_of_origin);
        }
        if (includes_pole && latitude_of_origin != signed_pole_latitude) {
            include_seam(signed_pole_latitude);
        }
    }
    return *out_xmin != HUGE_VAL && *out_ymin != HUGE_VAL && *out_xmax != HUGE_VAL && *out_ymax != HUGE_VAL;
}

} // namespace srs::detail
