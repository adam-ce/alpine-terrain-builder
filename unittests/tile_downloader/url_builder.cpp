#include <catch2/catch_test_macros.hpp>

#include "TileUrlBuilder.h"
#include "tile_path.h"

namespace {
constexpr radix::tile::Id tile { 3, { 1, 2 } };

void check_urls(const TileUrlFormat format, const std::string& coordinate_path)
{
    const BasemapTileUrlBuilder basemap("layer", "style", format);
    CHECK(basemap.build_url(tile) == "https://mapsneu.wien.gv.at/basemap/layer/style/google3857/" + coordinate_path + ".jpeg");

    const GatakiTileUrlBuilder gataki(format);
    CHECK(gataki.build_url(tile) == "https://gataki.cg.tuwien.ac.at/raw/basemap/tiles/" + coordinate_path + ".jpeg");
}
}

TEST_CASE("tile URL coordinate formats")
{
    SECTION("xy with downward y")
    {
        check_urls({ TileCoordinateOrder::Xy, TileYDirection::Down }, "3/1/2");
    }

    SECTION("yx with downward y")
    {
        check_urls({ TileCoordinateOrder::Yx, TileYDirection::Down }, "3/2/1");
    }

    SECTION("xy with upward legacy TMS y")
    {
        check_urls({ TileCoordinateOrder::Xy, TileYDirection::Up }, "3/1/5");
    }

    SECTION("yx with upward legacy TMS y")
    {
        check_urls({ TileCoordinateOrder::Yx, TileYDirection::Up }, "3/5/1");
    }
}

TEST_CASE("downloaded tile path uses Google and Mapbox layout")
{
    CHECK(google_tile_path("tiles", tile, ".jpeg") == std::filesystem::path("tiles/3/1/2.jpeg"));
}
