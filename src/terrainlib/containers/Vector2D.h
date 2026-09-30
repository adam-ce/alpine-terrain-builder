/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#pragma once

#include <cstdint>
#include <vector>

template <typename T>
class Vector2D {
public:
    using index_type = uint32_t;

    Vector2D(index_type rows, index_type cols, const T &initial_value = {})
        : _rows(rows), _cols(cols) {
        this->_data.resize(rows * cols, initial_value);
    }

    T &operator()(const index_type row, const index_type col) {
        return this->_data[(row * this->_cols) + col];
    }

    const T &operator()(const index_type row, const index_type col) const {
        return this->_data[(row * this->_cols) + col];
    }

    std::span<T> get_row(const index_type row) {
        const index_type start_index = row * this->_cols;
        return std::span<T>(this->_data.data() + start_index, this->_cols);
    }
    const std::span<const T> get_row(const index_type row) const {
        const index_type start_index = row * this->_cols;
        return std::span<const T>(this->_data.data() + start_index, this->_cols);
    }

    index_type rows() const {
        return this->_rows;
    }
    index_type cols() const {
        return this->_cols;
    }
    index_type size() const {
        return this->_data.size();
    }

private:
    index_type _rows;
    index_type _cols;
    std::vector<T> _data;
};
