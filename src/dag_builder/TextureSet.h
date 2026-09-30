/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
 * Copyright (C) 2026 Adam Celarek-Litofcenko
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
#include <cstddef>
#include <opencv2/core.hpp>
#include <optional>
#include <vector>

#include "ImageKey.h"

class TextureSet {
public:
    using Texture = cv::Mat;
    using Id = size_t;

    TextureSet() = default;
    TextureSet(std::vector<Texture> textures) : _textures(std::move(textures)) {}

    size_t size() const noexcept {
        return this->_textures.size();
    }

    bool empty() const noexcept {
        return this->_textures.empty();
    }

    void clear() {
        this->_textures.clear();
    }

    bool contains(const Texture &texture) const {
        return this->id_of(texture).has_value();
    }

    Id add(const Texture &texture) {
        const std::optional<Id> id = this->id_of(texture);
        if (id.has_value()) {
            return id.value();
        }
        this->_textures.push_back(texture);
        return static_cast<Id>(this->size() - 1);
    }

    std::optional<Id> id_of(const Texture &texture) const {
        const ImageKey key(texture);
        const auto it = std::find_if(
            this->_textures.begin(),
            this->_textures.end(),
            [key](const Texture &t) {
                return key == ImageKey(t);
            });

        if (it == this->_textures.end()) {
            return std::nullopt;
        }
        
        return std::distance(this->_textures.begin(), it);
    }

    auto begin() noexcept {
        return this->_textures.begin();
    }
    auto end() noexcept {
        return this->_textures.end();
    }

    auto begin() const noexcept {
        return this->_textures.begin();
    }
    auto end() const noexcept {
        return this->_textures.end();
    }

    auto cbegin() const noexcept {
        return this->_textures.cbegin();
    }
    auto cend() const noexcept {
        return this->_textures.cend();
    }

    Texture &operator[](const Id index) {
        ASSERT(index < this->size());
        return this->_textures[index];
    }

    const Texture &operator[](const Id index) const {
        ASSERT(index < this->size());
        return this->_textures[index];
    }

private:
    std::vector<Texture> _textures;
};
