#include "Mask.h"
#include <numbers>
#include <numeric>
#include <CGAL/Arr_trapezoid_ric_point_location.h>

#include "Dataset.h"
#include "vector_mask.h"

namespace rf_builder {
namespace {
// A static BVH over conservative edge boxes. False positives only cause more
// point queries; a negative query proves that the box contains no mask boundary.
class BoundaryIndex {
public:
    explicit BoundaryIndex(std::vector<CGAL::Bbox_2> boxes) : m_boxes(std::move(boxes))
    {
        m_nodes.reserve(m_boxes.size() * 2);
        if (!m_boxes.empty()) { build(0, m_boxes.size()); }
    }

    bool intersects(const CGAL::Bbox_2& box) const { return !m_nodes.empty() && intersects(0, box); }

private:
    struct Node {
        CGAL::Bbox_2 box;
        std::size_t begin = 0, end = 0;
        std::size_t left = 0, right = 0;
    };

    std::size_t build(std::size_t begin, std::size_t end)
    {
        auto box = m_boxes[begin];
        for (auto i = begin + 1; i < end; ++i) { box = box + m_boxes[i]; }
        const auto node = m_nodes.size();
        m_nodes.push_back({ .box = box });
        if (end - begin <= 8) {
            m_nodes[node].begin = begin;
            m_nodes[node].end = end;
        } else {
            const bool split_x = box.xmax() - box.xmin() > box.ymax() - box.ymin();
            const auto middle = begin + (end - begin) / 2;
            std::nth_element(m_boxes.begin() + begin, m_boxes.begin() + middle, m_boxes.begin() + end,
                [split_x](const auto& a, const auto& b) {
                    return split_x ? std::midpoint(a.xmin(), a.xmax()) < std::midpoint(b.xmin(), b.xmax())
                                   : std::midpoint(a.ymin(), a.ymax()) < std::midpoint(b.ymin(), b.ymax());
                });
            const auto left = build(begin, middle);
            const auto right = build(middle, end);
            m_nodes[node].left = left;
            m_nodes[node].right = right;
        }
        return node;
    }

    bool intersects(std::size_t index, const CGAL::Bbox_2& box) const
    {
        const auto& node = m_nodes[index];
        if (!CGAL::do_overlap(node.box, box)) { return false; }
        if (node.begin != node.end) {
            for (auto i = node.begin; i < node.end; ++i) {
                if (CGAL::do_overlap(m_boxes[i], box)) { return true; }
            }
            return false;
        }
        return intersects(node.left, box) || intersects(node.right, box);
    }

    std::vector<CGAL::Bbox_2> m_boxes;
    std::vector<Node> m_nodes;
};
}

struct Mask::Data {
    using Arrangement = vector_mask::PolygonSet2::Arrangement_2;
    vector_mask::PolygonSet2 polygons;
    // The locator observes the arrangement: destroy it before the polygon set.
    std::unique_ptr<CGAL::Arr_trapezoid_ric_point_location<Arrangement>> location;
    std::unique_ptr<BoundaryIndex> boundary;
    std::unique_ptr<OGRCoordinateTransformation> transform;
    std::vector<RasterTransform::Bounds> bounds;

    bool contains(const vector_mask::Point2& point) const
    {
        const auto found = location->locate(point);
        if (const auto* face = std::get_if<Arrangement::Face_const_handle>(&found)) {
            return (*face)->contained();
        }
        // Union boundary edges and vertices belong to the mask, including hole rims.
        return true;
    }

    void select(std::span<const double> x, std::span<const double> y, std::span<std::uint8_t> validity) const
    {
        const auto first = std::ranges::find_if(validity, [](auto value) { return value != 0; });
        if (first == validity.end()) { return; }
        if (validity.size() <= 8) {
            for (std::size_t i = 0; i < validity.size(); ++i) {
                if (validity[i]) { validity[i] = contains(vector_mask::Point2(x[i], y[i])); }
            }
            return;
        }
        const auto first_index = std::size_t(first - validity.begin());
        auto box = CGAL::Bbox_2(x[first_index], y[first_index], x[first_index], y[first_index]);
        for (auto i = first_index + 1; i < validity.size(); ++i) {
            if (validity[i]) { box = box + CGAL::Bbox_2(x[i], y[i], x[i], y[i]); }
        }
        // These are the actual transformed centres, so the enclosure remains
        // valid for curved projections and arbitrary point orderings.
        if (!boundary->intersects(box)) {
            const bool inside = contains(vector_mask::Point2(x[first_index], y[first_index]));
            for (auto& value : validity) {
                if (value) { value = inside; }
            }
            return;
        }
        const auto middle = validity.size() / 2;
        select(x.first(middle), y.first(middle), validity.first(middle));
        select(x.subspan(middle), y.subspan(middle), validity.subspan(middle));
    }
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
    auto& reference = loaded->srs;
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
    for (const auto& polygon : loaded->polygons.polygons_with_holes()) {
        const auto box = polygon.bbox();
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
    const auto& polygons = loaded->polygons.polygons_with_holes();
    data->polygons.join(polygons.begin(), polygons.end());
    data->location = std::make_unique<CGAL::Arr_trapezoid_ric_point_location<Data::Arrangement>>(data->polygons.arrangement());
    std::vector<CGAL::Bbox_2> edges;
    const auto& arrangement = data->polygons.arrangement();
    edges.reserve(arrangement.number_of_edges());
    for (auto edge = arrangement.edges_begin(); edge != arrangement.edges_end(); ++edge) {
        edges.push_back(edge->curve().bbox());
    }
    data->boundary = std::make_unique<BoundaryIndex>(std::move(edges));
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
    if (std::ranges::none_of(validity, [](auto value) { return value != 0; })) { return {}; }
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
    }
    m_data->select(x, y, validity);
    return {};
}

const std::vector<RasterTransform::Bounds>& Mask::bounds() const { return m_data->bounds; }

} // namespace rf_builder
