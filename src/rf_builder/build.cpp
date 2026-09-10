#include "build.h"

#include <chrono>
#include <cmath>
#include <fmt/chrono.h>
#include <limits>
#include "DatasetReader.h"
#include "Dataset.h"
#include "Mask.h"
#include "inputs.h"
#include "planning.h"
#include "raster_store/storage.h"

namespace rf_builder {
namespace {
Expected<std::vector<unsigned>> selected_bands(const Options& options, GDALDataset& dataset)
{
    auto bands = options.bands;
    if (bands.empty()) {
        if (options.mode == Mode::Scalar) {
            bands = { 1 };
        } else {
            for (const auto interpretation : { GCI_RedBand, GCI_GreenBand, GCI_BlueBand }) {
                unsigned selected = 0;
                for (int band = 1; band <= dataset.GetRasterCount(); ++band) {
                    if (dataset.GetRasterBand(band)->GetColorInterpretation() == interpretation) {
                        if (selected != 0) {
                            return Error::fail(Error::Code::InvalidInput, "ambiguous RGB colour interpretation; select source bands explicitly");
                        }
                        selected = unsigned(band);
                    }
                }
                if (selected == 0) {
                    return Error::fail(Error::Code::InvalidInput, "missing RGB colour interpretation; select source bands explicitly");
                }
                bands.push_back(selected);
            }
        }
    }
    if (bands.size() != (options.mode == Mode::Scalar ? 1u : 3u)) {
        return Error::fail(Error::Code::InvalidInput, "select one scalar band or three RGB bands");
    }
    for (const unsigned band : bands) {
        if (band == 0 || band > unsigned(dataset.GetRasterCount())) {
            return Error::fail(Error::Code::InvalidInput, "RF source band is out of range");
        }
        if (GDALDataTypeIsComplex(dataset.GetRasterBand(int(band))->GetRasterDataType())) {
            return Error::fail(Error::Code::Unsupported, "RF import does not support complex source bands");
        }
    }
    return bands;
}

template <typename PixelType>
Expected<Report> produce(const Options& options, GDALDataset& dataset, const RasterTransform& transform,
    const Mask& mask, const inputs::Record& record)
{
    std::optional<raster_store::storage::IndexedStorage<PixelType>> cache;
    if (options.cache) {
        if (auto valid = inputs::validate_cache(*options.cache, record); !valid) {
            return Error::propagate(std::move(valid));
        }
        auto metadata = raster_store::io::manifest::read_metadata(*options.cache);
        if (!metadata) { return Error::propagate(std::move(metadata)); }
        if (metadata->width != options.tile_side || metadata->height != options.tile_side || metadata->codec_selector != "amort") {
            return Error::fail(Error::Code::InvalidInput, "RF cache metadata disagrees with requested dimensions or codec");
        }
        auto opened = raster_store::storage::open<PixelType>(*options.cache, { .allow_incomplete = true });
        if (!opened) { return Error::propagate(std::move(opened), "open requested RF cache"); }
        auto table = raster_store::attribution::read_table(*options.cache / raster_store::io::manifest::index_file_name);
        if (!table) { return Error::propagate(std::move(table)); }
        auto entry = table->at(options.attribution_index);
        if (!entry) { return Error::propagate(std::move(entry)); }
        if (**entry != record.attribution) {
            return Error::fail(Error::Code::InvalidInput, "RF cache attribution entry disagrees with the output table");
        }
        if (auto compatible = inputs::check_link_filesystem(*options.cache, options.output); !compatible) {
            return Error::propagate(std::move(compatible));
        }
        cache.emplace(std::move(*opened));
    }
    raster_store::storage::CreateOptions create_options;
    create_options.tile_dimensions = glm::uvec2(options.tile_side);
    auto output = raster_store::storage::create<PixelType>(options.output, create_options);
    if (!output) { return Error::propagate(std::move(output)); }
    const auto input_path = output->base_path() / inputs::file_name;
    if (auto written = io::envelope::write_to_path<inputs::Schema>(record, input_path); !written) {
        return Error::propagate(std::move(written), "record RF build inputs before tile production");
    }
    Report report { .tile_side = options.tile_side };
    auto last_checkpoint = std::chrono::steady_clock::now();
    // One owner performs writes, index mutations and checkpoints synchronously.
    // No partially written or linked payload can enter a checkpoint.
    const auto checkpoint = [&]() -> Expected<void> {
        if (std::chrono::steady_clock::now() - last_checkpoint < std::chrono::minutes(2)) { return {}; }
        if (auto saved = output->save_index(); !saved) { return saved; }
        last_checkpoint = std::chrono::steady_clock::now();
        LOG_INFO("RF checkpoint: {} tiles, {} bytes", report.tile_count, report.tile_bytes);
        return {};
    };
    LOG_INFO("RF planning: counting candidate tiles");
    std::uint64_t total = 0;
    const auto source_pixel = [&](glm::dvec2 point) { return transform.source_pixel(point); };
    auto counted = planning::traverse(options.tile_side, transform.bounds(), mask.bounds(), source_pixel,
        [&](const radix::tile::Id&) -> Expected<void> { ++total; return {}; }, checkpoint);
    if (!counted) { return Error::propagate(std::move(counted), "count RF candidate tiles"); }
    if (total == 0) {
        LOG_INFO("RF progress: 0/0 candidate tiles (100.0%); remaining 0h 00m 00s");
    } else {
        LOG_INFO("RF progress: 0/{} candidate tiles (0.0%); remaining unknown until first tile completes", total);
    }
    const auto started = std::chrono::steady_clock::now();
    auto last_progress = started;
    std::uint64_t completed = 0;
    const auto progress = [&]() -> Expected<void> {
        ++completed;
        const auto now = std::chrono::steady_clock::now();
        if (completed == 1 || completed == total || now - last_progress >= std::chrono::seconds(10)) {
            const auto elapsed = std::chrono::duration<double>(now - started).count();
            const auto remaining = std::chrono::seconds(std::int64_t(std::ceil(elapsed * double(total - completed) / double(completed))));
            const auto finish = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now() + remaining);
            LOG_INFO("RF progress: {}/{} candidate tiles ({:.1f}%); remaining {}h {:02}m {:02}s; expected finish {:%Y-%m-%d %H:%M:%S} UTC; {} written, {} reused",
                completed, total, 100.0 * double(completed) / double(total), remaining.count() / 3600,
                remaining.count() / 60 % 60, remaining.count() % 60, finish, report.tile_count, report.reused_tiles);
            last_progress = now;
        }
        return checkpoint();
    };
    const auto visit = [&](const radix::tile::Id& key) -> Expected<void> {
        bool reuse = false;
        if (cache) {
            auto present = cache->has(key);
            if (!present) { return Error::propagate(std::move(present)); }
            reuse = *present;
        }
        if (reuse) {
            if (auto linked = output->copy_from(key, *cache); !linked) { return linked; }
            ++report.reused_tiles;
        } else {
            const auto bounds = RasterTransform::tile_bounds(key);
            auto samples = [&]() -> Expected<DatasetReader::Samples<PixelType>> {
                if constexpr (std::is_same_v<PixelType, float>) {
                    return DatasetReader::read_scalar(dataset, transform, bounds, options.tile_side, record.bands[0]);
                } else {
                    return DatasetReader::read_colour(dataset, transform, bounds, options.tile_side,
                        { record.bands[0], record.bands[1], record.bands[2] });
                }
            }();
            if (!samples) { return Error::propagate(std::move(samples), "read RF tile " + to_string(key)); }
            const double spacing = bounds.width() / options.tile_side;
            std::vector<glm::dvec2> centres(options.tile_side);
            for (unsigned row = 0; row < options.tile_side; ++row) {
                for (unsigned column = 0; column < options.tile_side; ++column) {
                    centres[column] = { bounds.min.x + (column + 0.5) * spacing, bounds.max.y - (row + 0.5) * spacing };
                }
                auto validity = std::span(samples->valid.buffer()).subspan(std::size_t(row) * options.tile_side, options.tile_side);
                if (auto selected = mask.select(centres, validity); !selected) {
                    return Error::propagate(std::move(selected));
                }
            }
            if (std::ranges::none_of(samples->valid.buffer(), [](auto valid) { return valid != 0; })) { return progress(); }
            raster_store::Tile<PixelType> tile(options.tile_side);
            tile.data = std::move(samples->data);
            for (std::size_t i = 0; i < samples->valid.buffer().size(); ++i) {
                if (samples->valid.buffer()[i]) {
                    tile.source_attribution.buffer()[i] = std::uint16_t(options.attribution_index);
                }
            }
            if (auto saved = output->save(key, tile); !saved) { return saved; }
        }
        auto path = output->path_for(key);
        if (!path) { return Error::propagate(std::move(path)); }
        std::error_code error;
        const auto bytes = std::filesystem::file_size(*path, error);
        if (error) { return Error::fail(Error::Code::Io, "measure RF tile payload", *path, error); }
        report.tile_bytes += bytes;
        ++report.tile_count;
        return progress();
    };
    auto produced = planning::traverse(options.tile_side, transform.bounds(), mask.bounds(),
        source_pixel, visit, checkpoint);
    if (!produced) { return Error::propagate(std::move(produced), "produce RF snapshot"); }
    LOG_INFO("RF finalizing: publishing {} tiles", report.tile_count);
    std::error_code error;
    if (!std::filesystem::remove(input_path, error)) {
        return Error::fail(Error::Code::Io, "remove RF input record before publication", input_path, error);
    }
    if (auto published = raster_store::storage::publish(std::move(*output)); !published) {
        return Error::propagate(std::move(published));
    }
    return report;
}
}

Expected<Report> build(const Options& options)
{
    if (options.tile_side == 0 || options.tile_side > unsigned((std::numeric_limits<int>::max)())
        || options.attribution_index == 0 || options.attribution_index >= raster_store::attribution::index_limit) {
        return Error::fail(Error::Code::InvalidInput, "RF import requires positive tile dimensions and an attribution index in 1..65534");
    }
    auto dataset_identifier = inputs::identifier(options.dataset);
    auto mask_identifier = inputs::identifier(options.mask);
    if (!dataset_identifier) { return Error::propagate(std::move(dataset_identifier)); }
    if (!mask_identifier) { return Error::propagate(std::move(mask_identifier)); }
    auto dataset = Dataset::open_raster(inputs::gdal_identifier(*dataset_identifier));
    if (!dataset) { return Error::fail(Error::Code::InvalidInput, "open RF source dataset", options.dataset); }
    auto transform = RasterTransform::create(*dataset->gdalDataset());
    if (!transform) { return Error::propagate(std::move(transform)); }
    auto bands = selected_bands(options, *dataset->gdalDataset());
    if (!bands) { return Error::propagate(std::move(bands)); }
    auto mask = Mask::open(inputs::gdal_identifier(*mask_identifier));
    if (!mask) { return Error::propagate(std::move(mask)); }
    auto partial_path = options.output;
    partial_path += ".part";
    auto table = raster_store::attribution::read_table(partial_path / raster_store::io::manifest::index_file_name);
    if (!table) { return Error::propagate(std::move(table)); }
    auto entry = table->at(options.attribution_index);
    if (!entry) { return Error::propagate(std::move(entry)); }
    const inputs::Record record { *dataset_identifier, *mask_identifier, *bands,
        options.mode, options.attribution_index, options.tile_side, **entry };
    if (options.mode == Mode::Scalar) {
        return produce<float>(options, *dataset->gdalDataset(), *transform, *mask, record);
    }
    return produce<glm::u8vec3>(options, *dataset->gdalDataset(), *transform, *mask, record);
}

} // namespace rf_builder
