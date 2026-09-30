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

#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#include <zstd_errors.h>

#include "io/compression.h"

#include <libassert/assert.hpp>
#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

namespace io::envelope {
namespace {

    using CompressionContext = std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)>;

} // namespace

Expected<std::vector<std::byte>> compress(const std::span<const std::byte> uncompressed_data, const CompressionAlgorithm compression_algorithm)
{
    if (uncompressed_data.size() > default_max_decompressed_size) {
        return Error::fail(Error::Code::ResourceExhausted, "uncompressed data exceeds the compression size limit");
    }

    int compression_level = 0;
    switch (compression_algorithm) {
    case CompressionAlgorithm::None:
        return std::vector<std::byte>(uncompressed_data.begin(), uncompressed_data.end());
    case CompressionAlgorithm::ZstdBestCompression:
        compression_level = ZSTD_maxCLevel();
        break;
    case CompressionAlgorithm::ZstdDefaultCompression:
        break;
    default:
        PANIC("unsupported compression algorithm", static_cast<unsigned>(compression_algorithm));
    }

    CompressionContext context { ZSTD_createCCtx(), &ZSTD_freeCCtx };
    if (!context) {
        return Error::fail(Error::Code::Internal, "failed to create the Zstandard compression context");
    }

    if (ZSTD_isError(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_compressionLevel, compression_level))) {
        return Error::fail(Error::Code::Internal, "failed to configure the Zstandard compression context");
    }

    const std::size_t capacity = ZSTD_compressBound(uncompressed_data.size());
    std::vector<std::byte> compressed_data(capacity);
    const std::size_t compressed_size
        = ZSTD_compress2(context.get(), compressed_data.data(), compressed_data.size(), uncompressed_data.data(), uncompressed_data.size());
    if (ZSTD_isError(compressed_size)) {
        return Error::fail(Error::Code::Internal, std::string("Zstandard compression failed: ") + ZSTD_getErrorName(compressed_size));
    }

    compressed_data.resize(compressed_size);
    return compressed_data;
}

Expected<std::vector<std::byte>> decompress(
    const std::span<const std::byte> compressed_data, const CompressionAlgorithm compression_algorithm, const std::size_t max_decompressed_size)
{
    ASSERT(magic_enum::enum_contains(compression_algorithm), "unsupported compression algorithm", static_cast<unsigned>(compression_algorithm));

    const std::size_t effective_max_decompressed_size = std::min(max_decompressed_size, default_max_decompressed_size);

    std::vector<std::byte> uncompressed_data;
    if (compression_algorithm == CompressionAlgorithm::None) {
        if (compressed_data.size() > effective_max_decompressed_size) {
            return Error::fail(Error::Code::ResourceExhausted, "decompressed data exceeds the configured size limit");
        }
        uncompressed_data.assign(compressed_data.begin(), compressed_data.end());
    } else {
        ZSTD_frameHeader frame_header {};
        const std::size_t frame_header_result = ZSTD_getFrameHeader(&frame_header, compressed_data.data(), compressed_data.size());
        if (ZSTD_isError(frame_header_result) || frame_header_result != 0) {
            return Error::fail(Error::Code::CorruptData, "invalid or incomplete Zstandard frame header");
        }

        const unsigned long long frame_content_size = ZSTD_getFrameContentSize(compressed_data.data(), compressed_data.size());
        if (frame_content_size == ZSTD_CONTENTSIZE_ERROR
            || (frame_content_size != ZSTD_CONTENTSIZE_UNKNOWN && frame_content_size > std::numeric_limits<std::size_t>::max())) {
            return Error::fail(Error::Code::CorruptData, "invalid Zstandard frame content size");
        }

        const bool content_size_is_known = frame_content_size != ZSTD_CONTENTSIZE_UNKNOWN;
        if (content_size_is_known && frame_content_size > effective_max_decompressed_size) {
            return Error::fail(Error::Code::ResourceExhausted, "decompressed data exceeds the configured size limit");
        }

        const std::size_t allocation_size = content_size_is_known ? static_cast<std::size_t>(frame_content_size) : effective_max_decompressed_size;
        uncompressed_data.resize(allocation_size);
        const std::size_t decompressed_size
            = ZSTD_decompress(uncompressed_data.data(), uncompressed_data.size(), compressed_data.data(), compressed_data.size());
        if (ZSTD_isError(decompressed_size)) {
            if (ZSTD_getErrorCode(decompressed_size) == ZSTD_error_checksum_wrong) {
                return Error::fail(Error::Code::CorruptData, "Zstandard checksum mismatch");
            }
            if (!content_size_is_known && ZSTD_getErrorCode(decompressed_size) == ZSTD_error_dstSize_tooSmall) {
                return Error::fail(Error::Code::ResourceExhausted, "decompressed data exceeds the configured size limit");
            }
            return Error::fail(Error::Code::CorruptData, std::string("Zstandard decompression failed: ") + ZSTD_getErrorName(decompressed_size));
        }
        if (content_size_is_known && decompressed_size != uncompressed_data.size()) {
            return Error::fail(Error::Code::CorruptData, "Zstandard decompressed size does not match its frame header");
        }

        uncompressed_data.resize(decompressed_size);
    }

    return uncompressed_data;
}

} // namespace io::envelope
