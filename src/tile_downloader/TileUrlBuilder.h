#pragma once

#include <string>

#include <fmt/core.h>
#include <radix/tile.h>

class TileUrlBuilder {
public:
    virtual ~TileUrlBuilder() = default;
    virtual std::string build_url(const radix::tile::Id &tile_id) const = 0;
};

class BasemapTileUrlBuilder : public TileUrlBuilder {
public:
    BasemapTileUrlBuilder(std::string layer, std::string style)
        : _layer(std::move(layer)), _style(std::move(style)) {}

    std::string build_url(const radix::tile::Id &tile_id) const override {
        const auto t = tile_id.to(radix::tile::Scheme::SlippyMap);
        return fmt::format(
            "https://mapsneu.wien.gv.at/basemap/{}/{}/google3857/{}/{}/{}.jpeg",
            this->_layer, this->_style, t.zoom_level, t.coords.y, t.coords.x);
    }

private:
    std::string _layer;
    std::string _style;
};

class GatakiTileUrlBuilder : public TileUrlBuilder {
public:
    std::string build_url(const radix::tile::Id &tile_id) const override {
        const auto t = tile_id.to(radix::tile::Scheme::SlippyMap);
        return fmt::format(
            "https://gataki.cg.tuwien.ac.at/raw/basemap/tiles/{}/{}/{}.jpeg",
            t.zoom_level, t.coords.y, t.coords.x);
    }
};
