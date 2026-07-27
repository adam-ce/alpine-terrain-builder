#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>

#include <curl/curl.h>
#include <fmt/core.h>
#include <radix/tile.h>

#include "ProgressIndicator.h"
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
    class Session;

    explicit TileLogger(unsigned root_zoom_level)
        : _root_zoom_level(root_zoom_level), _progress(PROGRESS_RESOLUTION) {}

    [[nodiscard]] Session start();

    // The tile turned out not to exist (or errored out), so its branch stops here.
    void missing(const radix::tile::Id &tile) {
        this->close_branch(tile);
    }

    // We chose not to descend into this tile's branch (already on disk, or past --max-zoom-level).
    void skipped(const radix::tile::Id &tile) {
        this->close_branch(tile);
    }

    void report_error(const radix::tile::Id &tile, const TileResult::Status &status) const {
        std::visit([&](const auto &r) {
            using T = std::decay_t<decltype(r)>;
            if constexpr (std::is_same_v<T, TileResult::HttpError>) {
                LOG_WARN("{}: HTTP error [{}]", format(tile), r.status_code);
            } else if constexpr (std::is_same_v<T, TileResult::BadContentType>) {
                LOG_WARN("{}: bad content type", format(tile));
            } else if constexpr (std::is_same_v<T, TileResult::CurlError>) {
                LOG_WARN("{}: cURL error [{}] {}", format(tile), unsigned(r.code), curl_easy_strerror(r.code));
            } else if constexpr (std::is_same_v<T, TileResult::TimedOut>) {
                LOG_WARN("{}: failed after 100 attempts", format(tile));
            }
        }, status);
    }

private:
    static constexpr size_t PROGRESS_RESOLUTION = 10'000;

    unsigned _root_zoom_level;
    ProgressIndicator _progress;
    std::jthread _progress_thread;
    size_t _steps_done = 0;
    std::map<unsigned, uint32_t> _level_counts;

    void start_monitoring() {
        this->_progress_thread = this->_progress.start_monitoring();
    }

    void finish_monitoring() {
        while (this->_steps_done < PROGRESS_RESOLUTION) {
            this->_progress.task_finished();
            this->_steps_done++;
        }
        if (this->_progress_thread.joinable()) {
            this->_progress_thread.join();
        }
    }

    void cancel_monitoring() noexcept {
        if (!this->_progress_thread.joinable()) {
            return;
        }

        this->_progress_thread.request_stop();
        try {
            this->_progress_thread.join();
        } catch (...) {
            // Session cleanup must not replace the active exception.
        }
    }

    static std::string format(const radix::tile::Id &tile) {
        return fmt::format("Tile[Zoom={}, X={}, Y={}]", tile.zoom_level, tile.coords.x, tile.coords.y);
    }

    // Every tile at `level` covers an equal 4^-(level - root) share of the whole
    // download tree, so summing that share for every closed branch gives the
    // fraction of the tree that is done.
    double completed_fraction() const {
        double fraction = 0.0;
        for (const auto &[level, count] : this->_level_counts) {
            fraction += static_cast<double>(count) / std::pow(4.0, static_cast<double>(level - this->_root_zoom_level));
        }
        return fraction;
    }

    void close_branch(const radix::tile::Id &tile) {
        this->_level_counts[tile.zoom_level]++;
        this->advance_progress();
    }

    void advance_progress() {
        const auto target = std::min(PROGRESS_RESOLUTION,
                                      static_cast<size_t>(this->completed_fraction() * double(PROGRESS_RESOLUTION)));
        while (this->_steps_done < target) {
            this->_progress.task_finished();
            this->_steps_done++;
        }
    }
};

class TileLogger::Session {
public:
    explicit Session(TileLogger &logger)
        : _logger(&logger) {
        this->_logger->start_monitoring();
    }

    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    Session(Session &&other) noexcept
        : _logger(std::exchange(other._logger, nullptr)) {}

    Session &operator=(Session &&) = delete;

    ~Session() {
        if (this->_logger) {
            this->_logger->cancel_monitoring();
        }
    }

    void finish() {
        this->_logger->finish_monitoring();
        this->_logger = nullptr;
    }

private:
    TileLogger *_logger;
};

inline TileLogger::Session TileLogger::start() {
    return Session(*this);
}
