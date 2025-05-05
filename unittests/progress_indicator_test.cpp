/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 Adam Celarek <last name at cg dot tuwien dot ac dot at>
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

#include "ParallelTileGenerator.h"
#include "ProgressIndicator.h"
#include <catch2/catch_test_macros.hpp>
#include <execution>
#include <thread>
#include <vector>

using namespace std::literals;
using namespace radix;

TEST_CASE("progress indicator")
{
    SECTION("throws on overflow")
    {
        {
            auto pi = ProgressIndicator(0);
            CHECK_THROWS(pi.taskFinished());
        }
        {
            auto pi = ProgressIndicator(1);
            pi.taskFinished();
            CHECK_THROWS(pi.taskFinished());
        }
    }
    SECTION("bar and message")
    {
        const std::string m5 = "-----";
        const std::string m10 = m5 + m5;
        const std::string m20 = m10 + m10;
        const std::string m50 = m20 + m20 + m10;
        const std::string b5 = "|||||";
        const std::string b10 = b5 + b5;
        const std::string b20 = b10 + b10;
        const std::string b50 = b20 + b20 + b10;

        auto pi = ProgressIndicator(4);
        CHECK(pi.progressBar() == m50 + m50);
        CHECK(pi.xOfYDoneMessagE() == "0/4");
        pi.taskFinished();
        CHECK(pi.progressBar() == b20 + b5 + m20 + m5 + m50);
        CHECK(pi.xOfYDoneMessagE() == "1/4");
        pi.taskFinished();
        CHECK(pi.progressBar() == b50 + m50);
        CHECK(pi.xOfYDoneMessagE() == "2/4");
        pi.taskFinished();
        CHECK(pi.progressBar() == b50 + b20 + b5 + m20 + m5);
        CHECK(pi.xOfYDoneMessagE() == "3/4");
        pi.taskFinished();
        CHECK(pi.progressBar() == b50 + b50);
        CHECK(pi.xOfYDoneMessagE() == "4/4");
    }

    SECTION("taskFinished is thread safe")
    {
        const auto tasks = std::vector<int>(1000000);
        auto pi = ProgressIndicator(tasks.size());
        std::for_each(std::execution::par, tasks.begin(), tasks.end(), [&](const auto&) { pi.taskFinished(); });
        CHECK_THROWS(pi.taskFinished());
    }

    SECTION("thread parallel reporting")
    {
        // no real test, because console output can't be easily tested. but at least we are test compiling the interface.
        const auto tasks = std::vector<int>(151);
        auto pi = ProgressIndicator(tasks.size());
        auto monitoring_thread = pi.startMonitoring();
        std::for_each(std::execution::par, tasks.begin(), tasks.end(), [&](const auto&) { std::this_thread::sleep_for(10ms); pi.taskFinished(); });
        monitoring_thread.join();
        CHECK_THROWS(pi.taskFinished());
    }

    SECTION("parallel processing interface")
    {
        class MockTileWriter : public ParallelTileWriterInterface {
        public:
            MockTileWriter()
                : ParallelTileWriterInterface(tile::Border::No, "empty")
            {
            }
            void write(const std::string&, const tile::Descriptor&, const HeightData&) const override { std::this_thread::sleep_for(1ms); }
        };

        auto generator = ParallelTileGenerator::make(ATB_TEST_DATA_DIR "/austria/at_mgi.tif", ctb::Grid::Srs::SphericalMercator, tile::Scheme::Tms, std::make_unique<MockTileWriter>(), "./unittest_tiles/");
        generator.setWarnOnMissingOverviews(false);
        generator.process({ 0, 8 }, true);
    }
}
