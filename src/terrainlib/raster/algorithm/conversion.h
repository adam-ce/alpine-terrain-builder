#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <functional>
#include <limits>
#include <tuple>
#include <type_traits>

#include <glm/glm.hpp>

namespace raster::algorithm {

namespace detail {
    template <typename T>
    struct PixelTraits {
        using Scalar = T;
        template <typename S>
        using Rebind = S;
        static constexpr unsigned components = 1;
        static T& component(T& pixel, unsigned) { return pixel; }
        static const T& component(const T& pixel, unsigned) { return pixel; }
    };

    template <glm::length_t N, typename T, glm::qualifier Q>
    struct PixelTraits<glm::vec<N, T, Q>> {
        using Scalar = T;
        template <typename S>
        using Rebind = glm::vec<N, S, Q>;
        static constexpr unsigned components = N;
        static T& component(glm::vec<N, T, Q>& pixel, unsigned i) { return pixel[static_cast<glm::length_t>(i)]; }
        static const T& component(const glm::vec<N, T, Q>& pixel, unsigned i) { return pixel[static_cast<glm::length_t>(i)]; }
    };

    template <typename T>
    concept NumericPixel = std::same_as<T, std::remove_cvref_t<T>>
        && ((std::is_integral_v<typename PixelTraits<T>::Scalar> && !std::same_as<typename PixelTraits<T>::Scalar, bool>
                && sizeof(typename PixelTraits<T>::Scalar) <= 8)
            || std::is_floating_point_v<typename PixelTraits<T>::Scalar>);

    template <typename T>
    concept FloatingPixel = NumericPixel<T> && std::is_floating_point_v<typename PixelTraits<T>::Scalar>;

    template <typename T>
    using LinearScalar = std::conditional_t<std::same_as<T, float> || (std::is_integral_v<T> && sizeof(T) <= 2),
        float,
        std::conditional_t<std::same_as<T, double> || sizeof(T) <= 4, double, long double>>;

    template <typename T>
    using LinearPixel = typename PixelTraits<T>::template Rebind<LinearScalar<typename PixelTraits<T>::Scalar>>;

    template <typename T>
    inline constexpr bool srgb_pixel
        = std::same_as<typename PixelTraits<T>::Scalar, std::uint8_t> && (PixelTraits<T>::components == 3 || PixelTraits<T>::components == 4);

    template <typename T>
    struct IsConversionTuple : std::false_type { };

    template <typename Decoder, typename Encoder>
    struct IsConversionTuple<std::tuple<Decoder, Encoder>> : std::true_type { };

    template <typename Conversion, typename T>
    using DecodedPixel = std::invoke_result_t<const std::tuple_element_t<0, Conversion>&, const T&>;

    template <typename Conversion, typename T>
    concept PixelConversion = IsConversionTuple<Conversion>::value && std::invocable<const std::tuple_element_t<0, Conversion>&, const T&>
        && NumericPixel<DecodedPixel<Conversion, T>> && std::invocable<const std::tuple_element_t<1, Conversion>&, const DecodedPixel<Conversion, T>&>
        && std::same_as<std::invoke_result_t<const std::tuple_element_t<1, Conversion>&, const DecodedPixel<Conversion, T>&>, T>;

    template <typename Conversion, typename T>
    concept ScalingConversion = PixelConversion<Conversion, T> && FloatingPixel<DecodedPixel<Conversion, T>>;

    template <typename T, typename W>
    T encode_linear(const W& pixel)
    {
        using S = typename PixelTraits<T>::Scalar;
        using F = typename PixelTraits<W>::Scalar;
        T result {};
        for (unsigned i = 0; i < PixelTraits<T>::components; ++i) {
            const F value = PixelTraits<W>::component(pixel, i);
            if constexpr (std::is_integral_v<S>) {
                const F rounded = std::round(value);
                // Check before conversion: an integer maximum can round up when
                // represented by the floating working type.
                if (rounded <= static_cast<F>((std::numeric_limits<S>::lowest)())) {
                    PixelTraits<T>::component(result, i) = (std::numeric_limits<S>::lowest)();
                } else if (rounded >= static_cast<F>((std::numeric_limits<S>::max)())) {
                    PixelTraits<T>::component(result, i) = (std::numeric_limits<S>::max)();
                } else {
                    PixelTraits<T>::component(result, i) = static_cast<S>(rounded);
                }
            } else {
                PixelTraits<T>::component(result, i) = static_cast<S>(value);
            }
        }
        return result;
    }
} // namespace detail

/// No conversion; reduce uses this tuple by default.
inline auto identity_conversion()
{
    const auto identity = [](const auto& value) { return value; };
    return std::tuple { identity, identity };
}

/// Numeric conversion with floating working components and rounded/clamped integer output.
template <detail::NumericPixel T>
auto linear_conversion()
{
    using W = detail::LinearPixel<T>;
    return std::tuple { [](const T& value) { return W(value); }, [](const W& value) { return detail::encode_linear<T>(value); } };
}

/// RGB(A) uint8 conversion to linear float values; alpha remains independent and linear.
template <typename T>
requires detail::srgb_pixel<T>
auto srgb_conversion()
{
    using W = detail::LinearPixel<T>;
    return std::tuple { [](const T& pixel) {
                           W result(pixel);
                           for (unsigned i = 0; i < detail::PixelTraits<T>::components; ++i) {
                               auto& value = detail::PixelTraits<W>::component(result, i);
                               value /= 255.f;
                               if (i < 3) {
                                   value = value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
                               }
                           }
                           return result;
                       },
        [](const W& pixel) {
            W result {};
            for (unsigned i = 0; i < detail::PixelTraits<T>::components; ++i) {
                float value = std::clamp(detail::PixelTraits<W>::component(pixel, i), 0.f, 1.f);
                if (i < 3) {
                    value = value <= 0.0031308f ? 12.92f * value : 1.055f * std::pow(value, 1.f / 2.4f) - 0.055f;
                }
                detail::PixelTraits<W>::component(result, i) = value * 255.f;
            }
            return detail::encode_linear<T>(result);
        } };
}

} // namespace raster::algorithm
