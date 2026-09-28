#include "produce.h"

#include "raster/algorithm/fold.h"
#include "raster/algorithm/zip_transform.h"
#include "raster_store/read_tile_with_halo.h"
#include "raster_store/scaler.h"
#include <bit>

namespace rf_merger::produce {
namespace {
    namespace algorithm = raster::algorithm;
    using selection::Side;
    constexpr auto method = algorithm::Resampling::Lanczos3;

    // Supplier pixels covering the leaf, in supplier coordinates without halo,
    // and the output phase within the conceptually upscaled window.
    struct Window {
        unsigned levels;
        glm::uvec2 origin;
        glm::uvec2 interior;
        glm::uvec2 phase;
        unsigned support;
    };

    // Follows the halo reader's distant-ancestor preparation for local phase.
    Expected<Window> window(const Key& supplier, const Key& leaf, unsigned side)
    {
        const unsigned levels = leaf.zoom_level - supplier.zoom_level;
        if (levels > 30) {
            return Error::fail(Error::Code::InvalidInput, "RF merger zoom gap exceeds 30 levels at " + to_string(leaf));
        }
        const unsigned factor = 1u << levels;
        const auto tile_offset = leaf.coords & glm::uvec2(factor - 1);
        const unsigned side_bits = unsigned(std::countr_zero(side));
        Window result { levels, {}, {}, {}, *algorithm::required_halo(int(levels), method) };
        if (levels <= side_bits) {
            result.origin = tile_offset * (side >> levels);
            result.phase = glm::uvec2(0);
        } else {
            result.origin = tile_offset >> (levels - side_bits);
            result.phase = (tile_offset & glm::uvec2((1u << (levels - side_bits)) - 1)) * side;
        }
        result.interior = (result.phase + glm::uvec2(side - 1)) / factor + glm::uvec2(1);
        return result;
    }

    template <typename PixelType>
    struct Source {
        Key key;
        raster_store::Tile<PixelType> tile;
        unsigned halo_width;
        bool attributed;
    };

    // Reads the supplier with the halo required by the scaling window only when
    // the window's support leaves the supplier's interior.
    template <typename PixelType>
    Expected<Source<PixelType>> read(const Input<PixelType>& input, const Key& supplier, const Key& leaf)
    {
        const unsigned side = input.metadata->nominal_tile_size;
        unsigned halo_width = 0;
        if (supplier != leaf) {
            auto geometry = window(supplier, leaf, side);
            if (!geometry) {
                return Error::propagate(std::move(geometry));
            }
            const auto support = glm::uvec2(geometry->support);
            if (glm::any(glm::lessThan(geometry->origin, support))
                || glm::any(glm::greaterThan(geometry->origin + geometry->interior + support, glm::uvec2(side)))) {
                halo_width = geometry->support;
            }
        }
        auto tile = halo_width == 0 ? input.storage->load(supplier)
                                    : raster_store::read_tile_with_halo(*input.storage, *input.metadata, supplier, halo_width, method);
        if (!tile) {
            return Error::propagate(std::move(tile), "read RF merger supplier " + to_string(supplier));
        }
        if (tile->data.size() != glm::uvec2(side + 2 * halo_width) || tile->source_attribution.size() != tile->data.size()) {
            return Error::fail(Error::Code::CorruptData, "RF merger supplier " + to_string(supplier) + " dimensions disagree with metadata");
        }
        auto interior = raster::make_view(std::as_const(tile->source_attribution), glm::uvec2(halo_width), glm::uvec2(side));
        if (!interior) {
            return Error::propagate(std::move(interior));
        }
        const bool attributed = algorithm::fold(*interior, false, [](bool any, std::uint16_t attribution) { return any || attribution != 0; });
        return Source<PixelType> { supplier, std::move(*tile), halo_width, attributed };
    }

    // Aligns a supplier to the leaf grid: native tiles are used as they are,
    // coarser ones are upscaled directly with the windowed paired scaler.
    template <typename PixelType>
    Expected<raster_store::Tile<PixelType>> align(Source<PixelType> source, const Key& leaf, const raster_store::io::manifest::Metadata& metadata)
    {
        if (source.key == leaf) {
            return std::move(source.tile);
        }
        const unsigned side = metadata.nominal_tile_size;
        auto geometry = window(source.key, leaf, side);
        if (!geometry) {
            return Error::propagate(std::move(geometry));
        }
        const auto origin = glm::i64vec2(geometry->origin) + glm::i64vec2(source.halo_width) - glm::i64vec2(geometry->support);
        const auto size = geometry->interior + glm::uvec2(2 * geometry->support);
        auto data = raster::make_view(std::as_const(source.tile.data), origin, size);
        if (!data) {
            return Error::propagate(std::move(data), "prepare RF merger scaling window");
        }
        auto attribution = raster::make_view(std::as_const(source.tile.source_attribution), origin, size);
        if (!attribution) {
            return Error::propagate(std::move(attribution), "prepare RF merger scaling window");
        }
        raster_store::Tile<PixelType> output(side);
        auto scaled = raster_store::scaler::scale(*data,
            *attribution,
            geometry->support,
            int(geometry->levels),
            method,
            glm::ivec2(geometry->phase),
            metadata.value_mapping,
            output.data,
            output.source_attribution);
        if (!scaled) {
            return Error::propagate(std::move(scaled), "upscale RF merger supplier " + to_string(source.key) + " to " + to_string(leaf));
        }
        return output;
    }

    statistics::Category only(Side side) { return side == Side::Left ? statistics::Category::LeftOnly : statistics::Category::RightOnly; }

    template <typename PixelType>
    Expected<Produced<PixelType>> whole(Source<PixelType> source, Side side, const Key& leaf, const raster_store::io::manifest::Metadata& metadata)
    {
        if (source.key == leaf) {
            return Link { side };
        }
        auto aligned = align(std::move(source), leaf, metadata);
        if (!aligned) {
            return Error::propagate(std::move(aligned));
        }
        return Created<PixelType> { std::move(*aligned), only(side) };
    }
} // namespace

template <typename PixelType>
Expected<Produced<PixelType>> produce(const Inputs<PixelType>& inputs, const partition::Leaf& leaf)
{
    if (!leaf.left && !leaf.right) {
        return Error::fail(Error::Code::Internal, "RF merger leaf " + to_string(leaf.key) + " has no supplier");
    }
    const auto& metadata = *inputs.left.metadata;
    if (!leaf.left || !leaf.right) {
        const Side side = leaf.left ? Side::Left : Side::Right;
        const auto& input = leaf.left ? inputs.left : inputs.right;
        const auto supplier = leaf.left ? *leaf.left : *leaf.right;
        if (supplier == leaf.key) {
            return Link { side };
        }
        auto source = read(input, supplier, leaf.key);
        if (!source) {
            return Error::propagate(std::move(source));
        }
        return whole(std::move(*source), side, leaf.key, metadata);
    }
    auto left = read(inputs.left, *leaf.left, leaf.key);
    if (!left) {
        return Error::propagate(std::move(left));
    }
    auto right = read(inputs.right, *leaf.right, leaf.key);
    if (!right) {
        return Error::propagate(std::move(right));
    }
    const auto winner = selection::whole_tile({ left->attributed, left->key.zoom_level }, { right->attributed, right->key.zoom_level });
    if (winner) {
        return *winner == Side::Left ? whole(std::move(*left), Side::Left, leaf.key, metadata) : whole(std::move(*right), Side::Right, leaf.key, metadata);
    }
    const Key left_key = left->key;
    const Key right_key = right->key;
    const selection::Pixel pixel(*inputs.ranks, left_key.zoom_level, right_key.zoom_level);
    auto aligned_left = align(std::move(*left), leaf.key, metadata);
    if (!aligned_left) {
        return Error::propagate(std::move(aligned_left));
    }
    auto aligned_right = align(std::move(*right), leaf.key, metadata);
    if (!aligned_right) {
        return Error::propagate(std::move(aligned_right));
    }
    auto choice = algorithm::zip_transform(
        aligned_left->source_attribution, aligned_right->source_attribution, [&pixel](std::uint16_t left_attribution, std::uint16_t right_attribution) {
            return std::uint8_t(pixel.right_wins(left_attribution, right_attribution));
        });
    if (!choice) {
        return Error::propagate(std::move(choice), "select RF merger pixels");
    }
    const auto right_pixels = algorithm::fold(*choice, std::uint64_t(0), [](std::uint64_t count, std::uint8_t right_wins) { return count + right_wins; });
    const auto pixels = std::uint64_t(choice->width()) * choice->height();
    if (right_pixels == 0) {
        if (left_key == leaf.key) {
            return Link { Side::Left };
        }
        return Created<PixelType> { std::move(*aligned_left), statistics::Category::LeftOnly };
    }
    if (right_pixels == pixels) {
        if (right_key == leaf.key) {
            return Link { Side::Right };
        }
        return Created<PixelType> { std::move(*aligned_right), statistics::Category::RightOnly };
    }
    raster_store::Tile<PixelType> output(metadata.nominal_tile_size);
    auto data = algorithm::zip_transform(
        *choice,
        aligned_left->data,
        aligned_right->data,
        [](std::uint8_t right_wins, const PixelType& left_value, const PixelType& right_value) { return right_wins ? right_value : left_value; },
        output.data);
    if (!data) {
        return Error::propagate(std::move(data), "assemble RF merger data");
    }
    auto attribution = algorithm::zip_transform(
        *choice,
        aligned_left->source_attribution,
        aligned_right->source_attribution,
        [](std::uint8_t right_wins, std::uint16_t left_attribution, std::uint16_t right_attribution) {
            return right_wins ? right_attribution : left_attribution;
        },
        output.source_attribution);
    if (!attribution) {
        return Error::propagate(std::move(attribution), "assemble RF merger attribution");
    }
    return Created<PixelType> { std::move(output), statistics::Category::Mixed };
}

template Expected<Produced<float>> produce(const Inputs<float>&, const partition::Leaf&);
template Expected<Produced<glm::u8vec3>> produce(const Inputs<glm::u8vec3>&, const partition::Leaf&);

} // namespace rf_merger::produce
