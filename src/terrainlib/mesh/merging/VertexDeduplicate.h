#pragma once

#include <functional>
#include <vector>

#include <glm/common.hpp>

namespace mesh::merging {

template <glm::length_t n_dims, typename Component, typename Meta>
class VertexDeduplicate {
public:
    using Vec = glm::vec<n_dims, Component>;

    virtual ~VertexDeduplicate() = default;

    virtual void add(const Vec& point, const Meta meta) = 0;
    virtual bool get(const Vec& point, std::vector<std::reference_wrapper<const Meta>> &duplicates) const = 0;
    virtual bool get_or_add(const Vec &point, const Meta meta, std::vector<std::reference_wrapper<const Meta>> &duplicates) {
        if (this->get(point, duplicates)) {
            return false;
        } else {
            this->add(point, meta);
            return true;
        }
    }
    std::vector<std::reference_wrapper<const Meta>> get(const Vec &point) const {
        std::vector<std::reference_wrapper<const Meta>> duplicates;
        this->get(point, duplicates);
        return duplicates;
    }
};

}
