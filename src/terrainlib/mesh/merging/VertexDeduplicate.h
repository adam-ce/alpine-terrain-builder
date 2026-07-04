#pragma once

#include <vector>

#include <glm/common.hpp>

namespace mesh::merging {

template <glm::length_t n_dims, typename Component, typename Meta>
class VertexDeduplicate {
public:
    using Vec = glm::vec<n_dims, Component>;
    using Matches = std::vector<Meta>;

    virtual ~VertexDeduplicate() = default;

    virtual void insert(const Vec& point, Meta meta) = 0;
    // Appends all matches for point to `matches`. Returns true if any were found.
    // Values are returned by copy to avoid dangling references into the backing store.
    virtual bool find(const Vec& point, Matches &matches) const = 0;
    virtual bool find_or_insert(const Vec &point, Meta meta, Matches &matches) {
        if (this->find(point, matches)) {
            return false;
        } else {
            this->insert(point, std::move(meta));
            return true;
        }
    }
    Matches find(const Vec &point) const {
        Matches matches;
        this->find(point, matches);
        return matches;
    }
};

} // namespace mesh::merging
