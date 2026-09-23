#include "nodata.h"

#include "Dataset.h"
#include <fmt/format.h>
#include <gdal_alg.h>

namespace rf_builder::gdal::nodata {

Expected<unsigned> halo(unsigned tile_side, unsigned search_radius, unsigned kernel_size)
{
    if (tile_side == 0 || kernel_size == 0 || kernel_size % 2 == 0) {
        return Error::fail(Error::Code::InvalidInput, "RF tile size and odd Gaussian kernel size must be positive");
    }
    const std::uint64_t width = std::uint64_t(search_radius) + kernel_size / 2;
    const std::uint64_t expanded = std::uint64_t(tile_side) + 2 * width;
    if (expanded > std::uint64_t((std::numeric_limits<int>::max)()) || expanded > std::vector<float>().max_size() / expanded) {
        return Error::fail(Error::Code::InvalidInput, "RF NoData halo exceeds GDAL dimensions or raster capacity");
    }
    return unsigned(width);
}

Window window(const radix::tile::Id& key, unsigned tile_side, unsigned halo_width)
{
    const auto bounds = RasterTransform::tile_bounds(key);
    const double spacing = bounds.width() / tile_side;
    const std::uint64_t rows = std::uint64_t(tile_side) << key.zoom_level;
    const std::uint64_t first_row = std::uint64_t(tile_side) * key.coords.y;
    const unsigned above = unsigned((std::min)(std::uint64_t(halo_width), first_row));
    const unsigned below = unsigned((std::min)(std::uint64_t(halo_width), rows - first_row - tile_side));
    return { { { bounds.min.x - halo_width * spacing, bounds.min.y - below * spacing },
                 { bounds.max.x + halo_width * spacing, bounds.max.y + above * spacing } },
        { tile_side + 2 * halo_width, tile_side + above + below },
        { halo_width, above } };
}

Expected<Processor> Processor::create(unsigned search_radius, unsigned kernel_size)
{
    auto weights = raster::algorithm::gaussian_kernel(kernel_size);
    if (!weights) {
        return Error::propagate(std::move(weights));
    }
    return Processor(search_radius, std::move(*weights));
}

void Processor::resize(radix::Raster<float>& raster, glm::uvec2 size)
{
    if (raster.size() != size) {
        raster = radix::Raster<float>(size);
    }
}

Expected<raster::View<const float>> Processor::complete(const radix::Raster<std::uint8_t>& valid, glm::uvec2 offset, unsigned side)
{
    if (m_search_radius != 0 && std::ranges::any_of(valid.buffer(), [](auto value) { return value != 0; })) {
        auto* driver = GetGDALDriverManager()->GetDriverByName("MEM");
        if (!driver) {
            return Error::fail(Error::Code::Unsupported, "GDAL MEM driver is required for NoData filling");
        }
        // These datasets only borrow worker-owned buffers. They die before any
        // resize. GDALFillNodata reads the mask without updating it.
        Dataset memory(driver->Create("", int(m_work.width()), int(m_work.height()), 0, GDT_Unknown, nullptr));
        if (!memory.gdalDataset()) {
            return Error::fail(Error::Code::Io, "create NoData memory dataset");
        }
        CPLStringList values_options;
        values_options.SetNameValue("DATAPOINTER", fmt::format("{}", static_cast<void*>(m_work.buffer().data())).c_str());
        CPLStringList mask_options;
        mask_options.SetNameValue("DATAPOINTER", fmt::format("{}", static_cast<const void*>(valid.buffer().data())).c_str());
        auto* dataset = memory.gdalDataset();
        if (dataset->AddBand(GDT_Float32, values_options.List()) != CE_None || dataset->AddBand(GDT_Byte, mask_options.List()) != CE_None) {
            return Error::fail(Error::Code::Io, "attach NoData working values and mask");
        }
        CPLStringList options;
        options.SetNameValue("TEMP_FILE_DRIVER", "MEM");
        if (GDALFillNodata(dataset->GetRasterBand(1), dataset->GetRasterBand(2), m_search_radius, 0, 0, options.List(), nullptr, nullptr) != CE_None) {
            return Error::fail(Error::Code::Io, "fill RF NoData pixels: " + std::string(CPLGetLastErrorMsg()));
        }
    }
    const unsigned size = unsigned(m_weights.size());
    if (size == 1) {
        auto view = raster::make_view(std::as_const(m_work), offset, glm::uvec2(side));
        if (!view) {
            return Error::propagate(std::move(view));
        }
        return *view;
    }
    const unsigned radius = size / 2;
    auto input = raster::make_clamped_view(m_work, glm::i64vec2(offset) - glm::i64vec2(radius), glm::uvec2(side + 2 * radius));
    if (!input) {
        return Error::propagate(std::move(input));
    }
    resize(m_horizontal, { side, side + 2 * radius });
    resize(m_smoothed, glm::uvec2(side));
    auto horizontal = raster::algorithm::window_transform(
        *input,
        { size, 1 },
        [&](const auto& values) -> float {
            double sum = 0;
            for (unsigned i = 0; i < size; ++i) {
                sum += values.pixel({ i, 0 }) * m_weights[i];
            }
            return float(sum);
        },
        m_horizontal);
    if (!horizontal) {
        return Error::propagate(std::move(horizontal));
    }
    auto vertical = raster::algorithm::window_transform(
        m_horizontal,
        { 1, size },
        [&](const auto& values) -> float {
            double sum = 0;
            for (unsigned i = 0; i < size; ++i) {
                sum += values.pixel({ 0, i }) * m_weights[i];
            }
            return float(sum);
        },
        m_smoothed);
    if (!vertical) {
        return Error::propagate(std::move(vertical));
    }
    return raster::make_view(std::as_const(m_smoothed));
}
} // namespace rf_builder::gdal::nodata
