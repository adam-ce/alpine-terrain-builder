#pragma once

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <libassert/assert.hpp>

#include "OffsetTable.h"

// Contiguous buffer partitioned into segments using an OffsetTable for range-based indexing.
template <typename TValue, typename TValueIndex = size_t, typename TSegmentIndex = TValueIndex>
class SegmentedBuffer {
public:
    using value_type = TValue;
    using index_type = TValueIndex;
    using segment_index = TSegmentIndex;
    using offset_range = typename OffsetTable<index_type>::range_type;

    SegmentedBuffer() {
        this->reset();
    }

    // Resets the buffer to its default state.
    void reset() {
        this->_offsets.clear();
        this->_data.clear();

        this->_offsets.append_length(0);
    }

    // Initializes the buffer with pre-defined segment sizes and pre-allocates backing storage.
    void init(const std::span<const index_type> segment_sizes, const value_type &value = {}) {
        if (segment_sizes.empty()) {
            this->reset();
            return;
        }

        this->_offsets.clear();
        this->_data.clear();

        this->_offsets.append_lengths(segment_sizes);
        this->_data.resize(this->_offsets.total_size(), value);
    }

    // Reserve size in the global backing storage.
    void reserve(const index_type size) {
        this->_data.reserve(size);
    }

    // Resize the last segment.
    void resize_last_segment(const index_type size, const value_type &value = {}) {
        if (size == 0) {
            return;
        }

        const segment_index last_segment = this->segment_count() - 1;
        const index_type new_end = this->_offsets.range(last_segment).begin + size;
        this->_offsets.set_end(last_segment, new_end);
        this->_data.resize(new_end, value);
    }

    // Adds a new empty segment with a pre-allocated capacity.
    void push_new_segment(const index_type size, const value_type &value = {}) {
        this->start_new_segment();
        this->resize_last_segment(size, value);
    }

    // Pushes a new segment and copies all elements from a container into it.
    template <typename TContainer>
    void push_new_segment(const TContainer &container) {
        this->start_new_segment();

        // resize
        const index_type added_size = container.size();
        this->resize_last_segment(added_size);

        // copy data
        const segment_index last_segment = this->segment_count() - 1;
        auto destination = this->segment(last_segment);
        std::copy(container.begin(), container.end(), destination.begin());
    }

    // Appends an item to the end of the last segment in the data buffer.
    void push_to_last_segment(const value_type &value) {
        const segment_index last_segment = this->segment_count() - 1;
        const offset_range range = this->_offsets.range(last_segment);

        this->_data.push_back(value);
        this->_offsets.set_end(last_segment, range.end + 1);
    }

    // Appends an item to the end of the last segment in the data buffer.
    void push_to_last_segment(value_type &&value) {
        const segment_index last_segment = this->segment_count() - 1;
        const offset_range range = this->_offsets.range(last_segment);

        this->_data.push_back(std::forward<value_type>(value));
        this->_offsets.set_end(last_segment, range.end + 1);
    }

    // Finalizes the current segment and starts a new one at the current buffer position.
    void start_new_segment() {
        if (this->segment_count() == 1 && this->segment_size(0) == 0) {
            return;
        }

        const segment_index last_segment = this->segment_count() - 1;
        this->_offsets.set_end(last_segment, this->total_size());
        this->_offsets.append_length(0);
    }

    // Accesses an element using segment-relative indexing.
    value_type &operator()(const segment_index segment_index, const index_type element_index) {
        return this->get_impl(*this, segment_index, element_index);
    }

    // Const-qualified access to elements using segment-relative indexing.
    const value_type &operator()(const segment_index segment_index, const index_type element_index) const {
        return this->get_impl(*this, segment_index, element_index);
    }

    // Returns the total number of segments currently tracked.
    segment_index segment_count() const noexcept {
        return this->_offsets.size();
    }

    // Returns the number of elements contained within a specific segment.
    index_type segment_size(const segment_index segment_index) const noexcept {
        return this->_offsets.element_size(segment_index);
    }

    // Returns the total number of elements across all segments.
    index_type total_size() const noexcept {
        return this->_data.size();
    }

    // Returns a readonly view of one segment of the buffer.
    std::span<const value_type> segment(const segment_index segment_index) const noexcept {
        return this->get_segment_impl(*this, segment_index);
    }
    // Returns a view of one segment of the buffer.
    std::span<value_type> segment(const segment_index segment_index) noexcept {
        return this->get_segment_impl(*this, segment_index);
    }

    // Returns a readonly flat view of the buffer.
    std::span<const value_type> flat() const noexcept {
        return this->_data;
    }
    // Returns a flat view of the buffer.
    std::span<value_type> flat() noexcept {
        return this->_data;
    }

private:
    template <typename Self>
    static inline auto &get_impl(Self &self, const segment_index segment_index, const index_type element_index) {
        DEBUG_ASSERT(segment_index < self._offsets.size());

        const offset_range range = self._offsets.range(segment_index);
        const index_type flat_index = range.begin + element_index;

        DEBUG_ASSERT(flat_index < range.end);
        return self._data[flat_index];
    }

    template <typename Self>
    static inline auto get_segment_impl(Self &self, const segment_index segment_index) {
        DEBUG_ASSERT(segment_index < self._offsets.size());

        const offset_range range = self._offsets.range(segment_index);
        const index_type length = range.end - range.begin;
        return std::span(self._data.data() + range.begin, length);
    }

    // Manages the boundaries of each segment.
    OffsetTable<index_type> _offsets;

    // The contiguous backing storage for all segments.
    std::vector<value_type> _data;
};
