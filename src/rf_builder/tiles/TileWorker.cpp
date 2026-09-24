#include "TileWorker.h"
#include "io/image.h"
#include "raster/algorithm/scale.h"
#include "raster_store/StoreTraits.h"
#include <algorithm>

namespace rf_builder::tiles {
TileWorker::TileWorker(
    const inputs::Record& record, const planning::Coverage& coverage, NetworkCounters& counters, Mask mask, std::size_t cache_bytes, RetryPolicy retry)
    : m_record(record)
    , m_coverage(coverage)
    , m_mask(std::move(mask))
    , m_http(counters, (std::max)(std::size_t(1024 * 1024), std::size_t(record.provider.tile_size) * record.provider.tile_size * 8 + 65536), retry)
    , m_offset(*provider::zoom_offset(record.provider, record.tile_side))
    , m_cache_bytes(cache_bytes)
{
}
Expected<std::unique_ptr<TileWorker>> TileWorker::open(
    const inputs::Record& record, const planning::Coverage& coverage, NetworkCounters& counters, std::size_t cache_bytes, RetryPolicy retry)
{
    if (auto offset = provider::zoom_offset(record.provider, record.tile_side); !offset) {
        return Error::propagate(std::move(offset));
    }
    auto mask = Mask::open(run::gdal_identifier(record.mask));
    if (!mask) {
        return Error::propagate(std::move(mask));
    }
    return std::unique_ptr<TileWorker>(new TileWorker(record, coverage, counters, std::move(*mask), cache_bytes, retry));
}
Expected<TileWorker::Image> TileWorker::image(const run::Key& key)
{
    if (auto found = m_lookup.find(key); found != m_lookup.end()) {
        m_cache.splice(m_cache.begin(), m_cache, found->second);
        return found->second->image;
    }
    auto fetched = m_http.get(provider::url(m_record.provider, key));
    if (!fetched) {
        return Error::propagate(std::move(fetched), "fetch source tile " + to_string(key));
    }
    Image decoded;
    if (*fetched) {
        auto result = io::image::decode_rgb8(**fetched);
        if (!result) {
            return Error::propagate(std::move(result), "decode source tile " + to_string(key));
        }
        if (result->size() != glm::uvec2(m_record.provider.tile_size)) {
            return Error::fail(Error::Code::CorruptData, "source tile dimensions disagree with provider configuration: " + to_string(key));
        }
        decoded = std::make_shared<const radix::Raster<glm::u8vec3>>(std::move(*result));
    }
    // Budget also charges entries for missing responses, list nodes and lookup
    // overhead, so an all-404 region cannot accumulate an unbounded journal.
    const std::size_t bytes = (decoded ? decoded->bytes().size() : 0) + 256;
    while (!m_cache.empty() && bytes > m_cache_bytes - m_retained_bytes) {
        m_retained_bytes -= m_cache.back().bytes;
        m_lookup.erase(m_cache.back().key);
        m_cache.pop_back();
    }
    if (bytes <= m_cache_bytes) {
        m_cache.push_front({ key, decoded, bytes });
        m_lookup.emplace(key, m_cache.begin());
        m_retained_bytes += bytes;
    }
    return decoded;
}
Expected<std::optional<TileWorker::Supplier>> TileWorker::available(const run::Key& key)
{
    std::optional<Supplier> result = std::nullopt;
    for (unsigned zoom = m_record.provider.min_zoom; zoom <= key.zoom_level; ++zoom) {
        const auto shift = key.zoom_level - zoom;
        const run::Key ancestor { zoom, { unsigned(std::uint64_t(key.coords.x) >> shift), unsigned(std::uint64_t(key.coords.y) >> shift) } };
        auto decoded = image(ancestor);
        if (!decoded) {
            return Error::propagate(std::move(decoded));
        }
        if (!*decoded) {
            break;
        }
        result = Supplier { ancestor, std::move(*decoded) };
    }
    return result;
}
Expected<bool> TileWorker::finer(const run::Key& source, const run::Key& candidate, unsigned matching_zoom)
{
    if (!m_coverage.intersects(source, candidate)) {
        return false;
    }
    auto decoded = image(source);
    if (!decoded) {
        return Error::propagate(std::move(decoded));
    }
    if (!*decoded) {
        return false;
    }
    if (source.zoom_level > matching_zoom) {
        return true;
    }
    if (source.zoom_level == m_record.provider.max_zoom) {
        return false;
    }
    // Release this decoded reference before descent; the bounded LRU owns reuse.
    decoded->reset();
    const auto children = raster_store::StoreTraits::children(source);
    for (const auto& child : *children) {
        auto found = finer(child, candidate, matching_zoom);
        if (!found) {
            return Error::propagate(std::move(found));
        }
        if (*found) {
            return true;
        }
    }
    return false;
}
Expected<glm::u8vec3> TileWorker::pixel(unsigned zoom, std::int64_t x, std::int64_t y, const Supplier& edge)
{
    const std::int64_t side = m_record.provider.tile_size;
    const std::int64_t extent = side * (std::int64_t(1) << zoom);
    const auto wrapped_x = (x % extent + extent) % extent;
    const auto clipped_y = std::clamp(y, std::int64_t(0), extent - 1);
    const run::Key key { zoom, { unsigned(wrapped_x / side), unsigned(clipped_y / side) } };
    if (key == edge.key) {
        return edge.image->pixel({ unsigned(wrapped_x % side), unsigned(clipped_y % side) });
    }
    auto supplier = available(key);
    if (!supplier) {
        return Error::propagate(std::move(supplier));
    }
    if (!*supplier) {
        // True coverage edge: extend this supplying tile's nearest sample.
        const auto local_x = std::clamp(x - std::int64_t(edge.key.coords.x) * side, std::int64_t(0), side - 1);
        const auto local_y = std::clamp(y - std::int64_t(edge.key.coords.y) * side, std::int64_t(0), side - 1);
        return edge.image->pixel({ unsigned(local_x), unsigned(local_y) });
    }
    if ((**supplier).key.zoom_level == zoom) {
        return (**supplier).image->pixel({ unsigned(wrapped_x % side), unsigned(clipped_y % side) });
    }
    radix::Raster<glm::u8vec3> result(1);
    if (auto sampled = sample(**supplier, zoom, { wrapped_x, clipped_y }, raster::make_view(result)); !sampled) {
        return Error::propagate(std::move(sampled));
    }
    return result.pixel({ 0, 0 });
}
Expected<void> TileWorker::sample(const Supplier& supplier, unsigned zoom, glm::u64vec2 origin, const raster::View<glm::u8vec3>& destination)
{
    const unsigned levels = zoom - supplier.key.zoom_level;
    const std::uint64_t factor = std::uint64_t(1) << levels;
    const auto first = origin / factor;
    const auto last = origin + glm::u64vec2(destination.size()) - std::uint64_t(1);
    const glm::uvec2 interior(last / factor - first + std::uint64_t(1));
    // Crop before scaling so even a distant ancestor needs only this window
    // and its one-pixel halo. Keep global coordinates entirely in integers.
    radix::Raster<glm::u8vec3> neighbourhood(interior + glm::uvec2(2));
    const glm::i64vec2 start = glm::i64vec2(first) - std::int64_t(1);
    // A partial output window may not use both sides of the halo. Fetch only
    // contributing samples so an unrelated neighbour cannot fail this tile.
    const glm::uvec2 begin { origin.x % factor < factor / 2 ? 0u : 1u, origin.y % factor < factor / 2 ? 0u : 1u };
    const glm::uvec2 end = interior + glm::uvec2(last.x % factor >= factor / 2 ? 1u : 0u, last.y % factor >= factor / 2 ? 1u : 0u);
    for (unsigned y = begin.y; y <= end.y; ++y) {
        for (unsigned x = begin.x; x <= end.x; ++x) {
            auto value = pixel(supplier.key.zoom_level, start.x + x, start.y + y, supplier);
            if (!value) {
                return Error::propagate(std::move(value));
            }
            neighbourhood.pixel({ x, y }) = *value;
        }
    }
    for (unsigned y = 0; y < neighbourhood.height(); ++y) {
        for (unsigned x = 0; x < neighbourhood.width(); ++x) {
            neighbourhood.pixel({ x, y }) = neighbourhood.pixel(glm::clamp(glm::uvec2(x, y), begin, end));
        }
    }
    return raster::algorithm::scale(neighbourhood,
        1,
        int(levels),
        raster::algorithm::Interpolation::Bilinear,
        raster::algorithm::Filter::Box,
        glm::uvec2(origin % factor),
        raster::algorithm::srgb_conversion<glm::u8vec3>(),
        destination);
}
Expected<void> TileWorker::assemble(
    raster_store::Tile<glm::u8vec3>& tile, std::vector<std::uint8_t>& valid, const run::Key& candidate, const run::Key& source, const Supplier& supplier)
{
    const unsigned side = m_record.provider.tile_size;
    const auto left = (std::uint64_t(source.coords.x) - (std::uint64_t(candidate.coords.x) << m_offset)) * side;
    const auto top = (std::uint64_t(source.coords.y) - (std::uint64_t(candidate.coords.y) << m_offset)) * side;
    if (source.zoom_level == supplier.key.zoom_level) {
        for (unsigned y = 0; y < side; ++y) {
            for (unsigned x = 0; x < side; ++x) {
                const auto index = (top + y) * m_record.tile_side + left + x;
                tile.data.buffer()[index] = supplier.image->pixel({ x, y });
                valid[index] = 1;
            }
        }
        return {};
    }
    auto destination = raster::make_view(tile.data, { unsigned(left), unsigned(top) }, glm::uvec2(side));
    if (!destination) {
        return Error::propagate(std::move(destination));
    }
    if (auto sampled = sample(supplier, source.zoom_level, glm::u64vec2(source.coords) * std::uint64_t(side), *destination); !sampled) {
        return Error::propagate(std::move(sampled));
    }
    for (unsigned y = 0; y < side; ++y) {
        for (unsigned x = 0; x < side; ++x) {
            const auto index = (top + y) * m_record.tile_side + left + x;
            valid[index] = 1;
        }
    }
    return {};
}
Expected<run::Prepared<glm::u8vec3>> TileWorker::prepare(const run::Key& key)
{
    const unsigned matching = key.zoom_level + m_offset;
    if (matching > m_record.provider.max_zoom) {
        return Error::fail(Error::Code::Internal, "online RF candidate exceeds source ceiling");
    }
    planning::Cursor roots(m_coverage, m_record.provider.min_zoom, key);
    for (;;) {
        auto source = roots.next();
        if (!source) {
            return Error::propagate(std::move(source));
        }
        if (!*source) {
            break;
        }
        auto refine = finer(**source, key, matching);
        if (!refine) {
            return Error::propagate(std::move(refine));
        }
        if (*refine) {
            return run::Prepared<glm::u8vec3>(m_coverage.children(key));
        }
    }
    raster_store::Tile<glm::u8vec3> tile(m_record.tile_side);
    std::vector<std::uint8_t> valid(std::size_t(m_record.tile_side) * m_record.tile_side, 0);
    planning::Cursor chunks(m_coverage, matching, key);
    for (;;) {
        auto source = chunks.next();
        if (!source) {
            return Error::propagate(std::move(source));
        }
        if (!*source) {
            break;
        }
        auto supplier = available(**source);
        if (!supplier) {
            return Error::propagate(std::move(supplier));
        }
        if (!*supplier) {
            continue;
        }
        if (auto prepared = assemble(tile, valid, key, **source, **supplier); !prepared) {
            return Error::propagate(std::move(prepared));
        }
    }
    const auto bounds = RasterTransform::tile_bounds(key);
    const double spacing = bounds.width() / m_record.tile_side;
    std::vector<glm::dvec2> centres(m_record.tile_side);
    for (unsigned row = 0; row < m_record.tile_side; ++row) {
        for (unsigned column = 0; column < m_record.tile_side; ++column) {
            centres[column] = { bounds.min.x + (column + 0.5) * spacing, bounds.max.y - (row + 0.5) * spacing };
        }
        if (auto selected = m_mask.select(centres, std::span(valid).subspan(std::size_t(row) * m_record.tile_side, m_record.tile_side)); !selected) {
            return Error::propagate(std::move(selected));
        }
    }
    if (std::ranges::none_of(valid, [](auto value) { return value != 0; })) {
        return run::Prepared<glm::u8vec3>(std::monostate());
    }
    for (std::size_t i = 0; i < valid.size(); ++i) {
        if (valid[i]) {
            tile.source_attribution.buffer()[i] = std::uint16_t(m_record.attribution_index);
        }
    }
    return run::Prepared<glm::u8vec3>(std::move(tile));
}
} // namespace rf_builder::tiles
