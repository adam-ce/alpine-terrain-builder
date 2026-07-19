#pragma once

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include <fmt/core.h>
#include <radix/tile.h>

#include "HttpClient.h"
#include "TileLogger.h"
#include "TileUrlBuilder.h"
#include "tile_path.h"

class TileDownloader {
public:
    TileDownloader(const TileUrlBuilder &url_builder, std::filesystem::path output_directory,
                   bool early_skip, std::optional<unsigned int> max_zoom_level)
        : _url_builder(url_builder),
          _output_directory(std::move(output_directory)),
          _early_skip(early_skip),
          _max_zoom_level(max_zoom_level) {}

    void download_recursive(const radix::tile::Id &root_id) {
        this->_logger.progress(root_id, "Connecting...");
        auto result = this->download_tile(root_id);
        this->_logger.result(root_id, result);

        if (is_failure(result)) {
            return;
        }

        const auto children = root_id.children();
        for (size_t i = 0; i < children.size(); i++) {
            if (this->_early_skip && i + 1 < children.size()) {
                if (this->tile_exists(children[i + 1])) {
                    this->_logger.skipped(children[i]);
                    continue;
                }
            }

            if (this->_max_zoom_level.has_value() && children[i].zoom_level > *this->_max_zoom_level) {
                this->_logger.skipped(children[i]);
                continue;
            }

            this->download_recursive(children[i]);
        }
    }

private:
    const TileUrlBuilder &_url_builder;
    std::filesystem::path _output_directory;
    HttpClient _http;
    TileLogger _logger;
    bool _early_skip;
    std::optional<unsigned int> _max_zoom_level;

    static bool is_failure(const TileResult::Status &result) {
        return std::holds_alternative<TileResult::Absent>(result)
            || std::holds_alternative<TileResult::HttpError>(result)
            || std::holds_alternative<TileResult::BadContentType>(result)
            || std::holds_alternative<TileResult::CurlError>(result)
            || std::holds_alternative<TileResult::TimedOut>(result);
    }

    bool tile_exists(const radix::tile::Id &tile) const {
        return std::filesystem::exists(this->tile_path(tile));
    }

    std::filesystem::path tile_path(const radix::tile::Id &tile) const {
        return google_tile_path(_output_directory, tile, ".jpeg");
    }

    static void ensure_parent_dirs(const std::filesystem::path &path) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec && !std::filesystem::exists(path.parent_path())) {
            throw std::runtime_error(fmt::format("failed to create directories \"{}\"", path.parent_path().string()));
        }
    }

    static void write_file(const std::filesystem::path &path, const std::vector<char> &data) {
        std::ofstream out(path, std::ios::binary);
        if (!out) {
            throw std::runtime_error(fmt::format("failed to open \"{}\" for writing", path.string()));
        }
        out.write(data.data(), data.size());
    }

    TileResult::Status download_tile(const radix::tile::Id &tile) {
        const auto path = std::filesystem::absolute(this->tile_path(tile));

        if (std::filesystem::exists(path)) {
            return TileResult::Skipped{};
        }

        ensure_parent_dirs(path);

        const std::string url = this->_url_builder.build_url(tile);
        auto progress_fn = [&](double fraction) {
            this->_logger.progress(tile, fraction);
        };

        for (int attempt = 0; attempt < 100; attempt++) {
            HttpResponse response = this->_http.get(url, progress_fn);

            if (response.curl_code == CURLE_OK && this->_http.is_image(response)) {
                write_file(path, response.body);
                return TileResult::Downloaded{};
            }

            if (response.curl_code == CURLE_HTTP_RETURNED_ERROR) {
                if (response.status_code == 404) {
                    return TileResult::Absent{};
                }
                return TileResult::HttpError{response.status_code};
            }

            if (response.curl_code == CURLE_OPERATION_TIMEDOUT) {
                continue;
            }

            if (response.curl_code == CURLE_OK) {
                return TileResult::BadContentType{};
            }

            return TileResult::CurlError{response.curl_code};
        }

        return TileResult::TimedOut{};
    }
};
