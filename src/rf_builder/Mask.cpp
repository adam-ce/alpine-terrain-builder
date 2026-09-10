#include "Mask.h"
#include <numbers>

#include "Dataset.h"
#include "vector_mask.h"

namespace rf_builder {

struct Mask::Data {
    vector_mask::ReferencedPolygonMask mask;
    std::unique_ptr<OGRCoordinateTransformation> transform;
    std::vector<RasterTransform::Bounds> bounds;
    std::vector<CGAL::Bbox_2> polygon_bounds;
};

Mask::Mask(std::unique_ptr<Data> data) : m_data(std::move(data)) { }
Mask::~Mask() = default;
Mask::Mask(Mask&&) noexcept = default;
Mask& Mask::operator=(Mask&&) noexcept = default;

Expected<Mask> Mask::open(const std::string& identifier)
{
    auto dataset = Dataset::open_vector(identifier);
    if (!dataset) {
        return Error::fail(Error::Code::InvalidInput, "open RF vector mask", identifier);
    }
    auto loaded = vector_mask::load_referenced_from_dataset(*dataset);
    if (!loaded) {
        return Error::fail(Error::Code::InvalidInput, "load RF mask: " + loaded.error().description(), identifier);
    }
    auto data = std::make_unique<Data>();
    data->mask = std::move(*loaded);
    auto& reference = data->mask.srs;
    // The common loader retains mesh compatibility. RF additionally rejects
    // mixed layer CRSs rather than interpreting coordinates in the first CRS.
    for (int layer = 0; layer < dataset->gdalDataset()->GetLayerCount(); ++layer) {
        const auto* layer_reference = dataset->gdalDataset()->GetLayer(layer)->GetSpatialRef();
        if (layer_reference) {
            auto normalized = *layer_reference;
            normalized.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if (!reference.IsSame(&normalized)) {
                return Error::fail(Error::Code::Unsupported, "RF mask layers must use the same CRS", identifier);
            }
        }
    }
    for (const auto& polygon : data->mask.polygons.polygons_with_holes()) {
        const auto box = polygon.bbox();
        data->polygon_bounds.push_back(box);
        if (reference.IsGeographic()) {
            const double half_period = std::numbers::pi / reference.GetAngularUnits();
            if (box.xmin() < -half_period || box.xmax() > half_period) {
                return Error::fail(Error::Code::InvalidInput, "split RF mask polygons at the antimeridian; out-of-range longitudes are unsupported", identifier);
            }
        }
        auto bounds = RasterTransform::coverage(reference, { { box.xmin(), box.ymin() }, { box.xmax(), box.ymax() } });
        if (!bounds) {
            return Error::propagate(std::move(bounds), "compute RF mask coverage");
        }
        data->bounds.insert(data->bounds.end(), bounds->begin(), bounds->end());
    }
    OGRSpatialReference mercator;
    mercator.importFromEPSG(3857);
    mercator.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    data->transform.reset(OGRCreateCoordinateTransformation(&mercator, &reference));
    if (!data->transform) {
        return Error::fail(Error::Code::InvalidInput, "create RF pixel-centre mask transformation");
    }
    return Mask(std::move(data));
}

Expected<void> Mask::select(const std::span<const glm::dvec2> centres, const std::span<std::uint8_t> validity) const
{
    if (centres.size() != validity.size()) {
        return Error::fail(Error::Code::Internal, "mask coordinates and validity dimensions disagree");
    }
    std::vector<double> x(centres.size()), y(centres.size());
    std::vector<int> success(centres.size());
    for (std::size_t i = 0; i < centres.size(); ++i) {
        x[i] = centres[i].x;
        y[i] = centres[i].y;
    }
    m_data->transform->Transform(int(centres.size()), x.data(), y.data(), nullptr, success.data());
    for (std::size_t i = 0; i < centres.size(); ++i) {
        if (!validity[i]) {
            continue;
        }
        if (!success[i] || !std::isfinite(x[i]) || !std::isfinite(y[i])) {
            const bool in_coverage = std::ranges::any_of(m_data->bounds, [&](const auto& bounds) {
                return centres[i].x >= bounds.min.x && centres[i].x <= bounds.max.x
                    && centres[i].y >= bounds.min.y && centres[i].y <= bounds.max.y;
            });
            if (!in_coverage) { validity[i] = 0; continue; }
            return Error::fail(Error::Code::InvalidInput, "transform valid RF pixel centre into mask CRS");
        }
        bool accepted = false;
        std::size_t polygon_index = 0;
        for (const auto& polygon : m_data->mask.polygons.polygons_with_holes()) {
            const auto& box = m_data->polygon_bounds[polygon_index++];
            if (x[i] < box.xmin() || x[i] > box.xmax() || y[i] < box.ymin() || y[i] > box.ymax()) {
                continue;
            }
            const vector_mask::Point2 point(x[i], y[i]);
            if (CGAL::bounded_side_2(polygon.outer_boundary().begin(), polygon.outer_boundary().end(), point) == CGAL::ON_UNBOUNDED_SIDE) {
                continue;
            }
            bool inside_hole = false;
            for (const auto& hole : polygon.holes()) {
                inside_hole = inside_hole || CGAL::bounded_side_2(hole.begin(), hole.end(), point) == CGAL::ON_BOUNDED_SIDE;
            }
            if (!inside_hole) {
                accepted = true;
                break;
            }
        }
        validity[i] = accepted;
    }
    return {};
}

const std::vector<RasterTransform::Bounds>& Mask::bounds() const { return m_data->bounds; }

} // namespace rf_builder
