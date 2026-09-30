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
Expected<std::vector<std::byte>> encode(const RGB8& image, Format format, EncodeOptions options = {});
/// PNG preserves all four channels. JPEG rejects RGBA input.
Expected<std::vector<std::byte>> encode(const RGBA8& image, Format format, EncodeOptions options = {});
/// RGB readers discard alpha. RGBA readers supply opaque alpha when absent.
/// Both return 8-bit channels and ignore EXIF orientation.
Expected<RGB8> decode_rgb8(std::span<const std::byte> bytes);
Expected<RGBA8> decode_rgba8(std::span<const std::byte> bytes);
Expected<RGB8> read_rgb8(const std::filesystem::path& path);
Expected<RGBA8> read_rgba8(const std::filesystem::path& path);

/// Select JPEG/PNG from the extension. Existing symlinks and nonregular files are rejected.
Expected<void> write(const RGB8& image, const std::filesystem::path& path, WriteOptions options = {});
Expected<void> write(const RGBA8& image, const std::filesystem::path& path, WriteOptions options = {});

} // namespace io::image
