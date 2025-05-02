#pragma once

#include <algorithm>
#include <vector>
#include <ranges>
#include <glm/glm.hpp>

namespace raster {

using Index = size_t;
using Coords = glm::vec<2, size_t>;

template <typename T>
class Raster {
public:
    Raster() = default;
    Raster(Index width, Index height)
        : _width(width), _height(height), _data(width * height) {
    }

    [[nodiscard]] Index width() const {
        return this->_width;
    }
    [[nodiscard]] Index height() const {
        return this->_height;
    }
    [[nodiscard]] decltype(auto) pixel(const Coords &coords) {
        assert(coords.x < this->_width);
        assert(coords.y < this->_height);
        const Index pixel_index = this->index(coords);
        if constexpr (std::is_same_v<T, bool>) {
            return static_cast<bool>(this->_data[pixel_index]);
        } else {
            return static_cast<T &>(this->_data[pixel_index]);
        }
    }
    [[nodiscard]] decltype(auto) pixel(const Coords &coords) const {
        assert(coords.x < this->_width);
        assert(coords.y < this->_height);
        const Index pixel_index = this->index(coords);
        if constexpr (std::is_same_v<T, bool>) {
            return static_cast<bool>(this->_data[pixel_index]);
        } else {
            return static_cast<const T &>(this->_data[pixel_index]);
        }
    }

    [[nodiscard]] Index index(const Coords& coords) const {
        return coords.y * this->_width + coords.x;
    }

    [[nodiscard]] T *data() {
        return this->_data.data();
    }
    [[nodiscard]] const T *data() const {
        return this->_data.data();
    }

    [[nodiscard]] Index size() const {
        return this->_data.size();
    }
    [[nodiscard]] auto begin() {
        return this->_data.begin();
    }
    [[nodiscard]] auto end() {
        return this->_data.end();
    }
    [[nodiscard]] auto begin() const {
        return this->_data.begin();
    }
    [[nodiscard]] auto end() const {
        return this->_data.end();
    }

private:
    unsigned _width = 0;
    unsigned _height = 0;
    std::vector<T> _data;
};

using Mask = Raster<bool>;
using HeightMap = Raster<float>;

template <typename F, typename In>
concept TransformFn = requires(F f, In in) {
    { f(in) };
};

template <typename F, typename In>
concept TransformFnWithCoords = requires(F f, In in, Coords coord) {
    { f(in, coord) };
};

template <typename In, TransformFn<In> F>
[[nodiscard]] auto transform(const Raster<In> &input, F &&f) {
    using Out = decltype(f(input.pixel(Coords(0, 0))));
    Raster<Out> output(input.width(), input.height());
    std::transform(input.begin(), input.end(), output.begin(), std::forward<F>(f));
    return output;
}

template <typename In, TransformFnWithCoords<In> F>
[[nodiscard]] auto transform(const Raster<In> &input, F &&f) {
    using Out = decltype(f(input.pixel(Coords(0, 0), Coords(0, 0))));
    Raster<Out> output(input.width(), input.height());
    for (Index y = 0; y < input.height(); ++y) {
        for (Index x = 0; x < input.width(); ++x) {
            Coords coords(x, y);
            output.pixel(coords) = std::forward<F>(f)(input.pixel(coords), coords);
        }
    }
    return output;
}

template <typename In, TransformFn<In> F>
[[nodiscard]] auto transform(const Raster<In> &input, const Mask &mask, F &&f) {
    using Out = decltype(f(input.pixel(Coords(0, 0))));
    Raster<Out> output(input.width(), input.height());
    for (Index y = 0; y < input.height(); ++y) {
        for (Index x = 0; x < input.width(); ++x) {
            Coords coords(x, y);
            if (mask.pixel(coords)) {
                output.pixel(coords) = std::forward<F>(f)(input.pixel(coords));
            }
        }
    }
    return output;
}

template <typename In, TransformFnWithCoords<In> F>
[[nodiscard]] auto transform(const Raster<In> &input, const Mask &mask, F &&f) {
    using Out = decltype(f(input.pixel(Coords(0, 0)), Coords(0, 0)));
    Raster<Out> output(input.width(), input.height());
    for (Index y = 0; y < input.height(); ++y) {
        for (Index x = 0; x < input.width(); ++x) {
            Coords coords(x, y);
            if (mask.pixel(coords)) {
                output.pixel(coords) = std::forward<F>(f)(input.pixel(coords), coords);
            }
        }
    }
    return output;
}
}