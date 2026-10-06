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

#include "Dataset.h"
#include "DatasetReader.h"
#include "Mask.h"
#include "TileWorker.h"
#include "inputs.h"
#include "planning.h"
#include "raster_store/storage.h"
#include <libassert/assert.hpp>
#include <limits>

namespace rf_builder::gdal {
namespace {
std::vector<unsigned> selected_bands(const Options& options, GDALDataset& dataset)
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
                            Error::raise(Error::Code::InvalidInput, "ambiguous RGB colour interpretation; select source bands explicitly");
                        }
                        selected = unsigned(band);
                    }
                }
                if (selected == 0) {
                    Error::raise(Error::Code::InvalidInput, "missing RGB colour interpretation; select source bands explicitly");
                }
                bands.push_back(selected);
            }
        }
    }
    if (bands.size() != (options.mode == Mode::Scalar ? 1u : 3u)) {
        Error::raise(Error::Code::InvalidInput, "select one scalar band or three RGB bands");
    }
    for (const unsigned band : bands) {
        if (band == 0 || band > unsigned(dataset.GetRasterCount())) {
            Error::raise(Error::Code::InvalidInput, "RF source band is out of range");
        }
        if (GDALDataTypeIsComplex(dataset.GetRasterBand(int(band))->GetRasterDataType())) {
            Error::raise(Error::Code::Unsupported, "RF import does not support complex source bands");
        }
    }
    return bands;
}

template <typename PixelType>
Report produce(
    const Options& options, const RasterTransform& transform, const Mask& mask, const inputs::Record& record, const std::function<bool()>& stop_requested)
{
    std::vector<TileWorker<PixelType>> workers;
    const auto source_pixel = [&](glm::dvec2 point) { return transform.source_pixel(point, false); };
    const auto halo = Error::asserting_unwrap(nodata::halo(options.tile_side, record.nodata_search_radius, record.nodata_smoothing_kernel_size));
    planning::Cursor cursor(options.tile_side, transform.bounds(), mask.bounds(), source_pixel, halo);
    run::Source<PixelType> source;
    source.attribution = record.attribution;
    source.validate_cache = [&](const auto& path) { return inputs::validate_cache(path, record); };
    source.write_inputs = [&](const auto& path) { return io::envelope::write_to_path<inputs::Schema>(record, path); };
    source.total = [&](const run::Poll& poll) {
        LOG_INFO("RF planning: counting candidate tiles");
        std::uint64_t total = 0;
        planning::traverse(options.tile_side, transform.bounds(), mask.bounds(), source_pixel, [&](const auto&) { ++total; }, poll, halo);
        return double(total);
    };
    source.next = [&](const run::Poll& poll) { return cursor.next(poll); };
    source.initialize = [&](unsigned jobs, const run::Poll& poll) {
        workers.reserve(jobs);
        for (unsigned i = 0; i < jobs; ++i) {
            poll();
            workers.push_back(TileWorker<PixelType>::open(record));
        }
    };
    source.prepare = [&](unsigned worker, const auto& key) {
        auto prepared = workers[worker].prepare(key);
        if (!prepared) {
            return run::Prepared<PixelType>(std::monostate());
        }
        return run::Prepared<PixelType>(std::move(*prepared));
    };
    source.weight = [](const auto&) { return 1.; };
    return run::execute<PixelType>({ options.output, options.tile_side, options.jobs, options.cache, options.attribution_index, options.value_mapping },
        std::move(source),
        stop_requested);
}
}

Report build(const Options& options, const std::function<bool()>& stop_requested)
{
    const run::Options output { options.output, options.tile_side, options.jobs, options.cache, options.attribution_index, options.value_mapping };
    run::validate_options(output);
    Error::throwing_unwrap(nodata::halo(options.tile_side, options.nodata_search_radius, options.nodata_smoothing_kernel_size));
    if (options.nodata_default_value.size() != 1 && !(options.mode == Mode::Colour && options.nodata_default_value.size() == 3)) {
        Error::raise(Error::Code::InvalidInput, "NoData fallback requires one scalar value or one/three RGB values");
    }
    std::array<float, 3> fallback {};
    for (unsigned channel = 0; channel < 3; ++channel) {
        const double value = options.nodata_default_value[options.nodata_default_value.size() == 1 ? 0 : channel];
        if (!std::isfinite(value) || std::abs(value) > (std::numeric_limits<float>::max)()
            || (options.mode == Mode::Colour && (value < 0 || value > 255 || value != std::round(value)))) {
            Error::raise(Error::Code::InvalidInput, "NoData fallback must be finite float data or RGB integers in 0..255");
        }
        fallback[channel] = float(value);
    }
    const auto dataset_identifier = Error::throwing_unwrap(inputs::identifier(options.dataset));
    const auto mask_identifier = Error::throwing_unwrap(inputs::identifier(options.mask));
    auto dataset = Error::throwing_unwrap(Dataset::open_raster(inputs::gdal_identifier(dataset_identifier)), "open RF source dataset");
    const auto transform = Error::throwing_unwrap(RasterTransform::create(*dataset.gdalDataset()));
    auto bands = selected_bands(options, *dataset.gdalDataset());
    const auto mask = Error::throwing_unwrap(Mask::open(inputs::gdal_identifier(mask_identifier)));
    const auto entry = Error::throwing_unwrap(run::attribution(output));
    inputs::Record record { dataset_identifier, mask_identifier, std::move(bands), options.mode, options.attribution_index, options.tile_side, entry };
    record.nodata_search_radius = options.nodata_search_radius;
    record.nodata_smoothing_kernel_size = options.nodata_smoothing_kernel_size;
    record.nodata_default_value = fallback;
    record.value_mapping = options.value_mapping.value_or(
        options.mode == Mode::Colour ? raster_store::pixel::default_mapping<glm::u8vec3> : raster_store::pixel::default_mapping<float>);
    if (options.mode == Mode::Scalar) {
        return produce<float>(options, transform, mask, record, stop_requested);
    }
    return produce<glm::u8vec3>(options, transform, mask, record, stop_requested);
}

} // namespace rf_builder::gdal
