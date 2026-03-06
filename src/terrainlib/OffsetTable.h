#pragma once

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

template <typename Index = size_t>
class OffsetTable {
public:
    using index_type = Index;

    struct range_type {
        index_type begin; // inclusive
        index_type end; // exclusive
    };

    struct locate_result {
        index_type element;
        range_type range;
    };

    OffsetTable() : _offsets{0} {}
    
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

    // Append an element given its length.
    void append_length(const index_type length) {
        const index_type next_offset = this->_offsets.back() + length;
        this->_offsets.push_back(next_offset);
    }

    // Append multiple elements given their lengths.
    void append_lengths(const std::span<const index_type> lengths) {
        const size_t current_size = this->_offsets.size();
        this->_offsets.reserve(current_size + lengths.size());

        for (const index_type length : lengths) {
            this->append_length(length);
        }
    }

    // Modifies the start of an element, shrinking or extending the previous one accordingly.
    void set_begin(const index_type element, const index_type new_begin) {
        if (element == 0) {
            throw std::out_of_range("cannot modify begin of first element");
        }
        if (element > this->size()) {
            throw std::out_of_range("element out of range");
        }

        const index_type prev_boundary = this->_offsets[element - 1];
        const index_type next_boundary = this->_offsets[element + 1];
        if (new_begin < prev_boundary || new_begin > next_boundary) {
            throw std::invalid_argument("new_begin out of valid adjacent boundaries");
        }

        this->_offsets[element] = new_begin;
    }

    // Modifies the end of an element, shrinking or extending the next one accordingly (if present).
    void set_end(const index_type element, const index_type new_end) {
        if (element >= this->size()) {
            throw std::out_of_range("element out of range");
        }

        const index_type prev_boundary = this->_offsets[element];
        if (element + 1 < this->size()) {
            const index_type next_boundary = this->_offsets[element + 2];
            if (new_end < prev_boundary || new_end > next_boundary) {
                throw std::invalid_argument("new_end out of valid adjacent boundaries");
            }
        } else {
            if (new_end < prev_boundary) {
                throw std::invalid_argument("new_end cannot be less than section begin");
            }
        }

        this->_offsets[element + 1] = new_end;
    }

    // Element -> [begin, end)
    range_type range(const index_type element) const {
        if (element >= this->size()) {
            throw std::out_of_range("element out of range");
        }

        const index_type begin = this->_offsets[element];
        const index_type end = this->_offsets[element + 1];

        return {begin, end};
    }

    index_type element_size(const index_type element) const {
        const range_type r = this->range(element);
        return r.end - r.begin;
    }

    // Buffer index -> owning element (and its [begin,end)).
    locate_result locate(const index_type buffer_index) const {
        if (buffer_index >= this->total_size()) {
            throw std::out_of_range("buffer index out of range");
        }

        // First offset > buffer_index, then step back one.
        const auto it = std::upper_bound(
            this->_offsets.begin(),
            this->_offsets.end(),
            buffer_index);

        const index_type element = static_cast<index_type>(std::distance(this->_offsets.begin(), it) - 1);

        return {element, this->range(element)};
    }

private:
    std::vector<index_type> _offsets;
};
