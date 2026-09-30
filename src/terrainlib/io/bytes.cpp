/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
 * Copyright (C) 2025 Adam Celarek-Litofcenko
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

#include <algorithm>
#include <fstream>
#include <system_error>

#include "io/bytes.h"
#include "io/utils.h"
#include "log.h"

namespace io {

Expected<void> write_bytes_to_path(const std::span<const std::byte> bytes, const std::filesystem::path& path, bool make_dirs)
{
    return write_bytes_to_path(bytes, path, WriteMode::Overwrite, make_dirs);
}

Expected<void> write_bytes_to_path(const std::span<const std::byte> bytes, const std::filesystem::path& path, WriteMode mode, bool make_dirs)
{
    LOG_TRACE("Writing bytes to path {}", path);

    if (make_dirs) {
        auto directories = utils::create_parent_directories(path);
        if (!directories) {
            return Error::propagate(std::move(directories), "write bytes");
        }
    }

    std::ofstream file(path, std::ios::binary | (mode == WriteMode::CreateNew ? std::ios::noreplace : std::ios::trunc));
    if (!file.is_open()) {
        std::error_code error;
        if (mode == WriteMode::CreateNew && std::filesystem::exists(std::filesystem::symlink_status(path, error))) {
            return Error::fail(Error::Code::AlreadyExists, "create file", path);
        }
        return Error::fail(Error::Code::Io, "open file for writing", path);
    }

    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file.good()) {
        return Error::fail(Error::Code::Io, "write bytes to", path);
    }

    file.flush();
    if (!file.good()) {
        return Error::fail(Error::Code::Io, "flush bytes to", path);
    }
    file.close();
    if (file.fail()) {
        return Error::fail(Error::Code::Io, "close file after writing", path);
    }

    return {};
}

Expected<std::vector<std::byte>> read_bytes_from_path(const std::filesystem::path& path, const std::size_t max_size)
{
    LOG_TRACE("Reading bytes from path {}", path);

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::error_code existence_error;
        const bool exists = std::filesystem::exists(path, existence_error);
        if (!existence_error && !exists) {
            return Error::fail(Error::Code::NotFound, "open file for reading", path);
        }
        return existence_error
                ? Error::fail(Error::Code::Io, "check existence of", path, existence_error)
                : Error::fail(Error::Code::Io, "open file for reading", path);
    }

    const std::streamsize size = file.tellg();
    if (size < 0) {
        return Error::fail(Error::Code::Io, "determine size of", path);
    }

    const auto read_size = static_cast<std::streamsize>(std::min(static_cast<std::size_t>(size), max_size));
    std::vector<std::byte> buffer(static_cast<size_t>(read_size));
    file.seekg(0);
    file.read(reinterpret_cast<char *>(buffer.data()), read_size);

    if (!file.good()) {
        return Error::fail(Error::Code::Io, "read bytes from", path);
    }

    return buffer;
}

}
