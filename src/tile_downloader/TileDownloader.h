#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <fmt/core.h>
#include <radix/tile.h>

#include "HttpClient.h"
#include "TileLogger.h"
#include "TileUrlBuilder.h"
#include "tile_path.h"
#include "write_file.h"

class TileDownloader {
public:
    TileDownloader(const TileUrlBuilder &url_builder, std::filesystem::path output_directory,
                   std::optional<unsigned int> max_zoom_level, unsigned root_zoom_level)
        : _url_builder(url_builder),
          _output_directory(std::move(output_directory)),
          _logger(root_zoom_level),
          _max_zoom_level(max_zoom_level) {}

    [[nodiscard]] bool download_recursive(const radix::tile::Id &root_id) {
        auto progress_session = this->_logger.start();
        const bool complete = this->download_recursive_core(root_id);
        progress_session.finish();
        return complete;
    }

private:
    const TileUrlBuilder &_url_builder;
    std::filesystem::path _output_directory;
    HttpClient _http;
    TileLogger _logger;
    std::optional<unsigned int> _max_zoom_level;

    [[nodiscard]] bool download_recursive_core(const radix::tile::Id &root_id) {
        auto result = this->download_tile(root_id);
        this->_logger.report_error(root_id, result);

        if (std::holds_alternative<TileResult::Skipped>(result)) {
            this->_logger.skipped(root_id);
            return true;
        }

        if (std::holds_alternative<TileResult::Absent>(result)) {
            this->_logger.missing(root_id);
            return true;
        }

        if (is_failure(result)) {
            this->_logger.missing(root_id);
            return false;
        }

        const auto children = root_id.children();
        bool children_complete = true;
        for (size_t i = 0; i < children.size(); i++) {
            if (this->_max_zoom_level.has_value() && children[i].zoom_level > *this->_max_zoom_level) {
                this->_logger.skipped(children[i]);
                continue;
            }

            if (!this->download_recursive_core(children[i])) {
                children_complete = false;
            }
        }

        if (!children_complete) {
            return false;
        }

        mark_tile_children_complete(this->tile_path(root_id));
        return true;
    }

    static bool is_failure(const TileResult::Status &result) {
        return std::holds_alternative<TileResult::HttpError>(result)
            || std::holds_alternative<TileResult::BadContentType>(result)
            || std::holds_alternative<TileResult::CurlError>(result)
            || std::holds_alternative<TileResult::TimedOut>(result);
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

    TileResult::Status download_tile(const radix::tile::Id &tile) {
        const auto path = std::filesystem::absolute(this->tile_path(tile));

        if (std::filesystem::exists(path)) {
            return TileResult::Skipped{};
        }
        if (std::filesystem::exists(children_pending_tile_path(path))) {
            return TileResult::ChildrenPending{};
        }

        ensure_parent_dirs(path);

        const std::string url = this->_url_builder.build_url(tile);

        for (int attempt = 0; attempt < 100; attempt++) {
            HttpResponse response = this->_http.get(url);

            if (response.curl_code == CURLE_OK && this->_http.is_image(response)) {
                write_file_children_pending(path, response.body);
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
