#include "merge.h"

#include "inputs.h"
#include "log.h"
#include "partition.h"
#include "priorities.h"
#include "produce.h"
#include "raster_store/TilePool.h"
#include "raster_store/storage.h"
#include <cerrno>
#include <chrono>
#include <cmath>
#include <sys/stat.h>

namespace rf_merger::merge {
namespace {
    using Key = radix::tile::Id;
    using selection::Side;
    namespace storage = raster_store::storage;
    using Metadata = raster_store::io::manifest::Metadata;

    Expected<void> check_link_filesystem(const std::filesystem::path& source, const std::filesystem::path& output)
    {
        struct stat source_status {};
        if (::stat(source.c_str(), &source_status) != 0) {
            return Error::fail(Error::Code::Io, "inspect RF merger input filesystem", source, std::error_code(errno, std::generic_category()));
        }
        auto parent = std::filesystem::absolute(output).parent_path();
        struct stat output_status {};
        while (::stat(parent.c_str(), &output_status) != 0) {
            if (errno != ENOENT || parent == parent.parent_path()) {
                return Error::fail(Error::Code::Io, "inspect RF merger output filesystem", parent, std::error_code(errno, std::generic_category()));
            }
            parent = parent.parent_path();
        }
        if (source_status.st_dev != output_status.st_dev) {
            return Error::fail(Error::Code::Unsupported, "RF merger input or cache and output must be on the same filesystem for hard links", source);
        }
        return {};
    }

    Expected<void> reject_inner(const partition::Index& index, std::string_view role)
    {
        for (const auto& [key, status] : index) {
            if (status == store::NodeStatus::Inner) {
                return Error::fail(Error::Code::InvalidInput, std::string(role) + " has a physical tile with physical descendants at " + to_string(key));
            }
        }
        return {};
    }

    Expected<void> check_inputs(const Metadata& left, const Metadata& right)
    {
        if (left.payload_type != right.payload_type || left.nominal_tile_size != right.nominal_tile_size || left.stored_tile_size != right.stored_tile_size
            || left.value_mapping != right.value_mapping) {
            return Error::fail(Error::Code::InvalidInput, "RF merger inputs differ in payload type, tile dimensions or value mapping");
        }
        const storage::CreateOptions defaults;
        if (left.codec_selector != defaults.codec_selector || right.codec_selector != defaults.codec_selector) {
            return Error::fail(Error::Code::InvalidInput, "RF merger input codecs must equal the output codec " + defaults.codec_selector);
        }
        if (left.halo_width != 0 || right.halo_width != 0) {
            return Error::fail(Error::Code::InvalidInput, "RF merger inputs must have zero stored halo");
        }
        if (left.nominal_tile_size < 64) {
            return Error::fail(Error::Code::Unsupported, "RF merger requires tiles of at least 64 pixels per side");
        }
        return {};
    }

    std::string remaining(double seconds)
    {
        if (!std::isfinite(seconds) || seconds > 100. * 365 * 24 * 3600) {
            return "remaining unknown until usable completion observations";
        }
        const auto total = std::int64_t(std::ceil(seconds));
        return fmt::format("estimated remaining {}h {:02}m {:02}s", total / 3600, total / 60 % 60, total % 60);
    }

    template <typename PixelType>
    Expected<Report> execute(const Options& options, const std::vector<std::uint16_t>& priorities, const std::function<bool()>& stop_requested)
    {
        auto opened_left = storage::open<PixelType>(options.left);
        if (!opened_left) {
            return Error::propagate(std::move(opened_left), "open left RF merger input");
        }
        auto opened_right = storage::open<PixelType>(options.right);
        if (!opened_right) {
            return Error::propagate(std::move(opened_right), "open right RF merger input");
        }
        auto [left, left_metadata] = std::move(*opened_left);
        auto [right, right_metadata] = std::move(*opened_right);
        if (auto checked = check_inputs(*left_metadata, *right_metadata); !checked) {
            return Error::propagate(std::move(checked));
        }
        if (auto checked = reject_inner(left->index(), "left RF merger input"); !checked) {
            return Error::propagate(std::move(checked));
        }
        if (auto checked = reject_inner(right->index(), "right RF merger input"); !checked) {
            return Error::propagate(std::move(checked));
        }
        const auto& metadata = *left_metadata;
        inputs::Record record;
        for (auto [path, fingerprint] : { std::pair { &options.left, &record.left }, std::pair { &options.right, &record.right } }) {
            auto computed = inputs::fingerprint(*path);
            if (!computed) {
                return Error::propagate(std::move(computed));
            }
            *fingerprint = std::move(*computed);
        }
        record.priorities = priorities;
        record.payload_type = metadata.payload_type;
        record.nominal_tile_size = metadata.nominal_tile_size;
        record.stored_tile_size = metadata.stored_tile_size;
        record.halo_width = metadata.halo_width;
        record.value_mapping = metadata.value_mapping;
        for (const auto* source : { &options.left, &options.right }) {
            if (auto checked = check_link_filesystem(*source, options.output); !checked) {
                return Error::propagate(std::move(checked));
            }
        }

        std::unique_ptr<const storage::IndexedStorage<PixelType>> cache;
        statistics::Totals totals;
        bool complete = true;
        if (options.cache) {
            if (auto valid = inputs::validate_cache(*options.cache, record); !valid) {
                return Error::propagate(std::move(valid));
            }
            if (auto checked = check_link_filesystem(*options.cache, options.output); !checked) {
                return Error::propagate(std::move(checked));
            }
            auto opened = storage::open<PixelType>(*options.cache, { .allow_incomplete = true });
            if (!opened) {
                return Error::propagate(std::move(opened), "open RF merger cache");
            }
            auto [input, cache_metadata] = std::move(*opened);
            if (cache_metadata->nominal_tile_size != metadata.nominal_tile_size || cache_metadata->stored_tile_size != metadata.stored_tile_size
                || cache_metadata->halo_width != 0 || cache_metadata->value_mapping != metadata.value_mapping
                || cache_metadata->codec_selector != metadata.codec_selector) {
                return Error::fail(Error::Code::InvalidInput, "RF merger cache metadata disagrees with the inputs");
            }
            if (auto checked = reject_inner(input->index(), "RF merger cache"); !checked) {
                return Error::propagate(std::move(checked), Error::Code::CorruptData, "validate RF merger cache");
            }
            for (const auto& [key, status] : input->index()) {
                if (status != store::NodeStatus::Leaf) {
                    continue;
                }
                auto leaf = partition::is_leaf(left->index(), right->index(), key);
                if (!leaf) {
                    return Error::propagate(std::move(leaf));
                }
                if (!*leaf) {
                    return Error::fail(Error::Code::CorruptData, "RF merger cache tile " + to_string(key) + " is not part of the merge output");
                }
            }
            auto restored = statistics::read(*options.cache);
            if (restored) {
                totals = *restored;
            } else {
                complete = false;
                LOG_WARN("RF merger cache statistics are missing or corrupt; statistics will be incomplete: {}", restored.error().to_string());
            }
            cache = std::move(input);
        }

        storage::CreateOptions create_options;
        create_options.nominal_tile_size = metadata.nominal_tile_size;
        create_options.halo_width = 0;
        create_options.value_mapping = metadata.value_mapping;
        create_options.compression_algorithm = options.compression_algorithm;
        create_options.checksum_algorithm = options.checksum_algorithm;
        auto created = storage::create<PixelType>(options.output, create_options);
        if (!created) {
            return Error::propagate(std::move(created), "create RF merger output");
        }
        auto output = std::move(created->first);
        const auto input_path = output->base_path() / inputs::file_name;
        if (auto written = io::envelope::write_to_path<inputs::Schema>(record, input_path); !written) {
            return Error::propagate(std::move(written), "write RF merger input record");
        }

        const auto ranks = priorities::ranks(priorities);
        const produce::Inputs<PixelType> sources { { left.get(), left_metadata.get() }, { right.get(), right_metadata.get() }, &ranks };
        const auto save = [&]() -> Expected<void> {
            if (auto written = statistics::write(totals, output->base_path()); !written) {
                return written;
            }
            return output->save_index();
        };

        LOG_INFO("RF merge planning: counting output leaves from the input indices");
        std::uint64_t total = 0;
        for (partition::Cursor counter(left->index(), right->index());;) {
            auto next = counter.next();
            if (!next) {
                return Error::propagate(std::move(next), "count RF merger output leaves");
            }
            if (!*next) {
                break;
            }
            ++total;
        }

        const unsigned jobs = unsigned((std::min)(std::uint64_t(options.jobs), (std::max)(total, std::uint64_t(1))));
        raster_store::TilePool<produce::Produced<PixelType>> pool(jobs, [&](unsigned, const Key& key) -> Expected<produce::Produced<PixelType>> {
            partition::Leaf leaf { key, std::nullopt, std::nullopt };
            for (auto [index, supplier] : { std::pair { &left->index(), &leaf.left }, std::pair { &right->index(), &leaf.right } }) {
                auto found = partition::supplier(*index, key);
                if (!found) {
                    return Error::propagate(std::move(found));
                }
                *supplier = *found;
            }
            return produce::produce(sources, leaf);
        });
        LOG_INFO("RF merge: {} output leaves; {} workers; at most {} outstanding tiles", total, jobs, 2 * jobs);

        const auto started = std::chrono::steady_clock::now();
        auto last_progress = started;
        auto last_checkpoint = started;
        std::uint64_t completed = 0, computed = 0, restored = 0;
        const auto progress = [&](bool force) {
            const auto now = std::chrono::steady_clock::now();
            if (!force && now - last_progress < std::chrono::seconds(10)) {
                return;
            }
            last_progress = now;
            const auto elapsed = std::chrono::duration<double>(now - started).count();
            const double seconds = computed == 0 ? INFINITY : elapsed * double(total - completed) / double(computed);
            LOG_INFO("RF merge progress: {}/{} leaves ({:.1f}%); elapsed {:.0f}s; {}; {} restored from cache",
                completed,
                total,
                total == 0 ? 100. : 100. * double(completed) / double(total),
                elapsed,
                completed == total ? "complete" : remaining(seconds),
                restored);
        };
        const auto finish = [&](const Key& key, statistics::Category category, bool from_cache) -> Expected<void> {
            auto path = output->path_for(key);
            if (!path) {
                return Error::propagate(std::move(path));
            }
            std::error_code error;
            const auto bytes = std::filesystem::file_size(*path, error);
            if (error) {
                return Error::fail(Error::Code::Io, "measure RF merger payload", *path, error);
            }
            if (!from_cache || !complete) {
                totals.add(category, bytes);
            }
            ++completed;
            if (from_cache) {
                ++restored;
            } else {
                ++computed;
            }
            progress(completed == 1);
            return {};
        };
        const auto link = [&](const Key& key, Side side) -> Expected<void> {
            const auto& input = side == Side::Left ? *left : *right;
            if (auto linked = output->copy_from(key, input); !linked) {
                return Error::propagate(std::move(linked), "hard-link RF merger tile " + to_string(key));
            }
            return finish(key, side == Side::Left ? statistics::Category::LinkedLeft : statistics::Category::LinkedRight, false);
        };
        // Cached leaves and single native suppliers need no pixel reads.
        const auto direct = [&](const partition::Leaf& leaf) -> Expected<bool> {
            if (cache) {
                auto status = cache->index().get(leaf.key);
                if (!status) {
                    return Error::propagate(std::move(status));
                }
                if (*status == store::NodeStatus::Leaf) {
                    if (auto linked = output->copy_from(leaf.key, *cache); !linked) {
                        return Error::propagate(std::move(linked), "hard-link RF merger cache tile " + to_string(leaf.key));
                    }
                    if (auto finished = finish(leaf.key, statistics::Category::Uncategorized, true); !finished) {
                        return Error::propagate(std::move(finished));
                    }
                    return true;
                }
            }
            if (leaf.left.has_value() != leaf.right.has_value() && leaf.left.value_or(leaf.right.value_or(Key {})) == leaf.key) {
                if (auto linked = link(leaf.key, leaf.left ? Side::Left : Side::Right); !linked) {
                    return Error::propagate(std::move(linked));
                }
                return true;
            }
            return false;
        };

        std::optional<Error> failure;
        bool cancelled = false, exhausted = false;
        const auto fail = [&](Error error) {
            if (!failure) {
                failure = std::move(error);
            }
            pool.stop();
        };
        const auto consume = [&](typename raster_store::TilePool<produce::Produced<PixelType>>::Completed done) -> Expected<void> {
            if (!done.result) {
                return Error::propagate(std::move(done.result), "produce RF merger tile " + to_string(done.key));
            }
            if (failure) {
                return {};
            }
            if (const auto* linked = std::get_if<produce::Link>(&*done.result)) {
                return link(done.key, linked->side);
            }
            auto& tile = std::get<produce::Created<PixelType>>(*done.result);
            if (auto saved = output->save(done.key, tile.tile); !saved) {
                return Error::propagate(std::move(saved), "write RF merger tile " + to_string(done.key));
            }
            return finish(done.key, tile.category, false);
        };

        const auto poll = [&] {
            if (auto error = pool.failure(); error && !failure) {
                fail(std::move(*error));
            }
            if (!cancelled && !failure && stop_requested && stop_requested()) {
                cancelled = true;
                LOG_INFO("RF merge cancellation requested: discarding queued work and finishing active tiles");
                pool.stop();
            }
            if (!failure && !cancelled && std::chrono::steady_clock::now() - last_checkpoint >= std::chrono::minutes(2)) {
                if (auto saved = save(); !saved) {
                    fail(std::move(saved).error());
                } else {
                    last_checkpoint = std::chrono::steady_clock::now();
                    LOG_INFO("RF merge checkpoint: {} tiles", completed);
                }
            }
            progress(false);
        };

        progress(true);
        partition::Cursor cursor(left->index(), right->index());
        std::optional<partition::Leaf> pending;
        for (;;) {
            poll();
            while (!failure && !cancelled && !exhausted) {
                if (!pending) {
                    auto next = cursor.next();
                    if (!next) {
                        fail(Error::propagate(std::move(next), "plan RF merger output").error());
                        break;
                    }
                    if (!*next) {
                        exhausted = true;
                        break;
                    }
                    pending = *next;
                }
                auto handled = direct(*pending);
                if (!handled) {
                    fail(std::move(handled).error());
                    break;
                }
                if (*handled) {
                    pending.reset();
                    poll();
                    continue;
                }
                if (!pool.submit(pending->key)) {
                    break;
                }
                pending.reset();
            }
            if (pool.outstanding() == 0 && (failure || cancelled || exhausted)) {
                break;
            }
            if (auto done = pool.take(std::chrono::milliseconds(100))) {
                if (auto consumed = consume(std::move(*done)); !consumed) {
                    fail(std::move(consumed).error());
                }
            }
        }
        pool.join();
        if (auto error = pool.failure(); error && !failure) {
            failure = std::move(*error);
        }
        if (failure) {
            if (auto saved = save(); !saved) {
                LOG_WARN("RF merge could not checkpoint after failure: {}", saved.error().to_string());
            }
            return Error::propagate(std::move(*failure), "produce RF merger snapshot");
        }
        if (cancelled) {
            if (auto saved = save(); !saved) {
                return Error::propagate(std::move(saved), "checkpoint cancelled RF merge");
            }
            LOG_INFO("RF merge cancelled: checkpointed {} tiles; incomplete snapshot retained", completed);
            return Error::fail(Error::Code::Cancelled, "RF merge cancelled; incomplete snapshot retained");
        }
        progress(true);
        LOG_INFO("RF merge finalizing: publishing {} tiles", completed);
        std::error_code error;
        if (!std::filesystem::remove(input_path, error)) {
            return Error::fail(Error::Code::Io, "remove RF merger input record before publication", input_path, error);
        }
        const auto statistics_path = output->base_path() / statistics::file_name;
        std::filesystem::remove(statistics_path, error);
        if (error) {
            return Error::fail(Error::Code::Io, "remove RF merger statistics before publication", statistics_path, error);
        }
        if (auto published = storage::publish(std::move(output)); !published) {
            return Error::propagate(std::move(published));
        }
        return Report { totals, complete, restored, metadata.nominal_tile_size };
    }
} // namespace

Expected<Report> run(const Options& options, const std::function<bool()>& stop_requested)
{
    if (options.jobs == 0) {
        return Error::fail(Error::Code::InvalidInput, "RF merger requires at least one job");
    }
    auto priorities = priorities::read(options.priorities);
    if (!priorities) {
        return Error::propagate(std::move(priorities));
    }
    auto metadata = raster_store::io::manifest::read_metadata(options.left);
    if (!metadata) {
        return Error::propagate(std::move(metadata), "read left RF merger input metadata");
    }
    if (metadata->payload_type == raster_store::pixel::identifier<float>()) {
        return execute<float>(options, *priorities, stop_requested);
    }
    if (metadata->payload_type == raster_store::pixel::identifier<glm::u8vec3>()) {
        return execute<glm::u8vec3>(options, *priorities, stop_requested);
    }
    return Error::fail(Error::Code::Unsupported, "RF merger supports float32 scalar and RGB8 payloads, not " + metadata->payload_type);
}

} // namespace rf_merger::merge
