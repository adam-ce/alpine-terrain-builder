#include "provider.h"
#include "io/bytes.h"
#include "raster_store/StoreTraits.h"
#include <array>
#include <bit>
#include <cctype>
#include <cpl_json.h>
#include <curl/curl.h>
#include <limits>
#include <string_view>

namespace rf_builder::tiles::provider {
namespace {
    // CPL's JSON reader accepts comments and trailing data. Validate the complete
    // flat provider grammar first; CPL still handles string decoding and field types.
    bool strict_object(std::string_view text)
    {
        std::size_t at = 0;
        const auto whitespace = [&] {
            while (at < text.size() && (text[at] == ' ' || text[at] == '\t' || text[at] == '\r' || text[at] == '\n')) {
                ++at;
            }
        };
        const auto take = [&](char value) {
            whitespace();
            if (at == text.size() || text[at] != value) {
                return false;
            }
            ++at;
            return true;
        };
        const auto string = [&] {
            if (!take('"')) {
                return false;
            }
            while (at < text.size()) {
                const auto value = static_cast<unsigned char>(text[at++]);
                if (value == '"') {
                    return true;
                }
                if (value < 0x20) {
                    return false;
                }
                if (value != '\\') {
                    continue;
                }
                if (at == text.size()) {
                    return false;
                }
                const auto escaped = text[at++];
                if (escaped == 'u') {
                    for (unsigned i = 0; i < 4; ++i) {
                        if (at == text.size() || !std::isxdigit(static_cast<unsigned char>(text[at++]))) {
                            return false;
                        }
                    }
                } else if (std::string_view("\"\\/bfnrt").find(escaped) == std::string_view::npos) {
                    return false;
                }
            }
            return false;
        };
        if (!take('{')) {
            return false;
        }
        unsigned fields = 0;
        for (;;) {
            if (!string() || !take(':')) {
                return false;
            }
            whitespace();
            if (at < text.size() && text[at] == '"') {
                if (!string()) {
                    return false;
                }
            } else {
                if (at < text.size() && text[at] == '-') {
                    ++at;
                }
                if (at == text.size() || text[at] < '0' || text[at] > '9') {
                    return false;
                }
                const auto first = text[at++];
                while (at < text.size() && text[at] >= '0' && text[at] <= '9') {
                    if (first == '0') {
                        return false;
                    }
                    ++at;
                }
            }
            ++fields;
            if (take('}')) {
                whitespace();
                return at == text.size() && fields == 5;
            }
            if (!take(',')) {
                return false;
            }
        }
    }
} // namespace
Expected<Settings> parse(const std::string& json)
{
    if (!strict_object(json)) {
        return Error::fail(Error::Code::InvalidInput, "provider must be a complete JSON object with string and integer fields");
    }
    CPLJSONDocument document;
    if (json.size() > unsigned((std::numeric_limits<int>::max)()) || !document.LoadMemory(json)) {
        return Error::fail(Error::Code::InvalidInput, "invalid provider JSON");
    }
    const auto root = document.GetRoot();
    constexpr std::array fields { "url_pattern", "y_direction", "min_zoom", "max_zoom", "tile_size" };
    if (root.GetType() != CPLJSONObject::Type::Object || root.GetChildren().size() != fields.size()) {
        return Error::fail(Error::Code::InvalidInput, "provider must contain exactly url_pattern, y_direction, min_zoom, max_zoom and tile_size");
    }
    for (const auto field : fields) {
        if (!root[field].IsValid()) {
            return Error::fail(Error::Code::InvalidInput, "missing provider field " + std::string(field));
        }
    }
    if (root["url_pattern"].GetType() != CPLJSONObject::Type::String || root["y_direction"].GetType() != CPLJSONObject::Type::String) {
        return Error::fail(Error::Code::InvalidInput, "provider URL and Y direction must be strings");
    }
    const auto direction = root["y_direction"].ToString();
    if (direction != "down" && direction != "up") {
        return Error::fail(Error::Code::InvalidInput, "provider y_direction must be down or up");
    }
    Settings result;
    result.url_pattern = root["url_pattern"].ToString();
    result.y_direction = direction == "down" ? YDirection::Down : YDirection::Up;
    std::array<unsigned*, 3> destinations { &result.min_zoom, &result.max_zoom, &result.tile_size };
    for (std::size_t i = 0; i < destinations.size(); ++i) {
        const auto value = root[fields[i + 2]];
        const auto type = value.GetType();
        const auto integer = value.ToLong();
        if ((type != CPLJSONObject::Type::Integer && type != CPLJSONObject::Type::Long) || integer < 0
            || std::uint64_t(integer) > (std::numeric_limits<unsigned>::max)()) {
            return Error::fail(Error::Code::InvalidInput, "provider zooms and tile size must be nonnegative integers in range");
        }
        *destinations[i] = unsigned(integer);
    }
    if (result.min_zoom > result.max_zoom || result.max_zoom > raster_store::StoreTraits::max_zoom_level || !std::has_single_bit(result.tile_size)
        || result.tile_size > 65535) {
        return Error::fail(Error::Code::InvalidInput, "invalid provider zoom range or power-of-two JPEG dimensions (maximum 65535)");
    }
    if ((!result.url_pattern.starts_with("http://") && !result.url_pattern.starts_with("https://"))
        || result.url_pattern.find_first_of(" \t\r\n") != std::string::npos || result.url_pattern.find('\0') != std::string::npos) {
        return Error::fail(Error::Code::InvalidInput, "provider URL must be an HTTP(S) template without whitespace or NUL");
    }
    for (const auto token : { "{zoom}", "{x}", "{y}" }) {
        if (result.url_pattern.find(token) == std::string::npos) {
            return Error::fail(Error::Code::InvalidInput, "provider URL requires {zoom}, {x} and {y}");
        }
    }
    const auto expanded = url(result, { 0, { 0, 0 } });
    auto* parsed = curl_url();
    if (!parsed) {
        return Error::fail(Error::Code::ResourceExhausted, "allocate provider URL parser");
    }
    const auto valid = curl_url_set(parsed, CURLUPART_URL, expanded.c_str(), 0);
    curl_url_cleanup(parsed);
    if (valid != CURLUE_OK || expanded.find_first_of("{}") != std::string::npos) {
        return Error::fail(Error::Code::InvalidInput, "invalid provider HTTP(S) URL template");
    }
    return result;
}
Expected<Settings> read(const std::filesystem::path& path)
{
    auto bytes = io::read_bytes_from_path(path);
    if (!bytes) {
        return Error::propagate(std::move(bytes), "read provider JSON");
    }
    return parse(std::string(bytes->begin(), bytes->end()));
}
Expected<unsigned> zoom_offset(const Settings& settings, unsigned output_side)
{
    if (!settings.tile_size || output_side < settings.tile_size || output_side % settings.tile_size != 0
        || !std::has_single_bit(output_side / settings.tile_size)) {
        return Error::fail(Error::Code::InvalidInput, "RF tile size must equal source tile size times a nonnegative power of two");
    }
    const auto offset = unsigned(std::countr_zero(output_side / settings.tile_size));
    if (settings.min_zoom < offset) {
        return Error::fail(
            Error::Code::InvalidInput, "minimum source zoom minus RF/source zoom offset is negative; raise provider min_zoom or reduce RF tile size");
    }
    if (std::uint64_t(output_side) * output_side > (std::numeric_limits<std::size_t>::max)() / 8) {
        return Error::fail(Error::Code::InvalidInput, "RF dimensions overflow pixel buffer arithmetic");
    }
    return offset;
}
std::string url(const Settings& settings, const radix::tile::Id& key)
{
    auto result = settings.url_pattern;
    const std::uint64_t y = settings.y_direction == YDirection::Down ? key.coords.y : (std::uint64_t(1) << key.zoom_level) - 1 - key.coords.y;
    const std::array<std::pair<std::string, std::string>, 3> substitutions {
        { { "{zoom}", std::to_string(key.zoom_level) }, { "{x}", std::to_string(key.coords.x) }, { "{y}", std::to_string(y) } }
    };
    for (const auto& [token, value] : substitutions) {
        std::size_t position = 0;
        while ((position = result.find(token, position)) != std::string::npos) {
            result.replace(position, token.size(), value);
            position += value.size();
        }
    }
    return result;
}
} // namespace rf_builder::tiles::provider
