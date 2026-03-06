#pragma once

#include <libassert/assert.hpp>
#include <span>
#include <vector>

#include "OffsetTable.h"

// Contiguous buffer partitioned into segments using an OffsetTable for range-based indexing.
template <typename TValue, typename TValueIndex = size_t, typename TSectionIndex = TValueIndex>
class SegmentedBuffer {
public:
    using value_type = TValue;
    using index_type = TValueIndex;
    using segment_index = TSectionIndex;
    using offset_range = typename OffsetTable<segment_index>::range_type;

    SegmentedBuffer() {
        this->_offsets.append_length(0);
        this->_counts.push_back(0);
    }

    // Initializes the buffer with pre-defined segment sizes and pre-allocates backing storage.
    void init(const std::span<const segment_index> segment_sizes, const value_type &value = {}) {
        this->_offsets.clear();
        this->_data.clear();
        this->_counts.clear();

        this->_offsets.append_lengths(segment_sizes);
        this->_data.resize(this->_offsets.total_size(), value);
        this->_counts.resize(this->segment_count(), 0);
    }

    // Reserve size in the global backing storage.
    void reserve(const index_type size) {
        this->_data.reserve(size);
    }

    // Reserve size for the last segment.
    void resize_last_segment(const index_type size, const value_type &value = {}) {
        if (size == 0) {
            return;
        }

        const segment_index last_segment = this->segment_count() - 1;
        const index_type segment_size = this->segment_size(last_segment); 
        
        // Only reallocate if the requested size exceeds current capacity
        if (size > segment_size) {
            const index_type new_end = this->_offsets.range(last_segment).begin + size;
            this->_offsets.set_end(new_end);
            this->_data.resize(new_end, value);
        }
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

        // update the fill count
        this->_counts[last_segment] = added_size;
    }

    // Pushes an element into a specific segment's reserved space.
    void push_to_segment(const segment_index section_index, value_type &&value) {
        if (section_index + 1 == this->segment_count()) {
            this->push_to_last_segment(std::forward<value_type>(value));
            return;
        }

        const offset_range range = this->_offsets.range(section_index);
        const segment_index segment_fill = this->_counts[section_index];
        DEBUG_ASSERT(segment_fill < (range.end - range.begin));

        const index_type flat_index = range.begin + segment_fill;
        this->_data[flat_index] = std::forward<value_type>(value);
        this->_counts[section_index]++;
    }

    // Appends an item to the end of the last segment in the data buffer, extending it if full.
    void push_to_last_segment(value_type &&value) {
        const segment_index last_segment = this->segment_count() - 1;
        const offset_range range = this->_offsets.range(last_segment);
        index_type &last_segment_fill = this->_counts[last_segment];
        const index_type reserved_size = range.end - range.begin;

        if (last_segment_fill < reserved_size) {
            // We have reserved space that hasn't been filled yet
            const index_type flat_index = range.begin + last_segment_fill;
            this->_data[flat_index] = std::forward<value_type>(value);
        } else {
            // No reserved space left, append to the end of the vector
            this->_data.push_back(std::forward<value_type>(value));
            // Update the offset table to reflect the new vector size
            this->_offsets.set_end(last_segment, this->total_size());
        }

        last_segment_fill++;
    }

    // Finalizes the current segment and starts a new one at the current buffer position.
    void start_new_segment() {
        if (this->segment_count() == 1 && this->segment_size(0) == 0) {
            return;
        }
        
        const segment_index last_segment = this->segment_count() - 1;
        this->_offsets.set_end(last_segment, this->_data.size());
        this->_offsets.append_length(0);
        this->_counts.push_back(0);
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
    segment_index segment_size(const segment_index segment_index) const noexcept {
        return this->_offsets.element_size(segment_index);
    }

    // Returns the total number of elements across all segments.
    index_type total_size() const noexcept {
        return static_cast<index_type>(this->_data.size());
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
        const offset_range range = self._offsets.range(segment_index);
        const index_type flat_index = static_cast<index_type>(range.begin) + element_index;

        DEBUG_ASSERT(flat_index < static_cast<index_type>(range.end));
        return self._data[flat_index];
    }

    template <typename Self>
    static inline auto &get_segment_impl(Self &self, const segment_index segment_index) {
        const offset_range range = self._offsets.range(segment_index);
        const index_type length = range.end - range.begin;
        return std::span(self._data.data() + range.begin, length);
    }

    // Manages the boundaries of each segment.
    OffsetTable<segment_index> _offsets;
    // Counts for each segment.
    std::vector<index_type> _counts;

    // The contiguous backing storage for all segments.
    std::vector<value_type> _data;
};
