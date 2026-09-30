/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek-Litofcenko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include <catch2/catch_test_macros.hpp>

#include "TileUrlBuilder.h"
#include "tile_path.h"

namespace {
constexpr radix::tile::Id tile { 3, { 1, 2 } };
}

TEST_CASE("configured tile provider URLs")
{
    {
        const TileUrlBuilder builder(tile_provider_config(TileDownloadProvider::Basemap));
        CHECK(builder.build_url(tile) == "https://mapsneu.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/3/2/1.jpeg");
    }

    {
        const TileUrlBuilder builder(tile_provider_config(TileDownloadProvider::Gataki));
        CHECK(builder.build_url(tile) == "https://gataki.cg.tuwien.ac.at/raw/basemap/tiles/3/2/1.jpeg");
    }
}

TEST_CASE("custom tile URL patterns")
{
    SECTION("zoom/x/y with downward y")
    {
        const TileUrlBuilder builder({ "https://example.test/{zoom}/{x}/{y}.png", TileYDirection::Down });
        CHECK(builder.build_url(tile) == "https://example.test/3/1/2.png");
    }

    SECTION("zoom/y/x with downward y")
    {
        const TileUrlBuilder builder({ "https://example.test/{zoom}/{y}/{x}.png", TileYDirection::Down });
        CHECK(builder.build_url(tile) == "https://example.test/3/2/1.png");
    }

    SECTION("upward legacy TMS y")
    {
        const TileUrlBuilder builder({ "https://example.test/{zoom}/{x}/{y}.png", TileYDirection::Up });
        CHECK(builder.build_url(tile) == "https://example.test/3/1/5.png");
    }
}

TEST_CASE("tile URL patterns require all coordinate placeholders")
{
    CHECK_THROWS_AS(TileUrlBuilder({ "https://example.test/{x}/{y}.png", TileYDirection::Down }), std::invalid_argument);
    CHECK_THROWS_AS(TileUrlBuilder({ "https://example.test/{zoom}/{y}.png", TileYDirection::Down }), std::invalid_argument);
    CHECK_THROWS_AS(TileUrlBuilder({ "https://example.test/{zoom}/{x}.png", TileYDirection::Down }), std::invalid_argument);
}

TEST_CASE("downloaded tile path uses Google and Mapbox layout")
{
    CHECK(google_tile_path("tiles", tile, ".jpeg") == std::filesystem::path("tiles/3/1/2.jpeg"));
}
