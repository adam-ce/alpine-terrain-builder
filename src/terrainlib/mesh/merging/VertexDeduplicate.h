#pragma once

#include <functional>
#include <vector>

#include <glm/common.hpp>

namespace mesh::merging {

template <glm::length_t n_dims, typename Component, typename Meta>
class VertexDeduplicate {
public:
    using Vec = glm::vec<n_dims, Component>;
    using Duplicate = std::reference_wrapper<const Meta>;
    using Duplicates = std::vector<Duplicate>;

    virtual ~VertexDeduplicate() = default;

    virtual void insert(const Vec& point, const Meta meta) = 0;
    virtual bool find(const Vec& point, Duplicates &duplicates) const = 0;
    virtual bool find_or_insert(const Vec &point, const Meta meta, Duplicates &duplicates) {
        if (this->find(point, duplicates)) {
            return false;
        } else {
            this->insert(point, meta);
            return true;
        }
    }
    Duplicates find(const Vec &point) const {
        Duplicates duplicates;
        this->find(point, duplicates);
        return duplicates;
    }
};

}
