/*******************************************************************************
 * Copyright 2014 GeoData <geodata@soton.ac.uk>
 * Copyright 2022 Adam Celarek-Litofcenko
 * Copyright (C) 2022 Martin Braunsperger
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.  You may obtain a copy
 * of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *******************************************************************************/

#include "gis/DatasetReader.h"
#include "gis/Dataset.h"
#include "gis/srs.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstddef>
#include <fmt/core.h>
#include <gdal.h>
#include <gdal_alg.h>
#include <gdal_priv.h>
#include <gdalwarper.h>
#include <libassert/assert.hpp>
#include <limits>
#include <memory>
#include <numbers>
#include <ogr_spatialref.h>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include <vrtdataset.h>

#include <radix/raster.h>
#include "cli_support/log.h"

namespace gis {

namespace {
std::string toWkt(const OGRSpatialReference& srs)
{
    char* wkt_char_string = nullptr;
    srs.exportToWkt(&wkt_char_string);
    std::string wkt_string(wkt_char_string);
    CPLFree(wkt_char_string);
    return wkt_string;
}

OGRSpatialReference target_srs(srs::Projection projection)
{
    switch (projection) {
    case srs::Projection::WebMercator:
        return srs::webmercator();
    case srs::Projection::Geographic:
        return srs::wgs84();
    }
    PANIC("unsupported projection", static_cast<unsigned>(projection));
}

using GdalWarpOptionsPtr = std::unique_ptr<GDALWarpOptions, decltype(&GDALDestroyWarpOptions)>;
using ApproxTransformer = std::unique_ptr<void, decltype(&GDALDestroyApproxTransformer)>;

// Maximum approximation error in source pixels.
constexpr double approximation_error = 0.125;

template <typename Pixel>
constexpr GDALDataType source_type = std::is_same_v<Pixel, float> ? GDT_Float32 : GDT_Byte;

static_assert(sizeof(glm::u8vec3) == 3 && alignof(glm::u8vec3) == 1 && std::is_trivially_copyable_v<glm::u8vec3>);
static_assert(offsetof(glm::u8vec3, x) == 0 && offsetof(glm::u8vec3, y) == 1 && offsetof(glm::u8vec3, z) == 2);

// The selected bands, followed by an alpha band exposing their effective
// masks unless all of them are valid everywhere.
struct Source {
    std::unique_ptr<VRTDataset> dataset;
    int alpha_band = 0;
    // Dataset bands whose GDAL data coverage status, where empty, proves a selected band invalid.
    std::vector<GDALRasterBand*> coverage {};
};

Expected<Source> make_source(GDALDataset& dataset, std::span<const unsigned> bands)
{
    Source source { std::make_unique<VRTDataset>(dataset.GetRasterXSize(), dataset.GetRasterYSize()) };
    std::vector<GDALRasterBand*> inputs;
    bool all_valid = true;
    for (const unsigned band : bands) {
        auto* input = dataset.GetRasterBand(int(band));
        if (source.dataset->AddBand(input->GetRasterDataType(), nullptr) != CE_None
            || static_cast<VRTSourcedRasterBand*>(source.dataset->GetRasterBand(source.dataset->GetRasterCount()))->AddSimpleSource(input) != CE_None) {
            return Error::fail(Error::Code::Io, fmt::format("expose source band {}: {}", band, CPLGetLastErrorMsg()));
        }
        auto* mask = input->GetMaskBand();
        if (!mask) {
            return Error::fail(Error::Code::Io, fmt::format("get mask of source band {}: {}", band, CPLGetLastErrorMsg()));
        }
        if (mask->GetRasterDataType() != GDT_Byte) {
            return Error::fail(Error::Code::Unsupported, fmt::format("mask of source band {} is not of type Byte", band));
        }
        const int flags = input->GetMaskFlags();
        all_valid = all_valid && (flags & GMF_ALL_VALID) != 0;
        inputs.push_back(input);
        // Empty areas of a band read as its NoData value, or as zero without one.
        if (flags == GMF_NODATA) {
            source.coverage.push_back(input);
        } else if ((flags & (GMF_ALL_VALID | GMF_NODATA)) == 0) {
            int has_nodata = FALSE;
            const double nodata = mask->GetNoDataValue(&has_nodata);
            if (!has_nodata || nodata == 0) {
                source.coverage.push_back(mask);
            }
        }
    }
    if (all_valid) {
        return source;
    }
    // GDAL returns per-dataset masks as one shared object. Otherwise, the
    // minimum is the AND of binary masks. It must not have NoData, which min skips.
    const bool shared = std::ranges::all_of(inputs, [&](auto* input) { return input->GetMaskBand() == inputs.front()->GetMaskBand(); });
    CPLStringList options;
    if (!shared) {
        options.SetNameValue("subclass", "VRTDerivedRasterBand");
        options.SetNameValue("PixelFunctionType", "min");
    }
    if (source.dataset->AddBand(GDT_Byte, options.List()) != CE_None) {
        return Error::fail(Error::Code::Io, fmt::format("create source validity band: {}", CPLGetLastErrorMsg()));
    }
    source.alpha_band = source.dataset->GetRasterCount();
    auto* alpha = static_cast<VRTSourcedRasterBand*>(source.dataset->GetRasterBand(source.alpha_band));
    for (auto* input : shared ? std::span(inputs).first(1) : std::span(inputs)) {
        if (alpha->AddMaskBandSource(input) != CE_None) {
            return Error::fail(Error::Code::Io, fmt::format("expose source band mask: {}", CPLGetLastErrorMsg()));
        }
    }
    return source;
}

// Exposes the source window GDAL computes for a destination window.
class WindowedWarpOperation : public GDALWarpOperation {
public:
    using GDALWarpOperation::ComputeSourceWindow;
};

// Whether GDAL knows a selected band to be invalid across the source window of the warp, without reading
// pixels: VRT areas without sources and sparse GTiff blocks. The warp reads one window per chunk, computed
// from samples along the chunk edges. Widened by a sampling interval of GDAL's 21 samples per edge and at
// least the Lanczos radius plus rounding, the output window yields a source window covering theirs.
bool known_empty(const Source& source, WindowedWarpOperation& operation, glm::uvec2 size)
{
    if (source.coverage.empty()) {
        return false;
    }
    CPLErrorStateBackuper quiet(CPLQuietErrorHandler);
    const glm::i64vec2 margin = glm::max(glm::i64vec2(4), glm::i64vec2((size + 19u) / 20u));
    const glm::i64vec2 widened = glm::i64vec2(size) + std::int64_t(2) * margin;
    if (widened.x > INT_MAX || widened.y > INT_MAX) {
        return false;
    }
    int x = 0, y = 0, width = 0, height = 0;
    double extra_x = 0, extra_y = 0, fill_ratio = 0;
    if (operation.ComputeSourceWindow(int(-margin.x), int(-margin.y), int(widened.x), int(widened.y), &x, &y, &width, &height, &extra_x, &extra_y, &fill_ratio)
            != CE_None
        || width <= 0 || height <= 0) {
        return false;
    }
    return std::ranges::any_of(source.coverage, [&](GDALRasterBand* band) {
        return band->GetDataCoverageStatus(x, y, width, height, GDAL_DATA_COVERAGE_STATUS_DATA, nullptr) == GDAL_DATA_COVERAGE_STATUS_EMPTY;
    });
}

// Warps all source bands at once into samples, with destination alpha as validity.
template <typename Pixel>
Expected<void> warp(const Source& source, const ApproxTransformer& transformer, typename DatasetReader<Pixel>::Samples& samples)
{
    constexpr int channel_count = int(DatasetReader<Pixel>::channel_count);
    const glm::uvec2 size = samples.data.size();
    auto* driver = GetGDALDriverManager()->GetDriverByName("MEM");
    if (!driver) {
        return Error::fail(Error::Code::Unsupported, "GDAL MEM driver is required");
    }
    // The destination bands only borrow the sample buffers.
    GDALDatasetUniquePtr destination(driver->Create("", int(size.x), int(size.y), 0, GDT_Unknown, nullptr));
    if (!destination) {
        return Error::fail(Error::Code::Io, fmt::format("create warp destination: {}", CPLGetLastErrorMsg()));
    }
    const auto add_band = [&](GDALDataType type, std::byte* data, GSpacing pixel_offset) {
        CPLStringList options;
        options.SetNameValue("DATAPOINTER", fmt::format("{}", static_cast<const void*>(data)).c_str());
        options.SetNameValue("PIXELOFFSET", fmt::format("{}", pixel_offset).c_str());
        options.SetNameValue("LINEOFFSET", fmt::format("{}", pixel_offset * GSpacing(size.x)).c_str());
        return destination->AddBand(type, options.List()) == CE_None;
    };
    auto* data = samples.data.bytes().data();
    for (int channel = 0; channel < channel_count; ++channel) {
        if (!add_band(source_type<Pixel>, data + channel * GDALGetDataTypeSizeBytes(source_type<Pixel>), GSpacing(sizeof(Pixel)))) {
            return Error::fail(Error::Code::Io, fmt::format("attach warp destination data: {}", CPLGetLastErrorMsg()));
        }
    }
    if (!add_band(GDT_Byte, samples.valid.bytes().data(), 1)) {
        return Error::fail(Error::Code::Io, fmt::format("attach warp destination validity: {}", CPLGetLastErrorMsg()));
    }

    auto options = GdalWarpOptionsPtr(GDALCreateWarpOptions(), &GDALDestroyWarpOptions);
    options->hSrcDS = source.dataset.get();
    options->hDstDS = destination.get();
    GDALWarpInitDefaultBandMapping(options.get(), channel_count);
    options->nSrcAlphaBand = source.alpha_band;
    options->nDstAlphaBand = channel_count + 1;
    options->eResampleAlg = GRA_Lanczos;
    options->eWorkingDataType = source_type<Pixel>;
    options->pfnTransformer = GDALApproxTransform;
    options->pTransformerArg = transformer.get();
    // A window without valid source coverage is an empty read rather than a failure.
    options->papszWarpOptions = CSLSetNameValue(options->papszWarpOptions, "INIT_DEST", "0");
    options->papszWarpOptions = CSLSetNameValue(options->papszWarpOptions, "ERROR_OUT_IF_EMPTY_SOURCE_WINDOW", "FALSE");
    // Callers parallelize across readers.
    options->papszWarpOptions = CSLSetNameValue(options->papszWarpOptions, "NUM_THREADS", "1");
    options->papszWarpOptions = CSLSetNameValue(options->papszWarpOptions, "SRC_ALPHA_MAX", "255");
    options->papszWarpOptions = CSLSetNameValue(options->papszWarpOptions, "DST_ALPHA_MAX", "255");
    WindowedWarpOperation operation;
    if (operation.Initialize(options.get()) != CE_None) {
        return Error::fail(Error::Code::Io, fmt::format("warp source bands: {}", CPLGetLastErrorMsg()));
    }
    // The samples are zero, as INIT_DEST leaves them for a warp without valid source pixels.
    if (known_empty(source, operation, size)) {
        return {};
    }
    if (operation.ChunkAndWarpImage(0, 0, int(size.x), int(size.y)) != CE_None) {
        return Error::fail(Error::Code::Io, fmt::format("warp source bands: {}", CPLGetLastErrorMsg()));
    }
    return {};
}
}

template <typename Pixel>
void DatasetReader<Pixel>::TransformerDeleter::operator()(Transformer* transformer) const
{
    GDALDestroyGenImgProjTransformer(transformer);
}

template <typename Pixel>
DatasetReader<Pixel>::DatasetReader(std::shared_ptr<Dataset> dataset,
    srs::Projection projection,
    std::array<unsigned, channel_count> bands,
    Pixel default_pixel,
    std::unique_ptr<Transformer, TransformerDeleter> transformer)
    : m_dataset(std::move(dataset))
    , m_projection(projection)
    , m_bands(bands)
    , m_default_pixel(default_pixel)
    , m_transformer(std::move(transformer))
{
}

template <typename Pixel>
Expected<DatasetReader<Pixel>> DatasetReader<Pixel>::make(
    std::shared_ptr<Dataset> dataset, const srs::Projection projection, const std::array<unsigned, channel_count> bands, const Pixel default_pixel)
{
    ASSERT(dataset);
    if constexpr (std::is_same_v<Pixel, float>) {
        if (!std::isfinite(default_pixel)) {
            return Error::fail(Error::Code::InvalidInput, "default pixel must be finite");
        }
    }
    auto& gdal_dataset = *dataset->gdalDataset();
    for (const unsigned band : bands) {
        if (band == 0 || band > unsigned(gdal_dataset.GetRasterCount())) {
            return Error::fail(Error::Code::InvalidInput,
                fmt::format("dataset {} does not contain band number {} (there are {} bands)", dataset->name(), band, gdal_dataset.GetRasterCount()));
        }
        const auto type = gdal_dataset.GetRasterBand(int(band))->GetRasterDataType();
        if (type != source_type<Pixel>) {
            return Error::fail(Error::Code::Unsupported,
                fmt::format("band {} of dataset {} has type {}, but the reader requires {}",
                    band,
                    dataset->name(),
                    GDALGetDataTypeName(type),
                    GDALGetDataTypeName(source_type<Pixel>)));
        }
    }

    auto source = dataset->srs();
    if (!source) {
        return Error::propagate(std::move(source), "read SRS of dataset " + dataset->name());
    }
    const auto target = target_srs(projection);
    const bool requires_reprojection = !source->IsSame(&target);
    if (source->IsGeographic() && std::abs(source->GetAngularUnits() - std::numbers::pi / 180) < 1e-12) {
        // GDAL wraps longitudes around the source centre only for sources up to about 360° wide.
        // Wider ones need it too, e.g. -0.5..360.5, as reprojected longitudes lie in -180..180.
        if (const auto bounds = dataset->bounds()) {
            source->SetExtension("GEOGCS", "CENTER_LONG", fmt::format("{}", (bounds->min.x + bounds->max.x) / 2).c_str());
        }
    }
    CPLStringList transformer_options;
    if (requires_reprojection) {
        transformer_options.SetNameValue("SRC_SRS", toWkt(*source).c_str());
        transformer_options.SetNameValue("DST_SRS", toWkt(target).c_str());
    }
    // Creates both transformation directions, so reads rebuild no PROJ state.
    std::unique_ptr<Transformer, TransformerDeleter> transformer(
        static_cast<Transformer*>(GDALCreateGenImgProjTransformer2(&gdal_dataset, nullptr, transformer_options.List())));
    if (!transformer) {
        return Error::fail(Error::Code::Unsupported, fmt::format("create transformer for dataset {}: {}", dataset->name(), CPLGetLastErrorMsg()));
    }
    return DatasetReader(std::move(dataset), projection, bands, default_pixel, std::move(transformer));
}

template <typename Pixel>
Expected<typename DatasetReader<Pixel>::Samples> DatasetReader<Pixel>::read(const radix::tile::SrsBounds& bounds, const glm::uvec2 size)
{
    if (size.x == 0 || size.y == 0 || size.x > unsigned(INT_MAX) || size.y > unsigned(INT_MAX)) {
        return Error::fail(Error::Code::InvalidInput, fmt::format("read size {}x{} is empty or exceeds GDAL dimensions", size.x, size.y));
    }
    const std::uint64_t count = std::uint64_t(size.x) * size.y;
    if (count > std::vector<Pixel>().max_size() || count > std::vector<std::uint8_t>().max_size()) {
        return Error::fail(Error::Code::ResourceExhausted, fmt::format("read size {}x{} exceeds raster capacity", size.x, size.y));
    }
    const glm::dvec2 pixel_size = (bounds.max - bounds.min) / glm::dvec2(size);
    const auto finite = [](glm::dvec2 value) { return std::isfinite(value.x) && std::isfinite(value.y); };
    if (!finite(bounds.min) || !finite(bounds.max) || !finite(pixel_size) || !(pixel_size.x > 0) || !(pixel_size.y > 0)) {
        return Error::fail(Error::Code::InvalidInput, "read bounds must be finite and nonempty");
    }

    CPLErrorReset();
    const std::array<double, 6> geo_transform { bounds.min.x, pixel_size.x, 0, bounds.max.y, 0, -pixel_size.y };
    GDALSetGenImgProjTransformerDstGeoTransform(m_transformer.get(), geo_transform.data());
    ApproxTransformer transformer(
        GDALCreateApproxTransformer(GDALGenImgProjTransform, m_transformer.get(), approximation_error), &GDALDestroyApproxTransformer);
    if (!transformer) {
        return Error::fail(Error::Code::Internal, fmt::format("create approximate transformer: {}", CPLGetLastErrorMsg()));
    }
    auto source = make_source(*m_dataset->gdalDataset(), m_bands);
    if (!source) {
        return Error::propagate(std::move(source), "prepare dataset " + m_dataset->name());
    }
    Samples result { radix::Raster<Pixel>(size), radix::Raster<std::uint8_t>(size) };
    if (auto warped = warp<Pixel>(*source, transformer, result); !warped) {
        return Error::propagate(std::move(warped), "read dataset " + m_dataset->name());
    }

    // Neither destination NoData nor INIT_DEST can replace this pass: GDAL
    // alters valid pixels equal to NoData and writes pixels with zero alpha.
    auto data = result.data.buffer();
    auto valid = result.valid.buffer();
    for (std::size_t i = 0; i < data.size(); ++i) {
        bool is_valid = valid[i] != 0;
        if constexpr (std::is_same_v<Pixel, float>) {
            is_valid = is_valid && std::isfinite(data[i]);
        }
        if (!is_valid) {
            data[i] = m_default_pixel;
            valid[i] = 0;
        }
    }
    return result;
}

template <typename Pixel>
Expected<glm::dvec2> DatasetReader<Pixel>::min_pixel_size(const radix::tile::SrsBounds& bounds) const
{
    constexpr unsigned samples_per_axis = 5;
    constexpr double step_fraction = 1e-3;

    auto coverage = m_projection == srs::Projection::WebMercator ? m_dataset->mercator_coverage() : m_dataset->geographic_coverage();
    if (!coverage) {
        return Error::propagate(std::move(coverage), "compute dataset coverage");
    }
    // Each sample is a point followed by its x and y neighbours. Steps point inwards,
    // so neighbours stay within the coverage and never cross the antimeridian.
    std::vector<glm::dvec2> points;
    std::vector<glm::dvec2> steps;
    for (const auto& box : *coverage) {
        const radix::tile::SrsBounds region { glm::max(bounds.min, box.min), glm::min(bounds.max, box.max) };
        if (region.min.x >= region.max.x || region.min.y >= region.max.y) {
            continue;
        }
        const glm::dvec2 centre = (region.min + region.max) / 2.0;
        const glm::dvec2 step = step_fraction * (region.max - region.min);
        for (unsigned y = 0; y < samples_per_axis; ++y) {
            for (unsigned x = 0; x < samples_per_axis; ++x) {
                const glm::dvec2 point = glm::mix(region.min, region.max, glm::dvec2(x, y) / double(samples_per_axis - 1));
                const glm::dvec2 inward_step = glm::mix(step, -step, glm::greaterThan(point, centre));
                points.insert(points.end(), { point, point + glm::dvec2(inward_step.x, 0), point + glm::dvec2(0, inward_step.y) });
                steps.push_back(inward_step);
            }
        }
    }
    if (steps.empty()) {
        return Error::fail(Error::Code::InvalidInput, "bounds do not intersect the coverage of dataset " + m_dataset->name());
    }

    auto source_srs = m_dataset->srs();
    if (!source_srs) {
        return Error::propagate(std::move(source_srs));
    }
    auto transform = srs::transformation(target_srs(m_projection), *source_srs);
    if (!transform) {
        return Error::propagate(std::move(transform));
    }
    if (auto transformed = srs::transform_points_inplace(transform->get(), points); !transformed) {
        return Error::propagate(std::move(transformed), "transform pixel size samples into dataset SRS");
    }

    // The coverage already required a north-up geotransform without rotation.
    std::array<double, 6> geo_transform {};
    const auto geo_transform_result = m_dataset->gdalDataset()->GetGeoTransform(geo_transform.data());
    ASSERT(geo_transform_result == CE_None);
    const glm::dvec2 source_pixel_size(geo_transform[1], geo_transform[5]);
    glm::dvec2 result(std::numeric_limits<double>::infinity());
    for (std::size_t i = 0; i < steps.size(); ++i) {
        // Source pixels traversed per target unit along target x and y.
        const glm::dvec2 along_x = glm::abs((points[3 * i + 1] - points[3 * i]) / (steps[i].x * source_pixel_size));
        const glm::dvec2 along_y = glm::abs((points[3 * i + 2] - points[3 * i]) / (steps[i].y * source_pixel_size));
        result = glm::min(result, 1.0 / glm::dvec2(along_x.x + along_x.y, along_y.x + along_y.y));
    }
    return result;
}

template class DatasetReader<float>;
template class DatasetReader<glm::u8vec3>;

} // namespace gis
