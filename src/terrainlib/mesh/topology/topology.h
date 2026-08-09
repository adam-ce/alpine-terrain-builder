#pragma once

#include <vector>
#include <cstdint>
#include <span>
#include <ranges>

#include <glm/common.hpp>
#include <libassert/assert.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/View.h"
#include "range_utils.h"

namespace mesh {

namespace detail {
template <typename T>
T checked_div(const T dividend, const T divisor) {
    DEBUG_ASSERT((dividend % divisor) == 0);
    return dividend / divisor;
}
}

class ComponentTopology {
public:
    [[nodiscard]] uint32_t vertex_count() const {
        return this->_vertex_count;
    }
    [[nodiscard]] uint32_t halfedge_count() const {
        return 3 * this->triangle_count();
    }
    [[nodiscard]] uint32_t edge_count() const {
        const uint32_t two_edge_count = (3 * this->triangle_count()) + this->boundary_edge_count();
        return detail::checked_div(two_edge_count, 2u);
    }
    [[nodiscard]] uint32_t triangle_count() const {
        return this->_face_count;
    }
    [[nodiscard]] uint32_t boundary_count() const {
        return this->_boundary_loop_count;
    }
    [[nodiscard]] uint32_t boundary_edge_count() const {
        return this->_boundary_edge_count;
    }

    [[nodiscard]] bool is_closed() const {
        return this->boundary_count() == 0;
    }
    [[nodiscard]] bool is_open() const {
        return this->boundary_count() > 0;
    }

    [[nodiscard]] int32_t chi() const {
        const int32_t chi =
            static_cast<int32_t>(this->vertex_count()) -
            static_cast<int32_t>(this->edge_count()) +
            static_cast<int32_t>(this->triangle_count());
        return chi;
    }
    [[nodiscard]] uint32_t genus() const {
        const int32_t two_genus = 2 
            - static_cast<int32_t>(this->boundary_count()) 
            - static_cast<int32_t>(this->chi());
        DEBUG_ASSERT(two_genus >= 0);
        const int32_t genus = detail::checked_div(two_genus, 2);
        return static_cast<uint32_t>(genus);
    }

    [[nodiscard]] bool is_disk(const bool allow_holes = false) const {
        if (this->genus() != 0) {
            return false;
        }
        if (allow_holes) {
            return this->is_open();
        } else {
            return this->boundary_count() == 1;
        }
    }

    [[nodiscard]] bool is_annulus() const {
        return this->genus() == 0 && this->boundary_count() == 2;
    }

    [[nodiscard]] bool is_sphere() const {
        return this->genus() == 0 && this->is_closed();
    }

    [[nodiscard]] bool is_torus() const {
        return this->genus() == 1 && this->is_closed();
    }

    static ComponentTopology create(
        const uint32_t vertex_count,
        const uint32_t face_count,
        const uint32_t boundary_loop_count,
        const uint32_t boundary_edge_count) {
        ComponentTopology c;
        c._vertex_count = vertex_count;
        c._face_count = face_count;
        c._boundary_loop_count = boundary_loop_count;
        c._boundary_edge_count = boundary_edge_count;
        return c;
    }

private:
    uint32_t _vertex_count = 0;
    uint32_t _face_count = 0;

    uint32_t _boundary_loop_count = 0;
    uint32_t _boundary_edge_count = 0;
};

class Topology {
public:
    [[nodiscard]] const ComponentTopology &component(const size_t index) const {
        return this->_components[index];
    }
    [[nodiscard]] std::span<const ComponentTopology> components() const {
        return this->_components;
    }

    [[nodiscard]] bool is_empty() const {
        return this->_components.empty();
    }
    [[nodiscard]] uint32_t component_count() const {
        return this->_components.size();
    }
    [[nodiscard]] bool is_single_component() const {
        return this->component_count() == 1;
    }

    [[nodiscard]] uint32_t vertex_count() const {
        return sum(this->_components, [](const ComponentTopology &c) { return c.vertex_count(); });
    }
    [[nodiscard]] uint32_t edge_count() const {
        return sum(this->_components, [](const ComponentTopology &c) { return c.edge_count(); });
    }
    [[nodiscard]] uint32_t triangle_count() const {
        return sum(this->_components, [](const ComponentTopology &c) { return c.triangle_count(); });
    }

    [[nodiscard]] bool is_closed() const {
        return !this->is_empty() && std::ranges::all_of(
                                        this->_components, [](const ComponentTopology &c) { return c.is_closed(); });
    }

    [[nodiscard]] bool is_open() const {
        return !this->is_empty() && !this->is_closed();
    }

    [[nodiscard]] bool is_disk(const bool allow_holes = false) const {
        return this->is_single_component() &&
               this->component(0).is_disk(allow_holes);
    }

    [[nodiscard]] bool is_disks(const bool allow_holes = false) const {
        return !this->is_empty() && std::ranges::all_of(
                                  this->_components,
                                  [allow_holes](const ComponentTopology &c) {
                                      return c.is_disk(allow_holes);
                                  });
    }

    [[nodiscard]] bool is_annulus() const {
        return this->is_single_component() &&
               this->component(0).is_annulus();
    }

    [[nodiscard]] bool is_sphere() const {
        return this->is_single_component() &&
               this->component(0).is_sphere();
    }

    [[nodiscard]] bool is_torus() const {
        return this->is_single_component() &&
               this->component(0).is_torus();
    }

    [[nodiscard]] int32_t chi() const {
        return sum(this->_components, [](const ComponentTopology &c) { return c.chi(); });
    }
    [[nodiscard]] uint32_t boundary_count() const {
        return sum(this->_components, [](const ComponentTopology &c) { return c.boundary_count(); });
    }

    [[nodiscard]] uint32_t genus() const {
        if (this->is_empty()) {
            return 0;
        }

        const int32_t component_count = static_cast<int32_t>(this->component_count());
        const int32_t boundary_count = static_cast<int32_t>(this->boundary_count());
        const int32_t chi = this->chi();

        const int32_t two_genus = 2 * component_count - boundary_count - chi;

        DEBUG_ASSERT(two_genus >= 0);
        DEBUG_ASSERT(two_genus % 2 == 0);

        return static_cast<uint32_t>(two_genus / 2);
    }

    static Topology create(std::vector<ComponentTopology> components) {
        Topology t;
        t._components = components;
        return t;
    }

private:
    std::vector<ComponentTopology> _components;
};

Topology compute_topology(const std::span<const glm::uvec3> triangles);
template <glm::length_t n_dims, typename T>
Topology compute_topology(const mesh::View_<n_dims, T> &mesh) {
    return compute_topology(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
Topology compute_topology(const mesh::Simple_<n_dims, T> &mesh) {
    return compute_topology(mesh.triangles);
}

}