#pragma once

#include <vector>
#include <algorithm>

class UnionFind {
public:
    using Index = size_t;

    explicit UnionFind(const size_t size)
        : _parents(size), _sizes(size, 1) {
        std::iota(this->_parents.begin(), this->_parents.end(), 0);
    }

    [[nodiscard]] Index find(const Index x) {
        Index &x_parent = this->_parents[x];
        if (x_parent != x) {
            const Index x_rep = this->find(x_parent);
            x_parent = x_rep;
        }
        return x_parent /* this is x_rep */;
    }

    void make_union(const Index x, const Index y) {
        const Index x_rep = this->find(x);
        const Index y_rep = this->find(y);

        if (x_rep == y_rep) {
            return;
        }

        const Index x_size = this->_sizes[x_rep];
        const Index y_size = this->_sizes[y_rep];
        if (x_size < y_size) {
            this->_parents[x_rep] = y_rep;
            this->_sizes[y_rep] += this->_sizes[x_rep];
        } else {
            this->_parents[y_rep] = x_rep;
            this->_sizes[x_rep] += this->_sizes[y_rep];
        }
    }

    [[nodiscard]] Index size() {
        return this->_parents.size();
    }

    [[nodiscard]] bool is_joint() {
        return std::find(this->_sizes.begin(), this->_sizes.end(), this->size()) != this->_sizes.end();
    }

    [[nodiscard]] bool is_disjoint() {
        return !this->is_joint();
    }

private:
    std::vector<Index> _parents;
    std::vector<Index> _sizes;
};
