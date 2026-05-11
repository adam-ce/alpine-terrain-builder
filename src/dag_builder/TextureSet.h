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
            [this, key](const Texture &t) {
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
