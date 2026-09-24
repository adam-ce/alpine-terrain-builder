#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

#include <glm/gtc/type_precision.hpp>
#include <radix/raster.h>

#include "Error.h"

namespace io::image {

using RGB8 = radix::Raster<glm::u8vec3>;
using RGBA8 = radix::Raster<glm::u8vec4>;

enum class Format { Jpeg, Png };

struct EncodeOptions {
    int jpeg_quality = 95;
    int png_compression = 1;
};

struct WriteOptions {
    EncodeOptions encoding {};
    bool overwrite = false;
    bool make_dirs = true;
};

/// RGB(A) channels, first row at the top. No flipping, scaling or premultiplication.
Expected<std::vector<std::uint8_t>> encode(const RGB8& image, Format format, EncodeOptions options = {});
/// PNG preserves all four channels. JPEG rejects RGBA input.
Expected<std::vector<std::uint8_t>> encode(const RGBA8& image, Format format, EncodeOptions options = {});
/// RGB readers discard alpha. RGBA readers supply opaque alpha when absent.
/// Both return 8-bit channels and ignore EXIF orientation.
Expected<RGB8> decode_rgb8(std::span<const std::uint8_t> bytes);
Expected<RGBA8> decode_rgba8(std::span<const std::uint8_t> bytes);
Expected<RGB8> read_rgb8(const std::filesystem::path& path);
Expected<RGBA8> read_rgba8(const std::filesystem::path& path);

/// Select JPEG/PNG from the extension. Existing symlinks and nonregular files are rejected.
Expected<void> write(const RGB8& image, const std::filesystem::path& path, WriteOptions options = {});
Expected<void> write(const RGBA8& image, const std::filesystem::path& path, WriteOptions options = {});

} // namespace io::image
