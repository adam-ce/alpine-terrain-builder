#pragma once

#include <algorithm>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <vector>

template <typename Index>
class OffsetTable_ {
public:
    using index_type = Index;

    struct range_type {
        index_type begin; // inclusive
        index_type end; // exclusive

        bool contains(index_type index) {
            return this->begin <= index && index < end;
        }

        bool empty() {
            return this->begin >= this->end;
        }
    };

    struct locate_result {
        index_type segment;
        range_type range;
    };

    OffsetTable_() {
        _offsets.push_back(0);
    }
    OffsetTable_(size_t size) {
        _offsets.reserve(size + 1);
        _offsets.push_back(0);
    }

    void clear() noexcept {
        this->_offsets.clear();
        this->_offsets.push_back(0);
    }

    void reserve(const index_type n) {
        this->_offsets.reserve(n + 1);
    }

    index_type size() const noexcept {
        return this->_offsets.size() - 1;
    }

    bool empty() const noexcept {
        return this->size() == 0;
    }

    index_type total_size() const noexcept {
        return this->_offsets.back();
    }

    // Append an segment given its length.
    void append_length(const index_type length) {
        const index_type next_offset = this->_offsets.back() + length;
        this->_offsets.push_back(next_offset);
    }

    // Append multiple segments given their lengths.
    void append_lengths(const std::span<const index_type> lengths) {
        const size_t current_size = this->_offsets.size();
        this->_offsets.reserve(current_size + lengths.size());

        for (const index_type length : lengths) {
            this->append_length(length);
        }
    }

    // Modifies the start of an segment, shrinking or extending the previous one accordingly.
    void set_begin(const index_type segment, const index_type new_begin) {
        if (segment == 0) {
            throw std::out_of_range("cannot modify begin of first segment");
        }
        if (segment >= this->size()) {
            throw std::out_of_range("segment out of range");
        }

        const index_type prev_boundary = this->_offsets[segment - 1];
        const index_type next_boundary = this->_offsets[segment + 1];
        if (new_begin < prev_boundary || new_begin > next_boundary) {
            throw std::invalid_argument("new_begin out of valid adjacent boundaries");
        }

        this->_offsets[segment] = new_begin;
    }

    // Modifies the end of an segment, shrinking or extending the next one accordingly (if present).
    void set_end(const index_type segment, const index_type new_end) {
        if (segment >= this->size()) {
            throw std::out_of_range("segment out of range");
        }

        const index_type prev_boundary = this->_offsets[segment];
        if (segment + 1 < this->size()) {
            const index_type next_boundary = this->_offsets[segment + 2];
            if (new_end < prev_boundary || new_end > next_boundary) {
                throw std::invalid_argument("new_end out of valid adjacent boundaries");
            }
        } else {
            if (new_end < prev_boundary) {
                throw std::invalid_argument("new_end cannot be less than section begin");
            }
        }

        this->_offsets[segment + 1] = new_end;
    }

    void remove_empty_segments() {
        const auto new_end = std::unique(this->_offsets.begin(), this->_offsets.end());
        this->_offsets.erase(new_end, this->_offsets.end());
    }

    bool in_same_segment(const index_type first, const index_type second) const {
        const range_type range = this->locate(first).range;
        return range.contains(second);
    }

    bool in_segment(const index_type segment, const index_type index) const {
        const range_type range = this->segment_range(segment);
        return range.contains(index);
    }

    index_type get_begin(const index_type segment) const {
        if (segment >= this->size()) {
            throw std::out_of_range("segment out of range");
        }

        return this->_offsets[segment];
    }

    index_type get_end(const index_type segment) const {
        if (segment >= this->size()) {
            throw std::out_of_range("segment out of range");
        }

        return this->_offsets[segment + 1];
    }

    index_type offset(const index_type segment, const index_type local_index) const {
        const range_type r = this->segment_range(segment);

        if (local_index >= r.end - r.begin) {
            throw std::out_of_range("local index out of range");
        }

        return r.begin + local_index;
    }

    // Inverse of offset(segment, local_index): buffer index -> local index within segment.
    index_type local_index(const index_type segment, const index_type buffer_index) const {
        const range_type r = this->segment_range(segment);
        return buffer_index - r.begin;
    }

    // Element -> [begin, end)
    range_type segment_range(const index_type segment) const {
        if (segment >= this->size()) {
            throw std::out_of_range("segment out of range");
        }

        const index_type begin = this->_offsets[segment];
        const index_type end = this->_offsets[segment + 1];

        return {begin, end};
    }

    index_type segment_size(const index_type segment) const {
        const range_type r = this->segment_range(segment);
        return r.end - r.begin;
    }

    // Buffer index -> owning segment (and its [begin,end)).
    locate_result locate(const index_type buffer_index) const {
        if (buffer_index >= this->total_size()) {
            throw std::out_of_range("buffer index out of range");
        }

        // First offset > buffer_index, then step back one.
        const auto it = std::upper_bound(
            this->_offsets.begin(),
            this->_offsets.end(),
            buffer_index);

        const index_type segment = static_cast<index_type>(std::distance(this->_offsets.begin(), it) - 1);

        return {segment, this->segment_range(segment)};
    }

private:
    std::vector<index_type> _offsets;
};

using OffsetTable = OffsetTable_<size_t>;
