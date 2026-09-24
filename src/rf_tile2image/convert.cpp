#include "convert.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <system_error>
#include <type_traits>

#include "raster/algorithm/fold.h"
#include "raster/algorithm/transform.h"
#include "raster_store/attribution.h"
#include "raster_store/io/TileCodec.h"
#include "raster_store/io/manifest.h"

namespace rf_tile2image {
namespace {

    namespace manifest = raster_store::io::manifest;

    Expected<std::filesystem::path> metadata_path(const Options& options)
    {
        if (!options.metadata.empty()) {
            return options.metadata;
        }
        std::error_code error;
        auto directory = std::filesystem::absolute(options.input, error).parent_path();
        if (error) {
            return Error::fail(Error::Code::Io, "resolve input path", options.input, error);
        }
        while (true) {
            auto candidate = directory / manifest::metadata_file_name;
            const auto status = std::filesystem::symlink_status(candidate, error);
            if (error && error != std::errc::no_such_file_or_directory) {
                return Error::fail(Error::Code::Io, "inspect metadata", candidate, error);
            }
            if (std::filesystem::exists(status)) {
                return candidate;
            }
            const auto parent = directory.parent_path();
            if (parent == directory) {
                return Error::fail(Error::Code::NotFound, "no raster_store.metadata found; use --metadata", options.input);
            }
            directory = parent;
        }
    }

    glm::u8vec3 attribution_colour(const std::uint16_t index)
    {
        // Multiplication by an odd number permutes the 24-bit integers and preserves zero.
        constexpr std::uint32_t multiplier = 0x9E3779;
        const auto hash = (std::uint32_t { index } * multiplier) & 0xFFFFFF;
        return { static_cast<std::uint8_t>(hash >> 16), static_cast<std::uint8_t>(hash >> 8), static_cast<std::uint8_t>(hash) };
    }

    glm::u8vec3 cubehelix(const long double fraction)
    {
        // Dave Green's standard Cubehelix: start=0.5, rotations=-1.5, hue=1, gamma=1.
        // https://people.phy.cam.ac.uk/dag9/CUBEHELIX/cubhlx.f (public domain)
        const auto value = static_cast<double>(std::clamp(fraction, 0.0L, 1.0L));
        const auto angle = 2 * std::numbers::pi * (0.5 / 3 + 1 - 1.5 * value);
        const auto amplitude = value * (1 - value) / 2;
        const auto cosine = std::cos(angle);
        const auto sine = std::sin(angle);
        const auto channel = [](const double component) { return static_cast<std::uint8_t>(std::lround(255 * std::clamp(component, 0.0, 1.0))); };
        return { channel(value + amplitude * (-0.14861 * cosine + 1.78277 * sine)),
            channel(value + amplitude * (-0.29227 * cosine - 0.90649 * sine)),
            channel(value + amplitude * 1.97294 * cosine) };
    }

    template <typename PixelType>
    Expected<io::image::RGB8> render_scalar(const radix::Raster<PixelType>& raster, const Options& options)
    {
        struct Extrema {
            long double minimum = std::numeric_limits<long double>::infinity();
            long double maximum = -std::numeric_limits<long double>::infinity();
        };
        auto [minimum, maximum] = raster::algorithm::fold(raster, Extrema {}, [](Extrema state, const PixelType& pixel) {
            const auto value = static_cast<long double>(pixel);
            if (std::isfinite(value)) {
                state.minimum = (std::min)(state.minimum, value);
                state.maximum = (std::max)(state.maximum, value);
            }
            return state;
        });
        const bool has_finite_values = std::isfinite(minimum);
        const bool diagnostic = !options.minimum && !options.maximum && minimum == maximum;
        minimum = options.minimum.value_or(minimum);
        maximum = options.maximum.value_or(maximum);
        if (has_finite_values && !diagnostic && minimum >= maximum) {
            return Error::fail(Error::Code::InvalidInput, "effective --min must be less than --max");
        }
        return raster::algorithm::transform(raster, [&](const PixelType& pixel) -> glm::u8vec3 {
            const auto value = static_cast<long double>(pixel);
            if (std::isfinite(value)) {
                if (diagnostic) {
                    return minimum == 0 ? glm::u8vec3(0, 0, 0) : (minimum > 0 ? glm::u8vec3(255, 0, 0) : glm::u8vec3(0, 0, 255));
                } else if (value <= minimum) {
                    return glm::u8vec3(0, 0, 0);
                } else if (value >= maximum) {
                    return glm::u8vec3(255, 255, 255);
                } else {
                    // Halving first avoids overflowing the difference for opposite extreme endpoints.
                    const auto width = maximum - minimum;
                    const auto fraction = std::isfinite(width) ? (value - minimum) / width : (value / 2 - minimum / 2) / (maximum / 2 - minimum / 2);
                    return cubehelix(fraction);
                }
            }
            return glm::u8vec3(255, 0, 255);
        });
    }

    template <typename PixelType>
    Expected<Images> read_typed(const Options& options, const manifest::Metadata& metadata)
    {
        auto node_path = options.input;
        node_path.replace_extension();
        auto tile = raster_store::io::TileCodec<PixelType>(glm::uvec2(metadata.stored_tile_size)).read(node_path);
        if (!tile) {
            return Error::propagate(std::move(tile), "read RF tile for preview");
        }
        Images images;
        if constexpr (std::is_arithmetic_v<PixelType>) {
            auto data = render_scalar(tile->data, options);
            if (!data) {
                return Error::propagate(std::move(data));
            }
            images.data = std::move(*data);
        } else {
            auto data = raster::algorithm::transform(tile->data, [](const PixelType& pixel) { return glm::u8vec3(pixel); });
            if (!data) {
                return Error::propagate(std::move(data));
            }
            images.data = std::move(*data);
        }
        if (raster::algorithm::fold(tile->source_attribution, false, [](bool invalid, const std::uint16_t& index) {
                return invalid || index >= raster_store::attribution::index_limit;
            })) {
            return Error::fail(Error::Code::CorruptData, "unsupported attribution index 65535");
        }
        auto attribution = raster::algorithm::transform(tile->source_attribution, attribution_colour);
        if (!attribution) {
            return Error::propagate(std::move(attribution));
        }
        images.attribution = std::move(*attribution);
        return images;
    }

    template <typename... PixelTypes>
    Expected<Images> dispatch_scalar(const Options& options, const manifest::Metadata& metadata)
    {
        Expected<Images> result = Error::fail(Error::Code::Unsupported, "unsupported scalar payload type: " + metadata.payload_type);
        ((metadata.payload_type == raster_store::pixel::identifier<PixelTypes>() ? (result = read_typed<PixelTypes>(options, metadata), true) : false) || ...);
        return result;
    }

    Expected<void> check_output(const std::filesystem::path& path, const bool overwrite)
    {
        std::error_code error;
        const auto status = std::filesystem::symlink_status(path, error);
        if (error && error != std::errc::no_such_file_or_directory) {
            return Error::fail(Error::Code::Io, "inspect output", path, error);
        }
        if (std::filesystem::exists(status) && !overwrite) {
            return Error::fail(Error::Code::AlreadyExists, "output exists; use --overwrite", path);
        }
        if (std::filesystem::exists(status) && !std::filesystem::is_regular_file(status)) {
            return Error::fail(Error::Code::InvalidInput, "output must be a regular file", path);
        }
        return {};
    }

} // namespace

Expected<Images> render_tile(const Options& options)
try {
    if (options.input.extension() != ".amort") {
        return Error::fail(Error::Code::InvalidInput, "input must be an .amort tile", options.input);
    }
    if ((options.minimum && !std::isfinite(*options.minimum)) || (options.maximum && !std::isfinite(*options.maximum))
        || (options.minimum && options.maximum && *options.minimum >= *options.maximum)) {
        return Error::fail(Error::Code::InvalidInput, "range endpoints must be finite, with --min less than --max");
    }
    auto path = metadata_path(options);
    if (!path) {
        return Error::propagate(std::move(path));
    }
    auto metadata = ::io::envelope::read_from_path<manifest::MetadataSchema>(*path);
    if (!metadata) {
        return Error::propagate(std::move(metadata), "read preview metadata");
    }
    if (auto valid = manifest::validate(*metadata); !valid) {
        return Error::propagate(std::move(valid), Error::Code::CorruptData, "validate preview metadata");
    }
    if (metadata->codec_selector != "amort") {
        return Error::fail(Error::Code::Unsupported, "unsupported tile codec: " + metadata->codec_selector);
    }
    if (metadata->stored_tile_size > static_cast<unsigned>((std::numeric_limits<int>::max)())) {
        return Error::fail(Error::Code::ResourceExhausted, "tile dimensions exceed image encoder limits");
    }
    if (metadata->value_mapping == raster_store::pixel::Mapping::Linear) {
        return dispatch_scalar<std::int8_t, std::uint8_t, std::int16_t, std::uint16_t, std::int32_t, std::uint32_t, std::int64_t, std::uint64_t, float, double>(
            options, *metadata);
    }
    if (options.minimum || options.maximum) {
        return Error::fail(Error::Code::InvalidInput, "--min and --max are only supported for linear scalar data");
    }
    if (metadata->payload_type == raster_store::pixel::identifier<glm::u8vec3>()) {
        return read_typed<glm::u8vec3>(options, *metadata);
    }
    if (metadata->payload_type == raster_store::pixel::identifier<glm::u8vec4>()) {
        return read_typed<glm::u8vec4>(options, *metadata);
    }
    return Error::fail(Error::Code::Unsupported, "sRGB mapping requires RGB8 or RGBA8 imagery");
} catch (const std::bad_alloc&) {
    return Error::fail(Error::Code::ResourceExhausted, "insufficient memory to render tile");
}

Expected<OutputPaths> convert(const Options& options)
try {
    const auto directory = options.output_directory.empty() ? options.input.parent_path() : options.output_directory;
    const auto stem = options.input.stem().string();
    OutputPaths paths { directory / (stem + ".jpg"), directory / (stem + ".png") };
    for (const auto& path : { paths.data, paths.attribution }) {
        if (auto checked = check_output(path, options.overwrite); !checked) {
            return Error::propagate(std::move(checked));
        }
    }
    auto images = render_tile(options);
    if (!images) {
        return Error::propagate(std::move(images));
    }
    const io::image::WriteOptions output_options { .overwrite = options.overwrite };
    if (auto written = io::image::write(images->data, paths.data, output_options); !written) {
        return Error::propagate(std::move(written));
    }
    if (auto written = io::image::write(images->attribution, paths.attribution, output_options); !written) {
        return Error::propagate(std::move(written));
    }
    return paths;
} catch (const std::bad_alloc&) {
    return Error::fail(Error::Code::ResourceExhausted, "insufficient memory to encode tile");
}

} // namespace rf_tile2image
