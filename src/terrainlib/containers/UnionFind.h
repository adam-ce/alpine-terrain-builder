#pragma once

#include <limits>
#include <numeric>
#include <unordered_map>
#include <vector>
#include <span>

#include <libassert/assert.hpp>

#include "containers/SegmentedBuffer.h"


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

    Index make_union(const Index x, const Index y) noexcept {
        const Index x_rep = this->find(x);
        const Index y_rep = this->find(y);

        if (x_rep == y_rep) {
            return x_rep;
        }

        this->_parents[x_rep] = y_rep;
        this->_set_count--;

        if constexpr (TrackSizes) {
            this->_sizes[y_rep] += this->_sizes[x_rep];
            this->_sizes[x_rep] = Size{0};
        }

        return y_rep;
    }

    bool is_same_set(const Index x, const Index y) const noexcept {
        return this->is_same_set_impl(*this, x, y);
    }
    bool is_same_set(const Index x, const Index y) noexcept {
        return this->is_same_set_impl(*this, x, y);
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

    [[nodiscard]] std::unordered_map<Index, std::vector<Index>> get_sets_as_map() const {
        return this->get_sets_as_map_impl(*this);
    }

    [[nodiscard]] std::unordered_map<Index, std::vector<Index>> get_sets_as_map() {
        return this->get_sets_as_map_impl(*this);
    }

    [[nodiscard]] SegmentedBuffer<Index, Size, Index> get_sets_compact() const {
        return this->get_sets_compact_impl(*this);
    }
    [[nodiscard]] SegmentedBuffer<Index, Size, Index> get_sets_compact() {
        return this->get_sets_compact_impl(*this);
    }

    [[nodiscard]] SegmentedBuffer<Index, Size, Index> get_sets_sparse() const {
        return this->get_sets_sparse_impl(*this);
    }
    [[nodiscard]] SegmentedBuffer<Index, Size, Index> get_sets_sparse() {
        return this->get_sets_sparse_impl(*this);
    }

    [[nodiscard]] std::vector<Index> get_set_labels() const {
        return this->get_set_labels_impl(*this);
    }
    [[nodiscard]] std::vector<Index> get_set_labels() {
        return this->get_set_labels_impl(*this);
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
    static bool is_same_set_impl(Self &self, const Index x, const Index y) noexcept {
        const Index x_rep = self.find(x);
        const Index y_rep = self.find(y);
        return x_rep == y_rep;
    }

    template <typename Self>
    static std::unordered_map<Index, std::vector<Index>> get_sets_as_map_impl(Self &self) {
        std::unordered_map<Index, std::vector<Index>> sets;

        const Size n = self.size();
        sets.reserve(self.set_count());

        for (Index item = 0; item < n; item++) {
            sets[self.find(item)].push_back(item);
        }

        return sets;
    }

    template <typename Self>
    static SegmentedBuffer<Index, Size, Index> get_sets_compact_impl(Self &self) {
        SegmentedBuffer<Index, Size, Index> sparse = get_sets_sparse_impl(self);
        sparse.remove_empty_segments();
        return sparse;
    }

    template <typename Self>
    static SegmentedBuffer<Index, Size, Index> get_sets_sparse_impl(Self &self) {
        const Size n = self.size();
        SegmentedBuffer<Index, Size, Index> sets;

        if constexpr (TrackSizes) {
            sets.init(std::span<const Size>(self._sizes));

            std::vector<Size> cursors(n, Size{0});
            for (Index item = 0; item < n; item++) {
                const Index rep = self.find(item);
                std::span<Index> set = sets.segment(rep);
                Size &cursor = cursors[rep];
                set[cursor] = item;
                cursor++;
            }
        } else {
            std::vector<Size> counts(n, Size{0});
            for (Index item = 0; item < n; item++) {
                counts[self.find(item)]++;
            }
            sets.init(std::span<const Size>(counts));

            for (Index item = 0; item < n; item++) {
                const Index rep = self.find(item);
                std::span<Index> set = sets.segment(rep);
                Size& remaining_count = counts[rep];
                Size cursor = set.size() - remaining_count;
                set[cursor] = item;
                remaining_count--;
            }
        }

        return sets;
    }

    template <typename Self>
    static std::vector<Index> get_set_labels_impl(Self &self) {
        const Size n = self.size();
        constexpr Index NO_LABEL = std::numeric_limits<Index>::max();

        std::vector<Index> labels(n, NO_LABEL);
        Index next_label = 0;

        for (Index item = 0; item < n; item++) {
            Index &representative_label = labels[self.find(item)];
            if (representative_label == NO_LABEL) {
                representative_label = next_label;
                next_label++;
            }

            labels[item] = representative_label;
        }

        DEBUG_ASSERT(next_label == self.set_count());
        return labels;
    }

private:
    std::vector<Index> _parents;
    std::conditional_t<TrackSizes, std::vector<Size>, std::monostate> _sizes;
    Size _set_count;
};

using UnionFind = UnionFind_<false>;
using UnionFindWithSizes = UnionFind_<true>;