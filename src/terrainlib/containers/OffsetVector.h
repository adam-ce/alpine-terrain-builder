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

#include <libassert/assert.hpp>

template <typename T>
struct OffsetVector {
    std::vector<T> data;
    size_t offset = 0;

    OffsetVector() = default;
    OffsetVector(std::vector<T> data, size_t offset = 0)
        : data(std::move(data)), offset(offset) {}

    [[nodiscard]] size_t size() const {
        return this->data.size();
    }
    [[nodiscard]] bool empty() const {
        return this->data.empty();
    }

    void resize(size_t new_size, const T& default_value={}) {
        this->data.resize(new_size, default_value);
    }

    [[nodiscard]] bool contains(size_t index) const {
        return index >= this->offset && index < this->offset + this->size();
    }

    [[nodiscard]] T &operator[](size_t index) {
        DEBUG_ASSERT(this->contains(index));
        return this->data[index - this->offset];
    }

    [[nodiscard]] const T &operator[](size_t index) const {
        DEBUG_ASSERT(this->contains(index));
        return this->data[index - this->offset];
    }

    [[nodiscard]] auto begin() {
        return this->data.begin();
    }
    [[nodiscard]] auto end() {
        return this->data.end();
    }
    [[nodiscard]] auto begin() const {
        return this->data.begin();
    }
    [[nodiscard]] auto end() const {
        return this->data.end();
    }
};
