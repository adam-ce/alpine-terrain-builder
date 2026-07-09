#pragma once

#include <vector>
#include <algorithm>

#include <libassert/assert.hpp>

template <bool TrackSizes = false>
class UnionFind {
public:
    using Index = size_t;
    using Size = size_t;

    explicit UnionFind(Size size)
        : _parents(size) {
        std::iota(this->_parents.begin(), this->_parents.end(), 0);
        if constexpr (TrackSizes) {
            this->_sizes.resize(size, 1);
        }
    }

    [[nodiscard]] Index find(const Index item) const {
        DEBUG_ASSERT(item < this->size());
        Index current = item;
        while (true) {
            const Index parent = this->_parents[current];
            if (current == parent) {
                return current;
            }
            current = parent;
        }
    }
    [[nodiscard]] Index find(const Index item) {
        DEBUG_ASSERT(item < this->size());
        const Index parent = this->_parents[item];
        if (parent == item) {
            return parent;
        }
        const Index rep = this->find(parent);
        this->_parents[item] = rep;
        return rep;
    }

    void make_union(const Index x, const Index y) {
        const Index x_rep = this->find(x);
        const Index y_rep = this->find(y);

        if (x_rep == y_rep) {
            return;
        }

        this->_parents[x_rep] = y_rep;
        if constexpr (TrackSizes) {
            this->_sizes[x_rep] += this->_sizes[y_rep];
        }
    }

    [[nodiscard]] Size size() const {
        return this->_parents.size();
    }

    template <bool Enabled = TrackSizes, typename = std::enable_if_t<Enabled>>
    [[nodiscard]] Index get_set_size(Index x) const {
        return this->_sizes[this->find(x)];
    }

    [[nodiscard]] bool is_joint() {
        if (this->_parents.empty()) {
            return true;
        }

        if constexpr (TrackSizes) {
            return this->get_set_size(0) == this->size();
        } else {
            const Index rep = this->find(0);
            return std::all_of(std::next(this->_parents.begin()), this->_parents.end(),
                               [&](Index i) { return this->find(i) == rep; });
        }
    }
    [[nodiscard]] bool is_disjoint() {
        return !this->is_joint();
    }

private:
    std::vector<Index> _parents;
    std::conditional_t<TrackSizes, std::vector<Size>, std::monostate> _sizes;
};
