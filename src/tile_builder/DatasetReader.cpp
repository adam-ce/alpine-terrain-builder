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
#include <memory>
#include <ogr_spatialref.h>
#include <utility>

#include "Dataset.h"
#include "Exception.h"
#include "Image.h"
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
        throw Exception("GDALCreateGenImgProjTransformer2 failed.");
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
        throw Exception(fmt::format("Dataset does not contain band number {} (there are {} bands).", band, dataset->n_bands()));
}

HeightData DatasetReader::read(const radix::tile::SrsBounds& bounds, unsigned width, unsigned height) const
{
    return readFrom(m_dataset, bounds, width, height);
}

HeightData DatasetReader::readWithOverviews(const radix::tile::SrsBounds& bounds, unsigned width, unsigned height) const
{
#ifdef ALP_ENABLE_OVERVIEW_READING
    auto transformer_args = make_image_transform_args(*this, m_dataset.get(), bounds, width, height);
    auto source_dataset = getOverviewDataset(m_dataset, transformer_args.get(), m_warn_on_missing_overviews);

    return readFrom(source_dataset, bounds, width, height);
#else
    return read(bounds, width, height);
#endif
}

HeightData DatasetReader::readFrom(const std::shared_ptr<Dataset>& source_dataset, const radix::tile::SrsBounds& bounds, unsigned width, unsigned height) const
{
    // if we have performance problems with the warping, it'd still be possible to approximate the warping operation with a linear transform (mostly when zoomed in / on higher zoom levels).
    // CTB does this in GDALTiler.cpp around line 375 ("// Decide if we are doing an approximate or exact transformation").

    auto warp_options = makeWarpOptions(*this, source_dataset.get(), bounds, width, height);
    auto adfGeoTransform = computeGeoTransform(bounds, width, height);
    auto warped_dataset = Dataset(static_cast<GDALDataset*>(GDALCreateWarpedVRT(source_dataset->gdalDataset(), int(width), int(height), adfGeoTransform.data(), warp_options.first.get())));

    auto* heights_band = warped_dataset.gdalDataset()->GetRasterBand(1); // non-owning pointer
    auto heights_data = HeightData(width, height);
    if (heights_band->RasterIO(GF_Read, 0, 0, int(width), int(height),
            static_cast<void*>(heights_data.data()), int(width), int(height), GDT_Float32, 0, 0)
        != CE_None)
        throw Exception("couldn't read data");

    return heights_data;
}
