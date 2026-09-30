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
#include <exception>
#include <functional>
#include <libassert/assert.hpp>
#include <sys/stat.h>

namespace rf_merger::merge {
namespace {
    using Key = radix::tile::Id;
    using selection::Side;
    namespace storage = raster_store::storage;
    using Metadata = raster_store::io::manifest::Metadata;

    void check_link_filesystem(const std::filesystem::path& source, const std::filesystem::path& output)
    {
        struct stat source_status {};
        if (::stat(source.c_str(), &source_status) != 0) {
            Error::raise(Error::Code::Io, "inspect RF merger input filesystem", source, std::error_code(errno, std::generic_category()));
        }
        auto parent = std::filesystem::absolute(output).parent_path();
        struct stat output_status {};
        while (::stat(parent.c_str(), &output_status) != 0) {
            if (errno != ENOENT || parent == parent.parent_path()) {
                Error::raise(Error::Code::Io, "inspect RF merger output filesystem", parent, std::error_code(errno, std::generic_category()));
            }
            parent = parent.parent_path();
        }
        if (source_status.st_dev != output_status.st_dev) {
            Error::raise(Error::Code::Unsupported, "RF merger input or cache and output must be on the same filesystem for hard links", source);
        }
    }

    void reject_inner(const partition::Index& index, std::string_view role, Error::Code code = Error::Code::InvalidInput)
    {
        for (const auto& [key, status] : index) {
            if (status == store::NodeStatus::Inner) {
                Error::raise(code, std::string(role) + " has a physical tile with physical descendants at " + to_string(key));
            }
        }
    }

    template <typename PixelType>
    void check_inputs(const Metadata& left, const Metadata& right)
    {
        if (left.payload_type != right.payload_type || left.nominal_tile_size != right.nominal_tile_size || left.stored_tile_size != right.stored_tile_size
            || left.value_mapping != right.value_mapping) {
            Error::raise(Error::Code::InvalidInput, "RF merger inputs differ in payload type, tile dimensions or value mapping");
        }
        const storage::CreateOptions defaults;
        if (left.codec_selector != defaults.codec_selector || right.codec_selector != defaults.codec_selector) {
            Error::raise(Error::Code::InvalidInput, "RF merger input codecs must equal the output codec " + defaults.codec_selector);
        }
        if (left.halo_width != 0 || right.halo_width != 0) {
            Error::raise(Error::Code::InvalidInput, "RF merger inputs must have zero stored halo");
        }
        if (left.nominal_tile_size < 64) {
            Error::raise(Error::Code::Unsupported, "RF merger requires tiles of at least 64 pixels per side");
        }
        if (left.value_mapping == raster_store::pixel::Mapping::SRGBA && !std::is_same_v<PixelType, glm::u8vec3>) {
            Error::raise(Error::Code::Unsupported, "RF merger supports the sRGB value mapping only for RGB8 payloads");
        }
    }

    // Counts output leaves from the indices and rejects unsupported zoom gaps.
    std::uint64_t count_leaves(const partition::Index& left, const partition::Index& right)
    {
        std::uint64_t total = 0;
        partition::Cursor cursor(left, right);
        while (const auto leaf = cursor.next()) {
            for (const auto& supplier : { leaf->left, leaf->right }) {
                if (supplier && leaf->key.zoom_level - supplier->zoom_level > produce::max_zoom_levels) {
                    Error::raise(Error::Code::Unsupported,
                        "RF merger supports zoom gaps of at most " + std::to_string(produce::max_zoom_levels) + " levels, not " + to_string(*supplier) + " to "
                            + to_string(leaf->key));
                }
            }
            ++total;
        }
        return total;
    }

    std::string remaining(double seconds)
    {
        if (!std::isfinite(seconds) || seconds > 100. * 365 * 24 * 3600) {
            return "remaining unknown until usable completion observations";
        }
        const auto total = std::int64_t(std::ceil(seconds));
        return fmt::format("estimated remaining {}h {:02}m {:02}s", total / 3600, total / 60 % 60, total % 60);
    }

    // Writes the statistics when an exception unwinds, before the output
    // storage saves its index on destruction, so recovery restores both.
    class StatisticsGuard {
    public:
        StatisticsGuard(const statistics::Totals& totals, std::filesystem::path directory)
            : m_totals(&totals)
            , m_directory(std::move(directory))
        {
        }
        StatisticsGuard(const StatisticsGuard&) = delete;
        StatisticsGuard& operator=(const StatisticsGuard&) = delete;
        ~StatisticsGuard()
        {
            if (std::uncaught_exceptions() == 0) {
                return;
            }
            if (auto written = statistics::write(*m_totals, m_directory); !written) {
                LOG_WARN("RF merge could not save statistics after a failure: {}", written.error().to_string());
            }
        }

    private:
        const statistics::Totals* m_totals;
        std::filesystem::path m_directory;
    };

    template <typename PixelType>
    Report execute(const Options& options, const std::vector<std::uint16_t>& priorities, const std::function<bool()>& stop_requested)
    {
        auto [left, left_metadata] = Error::throwing_unwrap(storage::open<PixelType>(options.left), "open left RF merger input");
        auto [right, right_metadata] = Error::throwing_unwrap(storage::open<PixelType>(options.right), "open right RF merger input");
        check_inputs<PixelType>(*left_metadata, *right_metadata);
        reject_inner(left->index(), "left RF merger input");
        reject_inner(right->index(), "right RF merger input");
        const auto& metadata = *left_metadata;
        inputs::Record record;
        record.left = Error::throwing_unwrap(inputs::fingerprint(options.left));
        record.right = Error::throwing_unwrap(inputs::fingerprint(options.right));
        record.priorities = priorities;
        record.payload_type = metadata.payload_type;
        record.nominal_tile_size = metadata.nominal_tile_size;
        record.stored_tile_size = metadata.stored_tile_size;
        record.halo_width = metadata.halo_width;
        record.value_mapping = metadata.value_mapping;
        check_link_filesystem(options.left, options.output);
        check_link_filesystem(options.right, options.output);
        LOG_INFO("RF merge planning: counting output leaves from the input indices");
        const auto total = count_leaves(left->index(), right->index());

        std::unique_ptr<const storage::IndexedStorage<PixelType>> cache;
        statistics::Totals totals;
        bool complete = true;
        if (options.cache) {
            Error::throwing_unwrap(inputs::validate_cache(*options.cache, record));
            check_link_filesystem(*options.cache, options.output);
            auto [input, cache_metadata]
                = Error::throwing_unwrap(storage::open<PixelType>(*options.cache, { .allow_incomplete = true }), "open RF merger cache");
            if (cache_metadata->nominal_tile_size != metadata.nominal_tile_size || cache_metadata->stored_tile_size != metadata.stored_tile_size
                || cache_metadata->halo_width != 0 || cache_metadata->value_mapping != metadata.value_mapping
                || cache_metadata->codec_selector != metadata.codec_selector) {
                Error::raise(Error::Code::InvalidInput, "RF merger cache metadata disagrees with the inputs");
            }
            reject_inner(input->index(), "RF merger cache", Error::Code::CorruptData);
            for (const auto& [key, status] : input->index()) {
                if (status == store::NodeStatus::Leaf && !partition::is_leaf(left->index(), right->index(), key)) {
                    Error::raise(Error::Code::CorruptData, "RF merger cache tile " + to_string(key) + " is not part of the merge output");
                }
            }
            if (auto restored = statistics::read(*options.cache)) {
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
        auto output = std::move(Error::throwing_unwrap(storage::create<PixelType>(options.output, create_options), "create RF merger output").first);
        const auto input_path = output->base_path() / inputs::file_name;
        Error::throwing_unwrap(io::envelope::write_to_path<inputs::Schema>(record, input_path), "write RF merger input record");
        const StatisticsGuard statistics_guard(totals, output->base_path());

        const auto ranks = priorities::ranks(priorities);
        const produce::Inputs<PixelType> sources { { left.get(), left_metadata.get() }, { right.get(), right_metadata.get() }, &ranks };
        const unsigned jobs = unsigned((std::min)(std::uint64_t(options.jobs), (std::max)(total, std::uint64_t(1))));
        // Worker exceptions travel through the pool as errors and are rethrown by the coordinator.
        raster_store::TilePool<produce::Produced<PixelType>> pool(jobs, [&](unsigned, const Key& key) -> Expected<produce::Produced<PixelType>> {
            try {
                return produce::produce(sources, { key, partition::supplier(left->index(), key), partition::supplier(right->index(), key) });
            } catch (const Error::Exception& exception) {
                return std::unexpected(exception.error());
            }
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
        const auto checkpoint = [&] {
            Error::throwing_unwrap(statistics::write(totals, output->base_path()));
            Error::throwing_unwrap(output->save_index(), "checkpoint RF merger index");
            last_checkpoint = std::chrono::steady_clock::now();
        };
        const auto finish = [&](const Key& key, statistics::Category category, bool from_cache) {
            const auto path = Error::asserting_unwrap(output->path_for(key));
            std::error_code error;
            const auto bytes = std::filesystem::file_size(path, error);
            if (error) {
                Error::raise(Error::Code::Io, "measure RF merger payload", path, error);
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
        };
        const auto link = [&](const Key& key, Side side) {
            Error::throwing_unwrap(output->copy_from(key, side == Side::Left ? *left : *right), "hard-link RF merger tile " + to_string(key));
            finish(key, side == Side::Left ? statistics::Category::LinkedLeft : statistics::Category::LinkedRight, false);
        };
        // Cached leaves and single native suppliers need no pixel reads.
        const auto direct = [&](const partition::Leaf& leaf) {
            if (cache && Error::asserting_unwrap(cache->index().get(leaf.key)) == store::NodeStatus::Leaf) {
                Error::throwing_unwrap(output->copy_from(leaf.key, *cache), "hard-link RF merger cache tile " + to_string(leaf.key));
                finish(leaf.key, statistics::Category::Uncategorized, true);
                return true;
            }
            if (leaf.left.has_value() != leaf.right.has_value() && leaf.left.value_or(leaf.right.value_or(Key {})) == leaf.key) {
                link(leaf.key, leaf.left ? Side::Left : Side::Right);
                return true;
            }
            return false;
        };
        const auto consume = [&](typename raster_store::TilePool<produce::Produced<PixelType>>::Completed done) {
            auto produced = Error::throwing_unwrap(std::move(done.result), "produce RF merger tile " + to_string(done.key));
            if (const auto* linked = std::get_if<produce::Link>(&produced)) {
                link(done.key, linked->side);
                return;
            }
            const auto& created = std::get<produce::Created<PixelType>>(produced);
            Error::throwing_unwrap(output->save(done.key, created.tile), "write RF merger tile " + to_string(done.key));
            finish(done.key, created.category, false);
        };

        // Cancellation discards queued work, saves the active tiles and throws.
        const auto poll = [&] {
            if (stop_requested && stop_requested()) {
                LOG_INFO("RF merge cancellation requested: discarding queued work and finishing active tiles");
                pool.stop();
                while (pool.outstanding() != 0) {
                    if (auto done = pool.take(std::chrono::milliseconds(100))) {
                        consume(std::move(*done));
                    }
                }
                checkpoint();
                Error::raise(Error::Code::Cancelled, "RF merge cancelled after " + std::to_string(completed) + " tiles; incomplete snapshot retained");
            }
            if (std::chrono::steady_clock::now() - last_checkpoint >= std::chrono::minutes(2)) {
                checkpoint();
                LOG_INFO("RF merge checkpoint: {} tiles", completed);
            }
            progress(false);
        };

        progress(true);
        partition::Cursor cursor(left->index(), right->index());
        std::optional<partition::Leaf> pending;
        bool exhausted = false;
        for (;;) {
            poll();
            while (!exhausted) {
                if (!pending) {
                    pending = cursor.next();
                    if (!pending) {
                        exhausted = true;
                        break;
                    }
                }
                if (direct(*pending)) {
                    pending.reset();
                    poll();
                    continue;
                }
                if (!pool.submit(pending->key)) {
                    break;
                }
                pending.reset();
            }
            if (exhausted && pool.outstanding() == 0) {
                break;
            }
            if (auto done = pool.take(std::chrono::milliseconds(100))) {
                consume(std::move(*done));
            }
        }
        pool.join();
        progress(true);
        LOG_INFO("RF merge finalizing: publishing {} tiles", completed);
        std::error_code error;
        if (!std::filesystem::remove(input_path, error)) {
            Error::raise(Error::Code::Io, "remove RF merger input record before publication", input_path, error);
        }
        const auto statistics_path = output->base_path() / statistics::file_name;
        std::filesystem::remove(statistics_path, error);
        if (error) {
            Error::raise(Error::Code::Io, "remove RF merger statistics before publication", statistics_path, error);
        }
        Error::throwing_unwrap(storage::publish(std::move(output)), "publish RF merger snapshot");
        return Report { totals, complete, restored, metadata.nominal_tile_size };
    }
} // namespace

Report run(const Options& options, const std::function<bool()>& stop_requested)
{
    ASSERT(options.jobs > 0);
    const auto priorities = Error::throwing_unwrap(priorities::read(options.priorities));
    const auto metadata = Error::throwing_unwrap(raster_store::io::manifest::read_metadata(options.left), "read left RF merger input metadata");
    if (metadata.payload_type == raster_store::pixel::identifier<float>()) {
        return execute<float>(options, priorities, stop_requested);
    }
    if (metadata.payload_type == raster_store::pixel::identifier<glm::u8vec3>()) {
        return execute<glm::u8vec3>(options, priorities, stop_requested);
    }
    Error::raise(Error::Code::Unsupported, "RF merger supports float32 scalar and RGB8 payloads, not " + metadata.payload_type);
}

} // namespace rf_merger::merge
