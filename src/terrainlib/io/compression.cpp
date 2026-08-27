#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#include <zstd_errors.h>

#include "io/compression.h"

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <utility>

namespace io::envelope {
namespace {

    using CompressionContext = std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)>;

    std::string crc32c_checksum(const Bytes& data)
    {
        static constexpr auto table = [] {
            constexpr std::uint32_t polynomial = 0x82f63b78u;

            std::array<std::uint32_t, 256> values {};
            for (std::size_t index = 0; index < values.size(); ++index) {
                auto crc = static_cast<std::uint32_t>(index);
                for (int bit = 0; bit < 8; ++bit) {
                    crc = (crc >> 1u) ^ ((crc & 1u) != 0u ? polynomial : 0u);
                }
                values[index] = crc;
            }
            return values;
        }();

        std::uint32_t crc = 0xffffffffu;
        for (const auto value : data) {
            const auto index = (crc ^ std::to_integer<std::uint8_t>(value)) & 0xffu;
            crc = table[index] ^ (crc >> 8u);
        }
        crc = ~crc;

        constexpr char hex_digits[] = "0123456789abcdef";
        std::string checksum(8, '0');
        for (std::size_t index = 0; index < checksum.size(); ++index) {
            const auto shift = static_cast<unsigned>((checksum.size() - index - 1) * 4);
            checksum[index] = hex_digits[(crc >> shift) & 0x0fu];
        }
        return checksum;
    }

    std::expected<void, ::Error> validate_algorithms(const CompressionAlgorithm compression_algorithm, const ChecksumAlgorithm checksum_algorithm)
    {
        switch (checksum_algorithm) {
        case ChecksumAlgorithm::None:
        case ChecksumAlgorithm::HandledByCompressionLib:
        case ChecksumAlgorithm::Crc32c:
            break;
        default:
            return std::unexpected(::Error::make(::Error::Code::Unsupported,
                "unsupported checksum algorithm " + std::to_string(static_cast<unsigned>(checksum_algorithm))));
        }

        switch (compression_algorithm) {
        case CompressionAlgorithm::None:
        case CompressionAlgorithm::ZstdBestCompressionWithChecksum:
        case CompressionAlgorithm::ZstdDefaultCompressionWithChecksum:
            break;
        default:
            return std::unexpected(::Error::make(::Error::Code::Unsupported,
                "unsupported compression algorithm " + std::to_string(static_cast<unsigned>(compression_algorithm))));
        }

        const bool no_compression = compression_algorithm == CompressionAlgorithm::None
            && (checksum_algorithm == ChecksumAlgorithm::None || checksum_algorithm == ChecksumAlgorithm::Crc32c);
        const bool zstd_with_checksum = (compression_algorithm == CompressionAlgorithm::ZstdBestCompressionWithChecksum
                                            || compression_algorithm == CompressionAlgorithm::ZstdDefaultCompressionWithChecksum)
            && (checksum_algorithm == ChecksumAlgorithm::HandledByCompressionLib || checksum_algorithm == ChecksumAlgorithm::Crc32c);
        if (!no_compression && !zstd_with_checksum) {
            return std::unexpected(::Error::make(::Error::Code::InvalidInput, "invalid compression and checksum algorithm combination"));
        }

        return {};
    }

} // namespace

std::expected<CompressedData, ::Error> compress_with_checksum(
    const Bytes& uncompressed_data, const CompressionAlgorithm compression_algorithm, const ChecksumAlgorithm checksum_algorithm)
{
    if (auto validation = validate_algorithms(compression_algorithm, checksum_algorithm); !validation) {
        return std::unexpected(std::move(validation).error());
    }
    if (uncompressed_data.size() > default_max_decompressed_size) {
        return std::unexpected(::Error::make(::Error::Code::ResourceExhausted, "uncompressed data exceeds the compression size limit"));
    }

    const std::string checksum = checksum_algorithm == ChecksumAlgorithm::Crc32c ? crc32c_checksum(uncompressed_data) : std::string {};

    if (compression_algorithm == CompressionAlgorithm::None) {
        return CompressedData { uncompressed_data, checksum };
    }

    CompressionContext context { ZSTD_createCCtx(), &ZSTD_freeCCtx };
    if (!context) {
        return std::unexpected(::Error::make(::Error::Code::Internal, "failed to create the Zstandard compression context"));
    }

    const int compression_level = compression_algorithm == CompressionAlgorithm::ZstdBestCompressionWithChecksum ? ZSTD_maxCLevel() : 0;
    if (ZSTD_isError(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_compressionLevel, compression_level))
        || ZSTD_isError(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_checksumFlag, 1))) {
        return std::unexpected(::Error::make(::Error::Code::Internal, "failed to configure the Zstandard compression context"));
    }

    const std::size_t capacity = ZSTD_compressBound(uncompressed_data.size());
    Bytes compressed_data(capacity);
    const std::size_t compressed_size
        = ZSTD_compress2(context.get(), compressed_data.data(), compressed_data.size(), uncompressed_data.data(), uncompressed_data.size());
    if (ZSTD_isError(compressed_size)) {
        return std::unexpected(::Error::make(::Error::Code::Internal,
            std::string("Zstandard compression failed: ") + ZSTD_getErrorName(compressed_size)));
    }

    compressed_data.resize(compressed_size);
    return CompressedData { std::move(compressed_data), checksum };
}

std::expected<Bytes, ::Error> checked_decompress(const Bytes& compressed_data,
    const CompressionAlgorithm compression_algorithm,
    const ChecksumAlgorithm checksum_algorithm,
    const std::string_view checksum,
    const std::size_t max_decompressed_size)
{
    if (auto validation = validate_algorithms(compression_algorithm, checksum_algorithm); !validation) {
        return std::unexpected(std::move(validation).error());
    }
    if (checksum_algorithm != ChecksumAlgorithm::Crc32c && !checksum.empty()) {
        return std::unexpected(::Error::make(::Error::Code::InvalidInput, "a checksum was supplied for an algorithm that does not accept one"));
    }
    if (checksum_algorithm == ChecksumAlgorithm::Crc32c && (checksum.size() != 8 || !std::all_of(checksum.begin(), checksum.end(), [](const char character) {
            return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
        }))) {
        return std::unexpected(::Error::make(::Error::Code::CorruptData, "invalid CRC32C checksum encoding"));
    }

    const std::size_t effective_max_decompressed_size = std::min(max_decompressed_size, default_max_decompressed_size);

    Bytes uncompressed_data;
    if (compression_algorithm == CompressionAlgorithm::None) {
        if (compressed_data.size() > effective_max_decompressed_size) {
            return std::unexpected(::Error::make(::Error::Code::ResourceExhausted, "decompressed data exceeds the configured size limit"));
        }
        uncompressed_data = compressed_data;
    } else {
        ZSTD_frameHeader frame_header {};
        const std::size_t frame_header_result = ZSTD_getFrameHeader(&frame_header, compressed_data.data(), compressed_data.size());
        if (ZSTD_isError(frame_header_result) || frame_header_result != 0) {
            return std::unexpected(::Error::make(::Error::Code::CorruptData, "invalid or incomplete Zstandard frame header"));
        }
        if (frame_header.checksumFlag == 0) {
            return std::unexpected(::Error::make(::Error::Code::CorruptData, "Zstandard frame does not contain the required checksum"));
        }

        const unsigned long long frame_content_size = ZSTD_getFrameContentSize(compressed_data.data(), compressed_data.size());
        if (frame_content_size == ZSTD_CONTENTSIZE_ERROR
            || (frame_content_size != ZSTD_CONTENTSIZE_UNKNOWN && frame_content_size > std::numeric_limits<std::size_t>::max())) {
            return std::unexpected(::Error::make(::Error::Code::CorruptData, "invalid Zstandard frame content size"));
        }

        const bool content_size_is_known = frame_content_size != ZSTD_CONTENTSIZE_UNKNOWN;
        if (content_size_is_known && frame_content_size > effective_max_decompressed_size) {
            return std::unexpected(::Error::make(::Error::Code::ResourceExhausted, "decompressed data exceeds the configured size limit"));
        }

        const std::size_t allocation_size = content_size_is_known ? static_cast<std::size_t>(frame_content_size) : effective_max_decompressed_size;
        uncompressed_data.resize(allocation_size);
        const std::size_t decompressed_size
            = ZSTD_decompress(uncompressed_data.data(), uncompressed_data.size(), compressed_data.data(), compressed_data.size());
        if (ZSTD_isError(decompressed_size)) {
            if (ZSTD_getErrorCode(decompressed_size) == ZSTD_error_checksum_wrong) {
                return std::unexpected(::Error::make(::Error::Code::CorruptData, "Zstandard checksum mismatch"));
            }
            if (!content_size_is_known && ZSTD_getErrorCode(decompressed_size) == ZSTD_error_dstSize_tooSmall) {
                return std::unexpected(::Error::make(::Error::Code::ResourceExhausted, "decompressed data exceeds the configured size limit"));
            }
            return std::unexpected(::Error::make(::Error::Code::CorruptData,
                std::string("Zstandard decompression failed: ") + ZSTD_getErrorName(decompressed_size)));
        }
        if (content_size_is_known && decompressed_size != uncompressed_data.size()) {
            return std::unexpected(::Error::make(::Error::Code::CorruptData, "Zstandard decompressed size does not match its frame header"));
        }

        uncompressed_data.resize(decompressed_size);
    }

    if (checksum_algorithm == ChecksumAlgorithm::Crc32c && crc32c_checksum(uncompressed_data) != checksum) {
        return std::unexpected(::Error::make(::Error::Code::CorruptData, "CRC32C checksum mismatch"));
    }
    return uncompressed_data;
}

} // namespace io::envelope
