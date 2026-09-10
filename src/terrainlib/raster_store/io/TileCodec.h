#pragma once

#include <cstring>
#include <memory>
#include <string>

#include "io/envelope.h"
#include "raster_store/Tile.h"
#include "raster_store/pixel_type.h"
#include "store/Codec.h"

namespace raster_store::io {

namespace tile_codec {

    namespace detail::v1 {

        struct RasterTile {
            std::uint32_t width;
            std::uint32_t height;
            ::io::envelope::Bytes data;
            ::io::envelope::Bytes source_attribution;
        };

    } // namespace detail::v1

    using TileSchema = ::io::envelope::PayloadSchema<"raster_store.Tile", ::io::envelope::Version<1, detail::v1::RasterTile>>;

} // namespace tile_codec

template <typename PixelType>
class TileCodec final : public store::Codec<Tile<PixelType>> {
    static_assert(sizeof(pixel_type::Format<PixelType>) > 0);

public:
    explicit TileCodec(const glm::uvec2 dimensions = glm::uvec2(default_tile_side),
        const ::io::envelope::CompressionAlgorithm compression_algorithm = ::io::envelope::CompressionAlgorithm::ZstdDefaultCompressionWithChecksum,
        const ::io::envelope::ChecksumAlgorithm checksum_algorithm = ::io::envelope::ChecksumAlgorithm::HandledByCompressionLib)
        : m_dimensions(dimensions)
        , m_compression_algorithm(compression_algorithm)
        , m_checksum_algorithm(checksum_algorithm)
    {
    }

    std::vector<std::filesystem::path> paths(const std::filesystem::path& node_path) const override
    {
        return { this->add_extension(node_path, ".amort") };
    }

    Expected<Tile<PixelType>> read(const std::filesystem::path& node_path) const override
    {
        if (auto valid = validate_dimensions(m_dimensions); !valid) {
            return Error::propagate(std::move(valid));
        }
        auto encoded = ::io::envelope::read_from_path<tile_codec::TileSchema>(paths(node_path).front());
        if (!encoded) {
            return Error::propagate(std::move(encoded), "read AMORT tile");
        }
        if (glm::uvec2(encoded->width, encoded->height) != m_dimensions) {
            return Error::fail(Error::Code::CorruptData, "AMORT tile dimensions do not match snapshot metadata");
        }
        const std::uint64_t count = std::uint64_t { encoded->width } * encoded->height;
        if (encoded->data.size() % sizeof(PixelType) != 0 || encoded->data.size() / sizeof(PixelType) != count
            || encoded->source_attribution.size() % sizeof(std::uint16_t) != 0
            || encoded->source_attribution.size() / sizeof(std::uint16_t) != count) {
            return Error::fail(Error::Code::CorruptData, "AMORT raster buffer byte counts do not match dimensions and payload type");
        }
        Tile<PixelType> tile(encoded->width);
        std::memcpy(tile.data.bytes().data(), encoded->data.data(), encoded->data.size());
        std::memcpy(tile.source_attribution.bytes().data(), encoded->source_attribution.data(), encoded->source_attribution.size());
        return tile;
    }

    Expected<void> write(const std::filesystem::path& node_path, const Tile<PixelType>& tile) const override
    {
        if (auto valid = validate_dimensions(m_dimensions); !valid) {
            return valid;
        }
        if (tile.data.size() != m_dimensions || tile.source_attribution.size() != m_dimensions) {
            return Error::fail(Error::Code::InvalidInput, "both tile rasters must match snapshot dimensions");
        }
        const auto data = tile.data.bytes();
        const auto source_attribution = tile.source_attribution.bytes();
        const tile_codec::detail::v1::RasterTile encoded { m_dimensions.x, m_dimensions.y,
            { data.begin(), data.end() }, { source_attribution.begin(), source_attribution.end() } };
        return ::io::envelope::write_to_path<tile_codec::TileSchema>(encoded, paths(node_path).front(), true, m_compression_algorithm, m_checksum_algorithm);
    }

private:
    glm::uvec2 m_dimensions;
    ::io::envelope::CompressionAlgorithm m_compression_algorithm;
    ::io::envelope::ChecksumAlgorithm m_checksum_algorithm;
};

namespace tile_codec {

    template <typename PixelType>
    Expected<std::unique_ptr<store::Codec<Tile<PixelType>>>> from_name(const std::string_view name, const glm::uvec2 dimensions,
        const ::io::envelope::CompressionAlgorithm compression_algorithm = ::io::envelope::CompressionAlgorithm::ZstdDefaultCompressionWithChecksum,
        const ::io::envelope::ChecksumAlgorithm checksum_algorithm = ::io::envelope::ChecksumAlgorithm::HandledByCompressionLib)
    {
        if (name == "amort") {
            return std::make_unique<TileCodec<PixelType>>(dimensions, compression_algorithm, checksum_algorithm);
        }
        return Error::fail(Error::Code::Unsupported, "unsupported raster codec selector: " + std::string(name));
    }

} // namespace tile_codec

} // namespace raster_store::io
