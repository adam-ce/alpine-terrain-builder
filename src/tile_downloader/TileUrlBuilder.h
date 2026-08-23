#pragma once

#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <radix/tile.h>

enum class TileDownloadProvider {
    Basemap,
    Gataki,
};

enum class TileYDirection {
    Down,
    Up,
};

struct TileProviderConfig {
    std::string url_pattern;
    TileYDirection y_direction = TileYDirection::Down;
};

[[nodiscard]] inline TileProviderConfig tile_provider_config(TileDownloadProvider provider)
{
    switch (provider) {
    case TileDownloadProvider::Basemap:
        return {
            "https://mapsneu.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/{zoom}/{y}/{x}.jpeg",
            TileYDirection::Down
        };
    case TileDownloadProvider::Gataki:
        return {
            "https://gataki.cg.tuwien.ac.at/raw/basemap/tiles/{zoom}/{y}/{x}.jpeg",
            TileYDirection::Down
        };
    }

    throw std::invalid_argument("unknown tile download provider");
}

class TileUrlBuilder {
public:
    explicit TileUrlBuilder(TileProviderConfig config)
        : _url_pattern(std::move(config.url_pattern))
        , _y_direction(config.y_direction)
    {
        if (_url_pattern.find("{zoom}") == std::string::npos
            || _url_pattern.find("{x}") == std::string::npos
            || _url_pattern.find("{y}") == std::string::npos) {
            throw std::invalid_argument("tile URL pattern must contain {zoom}, {x}, and {y}");
        }
    }

    [[nodiscard]] std::string build_url(const radix::tile::Id& tile_id) const
    {
        auto y = tile_id.coords.y;
        if (_y_direction == TileYDirection::Up) {
            if (tile_id.zoom_level >= std::numeric_limits<unsigned>::digits)
                throw std::invalid_argument("tile zoom level is too large for legacy TMS coordinates");
            y = (1u << tile_id.zoom_level) - y - 1;
        }

        auto url = _url_pattern;
        replace_all(url, "{zoom}", std::to_string(tile_id.zoom_level));
        replace_all(url, "{x}", std::to_string(tile_id.coords.x));
        replace_all(url, "{y}", std::to_string(y));
        return url;
    }

private:
    static void replace_all(std::string& value, std::string_view placeholder, const std::string& replacement)
    {
        size_t position = 0;
        while ((position = value.find(placeholder, position)) != std::string::npos) {
            value.replace(position, placeholder.size(), replacement);
            position += replacement.size();
        }
    }

    std::string _url_pattern;
    TileYDirection _y_direction;
};
