/*******************************************************************************
 * Copyright 2014 GeoData <geodata@soton.ac.uk>
 * Copyright 2022 Adam Celarek <lastname at cg tuwien ac at>
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

#include "DatasetReader.h"
#include "Dataset.h"

#include <cmath>
#include <fmt/core.h>
#include <gdal.h>
#include <gdal_priv.h>
#include <gdalwarper.h>
#include <vrtdataset.h>
#include <limits>
#include <numbers>
#include <memory>
#include <ogr_spatialref.h>
#include <utility>

#include "Dataset.h"
#include <stdexcept>
#include <radix/raster.h>
#include "ctb/types.hpp"
#include "log.h"

namespace {
std::string toWkt(const OGRSpatialReference& srs)
{
    char* wkt_char_string = nullptr;
    srs.exportToWkt(&wkt_char_string);
    std::string wkt_string(wkt_char_string);
    CPLFree(wkt_char_string);
    return wkt_string;
}

std::array<double, 6> computeGeoTransform(const radix::tile::SrsBounds& bounds, unsigned width, unsigned height)
{
    return { bounds.min.x, bounds.width() / width, 0,
        bounds.max.y, 0, -bounds.height() / height };
}

using GdalImageTransformArgsPtr = std::unique_ptr<void, decltype(&GDALDestroyGenImgProjTransformer)>;
GdalImageTransformArgsPtr make_image_transform_args(const DatasetReader& reader,
    Dataset* dataset,
    const radix::tile::SrsBounds& bounds, unsigned width, unsigned height)
{
    CPLStringList transformOptions;
    if (reader.isReprojecting()) {
        transformOptions.SetNameValue("SRC_SRS", reader.dataset_srs_wkt().c_str());
        transformOptions.SetNameValue("DST_SRS", reader.target_srs_wkt().c_str());
    }
    auto args = GdalImageTransformArgsPtr(GDALCreateGenImgProjTransformer2(dataset->gdalDataset(), nullptr, transformOptions.List()), &GDALDestroyGenImgProjTransformer);
    if (!args) {
        throw std::runtime_error("GDALCreateGenImgProjTransformer2 failed.");
    }

    const auto adfGeoTransform = computeGeoTransform(bounds, width, height);
    GDALSetGenImgProjTransformerDstGeoTransform(args.get(), adfGeoTransform.data());

    return args;
}

using GdalWarpOptionsPtr = std::unique_ptr<GDALWarpOptions, decltype(&GDALDestroyWarpOptions)>;
using WarpOptionData = std::pair<GdalWarpOptionsPtr, GdalImageTransformArgsPtr>;

WarpOptionData makeWarpOptions(const DatasetReader& reader, Dataset* dataset, const radix::tile::SrsBounds& bounds, unsigned width, unsigned height)
{
    auto options = GdalWarpOptionsPtr(GDALCreateWarpOptions(), &GDALDestroyWarpOptions);
    options->hSrcDS = dataset->gdalDataset();
    options->nBandCount = 1;
    options->eResampleAlg = GDALResampleAlg::GRA_Cubic;
    options->panSrcBands = static_cast<int*>(CPLMalloc(sizeof(int) * 1));
    options->panDstBands = static_cast<int*>(CPLMalloc(sizeof(int) * 1));
    options->padfSrcNoDataReal = static_cast<double*>(CPLMalloc(sizeof(double) * 1));
    options->padfSrcNoDataImag = static_cast<double*>(CPLMalloc(sizeof(double) * 1));
    options->padfDstNoDataReal = static_cast<double*>(CPLMalloc(sizeof(double) * 1));
    options->padfDstNoDataImag = static_cast<double*>(CPLMalloc(sizeof(double) * 1));
    {
        int bGotNoData = false;
        double noDataValue = dataset->gdalDataset()->GetRasterBand(1)->GetNoDataValue(&bGotNoData);
        if (!bGotNoData)
            noDataValue = -32768;

        options->padfSrcNoDataReal[0] = noDataValue;
        options->padfSrcNoDataImag[0] = 0;
        options->padfDstNoDataReal[0] = noDataValue;
        options->padfDstNoDataImag[0] = 0;

        options->panSrcBands[0] = int(reader.dataset_band());
        options->panDstBands[0] = 1;
    }
    constexpr auto use_approximation = true;
    if (use_approximation) {
        const auto error_threshold = 0.5;
        auto image_transform_args = make_image_transform_args(reader, dataset, bounds, width, height);
        options->pTransformerArg = GDALCreateApproxTransformer(GDALGenImgProjTransform, image_transform_args.get(), error_threshold);
        options->pfnTransformer = GDALApproxTransform;
        return { std::move(options), std::move(image_transform_args) };
    }

    options->pTransformerArg = make_image_transform_args(reader, dataset, bounds, width, height).release();
    options->pfnTransformer = GDALGenImgProjTransform;

    return { std::move(options), GdalImageTransformArgsPtr(nullptr, &GDALDestroyGenImgProjTransformer) };
}

#ifdef ALP_ENABLE_OVERVIEW_READING
std::shared_ptr<Dataset> getOverviewDataset(const std::shared_ptr<Dataset>& dataset, void* hTransformerArg, bool warn_on_missing_overviews)
{
    GDALDataset* poSrcDS = dataset->gdalDataset();
    int nOvLevel = -2;
    int nOvCount = poSrcDS->GetRasterBand(1)->GetOverviewCount();

    assert(nOvCount >= 0);
    if (nOvCount == 0) {
        if (warn_on_missing_overviews)
            TNTN_LOG_WARN("No dataset overviews found.");
        return dataset;
    }

    std::array<double, 6> adfSuggestedGeoTransform;
    std::array<double, 4> adfExtent;
    int nPixels;
    int nLines;
    /* Compute what the "natural" output resolution (in pixels) would be for this */
    /* input dataset */
    if (GDALSuggestedWarpOutput2(poSrcDS, GDALGenImgProjTransform, hTransformerArg,
            adfSuggestedGeoTransform.data(), &nPixels, &nLines,
            adfExtent.data(), 0)
        == CE_Failure) {
        TNTN_LOG_WARN("GDALSuggestedWarpOutput2 failed. We won't use dataset overviews!");
        return dataset;
    }

    double dfTargetRatio = 1.0 / adfSuggestedGeoTransform[1];
    //  if( dfTargetRatio <= 1.0 ) {
    //    TNTN_LOG_WARN(fmt::format("dfTargetRatio {} <= 1.0. We won't use dataset overviews!\n", dfTargetRatio));
    //    TNTN_LOG_WARN(fmt::format("Other values: nPixels={}, nLines={}, adfExtent={}/{}/{}/{}\n",
    //                              nPixels, nLines, adfExtent[0], adfExtent[1], adfExtent[2], adfExtent[3]));
    //    TNTN_LOG_WARN(fmt::format("Other values: adfSuggestedGeoTransform={}/{}/{}/{}/{}/{}\n",
    //                              adfSuggestedGeoTransform[0], adfSuggestedGeoTransform[1], adfSuggestedGeoTransform[2],
    //                              adfSuggestedGeoTransform[3], adfSuggestedGeoTransform[4], adfSuggestedGeoTransform[5]));
    //    return dataset;
    //  }

    int iOvr;
    for (iOvr = -1; iOvr < nOvCount - 1; iOvr++) {
        const auto dfOvrRatio = (iOvr < 0) ? 1.0 : double(poSrcDS->GetRasterXSize()) / poSrcDS->GetRasterBand(1)->GetOverview(iOvr)->GetXSize();
        const auto dfNextOvrRatio = double(poSrcDS->GetRasterXSize()) / poSrcDS->GetRasterBand(1)->GetOverview(iOvr + 1)->GetXSize();
        if (dfOvrRatio < dfTargetRatio && dfNextOvrRatio > dfTargetRatio)
            break;
        if (std::abs(dfOvrRatio - dfTargetRatio) < 1e-1)
            break;
    }
    iOvr += nOvLevel + 2;
    if (iOvr >= 0) {
        TNTN_LOG_DEBUG("WARPING: Selecting overview level {} for output dataset {}x{}\n", iOvr, nPixels, nLines);
        return std::make_shared<Dataset>(static_cast<GDALDataset*>(GDALCreateOverviewDataset(poSrcDS, iOvr, FALSE)));
    }
    return dataset;
}
#endif

}

DatasetReader::DatasetReader(const std::shared_ptr<Dataset>& dataset, const OGRSpatialReference& targetSRS, unsigned band, bool warn_on_missing_overviews)
    : m_dataset(dataset)
    , m_dataset_srs_wkt(toWkt(dataset->srs()))
    , m_target_srs_wkt(toWkt(targetSRS))
    , m_requires_reprojection(!dataset->srs().IsSame(&targetSRS))
    , m_warn_on_missing_overviews(warn_on_missing_overviews)
    , m_band(band)
{
    if (band > dataset->n_bands())
        throw std::runtime_error(fmt::format("Dataset does not contain band number {} (there are {} bands).", band, dataset->n_bands()));
}

radix::Raster<float> DatasetReader::read(const radix::tile::SrsBounds& bounds, unsigned width, unsigned height) const
{
    return readFrom(m_dataset, bounds, width, height);
}

radix::Raster<float> DatasetReader::readWithOverviews(const radix::tile::SrsBounds& bounds, unsigned width, unsigned height) const
{
#ifdef ALP_ENABLE_OVERVIEW_READING
    auto transformer_args = make_image_transform_args(*this, m_dataset.get(), bounds, width, height);
    auto source_dataset = getOverviewDataset(m_dataset, transformer_args.get(), m_warn_on_missing_overviews);

    return readFrom(source_dataset, bounds, width, height);
#else
    return read(bounds, width, height);
#endif
}

radix::Raster<float> DatasetReader::readFrom(const std::shared_ptr<Dataset>& source_dataset, const radix::tile::SrsBounds& bounds, unsigned width, unsigned height) const
{
    // if we have performance problems with the warping, it'd still be possible to approximate the warping operation with a linear transform (mostly when zoomed in / on higher zoom levels).
    // CTB does this in GDALTiler.cpp around line 375 ("// Decide if we are doing an approximate or exact transformation").

    auto warp_options = makeWarpOptions(*this, source_dataset.get(), bounds, width, height);
    auto adfGeoTransform = computeGeoTransform(bounds, width, height);
    auto warped_dataset = Dataset(static_cast<GDALDataset*>(GDALCreateWarpedVRT(source_dataset->gdalDataset(), int(width), int(height), adfGeoTransform.data(), warp_options.first.get())));

    auto* heights_band = warped_dataset.gdalDataset()->GetRasterBand(1); // non-owning pointer
    auto heights_data = radix::Raster<float>({ width, height });
    if (heights_band->RasterIO(GF_Read, 0, 0, int(width), int(height),
            static_cast<void*>(heights_data.buffer().data()), int(width), int(height), GDT_Float32, 0, 0)
        != CE_None)
        throw std::runtime_error("couldn't read data");

    return heights_data;
}

namespace {
struct WarpCoordinates {
    const RasterTransform* source;
    radix::tile::SrsBounds bounds;
    unsigned side;
    bool failed = false;
    unsigned column_padding = 0;
};

int transform_import(void* argument, int destination_to_source, int count,
    double* x, double* y, double*, int* success)
{
    auto& coordinates = *static_cast<WarpCoordinates*>(argument);
    const double spacing = coordinates.bounds.width() / coordinates.side;
    for (int i = 0; i < count; ++i) {
        const glm::dvec2 destination { coordinates.bounds.min.x + x[i] * spacing,
            coordinates.bounds.max.y - y[i] * spacing };
        glm::dvec2 canonical = destination;
        const double world_period = 2 * RasterTransform::world_half_extent;
        canonical.x -= world_period * std::floor((canonical.x + RasterTransform::world_half_extent) / world_period);
        const bool in_coverage = std::ranges::any_of(coordinates.source->bounds(), [&](const auto& bounds) {
            return canonical.x >= bounds.min.x && canonical.x <= bounds.max.x
                && canonical.y >= bounds.min.y && canonical.y <= bounds.max.y;
        });
        const auto point = destination_to_source
            ? coordinates.source->source_pixel(destination)
            : coordinates.source->mercator({ x[i] - coordinates.column_padding, y[i] });
        success[i] = bool(point);
        if (!point) {
            // Successful out-of-coverage probes are needed for GDAL's source
            // window estimation. Only failed excluded probes are nonfatal.
            coordinates.failed = coordinates.failed || !destination_to_source || in_coverage;
            x[i] = y[i] = HUGE_VAL;
        } else if (destination_to_source) {
            x[i] = point->x + coordinates.column_padding;
            y[i] = point->y;
        } else {
            // Choose the equivalent Mercator branch nearest this output window.
            const double period = 2 * RasterTransform::world_half_extent;
            const double centre = (coordinates.bounds.min.x + coordinates.bounds.max.x) / 2;
            const double world_x = point->x + period * std::round((centre - point->x) / period);
            x[i] = (world_x - coordinates.bounds.min.x) / spacing;
            y[i] = (coordinates.bounds.max.y - point->y) / spacing;
        }
    }
    return TRUE;
}
}

Expected<DatasetReader::Samples<float>> DatasetReader::read_scalar(GDALDataset& dataset,
    const RasterTransform& transform, const radix::tile::SrsBounds& bounds, const unsigned side, const unsigned band)
{
    if (side == 0 || side > unsigned((std::numeric_limits<int>::max)())
        || band == 0 || band > unsigned(dataset.GetRasterCount())) {
        return Error::fail(Error::Code::InvalidInput, "invalid RF read dimensions or source band");
    }
    auto* source_band = dataset.GetRasterBand(int(band));
    if (GDALDataTypeIsComplex(source_band->GetRasterDataType())) {
        return Error::fail(Error::Code::Unsupported, "complex raster bands cannot be imported as scalar or RGB data");
    }
    // A single-band VRT exposes that band's own mask as alpha, including
    // per-band masks which the low-level warper does not automatically apply.
    const auto& affine = transform.affine();
    const double period = transform.reference().IsGeographic()
        ? 2 * std::numbers::pi / transform.reference().GetAngularUnits()
        : 2 * RasterTransform::world_half_extent;
    const bool periodic = affine[2] == 0 && affine[4] == 0
        && (transform.reference().IsGeographic() || (transform.reference().GetAuthorityCode(nullptr) && std::string_view(transform.reference().GetAuthorityCode(nullptr)) == "3857"))
        && std::abs(std::abs(affine[1]) * dataset.GetRasterXSize() - period) < period * 1e-10;
    const unsigned padding = periodic ? (std::min)(8, dataset.GetRasterXSize()) : 0;
    VRTDataset source(dataset.GetRasterXSize() + int(2 * padding), dataset.GetRasterYSize());
    if (source.AddBand(source_band->GetRasterDataType(), nullptr) != CE_None
        || source.AddBand(GDT_Byte, nullptr) != CE_None) {
        return Error::fail(Error::Code::Io, "create RF source-band view");
    }
    auto* values = static_cast<VRTSourcedRasterBand*>(source.GetRasterBand(1));
    auto* validity = static_cast<VRTSourcedRasterBand*>(source.GetRasterBand(2));
    const auto attach = [&](VRTSourcedRasterBand* destination_band, GDALRasterBand* input_band) {
        const int width = dataset.GetRasterXSize();
        const int height = dataset.GetRasterYSize();
        if (destination_band->AddSimpleSource(input_band, 0, 0, width, height, padding, 0, width, height) != CE_None) {
            return false;
        }
        // A periodic global grid needs the opposite edge in the filter halo.
        // Seam-crossing regional grids already have contiguous source columns.
        return padding == 0
            || (destination_band->AddSimpleSource(input_band, width - int(padding), 0, padding, height,
                    0, 0, padding, height) == CE_None
                && destination_band->AddSimpleSource(input_band, 0, 0, padding, height,
                       width + padding, 0, padding, height) == CE_None);
    };
    if (!attach(values, source_band) || !attach(validity, source_band->GetMaskBand())) {
        return Error::fail(Error::Code::Io, "attach source values and validity to RF view");
    }
    auto* driver = GetGDALDriverManager()->GetDriverByName("MEM");
    if (!driver) {
        return Error::fail(Error::Code::Unsupported, "GDAL MEM driver is required");
    }
    Dataset destination(driver->Create("", int(side), int(side), 2, GDT_Float32, nullptr));
    auto options = GdalWarpOptionsPtr(GDALCreateWarpOptions(), &GDALDestroyWarpOptions);
    options->hSrcDS = &source;
    options->hDstDS = destination.gdalDataset();
    options->nBandCount = 1;
    options->panSrcBands = static_cast<int*>(CPLMalloc(sizeof(int)));
    options->panDstBands = static_cast<int*>(CPLMalloc(sizeof(int)));
    options->panSrcBands[0] = options->panDstBands[0] = 1;
    options->nSrcAlphaBand = options->nDstAlphaBand = 2;
    options->eResampleAlg = GRA_Lanczos;
    options->eWorkingDataType = GDT_Float32;
    options->papszWarpOptions = CSLSetNameValue(options->papszWarpOptions, "INIT_DEST", "0");
    // Our borrowed transformer context is synchronous and cannot be cloned by
    // GDAL workers. Do not inherit GDAL_NUM_THREADS from the environment.
    options->papszWarpOptions = CSLSetNameValue(options->papszWarpOptions, "NUM_THREADS", "1");
    options->papszWarpOptions = CSLSetNameValue(options->papszWarpOptions, "SRC_ALPHA_MAX", "255");
    options->papszWarpOptions = CSLSetNameValue(options->papszWarpOptions, "DST_ALPHA_MAX", "255");
    // Sample the interior too: a rotated source may lie wholly inside a window.
    options->papszWarpOptions = CSLSetNameValue(options->papszWarpOptions, "SAMPLE_GRID", "YES");
    options->papszWarpOptions = CSLSetNameValue(options->papszWarpOptions, "SAMPLE_STEPS", "21");
    int has_nodata = FALSE;
    const double nodata = source_band->GetNoDataValue(&has_nodata);
    if (has_nodata) {
        options->padfSrcNoDataReal = static_cast<double*>(CPLMalloc(sizeof(double)));
        options->padfSrcNoDataReal[0] = nodata;
    }
    WarpCoordinates coordinates { &transform, bounds, side, false, padding };
    options->pfnTransformer = transform_import;
    options->pTransformerArg = &coordinates;
    GDALWarpOperation operation;
    if (operation.Initialize(options.get()) != CE_None
        || operation.ChunkAndWarpImage(0, 0, int(side), int(side)) != CE_None || coordinates.failed) {
        return Error::fail(Error::Code::Io, "warp RF source band: " + std::string(CPLGetLastErrorMsg()));
    }
    Samples<float> result { radix::Raster<float>(side), radix::Raster<std::uint8_t>(side) };
    auto* output = destination.gdalDataset();
    if (output->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, int(side), int(side), result.data.buffer().data(),
            int(side), int(side), GDT_Float32, 0, 0) != CE_None) {
        return Error::fail(Error::Code::Io, "read RF warped values and validity");
    }
    std::vector<float> alpha(side);
    for (unsigned row = 0; row < side; ++row) {
        if (output->GetRasterBand(2)->RasterIO(GF_Read, 0, int(row), int(side), 1, alpha.data(),
                int(side), 1, GDT_Float32, 0, 0) != CE_None) {
            return Error::fail(Error::Code::Io, "read RF destination validity");
        }
        for (unsigned column = 0; column < side; ++column) {
            result.valid.buffer()[std::size_t(row) * side + column] = alpha[column] > 0;
        }
    }
    return result;
}

Expected<DatasetReader::Samples<glm::u8vec3>> DatasetReader::read_colour(GDALDataset& dataset,
    const RasterTransform& transform, const radix::tile::SrsBounds& bounds, const unsigned side,
    const std::array<unsigned, 3>& bands)
{
    if (side == 0 || side > unsigned((std::numeric_limits<int>::max)())) {
        return Error::fail(Error::Code::InvalidInput, "invalid RF RGB read dimensions");
    }
    Samples<glm::u8vec3> result { radix::Raster<glm::u8vec3>(side),
        radix::Raster<std::uint8_t>(glm::uvec2(side), 255) };
    for (unsigned channel = 0; channel < 3; ++channel) {
        auto samples = read_scalar(dataset, transform, bounds, side, bands[channel]);
        if (!samples) {
            return Error::propagate(std::move(samples), "read RGB channel " + std::to_string(channel));
        }
        // Convert one row at a time to keep GDAL's signed word count in range.
        for (unsigned row = 0; row < side; ++row) {
            const std::size_t offset = std::size_t(row) * side;
            GDALCopyWords(samples->data.buffer().data() + offset, GDT_Float32, sizeof(float),
                &result.data.buffer()[offset][channel], GDT_Byte, sizeof(glm::u8vec3), int(side));
        }
        for (std::size_t pixel = 0; pixel < result.data.buffer().size(); ++pixel) {
            result.valid.buffer()[pixel] = result.valid.buffer()[pixel] && samples->valid.buffer()[pixel];
        }
    }
    return result;
}
