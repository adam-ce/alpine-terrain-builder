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
#include <cerrno>
#include <fstream>
#include <system_error>
#include <utility>

#ifdef __linux__
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "io/byte.h"
#include "io/utils.h"
#include "cli_support/log.h"

namespace io::byte {

Expected<void> write(const span bytes, const std::filesystem::path& path, WriteMode mode, bool make_dirs)
{
    LOG_TRACE("Writing bytes to path {}", path);

    if (mode == WriteMode::Replace) {
        auto temporary_path = path;
        temporary_path += ".tmp";
        if (auto written = write(bytes, temporary_path, WriteMode::Overwrite, make_dirs); !written) {
            return written;
        }
        if (auto synced = utils::sync_file(temporary_path); !synced) {
            return synced;
        }
        std::error_code error;
        std::filesystem::rename(temporary_path, path, error);
        if (error) {
            return Error::fail(Error::Code::Io, "replace file", temporary_path, path, error);
        }
        return {};
    }

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

Expected<vector> read(const std::filesystem::path& path, const std::size_t max_size)
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
    vector buffer(static_cast<size_t>(read_size));
    file.seekg(0);
    file.read(reinterpret_cast<char *>(buffer.data()), read_size);

    if (!file.good()) {
        return Error::fail(Error::Code::Io, "read bytes from", path);
    }

    return buffer;
}

Expected<RandomAccessFile> RandomAccessFile::open(const std::filesystem::path& path)
{
    LOG_TRACE("Opening path {} for random access", path);
#ifdef __linux__
    const int descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) {
        const std::error_code error(errno, std::generic_category());
        return Error::fail(error == std::errc::no_such_file_or_directory ? Error::Code::NotFound : Error::Code::Io, "open file for reading", path, error);
    }
    RandomAccessFile file(descriptor, path);
    struct stat status {};
    if (::fstat(descriptor, &status) != 0) {
        return Error::fail(Error::Code::Io, "determine size of", path, std::error_code(errno, std::generic_category()));
    }
    file.m_size = static_cast<std::uint64_t>(status.st_size);
    return file;
#else
    return Error::fail(
        Error::Code::Unsupported, "random access reads are not implemented on this platform", path, std::make_error_code(std::errc::operation_not_supported));
#endif
}

RandomAccessFile::RandomAccessFile(const int descriptor, std::filesystem::path path)
    : m_descriptor(descriptor)
    , m_path(std::move(path))
{
}

RandomAccessFile::RandomAccessFile(RandomAccessFile&& other) noexcept
    : m_descriptor(std::exchange(other.m_descriptor, -1))
    , m_size(other.m_size)
    , m_path(std::move(other.m_path))
{
}

RandomAccessFile::~RandomAccessFile()
{
#ifdef __linux__
    if (m_descriptor >= 0) {
        ::close(m_descriptor);
    }
#endif
}

Expected<vector> RandomAccessFile::read(const std::uint64_t offset, const std::size_t size) const
{
    if (offset > m_size || size > m_size - offset) {
        return Error::fail(Error::Code::CorruptData, "read past the end of", m_path);
    }
    vector bytes(size);
#ifdef __linux__
    std::size_t read_size = 0;
    while (read_size < size) {
        const ::ssize_t count = ::pread(m_descriptor, bytes.data() + read_size, size - read_size, static_cast<::off_t>(offset + read_size));
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count < 0) {
            return Error::fail(Error::Code::Io, "read bytes from", m_path, std::error_code(errno, std::generic_category()));
        }
        // The file was truncated after it was opened.
        if (count == 0) {
            return Error::fail(Error::Code::CorruptData, "read past the end of", m_path);
        }
        read_size += static_cast<std::size_t>(count);
    }
#endif
    return bytes;
}

}
