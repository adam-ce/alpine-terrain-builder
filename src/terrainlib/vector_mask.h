#pragma once

#include <expected>
#include <memory>
#include <optional>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <CGAL/Multipolygon_with_holes_2.h>
#include <CGAL/Polygon_set_2.h>
#include "Dataset.h"
#include "mesh/cgal.h"
#include "srs.h"

namespace vector_mask {

using Kernel = cgal::kernel::epeck::Kernel;
using Point2 = Kernel::Point_2;
using Polygon2 = CGAL::Polygon_2<Kernel>;
using PolygonWithHoles2 = CGAL::Polygon_with_holes_2<Kernel>;
using PolygonSet2 = CGAL::Polygon_set_2<Kernel>;
using MultipolygonWithHoles2 = CGAL::Multipolygon_with_holes_2<Kernel>;

struct ReferencedPolygonMask {
    MultipolygonWithHoles2 polygons;
    OGRSpatialReference srs;
};

enum class LoadErrorKind {
    UnsupportedFormat,
    FileNotFound,
    EmptySource,
    InvalidGeometry,
    UnsupportedSpatialReference,
    ReadFailure,
};

class LoadError {
public:
    LoadError() = default;
    constexpr LoadError(LoadErrorKind kind)
        : kind(kind) {}

    operator LoadErrorKind() const {
        return this->kind;
    }
    constexpr bool operator==(LoadError other) const {
        return this->kind == other.kind;
    }
    constexpr bool operator!=(LoadError other) const {
        return this->kind != other.kind;
    }

    std::string description() const {
        switch (kind) {
        case LoadErrorKind::UnsupportedFormat:
            return "format not supported";
        case LoadErrorKind::FileNotFound:
            return "file not found";
        case LoadErrorKind::EmptySource:
            return "empty input source";
        case LoadErrorKind::InvalidGeometry:
            return "invalid geometry";
        case LoadErrorKind::ReadFailure:
            return "feature read failure";
        case LoadErrorKind::UnsupportedSpatialReference:
            return "unsupported spatial reference";
        default:
            return "unknown error";
        }
    }

    friend std::ostream &operator<<(std::ostream &os, const LoadError &err) {
        return os << err.description();
    }

private:
    LoadErrorKind kind;
};

inline constexpr double simplification_tolerance_metres = 0.1;

inline std::optional<double> simplification_tolerance(const OGRSpatialReference &srs) {
    if (srs.IsProjected()) {
        const double metres_per_unit = srs.GetLinearUnits();
        if (metres_per_unit > 0) {
            return simplification_tolerance_metres / metres_per_unit;
        }
    }

    if (srs.IsGeographic()) {
        OGRErr error = OGRERR_NONE;
        const double semi_major_axis_metres = srs.GetSemiMajor(&error);
        const double radians_per_unit = srs.GetAngularUnits();
        if (error == OGRERR_NONE && semi_major_axis_metres > 0 && radians_per_unit > 0) {
            return simplification_tolerance_metres / semi_major_axis_metres / radians_per_unit;
        }
    }

    return std::nullopt;
}

inline uint64_t point_count(const OGRGeometry &geometry) {
    const OGRwkbGeometryType geometry_type = wkbFlatten(geometry.getGeometryType());
    if (geometry_type == wkbPolygon) {
        const OGRPolygon *polygon = geometry.toPolygon();
        const OGRLinearRing *exterior_ring = polygon->getExteriorRing();
        uint64_t count = exterior_ring ? exterior_ring->getNumPoints() : 0;
        for (int i = 0; i < polygon->getNumInteriorRings(); ++i) {
            count += polygon->getInteriorRing(i)->getNumPoints();
        }
        return count;
    }

    if (geometry_type == wkbMultiPolygon || geometry_type == wkbGeometryCollection) {
        const OGRGeometryCollection *collection = geometry.toGeometryCollection();
        uint64_t count = 0;
        for (int i = 0; i < collection->getNumGeometries(); ++i) {
            count += point_count(*collection->getGeometryRef(i));
        }
        return count;
    }

    return 0;
}

inline std::unique_ptr<OGRGeometry> simplify_geometry(
    const OGRGeometry &geometry,
    const double tolerance
) {
    return std::unique_ptr<OGRGeometry>(geometry.SimplifyPreserveTopology(tolerance));
}

inline std::optional<Polygon2> convert_ring(const OGRLinearRing &ring, bool is_outer) {
    uint32_t num_points = ring.getNumPoints();
    if (ring.get_IsClosed()) {
        num_points--;
    }
    if (num_points < 3) {
        return std::nullopt;
    }

    Polygon2 polygon;
    for (uint32_t i = 0; i < num_points; i++) {
        polygon.push_back(Point2(ring.getX(i), ring.getY(i)));
    }

    if (!polygon.is_simple()) {
        // Contains self intersections or duplicate points
        LOG_WARN("Skipping non-simple polygon");
        return std::nullopt;
    }

    if (is_outer != polygon.is_counterclockwise_oriented()) {
        polygon.reverse_orientation();
    }

    return polygon;
}

inline std::optional<PolygonWithHoles2> convert_polygon(const OGRPolygon &ogr_polygon) {
    const OGRLinearRing *outer_ring = ogr_polygon.getExteriorRing();
    DEBUG_ASSERT(outer_ring);

    auto outer_opt = convert_ring(*outer_ring, true);
    if (!outer_opt) {
        return std::nullopt;
    }
    const Polygon2 outer = std::move(*outer_opt);

    PolygonWithHoles2 polygon(outer);
    const uint32_t num_holes = static_cast<uint32_t>(ogr_polygon.getNumInteriorRings());
    for (uint32_t i = 0; i < num_holes; i++) {
        const OGRLinearRing *inner = ogr_polygon.getInteriorRing(i);
        DEBUG_ASSERT(inner);
        auto hole_opt = convert_ring(*inner, false);
        if (!hole_opt) {
            continue;
        }
        Polygon2 hole = std::move(*hole_opt);
        polygon.add_hole(std::move(hole));
    }

    return polygon;
}

inline void process_geometry(const OGRGeometry &geometry, MultipolygonWithHoles2 &out) {
    const OGRwkbGeometryType geometry_type = wkbFlatten(geometry.getGeometryType()); // map 2.5d to 2d

    switch (geometry_type) {
    case wkbPolygon: {
        const OGRPolygon *polygon = geometry.toPolygon();
        DEBUG_ASSERT(polygon);
        if (auto result = convert_polygon(*polygon)) {
            out.add_polygon_with_holes(std::move(*result));
        }
        break;
    }

    case wkbMultiPolygon:
    case wkbGeometryCollection: {
        const OGRGeometryCollection *collection = geometry.toGeometryCollection();
        DEBUG_ASSERT(collection);
        const uint32_t num_children = collection->getNumGeometries();
        for (uint32_t i = 0; i < num_children; i++) {
            process_geometry(*collection->getGeometryRef(i), out);
        }
        break;
    }

    default:
        LOG_WARN("Skipping unsupported geometry type: {}", OGRGeometryTypeToName(geometry_type));
        break;
    }
}

inline std::expected<ReferencedPolygonMask, LoadError> load_referenced_from_dataset(Dataset& mask_dataset) {
    GDALDataset *dataset = mask_dataset.gdalDataset();

    OGRSpatialReference srs;
    // TODO: remove this try catch
    try {
        srs = mask_dataset.srs();
    } catch (std::runtime_error &e) {
        LOG_WARN("Mask does not reference an srs, assuming WGS84");
        srs = srs::wgs84();
        // srs.SetAxisMappingStrategy(OAMS_AUTHORITY_COMPLIANT);
    }

    const std::optional<double> tolerance = simplification_tolerance(srs);
    if (!tolerance) {
        LOG_ERROR("Cannot express the {} m mask simplification tolerance in the source SRS",
            simplification_tolerance_metres);
        return std::unexpected(LoadErrorKind::UnsupportedSpatialReference);
    }

    MultipolygonWithHoles2 polygons;
    CPLErrorReset();
    for (auto &&feature_layer_pair : dataset->GetFeatures()) {
        OGRGeometry *geometry = feature_layer_pair.feature->GetGeometryRef();
        if (!geometry) {
            LOG_ERROR("Mask feature has no geometry");
            return std::unexpected(LoadErrorKind::InvalidGeometry);
        }

        const uint64_t original_point_count = point_count(*geometry);
        std::unique_ptr<OGRGeometry> simplified = simplify_geometry(*geometry, *tolerance);
        if (!simplified || simplified->IsEmpty() || !simplified->IsValid()) {
            LOG_ERROR("Failed to simplify mask geometry while preserving its topology");
            return std::unexpected(LoadErrorKind::InvalidGeometry);
        }

        LOG_DEBUG("Simplified mask with {} m tolerance from {} to {} points",
            simplification_tolerance_metres, original_point_count, point_count(*simplified));
        process_geometry(*simplified, polygons);
    }

    if (CPLGetLastErrorType() >= CE_Failure) {
        return std::unexpected(LoadErrorKind::ReadFailure);
    }

    if (polygons.is_empty()) {
        LOG_ERROR("No valid polygons found in mask dataset '{}'", mask_dataset.name());
        return std::unexpected(LoadErrorKind::EmptySource);
    }

    return ReferencedPolygonMask{.polygons = std::move(polygons), .srs = std::move(srs)};
}


} // namespace vector_mask
