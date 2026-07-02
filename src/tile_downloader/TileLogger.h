#pragma once

#include <string>
#include <string_view>
#include <variant>

#include <curl/curl.h>
#include <fmt/core.h>
#include <radix/tile.h>

#include <spdlog/spdlog.h>

#include "log.h"

struct TileResult {
    struct Downloaded {};
    struct Skipped {};
    struct Absent {};
    struct HttpError { long status_code; };
    struct BadContentType {};
    struct CurlError { CURLcode code; };
    struct TimedOut {};

    using Status = std::variant<Downloaded, Skipped, Absent, HttpError, BadContentType, CurlError, TimedOut>;
};

class TileLogger {
public:
    void progress(const radix::tile::Id &tile, std::string_view status) const {
        if (Log::get_logger()->level() <= spdlog::level::info) {
            fmt::print("\33[2K\r{} ({})", this->format(tile), status);
            std::fflush(nullptr);
        }
    }

    void progress(const radix::tile::Id &tile, double fraction) const {
        if (fraction < 0) {
            this->progress(tile, "Downloading...");
        } else {
            this->progress(tile, fmt::format("Downloading {:.0f}%...", fraction * 100));
        }
    }

    void skipped(const radix::tile::Id &tile) const {
        this->final(tile, "Skipped");
    }

    void result(const radix::tile::Id &tile, const TileResult::Status &status) const {
        std::visit([&](const auto &r) {
            using T = std::decay_t<decltype(r)>;
            if constexpr (std::is_same_v<T, TileResult::Downloaded>) {
                this->final(tile, "Done");
            } else if constexpr (std::is_same_v<T, TileResult::Skipped>) {
                this->final(tile, "Skipped");
            } else if constexpr (std::is_same_v<T, TileResult::Absent>) {
                this->final(tile, "Absent");
            } else if constexpr (std::is_same_v<T, TileResult::HttpError>) {
                this->final(tile, fmt::format("Error HTTP [{}]", r.status_code));
            } else if constexpr (std::is_same_v<T, TileResult::BadContentType>) {
                this->final(tile, "Error, bad content type.");
            } else if constexpr (std::is_same_v<T, TileResult::CurlError>) {
                this->final(tile, fmt::format("Error cURL [{}] {}", unsigned(r.code), curl_easy_strerror(r.code)));
            } else if constexpr (std::is_same_v<T, TileResult::TimedOut>) {
                this->final(tile, "Failed after 100 attempts");
            }
        }, status);
    }

private:
    static std::string format(const radix::tile::Id &tile) {
        return fmt::format("Tile[Zoom={}, X={}, Y={}]", tile.zoom_level, tile.coords.x, tile.coords.y);
    }

    void final(const radix::tile::Id &tile, std::string_view status) const {
        if (Log::get_logger()->level() <= spdlog::level::info) {
            fmt::print("\33[2K\r{} ({})\n", this->format(tile), status);
            std::fflush(nullptr);
        }
    }
};
