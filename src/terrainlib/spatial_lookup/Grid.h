#pragma once

#include <vector>
#include <optional>
#include <span>

#include <glm/common.hpp>
#include <glm/gtx/component_wise.hpp>
#include <libassert/assert.hpp>

#include "log.h"
#include "mesh/SimpleMesh.h"
#include "spatial_lookup/NDLoopHelper.h"
#include "spatial_lookup/SpatialLookup.h"
#include "spatial_lookup/SpatialLookupExt.h"

namespace spatial_lookup {

template <glm::length_t n_dims, typename Component, typename Value>
class Grid : public SpatialLookup<n_dims, Component, Value> {
public:
    using Self = Grid<n_dims, Component, Value>;
    using Base = SpatialLookup<n_dims, Component, Value>;
    using Vec = glm::vec<n_dims, Component>;
    using CellIndex = glm::vec<n_dims, uint32_t>;
    using CellOffset = glm::vec<n_dims, int32_t>;

    Grid(const Vec origin, const Vec size, const CellIndex divisions)
        : origin(origin), size(size), divisions(divisions) {
        this->_point_count = 0;
        this->_data.resize(glm::compMul(divisions));
    }

    struct CellItem {                           
        Vec point;
        Value value;
    };

    struct Cell {
        std::vector<CellItem> items;
    };

    void clear() override {
        this->_data.clear();
        this->_point_count = 0;
    }

    void insert(const Vec &point, const Value value) override {
        if (!this->is_in_bounds(point)) {
            return;
        }

        const size_t cell_index = this->calculate_cell_index(point);

        const CellItem item{
            .point = point,
            .value = value};
        Cell &cell = this->_data[cell_index];
        cell.items.push_back(item);
        this->_point_count += 1;

        if (cell.items.size() >= 1000) {
            LOG_WARN("Large number of points inside single grid cell: {} ({} overall)", cell.items.size(), this->_point_count);
        }
    }

    template <typename Distance, typename Func>
    void for_all_near(const Vec &point, const Distance epsilon, Func &&func) {
        const_cast<const Self *>(this)->for_all_near<Distance>(point, epsilon, [&](const Vec &vec, const Value &value, const Distance distance_sq) {
            func(vec, const_cast<Value &>(value), distance_sq);
        });
    }
    template <typename Distance, typename Func>
    void for_all_near(const Vec &point, const Distance epsilon, Func &&func) const {
        DEBUG_ASSERT(epsilon > 0);

        if (!this->is_in_bounds(point)) {
            return;
        }

        const CellIndex grid_index = this->calculate_grid_index(point);
        const Vec cell_size = this->cell_size();
        uint32_t cell_radius;

        const Vec relative_cell_point = point - cell_size * Vec(grid_index);
        DEBUG_ASSERT(glm::all(glm::greaterThanEqual(relative_cell_point, Vec(0))));
        DEBUG_ASSERT(glm::all(glm::lessThan(relative_cell_point, cell_size)));
        const Vec distance_from_cell_bounds = glm::min(relative_cell_point, cell_size - relative_cell_point);
        DEBUG_ASSERT(glm::all(glm::greaterThanEqual(distance_from_cell_bounds, Vec(0))));
        DEBUG_ASSERT(glm::all(glm::lessThan(distance_from_cell_bounds * Component(2), cell_size)));
        if (glm::all(glm::greaterThanEqual(distance_from_cell_bounds, Vec(epsilon)))) {
            cell_radius = 0;
        } else {
            const Component max_cell_size = glm::compMax(cell_size);
            cell_radius = std::ceil(epsilon / max_cell_size);
            if (cell_radius > 1) {
                LOG_WARN("Grid lookup epsilon ({}) is high compared to cell size {} resulting in cell radius of {}",
                         epsilon, cell_size, cell_radius);
            }
        }

        const Distance epsilon_sq = epsilon * epsilon;
        CellOffset offset;

        NDLoopHelper<n_dims>::for_each_offset(cell_radius, offset, [&](const CellOffset &offset) {
            const CellOffset _neighbor_index = CellOffset(grid_index) + offset;
            if (!this->is_valid_grid_index(_neighbor_index)) {
                return;
            }

            const CellIndex neighbor_index(_neighbor_index);
            const size_t cell_index = this->calculate_cell_index(neighbor_index);
            const Cell &cell = this->_data[cell_index];

            for (const CellItem &item : cell.items) {
                const Distance distance_sq = helpers::distance_sq(point, item.point);
                if (distance_sq < epsilon_sq) {
                    func(item.point, item.value, distance_sq);
                }
            }
        });
    }
    void for_all_near(const Vec &point, const Component epsilon, typename Base::ForAllNear func) override {
        this->for_all_near<Component, typename Base::ForAllNear>(point, epsilon, std::move(func));
    }
    void for_all_near(const Vec &point, const Component epsilon, typename Base::ForAllNearConst func) const override {
        this->for_all_near<Component, typename Base::ForAllNearConst>(point, epsilon, std::move(func));
    }

    template <typename Distance>
    std::optional<std::reference_wrapper<Value>> find_nearest(const Vec &point, const Distance epsilon) {
        return helpers::find_nearest<Self, Vec, Distance, Value>(*this, point, epsilon);
    }
    template <typename Distance>
    std::optional<std::reference_wrapper<const Value>> find_nearest(const Vec &point, const Distance epsilon) const {
        return helpers::find_nearest<const Self, Vec, Distance, const Value>(*this, point, epsilon);
    }
    std::optional<std::reference_wrapper<Value>> find_nearest(const Vec &point, const Component epsilon) override {
        return this->find_nearest<Component>(point, epsilon);
    }
    std::optional<std::reference_wrapper<const Value>> find_nearest(const Vec &point, const Component epsilon) const override {
        return this->find_nearest<Component>(point, epsilon);
    }

    template <typename Distance, typename Vector>
    bool find_all_near(const Vec &point, const Distance epsilon, Vector &out) {
        return helpers::find_all_near<Self, Vec, Distance, Vector, Value>(*this, point, epsilon, out);
    }
    template <typename Distance, typename Vector>
    bool find_all_near(const Vec &point, const Distance epsilon, Vector &out) const {
        return helpers::find_all_near<const Self, Vec, Distance, Vector, const Value>(*this, point, epsilon, out);
    }
    bool find_all_near(const Vec &point, const Component epsilon, std::vector<std::reference_wrapper<Value>> &out) override {
        return this->find_all_near<Component, std::vector<std::reference_wrapper<Value>>>(point, epsilon, out);
    }
    bool find_all_near(const Vec &point, const Component epsilon, std::vector<std::reference_wrapper<const Value>> &out) const override {
        return this->find_all_near<Component, std::vector<std::reference_wrapper<const Value>>>(point, epsilon, out);
    }

    bool is_in_bounds(const Vec point) const {
        const Vec max_bounds = origin + size;
        return glm::all(glm::greaterThanEqual(point, origin)) &&
            glm::all(glm::lessThanEqual(point, max_bounds));
    }

    Vec cell_size() const {
        return this->size / Vec(this->divisions);
    }

    CellIndex calculate_grid_index(const Vec point) const {
        return CellIndex((point - this->origin) / this->cell_size());
    }

    size_t calculate_cell_index(const Vec point) const {
        const CellIndex grid_index = this->calculate_grid_index(point);
        return this->calculate_cell_index(grid_index);
    }

    size_t calculate_cell_index(const CellIndex grid_index) const {
        size_t index = 0;
        size_t stride = 1;

        for (size_t i = 0; i < n_dims; i++) {
            index += grid_index[i] * stride;
            stride *= divisions[i];
        }

        return index;
    }

    bool is_point_inside_cell(const Vec &point, const CellIndex &grid_index) const {
        const Vec cell_size = this->cell_size();
        const Vec cell_min = this->origin + Vec(grid_index) * cell_size;
        const Vec cell_max = cell_min + cell_size;

        return glm::all(glm::greaterThanEqual(point, cell_min)) && glm::all(glm::lessThanEqual(point, cell_max));
    }

    bool is_valid_grid_index(const CellOffset &grid_index) const {
        return glm::all(glm::greaterThanEqual(grid_index, CellOffset(0))) && this->is_valid_grid_index(CellIndex(grid_index));
    }
    bool is_valid_grid_index(const CellIndex &grid_index) const {
        return glm::all(glm::lessThan(grid_index, divisions));
    }

    const Cell &cell(const CellIndex &grid_index) const {
        return this->cell(this->calculate_cell_index(grid_index));
    }
    const Cell &cell(const size_t cell_index) const {
        return this->_data[cell_index];
    }

    const std::span<const Cell> cells() const {
        return this->_data;
    }

    /*const*/ Vec origin;
    /*const*/ Vec size;
    /*const*/ glm::vec<n_dims, uint32_t> divisions;

private:
    size_t _point_count;
    std::vector<Cell> _data;

    static_assert(SpatialLookupExt<Self, Vec, Component, Value>);
};

template <typename Value>
using Grid2d = Grid<2, double, Value>;
template <typename Value>
using Grid3d = Grid<3, double, Value>;

}

/*
namespace {
radix::geometry::Aabb3d pad_bounds(const radix::geometry::Aabb3d &bounds, const double percentage) {
    const glm::dvec3 bounds_padding = bounds.size() * percentage;
    const radix::geometry::Aabb3d padded_bounds(bounds.min - bounds_padding, bounds.max + bounds_padding);
    return padded_bounds;
}

template <glm::length_t n_dims, typename Component, typename Value>
Grid<n_dims, Component, Value> _construct_grid_for_meshes(const radix::geometry::Aabb3d &bounds, const size_t vertex_count) {
    const radix::geometry::Aabb3d padded_bounds = pad_bounds(bounds, 0.01);

    const double max_extends = max_component(padded_bounds.size());
    const glm::dvec3 relative_extends = padded_bounds.size() / max_extends;
    const glm::uvec3 grid_divisions = glm::max(glm::uvec3(2 * std::cbrt(vertex_count) * relative_extends), glm::uvec3(1));
    Grid<n_dims, Component, Value> grid(padded_bounds.min, padded_bounds.size(), grid_divisions);

    return grid;
}
} // namespace

template <glm::length_t n_dims, typename Component, typename Value>
inline Grid<n_dims, Component, Value> construct_grid_for_mesh(const SimpleMesh_<n_dims, Component> &mesh) {
    const radix::geometry::Aabb3d bounds = calculate_bounds(mesh);
    const size_t vertex_count = mesh.vertex_count();
    return _construct_grid_for_meshes<n_dims, Component, Value>(bounds, vertex_count);
}

template <glm::length_t n_dims, typename Component, typename Value>
inline Grid<n_dims, Component, Value> construct_grid_for_meshes(const std::span<const SimpleMesh_<n_dims, Component>> meshes) {
    const radix::geometry::Aabb3d bounds = calculate_bounds(meshes);
    const size_t maximal_merged_mesh_size = std::transform_reduce(
        meshes.begin(), meshes.end(), 0,
        [](const size_t a, const size_t b) { return a + b; },
        [](const SimpleMesh_<n_dims, Component> &mesh) { return mesh.vertex_count(); });
    return _construct_grid_for_meshes<n_dims, Component, Value>(bounds, maximal_merged_mesh_size);
}
*/