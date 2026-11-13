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

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include <glm/gtc/type_precision.hpp>
#include <radix/raster.h>

#include "Error.h"
#include "io/byte.h"

namespace io::image {

using RGB8 = radix::Raster<glm::u8vec3>;
using RGBA8 = radix::Raster<glm::u8vec4>;

enum class Format { Jpeg, Png, JpegXl };

struct EncodeOptions {
    int jpeg_quality = 95;
    int png_compression = 1;
    // Butteraugli distance 0..25, where 0 selects lossless coding.
    float jpegxl_distance = 0.6f;
    // 1 (fastest) to 10.
    int jpegxl_effort = 3;
};

struct WriteOptions {
    EncodeOptions encoding {};
    bool overwrite = false;
    bool make_dirs = true;
};

/// RGB(A) channels, first row at the top. No flipping, scaling or premultiplication.
Expected<byte::vector> encode(const RGB8& image, Format format, EncodeOptions options = {});
/// PNG and JPEG XL preserve all four channels. JPEG rejects RGBA input. JPEG XL tags the channels as sRGB.
Expected<byte::vector> encode(const RGBA8& image, Format format, EncodeOptions options = {});
/// RGB readers discard alpha. RGBA readers supply opaque alpha when absent.
/// Both return 8-bit channels and ignore EXIF orientation. CMYK JPEG images are unsupported. Given an expected
/// size, images of other sizes are corrupt data; JPEG and JPEG XL reject them before allocating their pixels.
Expected<RGB8> decode_rgb8(byte::span bytes, std::optional<glm::uvec2> expected_size = std::nullopt);
Expected<RGBA8> decode_rgba8(byte::span bytes, std::optional<glm::uvec2> expected_size = std::nullopt);
Expected<RGB8> read_rgb8(const std::filesystem::path& path);
Expected<RGBA8> read_rgba8(const std::filesystem::path& path);
/// Whether the bytes start with the JPEG signature; the readers above decode such bytes as JPEG.
bool is_jpeg(byte::span bytes);

/// Select JPEG/PNG/JPEG XL from the extension. Existing symlinks and nonregular files are rejected.
Expected<void> write(const RGB8& image, const std::filesystem::path& path, WriteOptions options = {});
Expected<void> write(const RGBA8& image, const std::filesystem::path& path, WriteOptions options = {});

} // namespace io::image
