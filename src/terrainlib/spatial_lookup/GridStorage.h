#pragma once

#include <algorithm>
#include <vector>
#include <optional>

#include <glm/common.hpp>
#include <glm/gtx/component_wise.hpp>
#include <libassert/assert.hpp>

#include "spatial_lookup/CellBasedStorage.h"

namespace spatial_lookup {

template <glm::length_t n_dims, typename Component, typename Value>
class GridStorage {
public:
    using Self = GridStorage<n_dims, Component, Value>;
    using Vec = glm::vec<n_dims, Component>;
    using Bounds = radix::geometry::Aabb<n_dims, Component>;
    using CellIndex = uint32_t;
    using CellOffset = int32_t;
    using GridIndex = glm::vec<n_dims, uint32_t>;
    using GridOffset = glm::vec<n_dims, int32_t>;

    GridStorage(const Vec origin, const Vec size, const GridIndex divisions)
        : _point_count(0), _origin(origin), _size(size), _divisions(divisions) {
        this->_data.resize(glm::compMul(divisions));
    }

    struct Item {
        Vec point;
        Value value;
    };

    struct Cell {
        std::vector<Item> items;
    };

    void clear() {
        for (Cell &cell : this->_data) {
            cell.items.clear();
        }
        this->_point_count = 0;
    }

    [[nodiscard]] bool empty() const {
        return this->_point_count == 0;
    }

    [[nodiscard]] size_t point_count() const {
        return this->_point_count;
    }

    [[nodiscard]] size_t cell_count() const {
        return this->_data.size();
    }

    [[nodiscard]] Bounds bounds() const {
        return Bounds{this->_origin, this->_origin + this->_size};
    }

    [[nodiscard]] GridIndex point_to_grid_index(const Vec &point) const {
        DEBUG_ASSERT(this->bounds().contains_inclusive(point));

        const Vec relative = (point - this->_origin) / this->cell_size();
        GridIndex index;
        for (glm::length_t i = 0; i < n_dims; i++) {
            const uint32_t division = this->_divisions[i];
            index[i] = std::min(static_cast<uint32_t>(relative[i]), division - 1);
        }
        return index;
    }
    [[nodiscard]] CellIndex point_to_cell_index(const Vec &point) const {
        return this->grid_to_cell_index(this->point_to_grid_index(point));
    }
    [[nodiscard]] CellIndex grid_to_cell_index(const GridIndex &grid_index) const {
        CellIndex index = 0;
        CellIndex stride = 1;

        for (CellIndex i = 0; i < n_dims; i++) {
            index += grid_index[i] * stride;
            stride *= this->_divisions[i];
        }

        return index;
    }
    [[nodiscard]] CellIndex grid_to_cell_index(const GridOffset &grid_index) const {
        CellOffset index = 0;
        CellOffset stride = 1;

        for (CellOffset i = 0; i < n_dims; i++) {
            index += grid_index[i] * stride;
            stride *= this->_divisions[i];
        }

        return CellIndex(index); // this may wrap, but we check the validity later anyways
    }
    [[nodiscard]] CellIndex offset_cell_index(const CellIndex index, const GridOffset &offset) const {
        return index + this->grid_to_cell_index(offset);
    }

    [[nodiscard]] GridIndex cell_to_grid_index(const CellIndex index) const {
        GridIndex grid_index;
        CellIndex current_index = index;
        for (glm::length_t i = 0; i < n_dims; i++) {
            grid_index[i] = current_index % this->_divisions[i];
            current_index /= this->_divisions[i];
        }
        return grid_index;
    }
    [[nodiscard]] Bounds cell_bounds(const CellIndex index) const {
        const GridIndex grid_index = this->cell_to_grid_index(index);
        const Vec cell_size = this->cell_size();
        const Vec cell_min = this->_origin + Vec(grid_index) * cell_size;
        return Bounds{
            .min = cell_min,
            .max = cell_min + cell_size
        };
    }

    bool insert(const Vec &point, Value value) {
        const CellIndex index = this->point_to_cell_index(point);

        if (!this->is_valid_cell_index(index)) {
            return false;
        }

        Cell &cell = this->_data[index];
        cell.items.push_back(Item{.point = point, .value = std::move(value)});
        this->_point_count += 1;

        return true;
    }

    template <typename Func>
    bool for_all_in_cell(const CellIndex index, Func &&func) const {
        if (!this->is_valid_cell_index(index)) {
            return false;
        }

        const Cell &cell = this->_data[index];
        for (const Item &item : cell.items) {
            func(item.point, item.value);
        }

        return !cell.items.empty();
    }
    template <typename Func>
    bool for_all_in_cell(const CellIndex index, Func &&func) {
        return const_cast<const Self *>(this)->for_all_in_cell(index, [&](const Vec &vec, const Value &value) {
            func(vec, const_cast<Value &>(value));
        });
    }

    [[nodiscard]] Vec cell_size() const {
        return this->_size / Vec(this->_divisions);
    }

    [[nodiscard]] bool is_valid_cell_index(const CellIndex index) const {
        return index < this->_data.size();
    }

private:
    size_t _point_count;
    std::vector<Cell> _data;
    Vec _origin;
    Vec _size;
    glm::vec<n_dims, uint32_t> _divisions;

    // static_assert(CellBasedStorage<Self, n_dims, Component, Value>);
};

} // namespace spatial_lookup
