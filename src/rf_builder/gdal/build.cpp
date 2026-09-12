#include "build.h"

#include "Dataset.h"
#include "DatasetReader.h"
#include "Mask.h"
#include "TileWorker.h"
#include "inputs.h"
#include "planning.h"
#include "raster_store/storage.h"
#include <limits>

namespace rf_builder::gdal {
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
Expected<Report> produce(
    const Options& options, const RasterTransform& transform, const Mask& mask, const inputs::Record& record, const std::function<bool()>& stop_requested)
{
    std::vector<TileWorker<PixelType>> workers;
    const auto source_pixel = [&](glm::dvec2 point) { return transform.source_pixel(point); };
    planning::Cursor cursor(options.tile_side, transform.bounds(), mask.bounds(), source_pixel);
    run::Source<PixelType> source;
    source.attribution = record.attribution;
    source.validate_cache = [&](const auto& path) { return inputs::validate_cache(path, record); };
    source.write_inputs = [&](const auto& path) { return io::envelope::write_to_path<inputs::Schema>(record, path); };
    source.total = [&](const run::Poll& poll) -> Expected<double> {
        LOG_INFO("RF planning: counting candidate tiles");
        std::uint64_t total = 0;
        auto counted = planning::traverse(
            options.tile_side,
            transform.bounds(),
            mask.bounds(),
            source_pixel,
            [&](const auto&) -> Expected<void> {
                ++total;
                return {};
            },
            poll);
        if (!counted) {
            return Error::propagate(std::move(counted));
        }
        return double(total);
    };
    source.next = [&](const run::Poll& poll) { return cursor.next(poll); };
    source.initialize = [&](unsigned jobs, const run::Poll& poll) -> Expected<void> {
        workers.reserve(jobs);
        for (unsigned i = 0; i < jobs; ++i) {
            if (auto checked = poll(); !checked) {
                return checked;
            }
            auto worker = TileWorker<PixelType>::open(record);
            if (!worker) {
                return Error::propagate(std::move(worker));
            }
            workers.push_back(std::move(*worker));
        }
        return {};
    };
    source.prepare = [&](unsigned worker, const auto& key) -> Expected<run::Prepared<PixelType>> {
        auto prepared = workers[worker].prepare(key);
        if (!prepared) {
            return Error::propagate(std::move(prepared));
        }
        if (!*prepared) {
            return run::Prepared<PixelType>(std::monostate());
        }
        return run::Prepared<PixelType>(std::move(**prepared));
    };
    source.weight = [](const auto&) { return 1.; };
    return run::execute<PixelType>(
        { options.output, options.tile_side, options.jobs, options.cache, options.attribution_index }, std::move(source), stop_requested);
}
}

Expected<Report> build(const Options& options, const std::function<bool()>& stop_requested)
{
    if (options.jobs == 0) {
        return Error::fail(Error::Code::InvalidInput, "RF worker count must be positive");
    }
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
        return produce<float>(options, *transform, *mask, record, stop_requested);
    }
    return produce<glm::u8vec3>(options, *transform, *mask, record, stop_requested);
}

} // namespace rf_builder::gdal
