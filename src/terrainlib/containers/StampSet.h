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

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

// Membership over a fixed index range, cleared in constant time by moving to the next generation.
// Entries left by an earlier generation no longer match and read as absent.
template <typename Generation = uint32_t>
class StampSet_ {
public:
    explicit StampSet_(const uint32_t capacity = 0) {
        this->reset(capacity);
    }

    // Empties the set and resizes it, keeping the memory it already holds.
    void reset(const uint32_t capacity) {
        this->_stamps.resize(capacity, no_generation);

        if (this->_generation == std::numeric_limits<Generation>::max()) {
            // Out of generations, so the stale entries have to go before one is reused.
            std::ranges::fill(this->_stamps, no_generation);
            this->_generation = no_generation;
        }
        this->_generation++;
    }

    bool contains(const uint32_t index) const {
        return this->_stamps[index] == this->_generation;
    }

    // True when the index was not already in the set.
    bool insert(const uint32_t index) {
        if (this->contains(index)) {
            return false;
        }
        this->_stamps[index] = this->_generation;
        return true;
    }

    uint32_t capacity() const {
        return this->_stamps.size();
    }

private:
    static constexpr Generation no_generation = 0;

    std::vector<Generation> _stamps; // per index, the generation that last inserted it
    Generation _generation = no_generation;
};

using StampSet = StampSet_<uint32_t>;