/*****************************************************************************
 * AlpineMaps.org
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

#include "image.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <string>

#include <jxl/decode.h>
#include <jxl/decode_cxx.h>
#include <jxl/encode.h>
#include <jxl/encode_cxx.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <turbojpeg.h>

#include "byte.h"
#include "conversion.h"
#include "raster/algorithm/transform.h"

namespace io::image {
namespace {

    Expected<void> validate_encoding(Format format, int channels, EncodeOptions options)
    {
        if (options.jpeg_quality < 0 || options.jpeg_quality > 100 || options.png_compression < 0 || options.png_compression > 9) {
            return Error::fail(Error::Code::InvalidInput, "JPEG quality must be 0..100 and PNG compression 0..9");
        }
        if (!(options.jpegxl_distance >= 0 && options.jpegxl_distance <= 25) || options.jpegxl_effort < 1 || options.jpegxl_effort > 10) {
            return Error::fail(Error::Code::InvalidInput, "JPEG XL distance must be 0..25 and effort 1..10");
        }
        if (format != Format::Jpeg && format != Format::Png && format != Format::JpegXl) {
            return Error::fail(Error::Code::Unsupported, "unsupported image format");
        }
        if (format == Format::Jpeg && channels == 4) {
            return Error::fail(Error::Code::Unsupported, "JPEG does not support alpha; remove alpha explicitly");
        }
        return {};
    }

    constexpr auto swap_red_blue = [](auto pixel) {
        std::swap(pixel.x, pixel.z);
        return pixel;
    };

    template <typename Pixel>
    Expected<byte::vector> encode_png(const radix::Raster<Pixel>& image, EncodeOptions options)
    try {
        auto bgr = raster::algorithm::transform(image, swap_red_blue);
        if (!bgr) {
            return Error::propagate(std::move(bgr));
        }
        auto converted = conversion::to_mat(*bgr);
        if (!converted) {
            return Error::propagate(std::move(converted));
        }
        std::vector<uchar> encoded;
        if (!cv::imencode(".png", *converted, encoded, { cv::IMWRITE_PNG_COMPRESSION, options.png_compression })) {
            return Error::fail(Error::Code::Io, "image encoding failed");
        }
        // OpenCV only encodes into a std::vector<uchar>.
        const auto bytes = std::as_bytes(std::span(encoded));
        return byte::vector(bytes.begin(), bytes.end());
    } catch (const cv::Exception& error) {
        return Error::fail(error.code == cv::Error::StsNoMem ? Error::Code::ResourceExhausted : Error::Code::Io, "encode image: " + error.msg);
    } catch (const std::bad_alloc&) {
        return Error::fail(Error::Code::ResourceExhausted, "encode image");
    }

    using TurboJpeg = std::unique_ptr<void, decltype(&tj3Destroy)>;
    using TurboJpegBuffer = std::unique_ptr<unsigned char, decltype(&tj3Free)>;

    // Baseline 4:2:0 JPEG, compressed straight from the raster. No restart markers: any restart interval
    // switches off libjpeg-turbo's fast Huffman decoder for the whole image (about 13% slower decoding).
    Expected<byte::vector> encode_jpeg(const radix::Raster<glm::u8vec3>& image, EncodeOptions options)
    try {
        if (image.width() == 0 || image.height() == 0 || image.width() > static_cast<unsigned>(std::numeric_limits<int>::max())
            || image.height() > static_cast<unsigned>(std::numeric_limits<int>::max())) {
            return Error::fail(Error::Code::InvalidInput, "JPEG needs a non-empty image with dimensions in the int range");
        }
        const TurboJpeg compressor(tj3Init(TJINIT_COMPRESS), &tj3Destroy);
        if (!compressor) {
            return Error::fail(Error::Code::ResourceExhausted, "create JPEG encoder");
        }
        // libjpeg maps quality 0 to 1; TurboJPEG rejects it.
        if (tj3Set(compressor.get(), TJPARAM_QUALITY, std::max(options.jpeg_quality, 1)) != 0 || tj3Set(compressor.get(), TJPARAM_SUBSAMP, TJSAMP_420) != 0) {
            return Error::fail(Error::Code::Internal, std::string("configure JPEG encoder: ") + tj3GetErrorStr(compressor.get()));
        }
        unsigned char* output = nullptr;
        std::size_t output_size = 0;
        const int result = tj3Compress8(compressor.get(),
            reinterpret_cast<const unsigned char*>(image.data()),
            static_cast<int>(image.width()),
            0,
            static_cast<int>(image.height()),
            TJPF_RGB,
            &output,
            &output_size);
        const TurboJpegBuffer encoded(output, &tj3Free);
        if (result != 0) {
            return Error::fail(Error::Code::Io, std::string("JPEG encoding failed: ") + tj3GetErrorStr(compressor.get()));
        }
        const auto bytes = std::as_bytes(std::span(encoded.get(), output_size));
        return byte::vector(bytes.begin(), bytes.end());
    } catch (const std::bad_alloc&) {
        return Error::fail(Error::Code::ResourceExhausted, "encode JPEG image");
    }

    template <typename Pixel>
    Expected<byte::vector> encode_jpegxl(const radix::Raster<Pixel>& image, EncodeOptions options)
    try {
        if (image.width() == 0 || image.height() == 0) {
            return Error::fail(Error::Code::InvalidInput, "JPEG XL cannot encode an empty image");
        }
        const auto encoder = JxlEncoderMake(nullptr);
        if (!encoder) {
            return Error::fail(Error::Code::ResourceExhausted, "create JPEG XL encoder");
        }
        const auto failed = [&encoder] {
            return Error::fail(
                JxlEncoderGetError(encoder.get()) == JXL_ENC_ERR_OOM ? Error::Code::ResourceExhausted : Error::Code::Io, "JPEG XL encoding failed");
        };
        const bool lossless = options.jpegxl_distance == 0;
        JxlBasicInfo info;
        JxlEncoderInitBasicInfo(&info);
        info.xsize = image.width();
        info.ysize = image.height();
        info.bits_per_sample = 8;
        info.num_color_channels = 3;
        info.alpha_bits = Pixel::length() == 4 ? 8 : 0;
        info.num_extra_channels = Pixel::length() == 4 ? 1 : 0;
        // Lossy coding converts to XYB; lossless coding must keep the original colour space.
        info.uses_original_profile = lossless ? JXL_TRUE : JXL_FALSE;
        JxlColorEncoding colour_encoding;
        JxlColorEncodingSetToSRGB(&colour_encoding, JXL_FALSE);
        if (JxlEncoderSetBasicInfo(encoder.get(), &info) != JXL_ENC_SUCCESS || JxlEncoderSetColorEncoding(encoder.get(), &colour_encoding) != JXL_ENC_SUCCESS) {
            return failed();
        }
        auto* settings = JxlEncoderFrameSettingsCreate(encoder.get(), nullptr);
        const JxlPixelFormat pixel_format { Pixel::length(), JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0 };
        const auto pixels = image.bytes();
        if (!settings || JxlEncoderFrameSettingsSetOption(settings, JXL_ENC_FRAME_SETTING_EFFORT, options.jpegxl_effort) != JXL_ENC_SUCCESS
            || JxlEncoderSetFrameDistance(settings, options.jpegxl_distance) != JXL_ENC_SUCCESS
            // Alpha uses the colour distance; libjxl would otherwise keep it lossless.
            || (Pixel::length() == 4 && JxlEncoderSetExtraChannelDistance(settings, 0, -1.0f) != JXL_ENC_SUCCESS)
            || (lossless && JxlEncoderSetFrameLossless(settings, JXL_TRUE) != JXL_ENC_SUCCESS)
            || JxlEncoderAddImageFrame(settings, &pixel_format, pixels.data(), pixels.size()) != JXL_ENC_SUCCESS) {
            return failed();
        }
        JxlEncoderCloseInput(encoder.get());
        byte::vector encoded(pixels.size() / 8 + 4096);
        std::size_t used = 0;
        JxlEncoderStatus status = JXL_ENC_NEED_MORE_OUTPUT;
        while (status == JXL_ENC_NEED_MORE_OUTPUT) {
            if (used == encoded.size()) {
                encoded.resize(2 * encoded.size());
            }
            auto* next = reinterpret_cast<std::uint8_t*>(encoded.data() + used);
            std::size_t available = encoded.size() - used;
            status = JxlEncoderProcessOutput(encoder.get(), &next, &available);
            used = encoded.size() - available;
        }
        if (status != JXL_ENC_SUCCESS) {
            return failed();
        }
        encoded.resize(used);
        return encoded;
    } catch (const std::bad_alloc&) {
        return Error::fail(Error::Code::ResourceExhausted, "encode JPEG XL image");
    }

    template <typename Pixel>
    Expected<byte::vector> encode_raster(const radix::Raster<Pixel>& image, Format format, EncodeOptions options)
    {
        if (auto valid = validate_encoding(format, Pixel::length(), options); !valid) {
            return Error::propagate(std::move(valid));
        }
        if (format == Format::JpegXl) {
            return encode_jpegxl(image, options);
        }
        // validate_encoding rejects RGBA JPEG.
        if constexpr (std::same_as<Pixel, glm::u8vec3>) {
            if (format == Format::Jpeg) {
                return encode_jpeg(image, options);
            }
        }
        return encode_png(image, options);
    }

    template <typename Pixel>
    Expected<radix::Raster<Pixel>> decode_opencv(byte::span bytes, std::optional<glm::uvec2> expected_size)
    try {
        if (bytes.empty() || bytes.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
            return Error::fail(Error::Code::CorruptData, "empty or oversized encoded image");
        }
        const cv::Mat buffer(1, static_cast<int>(bytes.size()), CV_8UC1, const_cast<std::byte*>(bytes.data()));
        // Preserve alpha and stored row order, without applying EXIF orientation.
        auto decoded = cv::imdecode(buffer, cv::IMREAD_UNCHANGED);
        if (decoded.empty()) {
            return Error::fail(Error::Code::CorruptData, "image decoding failed");
        }
        if (expected_size && glm::uvec2(decoded.cols, decoded.rows) != *expected_size) {
            return Error::fail(Error::Code::CorruptData, "image size differs from the expected size");
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
        if (auto converted = raster::algorithm::transform(*raster, swap_red_blue, *raster); !converted) {
            return Error::propagate(std::move(converted));
        }
        return raster;
    } catch (const cv::Exception& error) {
        return Error::fail(error.code == cv::Error::StsNoMem ? Error::Code::ResourceExhausted : Error::Code::CorruptData, "decode image: " + error.msg);
    } catch (const std::bad_alloc&) {
        return Error::fail(Error::Code::ResourceExhausted, "decode image");
    }

    // TurboJPEG adds opaque alpha and expands grey to RGB. Warnings, such as for truncated data, are errors.
    template <typename Pixel>
    Expected<radix::Raster<Pixel>> decode_jpeg(byte::span bytes, std::optional<glm::uvec2> expected_size)
    try {
        const TurboJpeg decompressor(tj3Init(TJINIT_DECOMPRESS), &tj3Destroy);
        if (!decompressor) {
            return Error::fail(Error::Code::ResourceExhausted, "create JPEG decoder");
        }
        const auto failed = [&decompressor](std::string operation) {
            return Error::fail(Error::Code::CorruptData, std::move(operation) + ": " + tj3GetErrorStr(decompressor.get()));
        };
        if (tj3Set(decompressor.get(), TJPARAM_STOPONWARNING, 1) != 0) {
            return Error::fail(Error::Code::Internal, std::string("configure JPEG decoder: ") + tj3GetErrorStr(decompressor.get()));
        }
        const auto* data = reinterpret_cast<const unsigned char*>(bytes.data());
        if (tj3DecompressHeader(decompressor.get(), data, bytes.size()) != 0) {
            return failed("read JPEG header");
        }
        const int width = tj3Get(decompressor.get(), TJPARAM_JPEGWIDTH);
        const int height = tj3Get(decompressor.get(), TJPARAM_JPEGHEIGHT);
        if (width <= 0 || height <= 0) {
            return Error::fail(Error::Code::CorruptData, "JPEG data contains no image");
        }
        const glm::uvec2 size(width, height);
        if (expected_size && size != *expected_size) {
            return Error::fail(Error::Code::CorruptData, "JPEG image size differs from the expected size");
        }
        const int colour_space = tj3Get(decompressor.get(), TJPARAM_COLORSPACE);
        if (colour_space == TJCS_CMYK || colour_space == TJCS_YCCK) {
            return Error::fail(Error::Code::Unsupported, "CMYK JPEG images are not supported");
        }
        if (tj3Get(decompressor.get(), TJPARAM_PRECISION) != 8) {
            return Error::fail(Error::Code::Unsupported, "RGB8/RGBA8 reading requires 8-bit image samples");
        }
        radix::Raster<Pixel> raster(size, radix::uninitialised);
        constexpr int pixel_format = std::same_as<Pixel, glm::u8vec4> ? TJPF_RGBA : TJPF_RGB;
        if (tj3Decompress8(decompressor.get(), data, bytes.size(), reinterpret_cast<unsigned char*>(raster.data()), 0, pixel_format) != 0) {
            return failed("JPEG decoding failed");
        }
        return raster;
    } catch (const std::bad_alloc&) {
        return Error::fail(Error::Code::ResourceExhausted, "decode JPEG image");
    }

    // Reads the first frame. libjxl drops or adds opaque alpha and expands grey to RGB.
    template <typename Pixel>
    Expected<radix::Raster<Pixel>> decode_jpegxl(byte::span bytes, std::optional<glm::uvec2> expected_size)
    try {
        const auto decoder = JxlDecoderMake(nullptr);
        if (!decoder) {
            return Error::fail(Error::Code::ResourceExhausted, "create JPEG XL decoder");
        }
        // Keep the stored row order, as for EXIF orientation.
        if (JxlDecoderSubscribeEvents(decoder.get(), JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE) != JXL_DEC_SUCCESS
            || JxlDecoderSetKeepOrientation(decoder.get(), JXL_TRUE) != JXL_DEC_SUCCESS
            || JxlDecoderSetInput(decoder.get(), reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()) != JXL_DEC_SUCCESS) {
            return Error::fail(Error::Code::Internal, "set up JPEG XL decoder");
        }
        JxlDecoderCloseInput(decoder.get());
        const JxlPixelFormat pixel_format { Pixel::length(), JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0 };
        std::optional<radix::Raster<Pixel>> raster;
        while (true) {
            switch (JxlDecoderProcessInput(decoder.get())) {
            case JXL_DEC_BASIC_INFO: {
                JxlBasicInfo info;
                if (JxlDecoderGetBasicInfo(decoder.get(), &info) != JXL_DEC_SUCCESS) {
                    return Error::fail(Error::Code::CorruptData, "read JPEG XL basic info");
                }
                const glm::uvec2 size(info.xsize, info.ysize);
                if (expected_size && size != *expected_size) {
                    return Error::fail(Error::Code::CorruptData, "JPEG XL image size differs from the expected size");
                }
                raster.emplace(size);
                break;
            }
            case JXL_DEC_NEED_IMAGE_OUT_BUFFER: {
                if (!raster) {
                    return Error::fail(Error::Code::CorruptData, "JPEG XL image data precedes its basic info");
                }
                const auto pixels = raster->bytes();
                if (JxlDecoderSetImageOutBuffer(decoder.get(), &pixel_format, pixels.data(), pixels.size()) != JXL_DEC_SUCCESS) {
                    return Error::fail(Error::Code::CorruptData, "JPEG XL image does not fit its basic info");
                }
                break;
            }
            case JXL_DEC_FULL_IMAGE:
                return std::move(*raster);
            case JXL_DEC_NEED_MORE_INPUT:
                return Error::fail(Error::Code::CorruptData, "truncated JPEG XL image");
            case JXL_DEC_SUCCESS:
                return Error::fail(Error::Code::CorruptData, "JPEG XL image has no frame");
            default:
                return Error::fail(Error::Code::CorruptData, "JPEG XL decoding failed");
            }
        }
    } catch (const std::bad_alloc&) {
        return Error::fail(Error::Code::ResourceExhausted, "decode JPEG XL image");
    }

    template <typename Pixel>
    Expected<radix::Raster<Pixel>> decode_raster(byte::span bytes, std::optional<glm::uvec2> expected_size = std::nullopt)
    {
        if (is_jpeg(bytes)) {
            return decode_jpeg<Pixel>(bytes, expected_size);
        }
        const auto signature = JxlSignatureCheck(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
        if (signature == JXL_SIG_CODESTREAM || signature == JXL_SIG_CONTAINER) {
            return decode_jpegxl<Pixel>(bytes, expected_size);
        }
        return decode_opencv<Pixel>(bytes, expected_size);
    }

    template <typename Pixel>
    Expected<radix::Raster<Pixel>> read_raster(const std::filesystem::path& path)
    try {
        auto bytes = byte::read(path);
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
        if (extension == ".jxl") {
            return Format::JpegXl;
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
        return byte::write(*bytes, path, options.overwrite ? byte::WriteMode::Overwrite : byte::WriteMode::CreateNew, options.make_dirs);
    }

} // namespace

Expected<byte::vector> encode(const RGB8& image, Format format, EncodeOptions options) { return encode_raster(image, format, options); }
Expected<byte::vector> encode(const RGBA8& image, Format format, EncodeOptions options) { return encode_raster(image, format, options); }
Expected<RGB8> decode_rgb8(byte::span bytes, std::optional<glm::uvec2> expected_size) { return decode_raster<glm::u8vec3>(bytes, expected_size); }
Expected<RGBA8> decode_rgba8(byte::span bytes, std::optional<glm::uvec2> expected_size) { return decode_raster<glm::u8vec4>(bytes, expected_size); }
Expected<RGB8> read_rgb8(const std::filesystem::path& path) { return read_raster<glm::u8vec3>(path); }
Expected<RGBA8> read_rgba8(const std::filesystem::path& path) { return read_raster<glm::u8vec4>(path); }
bool is_jpeg(byte::span bytes)
{
    return bytes.size() >= 3 && bytes[0] == std::byte { 0xFF } && bytes[1] == std::byte { 0xD8 } && bytes[2] == std::byte { 0xFF };
}
Expected<void> write(const RGB8& image, const std::filesystem::path& path, WriteOptions options) { return write_image(image, path, options); }
Expected<void> write(const RGBA8& image, const std::filesystem::path& path, WriteOptions options) { return write_image(image, path, options); }

} // namespace io::image
