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
    explicit TextureSet(const size_t count)
        : _textures(count) {
    }

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
        return static_cast<Id>(this->size());
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

    bool remove(const Texture &texture) {
        const auto id = this->id_of(texture);
        if (!id.has_value()) {
            return false;
        }

        erase_by_index(this->_textures, id.value());
        return true;
    }

    bool remove(const Id index) {
        if (index >= this->_textures.size()) {
            return false;
        }

        erase_by_index(this->_textures, index);
        return true;
    }

    cv::Mat &operator[](const Id index) {
        return this->_textures[index];
    }

    const cv::Mat &operator[](const Id index) const {
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
