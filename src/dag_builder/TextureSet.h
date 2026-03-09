#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <opencv2/core.hpp>
#include <optional>
#include <vector>

#include "vector_utils.h"

class TextureSet {
public:
    using Texture = cv::Mat;
    using Id = size_t;

    TextureSet() = default;

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

    Id add(const Texture& texture) {
        const std::optional<Id> id = this->id_of(texture);
        if (id.has_value()) {
            return id.value();
        }
        this->_textures.push_back(texture);
        return static_cast<Id>(this->size() - 1);
    }

    std::optional<Id> id_of(const Texture &texture) const {
        const auto it = std::find_if(
            this->_textures.begin(),
            this->_textures.end(),
            [this, &texture](const Texture &t) {
                return this->_mat_eq(t, texture);
            });

        if (it == this->_textures.end()) {
            return std::nullopt;
        }

        return static_cast<Id>(std::distance(this->_textures.begin(), it));
    }
    
    auto begin() noexcept { return this->_textures.begin(); }
    auto end() noexcept { return this->_textures.end(); }

    auto begin() const noexcept { return this->_textures.begin(); }
    auto end() const noexcept { return this->_textures.end(); }

    auto cbegin() const noexcept { return this->_textures.cbegin(); }
    auto cend() const noexcept { return this->_textures.cend(); }

    cv::Mat &operator[](const Id index) {
        ASSERT(index < this->size());
        return this->_textures[index];
    }

    const cv::Mat &operator[](const Id index) const {
        ASSERT(index < this->size());
        return this->_textures[index];
    }

private:
    bool _mat_eq(const cv::Mat &a, const cv::Mat &b) const {
        return a.data == b.data &&
               a.size() == b.size() &&
               a.type() == b.type();
    }

private:
    std::vector<cv::Mat> _textures;
};
