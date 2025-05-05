/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 Adam Celarek
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

#include <set>

#include "Dataset.h"
#include "DatasetReader.h"
#include "TopDownTiler.h"
#include "ctb/GlobalMercator.hpp"
#include "depth_first_tile_traverser.h"
#include <radix/tile.h>

using namespace radix;

TEST_CASE("depth_first_tile_traverser interface")
{
    struct ReadType { };
    const auto read_function = [&](tile::Descriptor) { return ReadType {}; };

    const auto aggregate_function = [](std::vector<ReadType>) { return ReadType {}; };

    const auto grid = ctb::GlobalMercator();
    const auto tiler = TopDownTiler(grid, grid.getExtent(), tile::Border::No, tile::Scheme::Tms);
    const tile::Id root_id = { 0, { 0, 0 }, tiler.scheme() };
    const unsigned max_zoom_level = 3;

    ReadType result = traverse_depth_first_and_aggregate(tiler, read_function, aggregate_function, root_id, max_zoom_level);
}

TEST_CASE("depth_first_tile_traverser basics")
{
    struct ReadType {
        glm::uvec2 d;
    };
    std::set<tile::Id> read_tiles;
    const auto read_function = [&](const tile::Descriptor& tile) {
        read_tiles.insert(tile.id);
        return ReadType {tile.id.coords};
    };

    std::vector<std::vector<ReadType>> aggregate_calls;
    const auto aggregate_function = [&](const std::vector<ReadType>& data) {
        aggregate_calls.push_back(data);
        REQUIRE(data.size() > 0);
        ReadType aggr = data.front();
        for (const auto& d : data) {
            aggr.d.x = std::min(aggr.d.x, d.d.x);
            aggr.d.y = std::max(aggr.d.y, d.d.y);
        }
        return aggr;
    };

    const auto grid = ctb::GlobalMercator();
    const auto tiler = TopDownTiler(grid, grid.getExtent(), tile::Border::No, tile::Scheme::Tms);
    const tile::Id root_id = { 0, { 0, 0 }, tiler.scheme() };

    SECTION("reads root tile #1")
    {
        const auto result = traverse_depth_first_and_aggregate(tiler, read_function, aggregate_function, root_id, 0);
        REQUIRE(read_tiles.size() == 1);
        CHECK(aggregate_calls.size() == 0);
        CHECK(read_tiles.contains(tile::Id{0, {0, 0}, tiler.scheme()}));
        CHECK(result.d == glm::uvec2 { 0, 0 });
    }

    SECTION("reads root tile #2")
    {
        const auto result = traverse_depth_first_and_aggregate(tiler, read_function, aggregate_function, { 2, { 1, 3 } }, 2);
        REQUIRE(read_tiles.size() == 1);
        CHECK(read_tiles.contains(tile::Id{2, {1, 3}, tiler.scheme()}));
        CHECK(result.d == glm::uvec2 { 1, 3 });
    }

    SECTION("reads only leaf tiles #1")
    {
        traverse_depth_first_and_aggregate(tiler, read_function, aggregate_function, { 0, { 0, 0 } }, 1);
        REQUIRE(read_tiles.size() == 4);
        CHECK(read_tiles.contains(tile::Id{1, {0, 0}, tiler.scheme()}));
        CHECK(read_tiles.contains(tile::Id{1, {0, 1}, tiler.scheme()}));
        CHECK(read_tiles.contains(tile::Id{1, {1, 0}, tiler.scheme()}));
        CHECK(read_tiles.contains(tile::Id{1, {1, 1}, tiler.scheme()}));
    }

    SECTION("aggregate is called correctly")
    {
        const auto result = traverse_depth_first_and_aggregate(tiler, read_function, aggregate_function, { 0, { 0, 0 } }, 1);
        REQUIRE(aggregate_calls.size() == 1);
        REQUIRE(aggregate_calls[0].size() == 4);
        REQUIRE(aggregate_calls[0][0].d == glm::uvec2 { 0, 0 });
        REQUIRE(aggregate_calls[0][1].d == glm::uvec2 { 1, 0 });
        REQUIRE(aggregate_calls[0][2].d == glm::uvec2 { 0, 1 });
        REQUIRE(aggregate_calls[0][3].d == glm::uvec2 { 1, 1 });
        CHECK(result.d.x == 0);
        CHECK(result.d.y == 1);
    }
}


TEST_CASE("depth_first_tile_traverser austrian heights")
{
    const auto grid = ctb::GlobalMercator();
    const auto dataset = Dataset::make_shared(ATB_TEST_DATA_DIR "/austria/at_100m_mgi.tif");
    //    const auto dataset = Dataset::make_shared(ATB_TEST_DATA_DIR "/austria/at_mgi.tif");
    const auto bounds = dataset->bounds(grid.getSRS());
    const auto tiler = TopDownTiler(grid, bounds, tile::Border::No, tile::Scheme::Tms);
    const auto tile_reader = DatasetReader(dataset, grid.getSRS(), 1, false);
    //    const auto dataset_reader = DatasetReader()
    std::set<tile::Id> read_tiles;
    const auto read_function = [&](const tile::Descriptor& tile) -> std::pair<float, float> {
        read_tiles.insert(tile.id);
        const auto tile_data = tile_reader.read(tile.srsBounds, tile.tileSize, tile.tileSize);
        auto [min, max] = std::ranges::minmax(tile_data);
        return std::make_pair(min, max);
    };

    std::vector<std::vector<std::pair<float, float>>> aggregate_calls;
    const auto aggregate_function = [&](const std::vector<std::pair<float, float>>& data) {
        aggregate_calls.push_back(data);
        REQUIRE(data.size() > 0);
        auto aggr = data.front();
        for (const auto& d : data) {
            aggr.first = std::min(aggr.first, d.first);
            aggr.second = std::max(aggr.second, d.second);
        }
        return aggr;
    };

    const tile::Id root_id = { 0, { 0, 0 }, tiler.scheme() };

    SECTION("reads root tile")
    {
        const auto result = traverse_depth_first_and_aggregate(tiler, read_function, aggregate_function, root_id, 0);
        REQUIRE(read_tiles.size() == 1);
        CHECK(aggregate_calls.size() == 0);
        CHECK(read_tiles.contains(tile::Id{0, {0, 0}, tiler.scheme()}));
        CHECK(result.first >= 0);
        CHECK(result.first <= 4000);
        CHECK(result.second >= 0);
        CHECK(result.second <= 4000);
    }

    SECTION("aggregate is called correctly")
    {
        const auto result = traverse_depth_first_and_aggregate(tiler, read_function, aggregate_function, root_id, 6);
        CHECK(read_tiles.size() == 6);
        CHECK(aggregate_calls.size() == 9);
        CHECK(read_tiles.contains(tile::Id{6, {33, 41}, tiler.scheme()}));
        CHECK(read_tiles.contains(tile::Id{6, {33, 42}, tiler.scheme()}));

        CHECK(read_tiles.contains(tile::Id{6, {34, 41}, tiler.scheme()}));
        CHECK(read_tiles.contains(tile::Id{6, {34, 42}, tiler.scheme()}));

        CHECK(read_tiles.contains(tile::Id{6, {35, 41}, tiler.scheme()}));
        CHECK(read_tiles.contains(tile::Id{6, {35, 42}, tiler.scheme()}));
        CHECK(result.first >= 0);
        CHECK(result.first <= 500);
        CHECK(result.second >= 2000);
        CHECK(result.second <= 4000);
    }
}
TEST_CASE("depth_first_tile_traverser aggregate is not called with an empty vector")
{
    SECTION("web mercator")
    {
        const auto grid = ctb::GlobalMercator();
        //    const auto bounds = tile::SrsBounds{ -180, -90, 0, 90 };
        auto bounds = grid.getExtent();

        // this provokes the following situation:
        // parent tile is produced, because its border overlaps the extents
        // child tiles have smaller pixels -> their border does not overlap the extents any more.
        bounds.min.x = (bounds.width() / 256) / 4;
        const auto tiler = TopDownTiler(grid, bounds, tile::Border::Yes, tile::Scheme::Tms);
        const tile::Id root_id = { 0, { 0, 0 }, tiler.scheme() };

        const auto read_function = [&](const tile::Descriptor&) -> int {
            return 0;
        };
        const auto aggregate_function = [&](const std::vector<int>& data) -> int {
            REQUIRE(!data.empty());
            return 0;
        };
        traverse_depth_first_and_aggregate(tiler, read_function, aggregate_function, root_id, 2);
    }
}
