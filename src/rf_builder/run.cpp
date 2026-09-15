#include "run.h"
#include "TilePool.h"
#include "raster_store/storage.h"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <fmt/chrono.h>
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

Expected<void> validate_options(const Options& options)
{
    if (options.jobs == 0 || options.tile_side == 0 || options.tile_side > unsigned((std::numeric_limits<int>::max)()) || options.attribution_index == 0
        || options.attribution_index >= raster_store::attribution::index_limit) {
        return Error::fail(Error::Code::InvalidInput, "RF requires positive jobs/dimensions and attribution in 1..65534");
    }
    return {};
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
Expected<Report> execute(const Options& options, Source<PixelType> source, const std::function<bool()>& stop_requested)
{
    if (auto checked = validate_options(options); !checked) {
        return Error::propagate(std::move(checked));
    }
    std::unique_ptr<const raster_store::storage::IndexedStorage<PixelType>> cache;
    if (options.cache) {
        if (auto valid = source.validate_cache(*options.cache); !valid) {
            return Error::propagate(std::move(valid));
        }
        auto opened = raster_store::storage::open<PixelType>(*options.cache, { .allow_incomplete = true });
        if (!opened) {
            return Error::propagate(std::move(opened));
        }
        auto [input, metadata] = std::move(*opened);
        if (metadata->width != options.tile_side || metadata->height != options.tile_side || metadata->codec_selector != "amort") {
            return Error::fail(Error::Code::InvalidInput, "RF cache metadata disagrees with requested dimensions or codec");
        }
        auto table = raster_store::attribution::read_table(*options.cache / raster_store::io::manifest::index_file_name);
        if (!table) {
            return Error::propagate(std::move(table));
        }
        auto entry = table->at(options.attribution_index);
        if (!entry) {
            return Error::propagate(std::move(entry));
        }
        if (**entry != source.attribution) {
            return Error::fail(Error::Code::InvalidInput, "RF cache attribution entry disagrees with the output table");
        }
        if (auto checked = check_link_filesystem(*options.cache, options.output); !checked) {
            return Error::propagate(std::move(checked));
        }
        if (source.refine_cached) {
            for (const auto& [key, status] : input->index()) {
                if (status == store::NodeStatus::Inner) {
                    return Error::fail(Error::Code::CorruptData, "online RF cache contains overlapping physical ancestors/descendants at " + to_string(key));
                }
            }
        }
        cache = std::move(input);
    }
    raster_store::storage::CreateOptions create_options;
    create_options.tile_dimensions = glm::uvec2(options.tile_side);
    auto created = raster_store::storage::create<PixelType>(options.output, create_options);
    if (!created) {
        return Error::propagate(std::move(created));
    }
    auto [output, metadata] = std::move(*created);
    const auto input_path = output->base_path() / "inputs.tmp";
    if (auto written = source.write_inputs(input_path); !written) {
        return Error::propagate(std::move(written));
    }
    Report report { .tile_side = options.tile_side };
    auto last_checkpoint = std::chrono::steady_clock::now();
    const auto checkpoint = [&]() -> Expected<void> {
        if (std::chrono::steady_clock::now() - last_checkpoint < std::chrono::minutes(2)) {
            return {};
        }
        if (auto saved = output->save_index(); !saved) {
            return saved;
        }
        last_checkpoint = std::chrono::steady_clock::now();
        LOG_INFO("RF checkpoint: {} tiles, {} bytes", report.tile_count, report.tile_bytes);
        return {};
    };
    const auto initial_poll = [&]() -> Expected<void> {
        if (stop_requested && stop_requested()) {
            return Error::fail(Error::Code::Cancelled, "RF import cancelled; incomplete snapshot retained");
        }
        return checkpoint();
    };
    auto counted = source.total(initial_poll);
    if (!counted) {
        return Error::propagate(std::move(counted));
    }
    const double total = *counted;
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
    if (auto initialized = source.initialize(jobs, initial_poll); !initialized) {
        return Error::propagate(std::move(initialized));
    }
    TilePool<Prepared<PixelType>> pool(jobs, source.prepare);
    // Each lane traverses depth first and has only one outstanding preparation.
    // Thus each stack retains at most three siblings per level, independently of
    // completion order. Pool slots bound all queued/active/completed payloads.
    struct Lane {
        std::vector<Key> pending;
        std::optional<Key> active = std::nullopt;
    };
    std::vector<Lane> lanes(std::size_t(jobs) * 2);
    bool exhausted = jobs == 0, cancelled = false;
    std::optional<Error> failure = std::nullopt;
    LOG_INFO("RF workers: {}; at most {} outstanding tiles", jobs, lanes.size());
    const auto poll = [&]() -> Expected<void> {
        if (auto error = pool.failure(); error && !failure) {
            failure = std::move(*error);
        }
        if (!cancelled && stop_requested && stop_requested()) {
            cancelled = true;
            LOG_INFO("RF cancellation requested: discarding queued work and finishing active tiles");
        }
        if (failure || cancelled) {
            pool.stop();
        }
        progress(false);
        if (failure) {
            return Error::propagate(Error(*failure));
        }
        if (cancelled) {
            return Error::fail(Error::Code::Cancelled, "RF import cancelled; incomplete snapshot retained");
        }
        return checkpoint();
    };
    const auto finish = [&](const Key& key, bool reused, bool payload) -> Expected<void> {
        if (payload) {
            auto path = output->path_for(key);
            if (!path) {
                return Error::propagate(std::move(path));
            }
            std::error_code error;
            const auto bytes = std::filesystem::file_size(*path, error);
            if (error) {
                return Error::fail(Error::Code::Io, "measure RF payload", *path, error);
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
        return checkpoint();
    };
    const auto subdivide = [&](Lane& lane, const Key& parent, Subdivide division) -> Expected<void> {
        const auto children = raster_store::StoreTraits::children(parent);
        if (!children || division.children.size() > 4) {
            return Error::fail(Error::Code::Internal, "invalid RF subdivision");
        }
        for (std::size_t i = 0; i < division.children.size(); ++i) {
            const auto child = division.children[i];
            if (std::ranges::find(*children, child) == children->end()
                || std::find(division.children.begin(), division.children.begin() + i, child) != division.children.begin() + i) {
                return Error::fail(Error::Code::Internal, "invalid or duplicate RF subdivision child");
            }
        }
        lane.pending.insert(lane.pending.end(), division.children.rbegin(), division.children.rend());
        return {};
    };
    const auto consume = [&](typename TilePool<Prepared<PixelType>>::Completed done) -> Expected<void> {
        auto lane = std::ranges::find_if(lanes, [&](const auto& item) { return item.active == done.key; });
        if (lane == lanes.end()) {
            return Error::fail(Error::Code::Internal, "completed RF candidate has no scheduling lane");
        }
        lane->active.reset();
        if (!done.result) {
            if (auto first = pool.failure()) {
                return Error::propagate(std::move(*first));
            }
            return Error::propagate(std::move(done.result));
        }
        if (failure) {
            return {};
        }
        if (auto* division = std::get_if<Subdivide>(&*done.result)) {
            return cancelled ? Expected<void>() : subdivide(*lane, done.key, std::move(*division));
        }
        auto* tile = std::get_if<raster_store::Tile<PixelType>>(&*done.result);
        if (tile) {
            if (auto saved = output->save(done.key, *tile); !saved) {
                return saved;
            }
        }
        return finish(done.key, false, tile != nullptr);
    };
    const auto schedule = [&](Lane& lane) -> Expected<void> {
        while (!lane.active) {
            if (auto checked = poll(); !checked) {
                return checked;
            }
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
                    return {};
                }
                auto next = source.next(poll);
                if (!next) {
                    return Error::propagate(std::move(next));
                }
                if (!*next) {
                    exhausted = true;
                    return {};
                }
                lane.pending.push_back(**next);
            }
            const auto key = lane.pending.back();
            lane.pending.pop_back();
            if (cache) {
                auto present = cache->index().get(key);
                if (!present) {
                    return Error::propagate(std::move(present));
                }
                if (*present && **present != store::NodeStatus::Virtual) {
                    if (auto linked = output->copy_from(key, *cache); !linked) {
                        return linked;
                    }
                    if (auto finished = finish(key, true, true); !finished) {
                        return finished;
                    }
                    continue;
                }
                if (*present && source.refine_cached) {
                    if (auto divided = subdivide(lane, key, source.refine_cached(key)); !divided) {
                        return divided;
                    }
                    continue;
                }
            }
            if (!pool.submit(key)) {
                if (auto first = pool.failure()) {
                    return Error::propagate(std::move(*first));
                }
                return Error::fail(Error::Code::Internal, "RF pool rejected available scheduling lane");
            }
            lane.active = key;
        }
        return {};
    };
    for (;;) {
        auto checked = poll();
        if (!checked && checked.error().code() != Error::Code::Cancelled && !failure) {
            failure = std::move(checked).error();
            pool.stop();
        }
        if (!failure && !cancelled) {
            for (auto& lane : lanes) {
                if (auto scheduled = schedule(lane); !scheduled) {
                    if (scheduled.error().code() != Error::Code::Cancelled && !failure) {
                        failure = std::move(scheduled).error();
                    }
                    pool.stop();
                    break;
                }
            }
        }
        if (pool.outstanding() == 0
            && (failure || cancelled || (exhausted && std::ranges::all_of(lanes, [](const auto& lane) { return lane.pending.empty(); })))) {
            break;
        }
        if (auto done = pool.take(std::chrono::milliseconds(100))) {
            if (auto consumed = consume(std::move(*done)); !consumed && !failure) {
                failure = std::move(consumed).error();
                pool.stop();
            }
        }
    }
    pool.join();
    if (auto checked = poll(); !checked && checked.error().code() != Error::Code::Cancelled && !failure) {
        failure = std::move(checked).error();
    }
    if (failure) {
        return Error::propagate(std::move(*failure), "produce RF snapshot");
    }
    if (cancelled) {
        if (auto saved = output->save_index(); !saved) {
            return Error::propagate(std::move(saved));
        }
        LOG_INFO("RF cancelled: checkpointed {} tiles; incomplete snapshot retained", report.tile_count);
        return Error::fail(Error::Code::Cancelled, "RF import cancelled; incomplete snapshot retained");
    }
    progress(true);
    LOG_INFO("RF finalizing: publishing {} tiles", report.tile_count);
    std::error_code error;
    if (!std::filesystem::remove(input_path, error)) {
        return Error::fail(Error::Code::Io, "remove RF input record before publication", input_path, error);
    }
    if (auto published = raster_store::storage::publish(std::move(output)); !published) {
        return Error::propagate(std::move(published));
    }
    return report;
}
template Expected<Report> execute<float>(const Options&, Source<float>, const std::function<bool()>&);
template Expected<Report> execute<glm::u8vec3>(const Options&, Source<glm::u8vec3>, const std::function<bool()>&);
} // namespace rf_builder::run
