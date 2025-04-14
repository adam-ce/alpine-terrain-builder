#ifndef TILEPROVIDER_H
#define TILEPROVIDER_H

#include <filesystem>
#include <optional>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>
#include <radix/tile.h>

class TileProvider {
public:
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
        if (tile_path.has_value() && TilePathProvider::is_usable_tile_path(tile_path.value())) {
            return cv::imread(tile_path.value());
        }
        return std::nullopt;
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
        return !tile_path.empty() && std::filesystem::exists(tile_path);
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

#endif
