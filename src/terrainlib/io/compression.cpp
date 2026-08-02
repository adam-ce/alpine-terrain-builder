#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#include <zstd_errors.h>

#include "io/compression.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

namespace io::envelope {
namespace {

using CompressionContext = std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)>;

std::expected<void, Error> validate_algorithms(
    const CompressionAlgorithm compression_algorithm,
    const ChecksumAlgorithm checksum_algorithm)
{
    switch (checksum_algorithm) {
    case ChecksumAlgorithm::None:
    case ChecksumAlgorithm::HandledByCompressionLib:
        break;
    default:
        return std::unexpected(Error{ErrorCode::UnsupportedChecksumAlgorithm});
    }

    switch (compression_algorithm) {
    case CompressionAlgorithm::None:
    case CompressionAlgorithm::ZstdBestCompressionWithChecksum:
        break;
    default:
        return std::unexpected(Error{ErrorCode::UnsupportedCompressionAlgorithm});
    }

    const bool no_compression = compression_algorithm == CompressionAlgorithm::None
        && checksum_algorithm == ChecksumAlgorithm::None;
    const bool zstd_with_checksum =
        compression_algorithm == CompressionAlgorithm::ZstdBestCompressionWithChecksum
        && checksum_algorithm == ChecksumAlgorithm::HandledByCompressionLib;
    if (!no_compression && !zstd_with_checksum) {
        return std::unexpected(Error{ErrorCode::InvalidAlgorithmCombination});
    }

    return {};
}

} // namespace

std::expected<CompressedData, Error> compress_with_checksum(
    const Bytes &uncompressed_data,
    const CompressionAlgorithm compression_algorithm,
    const ChecksumAlgorithm checksum_algorithm)
{
    if (const auto validation = validate_algorithms(compression_algorithm, checksum_algorithm); !validation) {
        return std::unexpected(validation.error());
    }
    if (uncompressed_data.size() > default_max_decompressed_size) {
        return std::unexpected(Error{ErrorCode::SizeLimitExceeded});
    }

    if (compression_algorithm == CompressionAlgorithm::None) {
        return CompressedData{uncompressed_data, {}};
    }

    CompressionContext context{ZSTD_createCCtx(), &ZSTD_freeCCtx};
    if (!context) {
        return std::unexpected(Error{ErrorCode::CompressionFailed});
    }

    if (ZSTD_isError(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_compressionLevel, ZSTD_maxCLevel()))
        || ZSTD_isError(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_checksumFlag, 1))) {
        return std::unexpected(Error{ErrorCode::CompressionFailed});
    }

    const std::size_t capacity = ZSTD_compressBound(uncompressed_data.size());
    Bytes compressed_data(capacity);
    const std::size_t compressed_size = ZSTD_compress2(
        context.get(),
        compressed_data.data(),
        compressed_data.size(),
        uncompressed_data.data(),
        uncompressed_data.size());
    if (ZSTD_isError(compressed_size)) {
        return std::unexpected(Error{ErrorCode::CompressionFailed});
    }

    compressed_data.resize(compressed_size);
    return CompressedData{std::move(compressed_data), {}};
}

std::expected<Bytes, Error> checked_decompress(
    const Bytes &compressed_data,
    const CompressionAlgorithm compression_algorithm,
    const ChecksumAlgorithm checksum_algorithm,
    const std::string_view checksum,
    const std::size_t max_decompressed_size)
{
    if (const auto validation = validate_algorithms(compression_algorithm, checksum_algorithm); !validation) {
        return std::unexpected(validation.error());
    }
    if (!checksum.empty()) {
        return std::unexpected(Error{ErrorCode::InvalidAlgorithmCombination});
    }

    const std::size_t effective_max_decompressed_size =
        std::min(max_decompressed_size, default_max_decompressed_size);

    if (compression_algorithm == CompressionAlgorithm::None) {
        if (compressed_data.size() > effective_max_decompressed_size) {
            return std::unexpected(Error{ErrorCode::SizeLimitExceeded});
        }
        return compressed_data;
    }

    ZSTD_frameHeader frame_header{};
    const std::size_t frame_header_result = ZSTD_getFrameHeader(
        &frame_header, compressed_data.data(), compressed_data.size());
    if (ZSTD_isError(frame_header_result) || frame_header_result != 0) {
        return std::unexpected(Error{ErrorCode::DecompressionFailed});
    }
    if (frame_header.checksumFlag == 0) {
        return std::unexpected(Error{ErrorCode::ChecksumMismatch});
    }

    const unsigned long long frame_content_size =
        ZSTD_getFrameContentSize(compressed_data.data(), compressed_data.size());
    if (frame_content_size == ZSTD_CONTENTSIZE_ERROR
        || (frame_content_size != ZSTD_CONTENTSIZE_UNKNOWN
            && frame_content_size > std::numeric_limits<std::size_t>::max())) {
        return std::unexpected(Error{ErrorCode::DecompressionFailed});
    }

    const bool content_size_is_known = frame_content_size != ZSTD_CONTENTSIZE_UNKNOWN;
    if (content_size_is_known && frame_content_size > effective_max_decompressed_size) {
        return std::unexpected(Error{ErrorCode::SizeLimitExceeded});
    }

    const std::size_t allocation_size = content_size_is_known
        ? static_cast<std::size_t>(frame_content_size)
        : effective_max_decompressed_size;
    Bytes uncompressed_data(allocation_size);
    const std::size_t decompressed_size = ZSTD_decompress(
        uncompressed_data.data(),
        uncompressed_data.size(),
        compressed_data.data(),
        compressed_data.size());
    if (ZSTD_isError(decompressed_size)) {
        if (ZSTD_getErrorCode(decompressed_size) == ZSTD_error_checksum_wrong) {
            return std::unexpected(Error{ErrorCode::ChecksumMismatch});
        }
        if (!content_size_is_known
            && ZSTD_getErrorCode(decompressed_size) == ZSTD_error_dstSize_tooSmall) {
            return std::unexpected(Error{ErrorCode::SizeLimitExceeded});
        }
        return std::unexpected(Error{ErrorCode::DecompressionFailed});
    }
    if (content_size_is_known && decompressed_size != uncompressed_data.size()) {
        return std::unexpected(Error{ErrorCode::DecompressionFailed});
    }

    uncompressed_data.resize(decompressed_size);
    return uncompressed_data;
}

} // namespace io::envelope
