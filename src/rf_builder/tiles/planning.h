#pragma once
#include "RasterTransform.h"
#include "run.h"
namespace rf_builder::tiles::planning {
using Bounds = RasterTransform::Bounds;
// Disjoint normalized Mercator rectangles, built once from conservative mask
// bounds. Their union supports both spatial pruning and additive area weights.
class Coverage {
public:
    explicit Coverage(const std::vector<Bounds>& mercator_bounds);
    double weight(const run::Key& key) const;
    bool intersects(const run::Key& key, const run::Key& region) const;
    run::Subdivide children(const run::Key& key) const;
    static Bounds bounds(const run::Key& key);

private:
    std::vector<Bounds> m_rectangles;
};
class Cursor {
public:
    Cursor(const Coverage& coverage, unsigned zoom, run::Key region = { 0, { 0, 0 } });
    Expected<std::optional<run::Key>> next(const run::Poll& poll = {});

private:
    const Coverage& m_coverage;
    unsigned m_zoom;
    run::Key m_region;
    std::vector<run::Key> m_pending { { 0, { 0, 0 } } };
};
} // namespace rf_builder::tiles::planning
