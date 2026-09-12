#include "planning.h"
#include "raster_store/StoreTraits.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace rf_builder::tiles::planning {
namespace {
    std::optional<Bounds> overlap(const Bounds& a, const Bounds& b)
    {
        Bounds result { glm::max(a.min, b.min), glm::min(a.max, b.max) };
        return result.min.x < result.max.x && result.min.y < result.max.y ? std::optional(result) : std::nullopt;
    }
} // namespace
Bounds Coverage::bounds(const run::Key& key)
{
    const double size = std::ldexp(1., -int(key.zoom_level));
    const glm::dvec2 origin = glm::dvec2(key.coords) * size;
    return { origin, origin + size };
}
Coverage::Coverage(const std::vector<Bounds>& mercator_bounds)
{
    const auto extent = RasterTransform::world_half_extent;
    for (const auto& box : mercator_bounds) {
        const Bounds normalized { { (box.min.x + extent) / (2 * extent), (extent - box.max.y) / (2 * extent) },
            { (box.max.x + extent) / (2 * extent), (extent - box.min.y) / (2 * extent) } };
        auto clipped = overlap(normalized, { { 0, 0 }, { 1, 1 } });
        if (!clipped) {
            continue;
        }
        std::vector<Bounds> pieces { *clipped };
        for (const auto& existing : m_rectangles) {
            std::vector<Bounds> remainder;
            for (const auto& piece : pieces) {
                const auto common = overlap(piece, existing);
                if (!common) {
                    remainder.push_back(piece);
                    continue;
                }
                const std::array<Bounds, 4> strips { { { piece.min, { common->min.x, piece.max.y } },
                    { { common->max.x, piece.min.y }, piece.max },
                    { { common->min.x, piece.min.y }, { common->max.x, common->min.y } },
                    { { common->min.x, common->max.y }, { common->max.x, piece.max.y } } } };
                for (const auto& strip : strips) {
                    if (strip.width() > 0 && strip.height() > 0) {
                        remainder.push_back(strip);
                    }
                }
            }
            pieces = std::move(remainder);
            if (pieces.empty()) {
                break;
            }
        }
        m_rectangles.insert(m_rectangles.end(), pieces.begin(), pieces.end());
    }
}
double Coverage::weight(const run::Key& key) const
{
    double area = 0;
    for (const auto& rectangle : m_rectangles) {
        if (auto common = overlap(bounds(key), rectangle)) {
            area += common->width() * common->height();
        }
    }
    return area;
}
bool Coverage::intersects(const run::Key& key, const run::Key& region) const
{
    const auto common = overlap(bounds(key), bounds(region));
    if (!common) {
        return false;
    }
    return std::ranges::any_of(m_rectangles, [&](const auto& rectangle) { return overlap(*common, rectangle).has_value(); });
}
run::Subdivide Coverage::children(const run::Key& key) const
{
    run::Subdivide result;
    if (auto children = raster_store::StoreTraits::children(key)) {
        for (const auto& child : *children) {
            if (weight(child) > 0) {
                result.children.push_back(child);
            }
        }
    }
    return result;
}
Cursor::Cursor(const Coverage& coverage, unsigned zoom, run::Key region)
    : m_coverage(coverage)
    , m_zoom(zoom)
    , m_region(region)
{
}
Expected<std::optional<run::Key>> Cursor::next(const run::Poll& poll)
{
    while (!m_pending.empty()) {
        if (poll) {
            if (auto checked = poll(); !checked) {
                return Error::propagate(std::move(checked));
            }
        }
        const auto key = m_pending.back();
        m_pending.pop_back();
        if (!m_coverage.intersects(key, m_region)) {
            continue;
        }
        if (key.zoom_level == m_zoom) {
            return std::optional(key);
        }
        auto children = raster_store::StoreTraits::children(key);
        if (!children) {
            return Error::fail(Error::Code::InvalidInput, "online root zoom exceeds supported tile keys");
        }
        m_pending.insert(m_pending.end(), children->rbegin(), children->rend());
    }
    return std::nullopt;
}
} // namespace rf_builder::tiles::planning
