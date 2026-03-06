#pragma once

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

class OffsetTable {
public:
    using index_type = size_t;

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
        this->_offsets.reserve(n);
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

    const std::vector<index_type> &offsets() const noexcept {
        return this->_offsets;
    }

    // Append an element given its length (must be > 0).
    void append_length(const index_type length) {
        if (length == 0) {
            throw std::invalid_argument("length must be > 0");
        }

        const index_type next = this->_offsets.back() + length;
        this->_offsets.push_back(next);
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

        const index_type element = static_cast<index_type>((it - this->_offsets.begin()) - 1);

        return {element, this->range(element)};
    }

private:
    std::vector<index_type> _offsets;
};
