#pragma once

#include <filesystem>
#include <optional>

#include <opencv2/opencv.hpp>
#include <radix/tile.h>

class TileProvider {
public:
    virtual ~TileProvider() = default;
    virtual std::optional<cv::Mat> get_tile(const radix::tile::Id tile) const = 0;
    virtual bool has_tile(const radix::tile::Id tile) const {
        return this->get_tile(tile).has_value();
    }
};

class TilePathProvider : public TileProvider {
public:
    virtual std::optional<std::filesystem::path> get_tile_path(const radix::tile::Id tile) const = 0;

    std::optional<cv::Mat> get_tile(const radix::tile::Id tile) const override {
        const std::optional<std::filesystem::path> tile_path = this->get_tile_path(tile);
        if (!tile_path.has_value() && !TilePathProvider::is_usable_tile_path(tile_path.value())) {
            return std::nullopt;
        }
        auto tile_img = cv::imread(tile_path.value());
        if (tile_img.empty()) {
            return std::nullopt;
        }
        return tile_img;
    }
    virtual bool has_tile(const radix::tile::Id tile) const override {
        const std::optional<std::filesystem::path> tile_path = this->get_tile_path(tile);
        return tile_path.has_value() && TilePathProvider::is_usable_tile_path(tile_path.value());
    }

    virtual std::optional<cv::Mat> load_tile_from_path(const std::filesystem::path& tile_path) const {
        if (TilePathProvider::is_usable_tile_path(tile_path)) {
            return cv::imread(tile_path);
        }
        return std::nullopt;
    }

private:
    static bool is_usable_tile_path(const std::filesystem::path &tile_path) {
        return !tile_path.empty() && std::filesystem::is_regular_file(tile_path);
    }
};

class StaticTileProvider : public TileProvider {
public:
    std::unordered_map<radix::tile::Id, cv::Mat, radix::tile::Id::Hasher> tiles;

    StaticTileProvider(const std::unordered_map<radix::tile::Id, cv::Mat, radix::tile::Id::Hasher>& tiles) {
        // TODO: remove this once the == operator of tile::Id is updated.
        for (const auto& tile : tiles) {
            const radix::tile::Id tile_id = tile.first.to(radix::tile::Scheme::SlippyMap);
            this->tiles[tile_id] = tile.second;
        }
    }

    virtual std::optional<cv::Mat> get_tile(const radix::tile::Id tile_id) const override {
        const auto tile = this->tiles.find(tile_id.to(radix::tile::Scheme::SlippyMap));
        if (tile != this->tiles.end()) {
            return tile->second;
        } else {
            return std::nullopt;
        }
    }

    virtual bool has_tile(const radix::tile::Id tile_id) const override {
        return this->tiles.find(tile_id.to(radix::tile::Scheme::SlippyMap)) != this->tiles.cend();
    }
};

class EmptyTileProvider final : public TileProvider {
public:
    virtual std::optional<cv::Mat> get_tile(const radix::tile::Id) const override {
        return std::nullopt;
    }

    virtual bool has_tile(const radix::tile::Id) const override {
        return false;
    }
};

template <typename Inner>
class ZoomRangeTileProvider final : public TileProvider {
public:
    template <typename P>
    ZoomRangeTileProvider(
        P &&inner,
        std::optional<uint32_t> min_zoom,
        std::optional<uint32_t> max_zoom)
        : _inner(std::move(inner)),
          _min_zoom(min_zoom.value_or(std::numeric_limits<uint32_t>::min())),
          _max_zoom(max_zoom.value_or(std::numeric_limits<uint32_t>::max())) {}

    std::optional<cv::Mat> get_tile(const radix::tile::Id tile_id) const override {
        if (!this->is_tile_in_range(tile_id)) {
            return std::nullopt;
        }

        return this->inner().get_tile(tile_id);
    }

    bool has_tile(const radix::tile::Id tile_id) const override {
        if (!this->is_tile_in_range(tile_id)) {
            return false;
        }

        return this->inner().has_tile(tile_id);
    }

private:
    bool is_tile_in_range(const radix::tile::Id tile_id) const {
        if (tile_id.zoom_level < this->_min_zoom) {
            return false;
        }

        if (tile_id.zoom_level > this->_max_zoom) {
            return false;
        }

        return true;
    }

    const TileProvider &inner() const {
        if constexpr (requires { *this->_inner; }) { // pointers
            return *this->_inner;
        } else { // value
            return this->_inner;
        }
    }

    Inner _inner;
    uint32_t _min_zoom;
    uint32_t _max_zoom;
};

class BasemapSchemeTilePathProvider : public TilePathProvider {
public:
    BasemapSchemeTilePathProvider(std::filesystem::path base_path)
        : base_path(base_path) {}

    std::optional<std::filesystem::path> get_tile_path(const radix::tile::Id tile_id) const override {
        return base_path / std::to_string(tile_id.zoom_level) / std::to_string(tile_id.coords.y) / (std::to_string(tile_id.coords.x) + ".jpeg");
    }

private:
    std::filesystem::path base_path;
};
