#include "build.h"
#include "TileWorker.h"
#include "jpeg.h"
namespace rf_builder::tiles {
Expected<run::Report> build(const Options& options, const std::function<bool()>& stop_requested)
{
    if (auto valid = run::validate_options(options.output); !valid) {
        return Error::propagate(std::move(valid));
    }
    auto provider = provider::read(options.provider);
    if (!provider) {
        return Error::propagate(std::move(provider));
    }
    auto offset = provider::zoom_offset(*provider, options.output.tile_side);
    if (!offset) {
        return Error::propagate(std::move(offset));
    }
    auto identifier = run::identifier(options.mask);
    if (!identifier) {
        return Error::propagate(std::move(identifier));
    }
    auto entry = run::attribution(options.output);
    if (!entry) {
        return Error::propagate(std::move(entry));
    }
    inputs::Record record;
    record.value_mapping = options.output.value_mapping.value_or(raster_store::pixel::default_mapping<glm::u8vec3>);
    record.provider = *provider;
    record.tile_side = options.output.tile_side;
    record.mask = *identifier;
    record.attribution_index = options.output.attribution_index;
    record.attribution = *entry;
    record.decoder_version = jpeg::version();
    // Fail incompatible cache records before even loading the mask's geometry.
    if (options.output.cache) {
        if (auto checked = inputs::validate_cache(*options.output.cache, record); !checked) {
            return Error::propagate(std::move(checked));
        }
    }
    auto mask = Mask::open(run::gdal_identifier(*identifier));
    if (!mask) {
        return Error::propagate(std::move(mask));
    }
    const planning::Coverage coverage(mask->bounds());
    planning::Cursor roots(coverage, provider->min_zoom - *offset);
    NetworkCounters counters;
    std::vector<std::unique_ptr<TileWorker>> workers;
    run::Source<glm::u8vec3> source;
    source.attribution = *entry;
    source.validate_cache = [&](const auto& path) { return inputs::validate_cache(path, record); };
    source.write_inputs = [&](const auto& path) { return io::envelope::write_to_path<inputs::Schema>(record, path); };
    source.total = [&](const run::Poll& poll) -> Expected<double> {
        if (auto checked = poll(); !checked) {
            return Error::propagate(std::move(checked));
        }
        return coverage.weight({ 0, { 0, 0 } });
    };
    source.next = [&](const run::Poll& poll) { return roots.next(poll); };
    source.initialize = [&](unsigned jobs, const run::Poll& poll) -> Expected<void> {
        for (unsigned worker = 0; worker < jobs; ++worker) {
            if (auto checked = poll(); !checked) {
                return checked;
            }
            auto opened = TileWorker::open(record, coverage, counters, options.source_cache_bytes, options.retry);
            if (!opened) {
                return Error::propagate(std::move(opened));
            }
            workers.push_back(std::move(*opened));
        }
        return {};
    };
    source.prepare = [&](unsigned worker, const auto& key) { return workers[worker]->prepare(key); };
    source.refine_cached = [&](const auto& key) { return coverage.children(key); };
    source.weight = [&](const auto& key) { return coverage.weight(key); };
    source.network_stats
        = [&] { return run::NetworkStats { counters.requests.load(std::memory_order_relaxed), counters.bytes.load(std::memory_order_relaxed) }; };
    LOG_INFO("RF online source: {}..{}, {} pixels; RF/source zoom offset {}; retained source cache budget {} bytes per worker; JPEG {}",
        provider->min_zoom,
        provider->max_zoom,
        provider->tile_size,
        *offset,
        options.source_cache_bytes,
        record.decoder_version);
    return run::execute(options.output, std::move(source), stop_requested);
}
} // namespace rf_builder::tiles
