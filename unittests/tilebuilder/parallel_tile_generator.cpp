/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
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

#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <utility>
#include "ParallelTileGenerator.h"
#include <catch2/catch_test_macros.hpp>

using namespace radix;

TEST_CASE("parallel tile generator")
{
    std::atomic<int> tile_counter = 0;
    std::atomic<int> validation_error_counter = 0;
    std::filesystem::remove_all("./unittest_tiles/");

    class MockTileWriter : public ParallelTileWriterInterface {
        std::atomic<int>* m_tile_counter = nullptr;
        std::atomic<int>* m_validation_error_counter = nullptr;

    public:
        MockTileWriter(std::atomic<int>* tile_counter, std::atomic<int>* validation_error_counter)
            : ParallelTileWriterInterface(radix::tile::Border::No, "empty")
            , m_tile_counter(tile_counter)
            , m_validation_error_counter(validation_error_counter)
        {
        }
        void write(const std::string& file_path, const radix::tile::Descriptor& tile, const HeightData& heights) const override
        {
            if (file_path.empty() || tile.gridSize != 256 || heights.width() != 256 || heights.height() != 256)
                (*m_validation_error_counter)++;
            (*m_tile_counter)++;

            std::ofstream ofs(file_path);
            ofs << "this is some text in the new file\n";
        }
    };

    std::filesystem::path base_path = "./unittest_tiles/";
    auto generator = ParallelTileGenerator::make(ATB_TEST_DATA_DIR "/austria/at_mgi.tif", ctb::Grid::Srs::SphericalMercator, radix::tile::Scheme::Tms, std::make_unique<MockTileWriter>(&tile_counter, &validation_error_counter), base_path);
    generator.setWarnOnMissingOverviews(false);
    SECTION("dataset tiles only")
    {
        generator.process({ 0, 7 });
        CHECK(validation_error_counter == 0);
        CHECK(tile_counter == 27);
        CHECK(std::filesystem::exists(base_path / "0" / "0" / "0.empty"));
        CHECK(std::filesystem::exists(base_path / "1" / "1" / "1.empty"));
        CHECK(std::filesystem::exists(base_path / "2" / "2" / "2.empty"));
        CHECK(std::filesystem::exists(base_path / "3" / "4" / "5.empty"));
        CHECK(std::filesystem::exists(base_path / "4" / "8" / "10.empty"));
        CHECK(std::filesystem::exists(base_path / "5" / "16" / "20.empty"));
        CHECK(std::filesystem::exists(base_path / "7" / "70" / "84.empty"));
    }
    SECTION("world wide tiles")
    {
        generator.process({ 0, 2 }, false, true);
        CHECK(validation_error_counter == 0);
        CHECK(tile_counter == 21);
        CHECK(std::filesystem::exists(base_path / "0" / "0" / "0.empty"));

        CHECK(std::filesystem::exists(base_path / "1" / "0" / "0.empty"));
        CHECK(std::filesystem::exists(base_path / "1" / "0" / "1.empty"));
        CHECK(std::filesystem::exists(base_path / "1" / "1" / "0.empty"));
        CHECK(std::filesystem::exists(base_path / "1" / "1" / "1.empty"));

        CHECK(std::filesystem::exists(base_path / "2" / "0" / "0.empty"));
        CHECK(std::filesystem::exists(base_path / "2" / "0" / "1.empty"));
        CHECK(std::filesystem::exists(base_path / "2" / "0" / "2.empty"));
        CHECK(std::filesystem::exists(base_path / "2" / "0" / "3.empty"));
        CHECK(std::filesystem::exists(base_path / "2" / "1" / "0.empty"));
        CHECK(std::filesystem::exists(base_path / "2" / "1" / "1.empty"));
        CHECK(std::filesystem::exists(base_path / "2" / "1" / "2.empty"));
        CHECK(std::filesystem::exists(base_path / "2" / "1" / "3.empty"));
        CHECK(std::filesystem::exists(base_path / "2" / "2" / "0.empty"));
        CHECK(std::filesystem::exists(base_path / "2" / "2" / "1.empty"));
        CHECK(std::filesystem::exists(base_path / "2" / "2" / "2.empty"));
        CHECK(std::filesystem::exists(base_path / "2" / "2" / "3.empty"));
        CHECK(std::filesystem::exists(base_path / "2" / "3" / "0.empty"));
        CHECK(std::filesystem::exists(base_path / "2" / "3" / "1.empty"));
        CHECK(std::filesystem::exists(base_path / "2" / "3" / "2.empty"));
        CHECK(std::filesystem::exists(base_path / "2" / "3" / "3.empty"));
    }
}
