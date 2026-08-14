#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <vector>

#include <libassert/assert.hpp>

#include "enumerate.h"
#include "mesh/connectivity/vertex_index_range.h"
#include "containers/OffsetVector.h"

class VertexMap {
public:
    static constexpr uint32_t invalid_index = static_cast<uint32_t>(-1);
    VertexMap() = default;

    static VertexMap identity(const uint32_t size, const uint32_t offset = 0) {
        std::vector<uint32_t> forward(size);
        std::iota(forward.begin(), forward.end(), 0);
        return VertexMap::from_forward(forward, offset);
    }
    static VertexMap identity(const std::span<const glm::uvec3> triangles) {
        const auto range = mesh::find_vertex_index_range(triangles);
        return VertexMap::identity(range.size(), range.start);
    }
    static VertexMap from_forward(std::vector<uint32_t> forward, const uint32_t offset = 0) {
        return from_forward(OffsetVector(std::move(forward), offset));
    }
    static VertexMap from_forward(OffsetVector<uint32_t> forward) {
        VertexMap m;
        m._forward = std::move(forward);
        return m;
    }
    static VertexMap from_backward(std::vector<uint32_t> backward, const uint32_t offset = 0) {
        return from_backward(OffsetVector(std::move(backward), offset));
    }
    static VertexMap from_backward(OffsetVector<uint32_t> backward) {
        VertexMap m;
        m._backward = std::move(backward);
        return m;
    }
    static VertexMap from(OffsetVector<uint32_t> forward, OffsetVector<uint32_t> backward) {
        VertexMap m;
        m._forward = std::move(forward);
        m._backward = std::move(backward);
        return m;
    }

    [[nodiscard]] uint32_t old_vertex_count() const {
        this->build_forward();
        return this->_forward.size();
    }
    [[nodiscard]] uint32_t new_vertex_count() const {
        this->build_backward();
        return this->_backward.size();
    }

    [[nodiscard]] const OffsetVector<uint32_t>& forward() const {
        this->build_forward();
        return this->_forward;
    }
    [[nodiscard]] const OffsetVector<uint32_t>& backward() const {
        this->build_backward();
        return this->_backward;
    }
    [[nodiscard]] OffsetVector<uint32_t> &forward() {
        this->build_forward();
        return this->_forward;
    }
    [[nodiscard]] OffsetVector<uint32_t> &backward() {
        this->build_backward();
        return this->_backward;
    }

    [[nodiscard]] uint32_t map_forward(const uint32_t old_index) const {
        this->build_forward();
        return this->_forward[old_index];
    }
    [[nodiscard]] uint32_t map_backward(const uint32_t new_index) const {
        this->build_backward();
        return this->_backward[new_index];
    }

    [[nodiscard]] glm::uvec3 map_triangle_forward(const glm::uvec3 old_triangle) const {
        return {
            this->map_forward(old_triangle.x),
            this->map_forward(old_triangle.y),
            this->map_forward(old_triangle.z)};
    }

    [[nodiscard]] glm::uvec3 map_triangle_backward(const glm::uvec3 new_triangle) const {
        return {
            this->map_backward(new_triangle.x),
            this->map_backward(new_triangle.y),
            this->map_backward(new_triangle.z)};
    }

private:
    mutable OffsetVector<uint32_t> _forward;
    mutable OffsetVector<uint32_t> _backward;

    static OffsetVector<uint32_t> invert_mapping(const OffsetVector<uint32_t> &map) {
        if (map.empty()) {
            return {};
        }

        uint32_t min_index = invalid_index;
        uint32_t max_index = 0;
        for (const uint32_t new_index : map) {
            if (new_index != invalid_index) {
                min_index = std::min(min_index, new_index);
                max_index = std::max(max_index, new_index);
            }
        }

        if (min_index == invalid_index) {
            return {};
        }

        const uint32_t offset = min_index;
        const uint32_t range = max_index - min_index + 1;
        OffsetVector<uint32_t> inverse;
        inverse.offset = offset;
        inverse.resize(range, invalid_index);

        for (const auto [i, new_index] : enumerate(map)) {
            const uint32_t old_index = map.offset + i;
            if (new_index != invalid_index) {
                DEBUG_ASSERT(inverse[new_index] == invalid_index);
                inverse[new_index] = old_index;
            }
        }

        return inverse;
    }

    void build_forward() const {
        if (!this->_forward.empty()) {
            return;
        }
        this->_forward = invert_mapping(this->_backward);
    }
    void build_backward() const {
        if (!this->_backward.empty()) {
            return;
        }
        this->_backward = invert_mapping(this->_forward);
    }
};
