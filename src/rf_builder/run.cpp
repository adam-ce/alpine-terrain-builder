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

#include "run.h"
#include "raster_store/TilePool.h"
#include "raster_store/storage.h"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <fmt/chrono.h>
#include <libassert/assert.hpp>
#include <limits>
#include <memory>
#include <sys/stat.h>

namespace rf_builder::run {
Expected<std::string> identifier(const std::string& input)
{
    if (input.empty()) {
        return Error::fail(Error::Code::InvalidInput, "empty RF input identifier");
    }
    if (input.starts_with("http://") || input.starts_with("https://") || input.starts_with("/vsi")) {
        return input;
    }
    std::error_code error;
    const auto path = std::filesystem::absolute(input, error);
    if (error) {
        return Error::fail(Error::Code::Io, "resolve RF input identifier", input, error);
    }
    return path.lexically_normal().string();
}

std::string gdal_identifier(const std::string& input) { return input.starts_with("http://") || input.starts_with("https://") ? "/vsicurl/" + input : input; }

Expected<void> check_link_filesystem(const std::filesystem::path& cache, const std::filesystem::path& output)
{
    struct stat cache_status {};
    if (::stat(cache.c_str(), &cache_status) != 0) {
        return Error::fail(Error::Code::Io, "inspect RF cache filesystem", cache, std::error_code(errno, std::generic_category()));
    }
    auto parent = std::filesystem::absolute(output).parent_path();
    struct stat output_status {};
    while (::stat(parent.c_str(), &output_status) != 0) {
        if (errno != ENOENT || parent == parent.parent_path()) {
            return Error::fail(Error::Code::Io, "inspect RF output filesystem", parent, std::error_code(errno, std::generic_category()));
        }
        parent = parent.parent_path();
    }
    if (cache_status.st_dev != output_status.st_dev) {
        return Error::fail(Error::Code::Unsupported, "RF cache and output must be on the same filesystem for hard-link reuse");
    }
    return {};
}

void validate_options(const Options& options)
{
    // The command line requires positive worker counts and tile sizes.
    ASSERT(options.jobs > 0);
    ASSERT(options.tile_side > 0);
    if (options.tile_side > unsigned((std::numeric_limits<int>::max)()) || options.attribution_index == 0
        || options.attribution_index >= raster_store::attribution::index_limit) {
        Error::raise(Error::Code::InvalidInput, "RF requires tile dimensions within GDAL limits and attribution in 1..65534");
    }
}
Expected<raster_store::attribution::Entity> attribution(const Options& options)
{
    auto partial = options.output;
    partial += ".part";
    auto table = raster_store::attribution::read_table(partial / raster_store::io::manifest::index_file_name);
    if (!table) {
        return Error::propagate(std::move(table));
    }
    auto entry = table->at(options.attribution_index);
    if (!entry) {
        return Error::propagate(std::move(entry));
    }
    return **entry;
}

template <typename PixelType>
Report execute(const Options& options, Source<PixelType> source, const std::function<bool()>& stop_requested)
{
    validate_options(options);
    std::unique_ptr<const raster_store::storage::IndexedStorage<PixelType>> cache;
    if (options.cache) {
        Error::throwing_unwrap(source.validate_cache(*options.cache));
        auto [input, metadata] = Error::throwing_unwrap(raster_store::storage::open<PixelType>(*options.cache, { .allow_incomplete = true }), "open RF cache");
        if (metadata->nominal_tile_size != options.tile_side || metadata->stored_tile_size != options.tile_side || metadata->halo_width != 0
            || metadata->value_mapping != options.value_mapping.value_or(raster_store::pixel::default_mapping<PixelType>)
            || metadata->codec_selector != "amort") {
            Error::raise(Error::Code::InvalidInput, "RF cache metadata disagrees with requested dimensions, halo, mapping, or codec");
        }
        const auto table = Error::throwing_unwrap(raster_store::attribution::read_table(*options.cache / raster_store::io::manifest::index_file_name));
        if (*Error::throwing_unwrap(table.at(options.attribution_index)) != source.attribution) {
            Error::raise(Error::Code::InvalidInput, "RF cache attribution entry disagrees with the output table");
        }
        Error::throwing_unwrap(check_link_filesystem(*options.cache, options.output));
        if (source.refine_cached) {
            for (const auto& [key, status] : input->index()) {
                if (status == store::NodeStatus::Inner) {
                    Error::raise(Error::Code::CorruptData, "online RF cache contains overlapping physical ancestors/descendants at " + to_string(key));
                }
            }
        }
        cache = std::move(input);
    }
    raster_store::storage::CreateOptions create_options;
    create_options.nominal_tile_size = options.tile_side;
    create_options.halo_width = 0;
    create_options.value_mapping = options.value_mapping;
    auto output = std::move(Error::throwing_unwrap(raster_store::storage::create<PixelType>(options.output, create_options), "create RF output").first);
    const auto input_path = output->base_path() / "inputs.tmp";
    Error::throwing_unwrap(source.write_inputs(input_path), "write RF input record");
    Report report { .tile_side = options.tile_side };
    auto last_checkpoint = std::chrono::steady_clock::now();
    const auto checkpoint = [&] {
        if (std::chrono::steady_clock::now() - last_checkpoint < std::chrono::minutes(2)) {
            return;
        }
        Error::throwing_unwrap(output->save_index(), "checkpoint RF index");
        last_checkpoint = std::chrono::steady_clock::now();
        LOG_INFO("RF checkpoint: {} tiles, {} bytes", report.tile_count, report.tile_bytes);
    };
    const auto cancel = [&] {
        Error::throwing_unwrap(output->save_index(), "checkpoint cancelled RF import");
        LOG_INFO("RF cancelled: checkpointed {} tiles; incomplete snapshot retained", report.tile_count);
        Error::raise(Error::Code::Cancelled, "RF import cancelled; incomplete snapshot retained");
    };
    const auto initial_poll = [&] {
        if (stop_requested && stop_requested()) {
            cancel();
        }
        checkpoint();
    };
    const double total = source.total(initial_poll);
    const bool geographic = bool(source.refine_cached);
    const auto started = std::chrono::steady_clock::now();
    auto last_progress = started;
    double completed = 0, noncached_completed = 0;
    std::uint64_t candidates = 0;
    const auto progress = [&](bool force) {
        const auto now = std::chrono::steady_clock::now();
        if (!force && now - last_progress < std::chrono::seconds(10)) {
            return;
        }
        last_progress = now;
        const auto elapsed = std::chrono::duration<double>(now - started).count();
        const auto remaining_weight = (std::max)(0., total - completed);
        const double observed = geographic ? noncached_completed : completed;
        const double seconds = observed > 0 ? std::ceil(elapsed * remaining_weight / observed) : (remaining_weight == 0 ? 0 : INFINITY);
        const bool known = std::isfinite(seconds) && seconds < double((std::numeric_limits<std::int64_t>::max)() / 2);
        std::string eta = "remaining unknown until usable completion observations";
        if (known) {
            const auto remaining = std::chrono::seconds(std::int64_t(seconds));
            // Keep chrono's nanosecond system_clock arithmetic in range.
            if (seconds < 100. * 365 * 24 * 3600) {
                const auto finish = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now() + remaining);
                eta = fmt::format("remaining {}h {:02}m {:02}s; expected finish {:%Y-%m-%d %H:%M:%S} UTC",
                    remaining.count() / 3600,
                    remaining.count() / 60 % 60,
                    remaining.count() % 60,
                    finish);
            }
        }
        const auto percent = total > 0 ? (std::min)(100., 100. * completed / total) : 100.;
        if (geographic) {
            const auto stats = source.network_stats();
            LOG_INFO("RF progress: estimated {:.1f}%; elapsed {:.0f}s; estimated {}; {} written, {} reused; {} requests, {} downloaded bytes",
                percent,
                elapsed,
                eta,
                report.tile_count - report.reused_tiles,
                report.reused_tiles,
                stats.requests,
                stats.bytes);
        } else {
            LOG_INFO("RF progress: {}/{} candidate tiles ({:.1f}%); {}; {} written, {} reused",
                candidates,
                std::uint64_t(total),
                percent,
                eta,
                report.tile_count,
                report.reused_tiles);
        }
    };
    progress(true);
    const unsigned jobs = geographic ? (total > 0 ? options.jobs : 0) : unsigned((std::min)(double(options.jobs), total));
    source.initialize(jobs, initial_poll);
    // Worker exceptions travel through the pool as errors and are rethrown by the coordinator.
    raster_store::TilePool<Prepared<PixelType>> pool(jobs, source.prepare);
    // Each lane traverses depth first and has only one outstanding preparation.
    // Thus each stack retains at most three siblings per level, independently of
    // completion order. Pool slots bound all queued/active/completed payloads.
    struct Lane {
        std::vector<Key> pending;
        std::optional<Key> active = std::nullopt;
    };
    std::vector<Lane> lanes(std::size_t(jobs) * 2);
    bool exhausted = jobs == 0, cancelled = false;
    LOG_INFO("RF workers: {}; at most {} outstanding tiles", jobs, lanes.size());
    const auto finish = [&](const Key& key, bool reused, bool payload) {
        if (payload) {
            const auto path = Error::asserting_unwrap(output->path_for(key));
            std::error_code error;
            const auto bytes = std::filesystem::file_size(path, error);
            if (error) {
                Error::raise(Error::Code::Io, "measure RF payload", path, error);
            }
            report.tile_bytes += bytes;
            ++report.tile_count;
            if (reused) {
                ++report.reused_tiles;
            }
        }
        const auto weight = source.weight(key);
        completed += weight;
        if (!reused) {
            noncached_completed += weight;
        }
        ++candidates;
        progress(candidates == 1 || completed >= total);
        checkpoint();
    };
    const auto subdivide = [&](Lane& lane, const Key& parent, Subdivide division) {
        const auto children = raster_store::StoreTraits::children(parent);
        ASSERT(children && division.children.size() <= 4, to_string(parent));
        for (std::size_t i = 0; i < division.children.size(); ++i) {
            const auto child = division.children[i];
            ASSERT(std::ranges::find(*children, child) != children->end()
                    && std::find(division.children.begin(), division.children.begin() + i, child) == division.children.begin() + i,
                to_string(parent),
                to_string(child));
        }
        lane.pending.insert(lane.pending.end(), division.children.rbegin(), division.children.rend());
    };
    const auto consume = [&](typename raster_store::TilePool<Prepared<PixelType>>::Completed done) {
        auto lane = std::ranges::find_if(lanes, [&](const auto& item) { return item.active == done.key; });
        ASSERT(lane != lanes.end(), to_string(done.key));
        lane->active.reset();
        auto prepared = Error::throwing_unwrap(std::move(done.result), "produce RF snapshot");
        if (auto* division = std::get_if<Subdivide>(&prepared)) {
            if (!cancelled) {
                subdivide(*lane, done.key, std::move(*division));
            }
            return;
        }
        auto* tile = std::get_if<raster_store::Tile<PixelType>>(&prepared);
        if (tile) {
            Error::throwing_unwrap(output->save(done.key, *tile), "write RF tile " + to_string(done.key));
        }
        finish(done.key, false, tile != nullptr);
    };
    // Cancellation discards queued work, saves the active tiles and throws.
    const auto poll = [&] {
        if (stop_requested && stop_requested()) {
            LOG_INFO("RF cancellation requested: discarding queued work and finishing active tiles");
            cancelled = true;
            pool.stop();
            while (pool.outstanding() != 0) {
                if (auto done = pool.take(std::chrono::milliseconds(100))) {
                    consume(std::move(*done));
                }
            }
            cancel();
        }
        progress(false);
        checkpoint();
    };
    const auto schedule = [&](Lane& lane) {
        while (!lane.active) {
            poll();
            if (lane.pending.empty()) {
                // Idle lanes take a sibling subtree before requesting another
                // root. Without this, a one-root import uses only one worker.
                auto donor = std::ranges::find_if(lanes, [&](const auto& other) { return &other != &lane && !other.pending.empty(); });
                if (donor != lanes.end()) {
                    lane.pending.push_back(donor->pending.back());
                    donor->pending.pop_back();
                    continue;
                }
                if (exhausted) {
                    return;
                }
                const auto next = source.next(poll);
                if (!next) {
                    exhausted = true;
                    return;
                }
                lane.pending.push_back(*next);
            }
            const auto key = lane.pending.back();
            lane.pending.pop_back();
            if (cache) {
                const auto present = Error::asserting_unwrap(cache->index().get(key));
                if (present && *present != store::NodeStatus::Virtual) {
                    Error::throwing_unwrap(output->copy_from(key, *cache), "hard-link RF cache tile " + to_string(key));
                    finish(key, true, true);
                    continue;
                }
                if (present && source.refine_cached) {
                    subdivide(lane, key, source.refine_cached(key));
                    continue;
                }
            }
            // A worker failure stopped the pool; consuming its result throws.
            if (!pool.submit(key)) {
                return;
            }
            lane.active = key;
        }
    };
    for (;;) {
        poll();
        for (auto& lane : lanes) {
            schedule(lane);
        }
        if (pool.outstanding() == 0 && exhausted && std::ranges::all_of(lanes, [](const auto& lane) { return lane.pending.empty(); })) {
            break;
        }
        if (auto done = pool.take(std::chrono::milliseconds(100))) {
            consume(std::move(*done));
        }
    }
    pool.join();
    progress(true);
    LOG_INFO("RF finalizing: publishing {} tiles", report.tile_count);
    std::error_code error;
    if (!std::filesystem::remove(input_path, error)) {
        Error::raise(Error::Code::Io, "remove RF input record before publication", input_path, error);
    }
    Error::throwing_unwrap(raster_store::storage::publish(std::move(output)), "publish RF snapshot");
    return report;
}
template Report execute<float>(const Options&, Source<float>, const std::function<bool()>&);
template Report execute<glm::u8vec3>(const Options&, Source<glm::u8vec3>, const std::function<bool()>&);
} // namespace rf_builder::run
