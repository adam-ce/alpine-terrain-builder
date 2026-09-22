#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <numeric>
#include <span>

#include "conversion.h"

namespace raster::algorithm {
namespace detail {
    enum class Reduction {
        Minimum,
        Maximum,
        Median,
    };

    template <Reduction operation, NumericPixel T>
    T reduce_components(std::span<const T> samples)
    {
        assert(samples.size() == 4);
        T result {};
        using S = typename PixelTraits<T>::Scalar;
        for (unsigned component = 0; component < PixelTraits<T>::components; ++component) {
            std::array<S, 4> values {};
            for (std::size_t i = 0; i < samples.size(); ++i) {
                values[i] = PixelTraits<T>::component(samples[i], component);
            }
            const auto end = values.begin() + samples.size();
            S value {};
            if constexpr (operation == Reduction::Minimum) {
                value = *std::min_element(values.begin(), end);
            } else if constexpr (operation == Reduction::Maximum) {
                value = *std::max_element(values.begin(), end);
            } else {
                std::sort(values.begin(), end);
                const S lower = values[1];
                const S upper = values[2];
                if constexpr (std::is_integral_v<S>) {
                    // midpoint rounds toward its first argument. The sign of the
                    // mathematical mean chooses the direction for halfway cases.
                    bool negative = false;
                    if constexpr (std::is_signed_v<S>) {
                        negative = upper < 0 || (lower < 0 && lower < -upper);
                    }
                    value = negative ? std::midpoint(lower, upper) : std::midpoint(upper, lower);
                } else {
                    value = std::midpoint(lower, upper);
                }
            }
            PixelTraits<T>::component(result, component) = value;
        }
        return result;
    }
} // namespace detail

struct Min {
    template <detail::NumericPixel T>
    T operator()(std::span<const T> samples) const
    {
        return detail::reduce_components<detail::Reduction::Minimum>(samples);
    }
};

struct Max {
    template <detail::NumericPixel T>
    T operator()(std::span<const T> samples) const
    {
        return detail::reduce_components<detail::Reduction::Maximum>(samples);
    }
};

struct Median {
    template <detail::NumericPixel T>
    T operator()(std::span<const T> samples) const
    {
        return detail::reduce_components<detail::Reduction::Median>(samples);
    }
};

/// Most frequent scalar-like value; ties choose the first occurrence, including zero.
struct Mode {
    template <typename T>
    T operator()(std::span<const T> samples) const
    {
        assert(samples.size() == 4);
        std::size_t best = 0;
        unsigned best_count = 0;
        for (std::size_t i = 0; i < samples.size(); ++i) {
            unsigned count = 1;
            for (std::size_t j = 0; j < samples.size(); ++j) {
                if (i != j && samples[i] == samples[j]) {
                    ++count;
                }
            }
            if (count > best_count) {
                best = i;
                best_count = count;
            }
        }
        return samples[best];
    }
};
} // namespace raster::algorithm
