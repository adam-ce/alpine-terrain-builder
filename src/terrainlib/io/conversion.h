#pragma once

#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <new>
#include <type_traits>

#include <glm/glm.hpp>
#include <opencv2/core.hpp>
#include <radix/raster.h>

namespace io::conversion {

enum class ErrorCode {
    EmptyInput,
    UnsupportedDimensions,
    UnsupportedPixelType,
    PixelTypeMismatch,
    DimensionsOutOfRange,
    AllocationFailed,
    OpenCvFailure,
};

struct Error {
    ErrorCode code;
    int expected_cv_type { -1 };
    int actual_cv_type { -1 };

    constexpr bool operator==(const Error&) const = default;
};

namespace detail {

    template <typename Channel>
    inline constexpr int cv_depth = -1;

    template <>
    inline constexpr int cv_depth<std::uint8_t> = CV_8U;
    template <>
    inline constexpr int cv_depth<std::int8_t> = CV_8S;
    template <>
    inline constexpr int cv_depth<std::uint16_t> = CV_16U;
    template <>
    inline constexpr int cv_depth<std::int16_t> = CV_16S;
    template <>
    inline constexpr int cv_depth<std::int32_t> = CV_32S;
    template <>
    inline constexpr int cv_depth<float> = CV_32F;
    template <>
    inline constexpr int cv_depth<double> = CV_64F;

    template <typename Pixel>
    struct PixelTraits {
        using channel_type = std::remove_cv_t<Pixel>;
        static constexpr int channels = 1;
        static constexpr bool supported = cv_depth<channel_type> >= 0;

        static channel_type& channel(Pixel& pixel, const int) { return pixel; }

        static const channel_type& channel(const Pixel& pixel, const int) { return pixel; }
    };

    template <glm::length_t Length, typename Channel, glm::qualifier Qualifier>
    struct PixelTraits<glm::vec<Length, Channel, Qualifier>> {
        using channel_type = Channel;
        static constexpr int channels = static_cast<int>(Length);
        static constexpr bool supported = channels >= 2 && channels <= 4 && cv_depth<channel_type> >= 0;

        static channel_type& channel(glm::vec<Length, Channel, Qualifier>& pixel, const int index) { return pixel[static_cast<glm::length_t>(index)]; }

        static const channel_type& channel(const glm::vec<Length, Channel, Qualifier>& pixel, const int index)
        {
            return pixel[static_cast<glm::length_t>(index)];
        }
    };

    template <typename Pixel>
    using Traits = PixelTraits<std::remove_cv_t<Pixel>>;

    template <typename Pixel>
    inline constexpr int cv_type = Traits<Pixel>::supported ? CV_MAKETYPE(cv_depth<typename Traits<Pixel>::channel_type>, Traits<Pixel>::channels) : -1;

    template <typename Pixel>
    inline constexpr bool has_packed_layout = Traits<Pixel>::supported
        && std::is_trivially_copyable_v<Pixel> && sizeof(Pixel) == sizeof(typename Traits<Pixel>::channel_type) * Traits<Pixel>::channels;

    template <typename Pixel>
    void copy_mat_to_raster(const cv::Mat& source, radix::Raster<Pixel>& destination)
    {
        using PixelInfo = Traits<Pixel>;
        using Channel = typename PixelInfo::channel_type;

        const auto row_bytes = static_cast<std::size_t>(source.cols) * sizeof(Pixel);
        if constexpr (has_packed_layout<Pixel>) {
            for (int row = 0; row < source.rows; ++row) {
                std::memcpy(destination.data() + static_cast<std::size_t>(row) * source.cols, source.ptr(row), row_bytes);
            }
        } else {
            for (int row = 0; row < source.rows; ++row) {
                const Channel* source_row = source.ptr<Channel>(row);
                for (int column = 0; column < source.cols; ++column) {
                    Pixel& pixel = destination.pixel({
                        static_cast<unsigned>(column),
                        static_cast<unsigned>(row),
                    });
                    for (int channel = 0; channel < PixelInfo::channels; ++channel) {
                        PixelInfo::channel(pixel, channel) = source_row[column * PixelInfo::channels + channel];
                    }
                }
            }
        }
    }

    template <typename Pixel>
    void copy_raster_to_mat(const radix::Raster<Pixel>& source, cv::Mat& destination)
    {
        using PixelInfo = Traits<Pixel>;
        using Channel = typename PixelInfo::channel_type;

        const auto row_bytes = static_cast<std::size_t>(destination.cols) * sizeof(Pixel);
        if constexpr (has_packed_layout<Pixel>) {
            for (int row = 0; row < destination.rows; ++row) {
                std::memcpy(destination.ptr(row), source.data() + static_cast<std::size_t>(row) * destination.cols, row_bytes);
            }
        } else {
            for (int row = 0; row < destination.rows; ++row) {
                Channel* destination_row = destination.ptr<Channel>(row);
                for (int column = 0; column < destination.cols; ++column) {
                    const Pixel& pixel = source.pixel({
                        static_cast<unsigned>(column),
                        static_cast<unsigned>(row),
                    });
                    for (int channel = 0; channel < PixelInfo::channels; ++channel) {
                        destination_row[column * PixelInfo::channels + channel] = PixelInfo::channel(pixel, channel);
                    }
                }
            }
        }
    }

} // namespace detail

template <typename Pixel>
std::expected<radix::Raster<Pixel>, Error> to_raster(const cv::Mat& source)
{
    if constexpr (!detail::Traits<Pixel>::supported) {
        return std::unexpected(Error { ErrorCode::UnsupportedPixelType });
    } else {
        if (source.empty()) {
            return std::unexpected(Error { ErrorCode::EmptyInput });
        }
        if (source.dims != 2) {
            return std::unexpected(Error { ErrorCode::UnsupportedDimensions });
        }
        if (source.type() != detail::cv_type<Pixel>) {
            return std::unexpected(Error {
                ErrorCode::PixelTypeMismatch,
                detail::cv_type<Pixel>,
                source.type(),
            });
        }
        if (source.cols < 0 || source.rows < 0 || static_cast<unsigned long long>(source.cols) > std::numeric_limits<unsigned>::max()
            || static_cast<unsigned long long>(source.rows) > std::numeric_limits<unsigned>::max()) {
            return std::unexpected(Error { ErrorCode::DimensionsOutOfRange });
        }

        try {
            radix::Raster<Pixel> destination({
                static_cast<unsigned>(source.cols),
                static_cast<unsigned>(source.rows),
            });
            detail::copy_mat_to_raster(source, destination);
            return destination;
        } catch (const std::bad_alloc&) {
            return std::unexpected(Error { ErrorCode::AllocationFailed });
        } catch (const cv::Exception&) {
            return std::unexpected(Error { ErrorCode::OpenCvFailure });
        }
    }
}

template <typename Pixel>
std::expected<cv::Mat, Error> to_mat(const radix::Raster<Pixel>& source)
{
    if constexpr (!detail::Traits<Pixel>::supported) {
        return std::unexpected(Error { ErrorCode::UnsupportedPixelType });
    } else {
        if (source.width() == 0 || source.height() == 0) {
            return std::unexpected(Error { ErrorCode::EmptyInput });
        }
        if (source.width() > static_cast<unsigned>(std::numeric_limits<int>::max())
            || source.height() > static_cast<unsigned>(std::numeric_limits<int>::max())) {
            return std::unexpected(Error { ErrorCode::DimensionsOutOfRange });
        }

        try {
            cv::Mat destination(static_cast<int>(source.height()), static_cast<int>(source.width()), detail::cv_type<Pixel>);
            detail::copy_raster_to_mat(source, destination);
            return destination;
        } catch (const std::bad_alloc&) {
            return std::unexpected(Error { ErrorCode::AllocationFailed });
        } catch (const cv::Exception&) {
            return std::unexpected(Error { ErrorCode::OpenCvFailure });
        }
    }
}

} // namespace io::conversion
