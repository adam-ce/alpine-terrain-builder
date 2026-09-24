#include "image.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <new>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "bytes.h"
#include "conversion.h"
#include "raster/algorithm/transform.h"

namespace io::image {
namespace {

    Expected<void> validate_encoding(Format format, int channels, EncodeOptions options)
    {
        if (options.jpeg_quality < 0 || options.jpeg_quality > 100 || options.png_compression < 0 || options.png_compression > 9) {
            return Error::fail(Error::Code::InvalidInput, "JPEG quality must be 0..100 and PNG compression 0..9");
        }
        if (format != Format::Jpeg && format != Format::Png) {
            return Error::fail(Error::Code::Unsupported, "unsupported image format");
        }
        if (format == Format::Jpeg && channels == 4) {
            return Error::fail(Error::Code::Unsupported, "JPEG does not support alpha; remove alpha explicitly");
        }
        return {};
    }

    template <typename Pixel>
    Pixel swap_red_blue(Pixel pixel)
    {
        std::swap(pixel.x, pixel.z);
        return pixel;
    }

    template <typename Pixel>
    Expected<std::vector<std::uint8_t>> encode_raster(const radix::Raster<Pixel>& image, Format format, EncodeOptions options)
    try {
        if (auto valid = validate_encoding(format, Pixel::length(), options); !valid) {
            return Error::propagate(std::move(valid));
        }
        auto bgr = raster::algorithm::transform(image, swap_red_blue<Pixel>);
        if (!bgr) {
            return Error::propagate(std::move(bgr));
        }
        auto converted = conversion::to_mat(*bgr);
        if (!converted) {
            return Error::propagate(std::move(converted));
        }
        std::vector<std::uint8_t> bytes;
        const auto parameters = format == Format::Jpeg ? std::vector<int> { cv::IMWRITE_JPEG_QUALITY, options.jpeg_quality }
                                                       : std::vector<int> { cv::IMWRITE_PNG_COMPRESSION, options.png_compression };
        if (!cv::imencode(format == Format::Jpeg ? ".jpg" : ".png", *converted, bytes, parameters)) {
            return Error::fail(Error::Code::Io, "image encoding failed");
        }
        return bytes;
    } catch (const cv::Exception& error) {
        return Error::fail(error.code == cv::Error::StsNoMem ? Error::Code::ResourceExhausted : Error::Code::Io, "encode image: " + error.msg);
    } catch (const std::bad_alloc&) {
        return Error::fail(Error::Code::ResourceExhausted, "encode image");
    }

    template <typename Pixel>
    Expected<radix::Raster<Pixel>> decode_raster(std::span<const std::uint8_t> bytes)
    try {
        if (bytes.empty() || bytes.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
            return Error::fail(Error::Code::CorruptData, "empty or oversized encoded image");
        }
        const cv::Mat buffer(1, static_cast<int>(bytes.size()), CV_8UC1, const_cast<std::uint8_t*>(bytes.data()));
        // Preserve alpha and stored row order, without applying EXIF orientation.
        auto decoded = cv::imdecode(buffer, cv::IMREAD_UNCHANGED);
        if (decoded.empty()) {
            return Error::fail(Error::Code::CorruptData, "image decoding failed");
        }
        if (decoded.depth() != CV_8U) {
            return Error::fail(Error::Code::Unsupported, "RGB8/RGBA8 reading requires 8-bit image samples");
        }
        constexpr bool alpha = std::same_as<Pixel, glm::u8vec4>;
        switch (decoded.channels()) {
        case 1:
            cv::cvtColor(decoded, decoded, alpha ? cv::COLOR_GRAY2BGRA : cv::COLOR_GRAY2BGR);
            break;
        case 3:
            if constexpr (alpha) {
                cv::cvtColor(decoded, decoded, cv::COLOR_BGR2BGRA);
            }
            break;
        case 4:
            if constexpr (!alpha) {
                cv::cvtColor(decoded, decoded, cv::COLOR_BGRA2BGR);
            }
            break;
        default:
            return Error::fail(Error::Code::Unsupported, "unsupported image channel count");
        }
        auto raster = conversion::to_raster<Pixel>(decoded);
        if (!raster) {
            return Error::propagate(std::move(raster));
        }
        if (auto converted = raster::algorithm::transform(*raster, swap_red_blue<Pixel>, *raster); !converted) {
            return Error::propagate(std::move(converted));
        }
        return raster;
    } catch (const cv::Exception& error) {
        return Error::fail(error.code == cv::Error::StsNoMem ? Error::Code::ResourceExhausted : Error::Code::CorruptData, "decode image: " + error.msg);
    } catch (const std::bad_alloc&) {
        return Error::fail(Error::Code::ResourceExhausted, "decode image");
    }

    template <typename Pixel>
    Expected<radix::Raster<Pixel>> read_raster(const std::filesystem::path& path)
    try {
        auto bytes = read_bytes_from_path(path);
        if (!bytes) {
            return Error::propagate(std::move(bytes));
        }
        return decode_raster<Pixel>(*bytes);
    } catch (const std::bad_alloc&) {
        return Error::fail(Error::Code::ResourceExhausted, "read image", path);
    }

    Expected<Format> format_from_extension(const std::filesystem::path& path)
    {
        auto extension = path.extension().string();
        std::ranges::transform(extension, extension.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        if (extension == ".jpg" || extension == ".jpeg") {
            return Format::Jpeg;
        }
        if (extension == ".png") {
            return Format::Png;
        }
        return Error::fail(Error::Code::Unsupported, "unsupported image extension", path);
    }

    template <typename Pixel>
    Expected<void> write_image(const radix::Raster<Pixel>& image, const std::filesystem::path& path, WriteOptions options)
    {
        auto format = format_from_extension(path);
        if (!format) {
            return Error::propagate(std::move(format));
        }
        std::error_code error;
        const auto status = std::filesystem::symlink_status(path, error);
        if (error && error != std::errc::no_such_file_or_directory) {
            return Error::fail(Error::Code::Io, "inspect image output", path, error);
        }
        if (std::filesystem::exists(status)) {
            if (!options.overwrite) {
                return Error::fail(Error::Code::AlreadyExists, "image output exists", path);
            }
            if (!std::filesystem::is_regular_file(status)) {
                return Error::fail(Error::Code::InvalidInput, "image output must be a regular file", path);
            }
        }
        auto bytes = encode(image, *format, options.encoding);
        if (!bytes) {
            return Error::propagate(std::move(bytes), "encode image for " + path.string());
        }
        return write_bytes_to_path(*bytes, path, options.overwrite ? WriteMode::Overwrite : WriteMode::CreateNew, options.make_dirs);
    }

} // namespace

Expected<std::vector<std::uint8_t>> encode(const RGB8& image, Format format, EncodeOptions options) { return encode_raster(image, format, options); }
Expected<std::vector<std::uint8_t>> encode(const RGBA8& image, Format format, EncodeOptions options) { return encode_raster(image, format, options); }
Expected<RGB8> decode_rgb8(std::span<const std::uint8_t> bytes) { return decode_raster<glm::u8vec3>(bytes); }
Expected<RGBA8> decode_rgba8(std::span<const std::uint8_t> bytes) { return decode_raster<glm::u8vec4>(bytes); }
Expected<RGB8> read_rgb8(const std::filesystem::path& path) { return read_raster<glm::u8vec3>(path); }
Expected<RGBA8> read_rgba8(const std::filesystem::path& path) { return read_raster<glm::u8vec4>(path); }
Expected<void> write(const RGB8& image, const std::filesystem::path& path, WriteOptions options) { return write_image(image, path, options); }
Expected<void> write(const RGBA8& image, const std::filesystem::path& path, WriteOptions options) { return write_image(image, path, options); }

} // namespace io::image
