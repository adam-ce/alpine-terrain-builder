#pragma once

#include <algorithm>
#include <numeric>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

#include <libassert/assert.hpp>

template <bool TrackSizes = false, typename IndexT = size_t, typename SizeT = size_t>
class UnionFind_ {
public:
    using Index = IndexT;
    using Size = SizeT;

    explicit UnionFind_(const Size size = 0) {
        this->reset(size);
    }

    void reset() noexcept {
        this->reset(this->size());
    }
    void reset(const Size size) noexcept {
        this->_parents.resize(size);
        std::iota(this->_parents.begin(), this->_parents.end(), Index{0});
        if constexpr (TrackSizes) {
            this->_sizes.assign(size, Size{1});
        }
        this->_set_count = size;
    }

    [[nodiscard]] Index find(const Index item) const noexcept {
        return this->find_impl(*this, item);
    }

    [[nodiscard]] Index find(const Index item) noexcept {
        return this->find_impl(*this, item);
    }

    void make_union(const Index x, const Index y) noexcept {
        const Index x_rep = this->find(x);
        const Index y_rep = this->find(y);

        if (x_rep == y_rep) {
            return;
        }

        this->_parents[x_rep] = y_rep;
        this->_set_count--;

        if constexpr (TrackSizes) {
            this->_sizes[y_rep] += this->_sizes[x_rep];
        }
    }

    [[nodiscard]] Size size() const noexcept {
        return static_cast<Size>(this->_parents.size());
    }

    [[nodiscard]] Size set_count() const noexcept {
        return this->_set_count;
    }

    template <bool Enabled = TrackSizes> requires(Enabled)
    [[nodiscard]] Size get_set_size(const Index x) const noexcept {
        return this->_sizes[this->find(x)];
    }

    [[nodiscard]] bool is_joint() const noexcept {
        return this->set_count() == 1;
    }

    [[nodiscard]] bool is_disjoint() const noexcept {
        return !this->is_joint();
    }

    [[nodiscard]] std::unordered_map<Index, std::vector<Index>> get_sets() const {
        return this->get_sets_impl(*this);
    }

    [[nodiscard]] std::unordered_map<Index, std::vector<Index>> get_sets() {
        return this->get_sets_impl(*this);
    }

private:
    template <typename Self>
    static Index find_impl(Self &self, const Index item) noexcept {
        DEBUG_ASSERT(item < self.size());

        Index root = item;
        while (self._parents[root] != root) {
            root = self._parents[root];
        }

        if constexpr (!std::is_const_v<Self>) {
            Index current = item;
            while (self._parents[current] != current) {
                const Index next = self._parents[current];
                self._parents[current] = root;
                current = next;
            }
        }

        return root;
    }

    template <typename Self>
    static std::unordered_map<Index, std::vector<Index>> get_sets_impl(Self &self) {
        std::unordered_map<Index, std::vector<Index>> sets;

        const Size n = self.size();
        sets.reserve(self.set_count());

        for (Index item = 0; item < n; item++) {
            sets[self.find(item)].push_back(item);
        }

        return sets;
    }

private:
    std::vector<Index> _parents;
    std::conditional_t<TrackSizes, std::vector<Size>, std::monostate> _sizes;
    Size _set_count;
};

using UnionFind = UnionFind_<false>;
using UnionFindWithSizes = UnionFind_<true>;