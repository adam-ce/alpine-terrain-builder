#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <span>
#include <string_view>
#include <optional>

#include <curl/curl.h>
#include <fmt/core.h>

#include "ctb/Grid.hpp"
#include "srs.h"

using namespace std::literals;
using namespace radix;

static unsigned verbosity = 1;

bool char_equals_ignore_case(const char a, const char b) {
    return std::tolower(std::char_traits<char>::to_int_type(a)) ==
           std::tolower(std::char_traits<char>::to_int_type(b));
}
bool string_equals_ignore_case(const std::string_view &a, const std::string_view &b) {
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), char_equals_ignore_case);
}

unsigned int svtoui(const std::string_view s) {
    unsigned int n;
    std::from_chars_result res = std::from_chars(s.begin(), s.end(), n);

    // These two exceptions reflect the behavior of std::stoi.
    if (res.ec == std::errc::invalid_argument) {
        throw std::invalid_argument{"invalid_argument"};
    } else if (res.ec == std::errc::result_out_of_range) {
        throw std::out_of_range{"out_of_range"};
    }

    return n;
}

bool stob(const std::string_view s) {
    if (string_equals_ignore_case(s, "true") || s == "1") {
        return true;
    }
    if (string_equals_ignore_case(s, "false") || s == "0") {
        return false;
    }
    throw std::invalid_argument{"invalid_argument"};
}

template <class K, class V>
constexpr const V &map_get_or_default(const std::map<K, V> &map, const K &key, const V &default_value) {
    const auto iter = map.find(key);
    if (iter == map.end()) {
        return default_value;
    }
    return iter->second;
}

constexpr auto &map_get_required(const auto &map, const auto &key) {
    const auto itr = map.find(key);
    return itr != map.cend() ? itr->second : throw std::runtime_error(fmt::format("missing argument \"{}\"", key));
}

void string_replace_all(
    std::string &s,
    std::string_view find,
    std::string_view replace) {
    size_t pos = 0;
    const size_t find_len = find.length();
    const size_t replace_len = replace.length();

    while ((pos = s.find(find, pos)) != std::string::npos) {
        s.replace(pos, find_len, replace);
        pos += replace_len; // Move past the replaced part
    }
}

bool create_directories2(const std::filesystem::path& path) {
    std::error_code err;
    err.clear();

    if (!std::filesystem::create_directories(path, err)) {
        if (std::filesystem::exists(path)) {
            // The folder already exists:
            err.clear();
            return true;
        }

        return false;
    }

    return true;
}

class TileUrlBuilder {
public:
    virtual std::string build_url(const tile::Id &tile_id) const = 0;
};

// https://mapsneu.wien.gv.at/basemapneu/1.0.0/WMTSCapabilities.xml
class BasemapTileUrlBuilder : public TileUrlBuilder {
public:
    BasemapTileUrlBuilder(const std::map<std::string_view, std::string_view> &args) {
        this->layer = map_get_or_default(args, "layer"sv, "bmaporthofoto30cm"sv);
        this->style = map_get_or_default(args, "style"sv, "normal"sv);
    }

    std::string build_url(const tile::Id &tile_id) const {
        const tile::Id google_tile_id = tile_id.to(tile::Scheme::SlippyMap);
        return fmt::format(
            "https://mapsneu.wien.gv.at/basemap/{}/{}/google3857/{}/{}/{}.jpeg",
            layer, style, google_tile_id.zoom_level, google_tile_id.coords.y, google_tile_id.coords.x);
    }

private:
    std::string_view layer;
    std::string_view style;
};

// https://gataki.cg.tuwien.ac.at/raw/basemap/tiles/11/710/1098.jpeg
class GatakiTileUrlBuilder : public TileUrlBuilder {
public:
    GatakiTileUrlBuilder(const std::map<std::string_view, std::string_view> &args) {}

    std::string build_url(const tile::Id &tile_id) const {
        const tile::Id google_tile_id = tile_id.to(tile::Scheme::SlippyMap);
        return fmt::format(
            "https://gataki.cg.tuwien.ac.at/raw/basemap/tiles/{}/{}/{}.jpeg",
            google_tile_id.zoom_level, google_tile_id.coords.y, google_tile_id.coords.x);
    }

private:
};

static std::string format_tile(const tile::Id tile) {
    return fmt::format("Tile[Zoom={}, X={}, Y={}]", tile.zoom_level, tile.coords.x, tile.coords.y);
}

void print_tile(const tile::Id tile) {
    std::string tile_str = format_tile(tile);
    fmt::print("{}", tile_str);
    std::fflush(nullptr);
}

void update_tile_status(const tile::Id tile, const std::string_view status, const bool final) {
    std::string tile_str = format_tile(tile);
    fmt::print("\33[2K\r{} ({}){}", tile_str, status, final ? "\n" : "");
    std::fflush(nullptr);
}

struct WriteCallbackData {
    std::ofstream file;
    std::filesystem::path path;
};

// Write callback function to write downloaded data to the file
size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    WriteCallbackData &data = *static_cast<WriteCallbackData *>(userdata);

    // Check if the file is opened
    if (!data.file.is_open()) {
        // If the file is not opened yet, try to open it
        data.file.open(data.path, std::ios::binary);

        if (!data.file.is_open()) {
            // Handle file opening failure
            return 0; // Returning 0 will abort the download
        }
    }

    // Write the data to the file and return the number of bytes written
    size_t total_size = size * nmemb;
    data.file.write(static_cast<char *>(ptr), total_size);

    return total_size;
}

struct ProgressCallbackData {
    tile::Id tile;
};

int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    ProgressCallbackData &data = *static_cast<ProgressCallbackData *>(clientp);

    if (dltotal == 0) {
        if (verbosity > 0)
            update_tile_status(data.tile, "Downloading...", false);
        return 0;
    }

    // Calculate the download progress percentage
    const double progress = ((double)dlnow / (double)dltotal) * 100;

    // Write it to the console
    if (verbosity > 0)
        update_tile_status(data.tile, fmt::format("Downloading {:.0f}%...", progress), false);

    return 0;
}

enum class ContentType {
    Image,
    Other,
    Unknown,
};

ContentType read_content_type(CURL *curl) {
    char *content_type = NULL;
    const auto res = curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type);
    if (res != CURLE_OK || content_type == nullptr)
        return ContentType::Unknown;

    const std::string content_type_string = content_type;
    if (content_type_string.rfind("image", 0) == 0)
        return ContentType::Image;
    return ContentType::Other;
}

enum DownloadResult {
    Downloaded,
    Skipped,
    Absent,
    Failed
};

class TileDownloader
{
public:
    TileDownloader(TileUrlBuilder *url_builder, const std::map<std::string_view, std::string_view> &args) {
        this->url_builder = url_builder;
        this->output_path = map_get_or_default(args,
                                               "output"sv,
                                               "tiles/{zoom}/{y}/{x}.{ext}"sv);
        this->early_skip = stob(map_get_or_default(args, "early-skip"sv, "true"sv));

        if (args.contains("max-zoom-level")) {
            this->max_zoom_level = svtoui(args.at("max-zoom-level"));
        }

        this->curl = curl_easy_init();
        if (!this->curl) {
            throw std::runtime_error("failed to init cURL");
        }
    }

    ~TileDownloader() {
        if (this->curl) {
            curl_easy_cleanup(this->curl);
        }
    }

    DownloadResult download_tile_by_url(const tile::Id tile, const std::string &url, const std::filesystem::path &path) const {
        if (verbosity > 0)
            print_tile(tile);

        // Skip tile if it already exists.
        const std::filesystem::path absolute_path = std::filesystem::absolute(path);
        if (std::filesystem::exists(absolute_path)) {
            if (verbosity > 0)
                update_tile_status(tile, "Skipped", true);

            return DownloadResult::Skipped;
        }

        // Create parent directories
        const std::filesystem::path parent_path = absolute_path.parent_path();
        if (!create_directories2(parent_path)) {
            throw std::runtime_error(fmt::format("failed to create directories \"{}\"", parent_path.string()));
        }

        // Perform actual download
        if (verbosity > 0)
            update_tile_status(tile, "Connecting...", false);

        for (int i = 0; i < 100; i++) {
            // Set the URL to download
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

            // Create a custom data structure to pass file-related information to the write callback
            WriteCallbackData write_callback_data;
            write_callback_data.path = path;
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_callback_data);

            // Set the progress callback function and pass the logger state
            ProgressCallbackData progress_callback_data;
            progress_callback_data.tile = tile;
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_callback_data);

            // Disable timeouts and fail on HTTP status > 400
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
            curl_easy_setopt(curl, CURLOPT_FAILONERROR, TRUE);

            const CURLcode res = curl_easy_perform(curl);

            if (write_callback_data.file.is_open()) {
                write_callback_data.file.close();
            }

            const auto content_type = read_content_type(this->curl);

            if (res == CURLE_OK && content_type == ContentType::Image) {
                if (verbosity > 0)
                    update_tile_status(tile, "Done", true);
                return DownloadResult::Downloaded;
            }

            // remove empty or partial file.
            std::filesystem::remove(absolute_path);

            // Handle errors
            const std::string_view message = curl_easy_strerror(res);
            const unsigned int code = static_cast<unsigned int>(res);

            if (res == CURLE_HTTP_RETURNED_ERROR) {
                long status;
                curl_easy_getinfo(this->curl, CURLINFO_RESPONSE_CODE, &status);

                if (status == 404) {
                    if (verbosity > 0)
                        update_tile_status(tile, "Absent", true);
                    return DownloadResult::Absent;
                }

                update_tile_status(tile, fmt::format("Error HTTP [{}]", status), true);
            } else if (content_type == ContentType::Other) {
                update_tile_status(tile, fmt::format("Error, bad content type."), true);
            } else {
                // no matter how high the timeout is set, the request still times out, so we just try again if we do.
                if (res == CURLE_OPERATION_TIMEDOUT) {
                    continue;
                }

                update_tile_status(tile, fmt::format("Error cURL [{}] {}", code, message), true);
            }

            return DownloadResult::Failed;
        }
        update_tile_status(tile, fmt::format("Failed after 100 attempts"), true);

        return DownloadResult::Failed;
    }

    DownloadResult download_tile_by_id(const tile::Id tile) const {
        const std::string url = this->url_builder->build_url(tile);
        const std::string path = this->get_tile_path(tile);
        return this->download_tile_by_url(tile, url, path);
    }

    DownloadResult download_tile_by_id_recursive(const tile::Id root_id) const {
        const std::string url = this->url_builder->build_url(root_id);
        const DownloadResult result = this->download_tile_by_id(root_id);

        if (result == DownloadResult::Failed || result == DownloadResult::Absent) {
            return result;
        }

        const std::array<tile::Id, 4> subtiles = root_id.children();

        for (size_t i = 0; i < subtiles.size(); i++) {
            const tile::Id &tile = subtiles[i];

            if (early_skip && i + 1 < subtiles.size()) {
                const tile::Id &next_tile = subtiles[i + 1];
                const std::string next_tile_path = this->get_tile_path(next_tile);
                if (std::filesystem::exists(next_tile_path)) {
                    print_tile(tile);
                    update_tile_status(tile, "Skipped", true);
                    continue;
                }
            }
            
            if (this->max_zoom_level.has_value() && tile.zoom_level > this->max_zoom_level.value()) {
                print_tile(tile);
                update_tile_status(tile, "Skipped", true);
                continue;
            }
            this->download_tile_by_id_recursive(tile);
        }

        return result;
    }

private:
    TileUrlBuilder *url_builder;
    std::string_view output_path;
    std::string_view file_name_template;
    CURL *curl;
    bool early_skip;
    std::optional<int> max_zoom_level;

    std::string get_tile_path(const tile::Id tile) const {
        std::string file_path(this->output_path);
        string_replace_all(file_path, "{zoom}"sv, std::to_string(tile.zoom_level));
        string_replace_all(file_path, "{x}"sv, std::to_string(tile.coords.x));
        string_replace_all(file_path, "{y}"sv, std::to_string(tile.coords.y));
        string_replace_all(file_path, "{ext}"sv, "jpeg"sv);
        return file_path;
    }
};

int main(int argc, char *argv[]) {
    // Copy args into vector for easier access.
    const std::vector<std::string_view> args(argv + 1, argv + argc);

    // Parse argument list as key-value pairs and place into map.
    std::map<std::string_view, std::string_view> arg_map;
    for (auto arg = args.begin(); arg != args.end(); arg++) {
        std::string_view key = *arg;
        if (key.starts_with("--")) {
            key = key.substr(2);
        } else if (key.starts_with("-") || key.starts_with("/")) {
            key = key.substr(1);
        } else {
            throw std::runtime_error(fmt::format("unexpected value \"{}\"", key));
        }

        if (arg + 1 == args.end()) {
            throw std::runtime_error(fmt::format("no value for key \"{}\"", key));
        }
        arg += 1;
        const std::string_view value = *arg;

        arg_map.emplace(key, value);
    }

    // Check provider and create corresponding url builder
    if (!arg_map.contains("provider")) {
        throw std::runtime_error("no tile provider given");
    }
    const std::string_view provider = arg_map.at("provider");

    std::unique_ptr<TileUrlBuilder> url_builder;
    if (string_equals_ignore_case(provider, "basemap")) {
        url_builder = std::make_unique<BasemapTileUrlBuilder>(arg_map);
    } else if (string_equals_ignore_case(provider, "gataki")) {
        url_builder = std::make_unique<GatakiTileUrlBuilder>(arg_map);
    } else {
        throw std::runtime_error(fmt::format("unsupported tile provider \"{}\"", provider));
    }

    // Parse spatial reference system and scheme
    const unsigned int srs = svtoui(map_get_or_default(arg_map, "srs"sv, "3857"sv));
    const std::string_view scheme_str = map_get_or_default(arg_map, "scheme"sv, "SlippyMap"sv);
    tile::Scheme scheme;
    if (string_equals_ignore_case(scheme_str, "SlippyMap") || string_equals_ignore_case(scheme_str, "Google") || string_equals_ignore_case(scheme_str, "XYZ")) {
        scheme = tile::Scheme::SlippyMap;
    } else if (string_equals_ignore_case(scheme_str, "TMS")) {
        scheme = tile::Scheme::Tms;
    } else {
        throw std::runtime_error(fmt::format("unsupported srs scheme \"{}\"", scheme_str));
    }

    if (srs != static_cast<int>(ctb::Grid::Srs::SphericalMercator)) {
        throw std::runtime_error(fmt::format("unsupported srs EPSG \"{}\"", srs));
    }

    if (arg_map.contains("verbosity")) {
        verbosity = svtoui(arg_map["verbosity"]);
    }

    // Construct root tile
    const unsigned int zoom = svtoui(map_get_required(arg_map, "zoom"));
    const unsigned int x = svtoui(map_get_required(arg_map, "x"));
    const unsigned int y = svtoui(map_get_required(arg_map, "y"));
    const tile::Id root_id = {zoom, {x, y}, scheme};

    // Download tile and subtiles recursively.
    TileDownloader downloader(url_builder.get(), arg_map);
    downloader.download_tile_by_id_recursive(root_id);
}
