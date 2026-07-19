#pragma once

#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

#include <fmt/core.h>
#include <radix/tile.h>

enum class TileCoordinateOrder {
    Xy,
    Yx
};

enum class TileYDirection {
    Down,
    Up
};

struct TileUrlFormat {
    TileCoordinateOrder coordinate_order = TileCoordinateOrder::Xy;
    TileYDirection y_direction = TileYDirection::Down;

    [[nodiscard]] std::pair<unsigned, unsigned> coordinates(const radix::tile::Id& tile_id) const
    {
        auto y = tile_id.coords.y;
        if (y_direction == TileYDirection::Up) {
            if (tile_id.zoom_level >= std::numeric_limits<unsigned>::digits)
                throw std::invalid_argument("tile zoom level is too large for legacy TMS coordinates");
            y = (1u << tile_id.zoom_level) - y - 1;
        }

        if (coordinate_order == TileCoordinateOrder::Xy)
            return { tile_id.coords.x, y };
        return { y, tile_id.coords.x };
    }
};

class TileUrlBuilder {
public:
    virtual ~TileUrlBuilder() = default;
    virtual std::string build_url(const radix::tile::Id &tile_id) const = 0;
};

class BasemapTileUrlBuilder : public TileUrlBuilder {
public:
    BasemapTileUrlBuilder(std::string layer, std::string style, TileUrlFormat format = {})
        : _layer(std::move(layer))
        , _style(std::move(style))
        , _format(format)
    {
    }

    std::string build_url(const radix::tile::Id &tile_id) const override {
        const auto [first_coordinate, second_coordinate] = _format.coordinates(tile_id);
        return fmt::format(
            "https://mapsneu.wien.gv.at/basemap/{}/{}/google3857/{}/{}/{}.jpeg",
            _layer, _style, tile_id.zoom_level, first_coordinate, second_coordinate);
    }

private:
    std::string _layer;
    std::string _style;
    TileUrlFormat _format;
};

class GatakiTileUrlBuilder : public TileUrlBuilder {
public:
    explicit GatakiTileUrlBuilder(TileUrlFormat format = {})
        : _format(format)
    {
    }

    std::string build_url(const radix::tile::Id &tile_id) const override {
        const auto [first_coordinate, second_coordinate] = _format.coordinates(tile_id);
        return fmt::format(
            "https://gataki.cg.tuwien.ac.at/raw/basemap/tiles/{}/{}/{}.jpeg",
            tile_id.zoom_level, first_coordinate, second_coordinate);
    }

private:
    TileUrlFormat _format;
};
