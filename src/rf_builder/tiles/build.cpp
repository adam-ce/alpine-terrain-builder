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

#include "build.h"
#include "TileWorker.h"
namespace rf_builder::tiles {
run::Report build(const Options& options, const std::function<bool()>& stop_requested)
{
    run::validate_options(options.output);
    const auto provider = Error::throwing_unwrap(provider::read(options.provider));
    const auto offset = Error::throwing_unwrap(provider::zoom_offset(provider, options.output.tile_side));
    if (provider.max_zoom - provider.min_zoom > TileWorker::max_fallback_levels) {
        Error::raise(Error::Code::Unsupported,
            "online RF import supports provider zoom ranges of at most " + std::to_string(TileWorker::max_fallback_levels) + " levels for ancestor fallback");
    }
    const auto identifier = Error::throwing_unwrap(run::identifier(options.mask));
    const auto entry = Error::throwing_unwrap(run::attribution(options.output));
    inputs::Record record;
    record.value_mapping = options.output.value_mapping.value_or(raster_store::pixel::default_mapping<glm::u8vec3>);
    record.provider = provider;
    record.tile_side = options.output.tile_side;
    record.mask = identifier;
    record.attribution_index = options.output.attribution_index;
    record.attribution = entry;
    // Fail incompatible cache records before even loading the mask's geometry.
    if (options.output.cache) {
        Error::throwing_unwrap(inputs::validate_cache(*options.output.cache, record));
    }
    const auto mask = Error::throwing_unwrap(Mask::open(run::gdal_identifier(identifier)));
    const planning::Coverage coverage(mask.bounds());
    planning::Cursor roots(coverage, provider.min_zoom - offset);
    NetworkCounters counters;
    std::vector<std::unique_ptr<TileWorker>> workers;
    run::Source<glm::u8vec3> source;
    source.attribution = entry;
    source.validate_cache = [&](const auto& path) { return inputs::validate_cache(path, record); };
    source.write_inputs = [&](const auto& path) { return io::envelope::write_to_path<inputs::Schema>(record, path); };
    source.total = [&](const run::Poll& poll) {
        poll();
        return coverage.weight({ 0, { 0, 0 } });
    };
    source.next = [&](const run::Poll& poll) { return roots.next(poll); };
    source.initialize = [&](unsigned jobs, const run::Poll& poll) {
        for (unsigned worker = 0; worker < jobs; ++worker) {
            poll();
            workers.push_back(TileWorker::open(record, coverage, counters, options.source_cache_bytes, options.retry));
        }
    };
    source.prepare = [&](unsigned worker, const auto& key) { return workers[worker]->prepare(key); };
    source.refine_cached = [&](const auto& key) { return coverage.children(key); };
    source.weight = [&](const auto& key) { return coverage.weight(key); };
    source.network_stats
        = [&] { return run::NetworkStats { counters.requests.load(std::memory_order_relaxed), counters.bytes.load(std::memory_order_relaxed) }; };
    LOG_INFO("RF online source: {}..{}, {} pixels; RF/source zoom offset {}; retained source cache budget {} bytes per worker",
        provider.min_zoom,
        provider.max_zoom,
        provider.tile_size,
        offset,
        options.source_cache_bytes);
    return run::execute(options.output, std::move(source), stop_requested);
}
} // namespace rf_builder::tiles
