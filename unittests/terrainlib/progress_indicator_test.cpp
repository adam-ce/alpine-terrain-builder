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

#include "ProgressIndicator.h"
#include <catch2/catch_test_macros.hpp>
#include <execution>
#include <thread>
#include <vector>

using namespace std::literals;


TEST_CASE("progress indicator")
{
    SECTION("throws on overflow")
    {
        {
            auto pi = ProgressIndicator(0);
            CHECK_THROWS(pi.task_finished());
        }
        {
            auto pi = ProgressIndicator(1);
            pi.task_finished();
            CHECK_THROWS(pi.task_finished());
        }
    }

    SECTION("message")
    {
        ProgressIndicator pi(4);
        // Step 0
        CHECK(pi.x_of_y_done_message() == "0/4");

        // Step 1
        pi.task_finished();
        CHECK(pi.x_of_y_done_message() == "1/4");

        // Step 2
        pi.task_finished();
        CHECK(pi.x_of_y_done_message() == "2/4");

        // Step 3
        pi.task_finished();
        CHECK(pi.x_of_y_done_message() == "3/4");

        // Step 4
        pi.task_finished();
        CHECK(pi.x_of_y_done_message() == "4/4");
    }

    SECTION("bar small")
    {
        ProgressIndicator pi(2);
        // Step 0
        CHECK(pi.progress_bar(2) == "[]");
        CHECK(pi.progress_bar(3) == "[ ]");
        
        // Step 1
        pi.task_finished();
        CHECK(pi.progress_bar(2) == "[]");
        CHECK(pi.progress_bar(3) == "[>]");

        // Step 2
        pi.task_finished();
        CHECK(pi.progress_bar(2) == "[]");
        CHECK(pi.progress_bar(3) == "[=]");
    }

    SECTION("task_finished is thread safe")
    {
        const auto tasks = std::vector<int>(1000000);
        auto pi = ProgressIndicator(tasks.size());
        std::for_each(std::execution::par, tasks.begin(), tasks.end(), [&](const auto&) { pi.task_finished(); });
        CHECK_THROWS(pi.task_finished());
    }

    SECTION("thread parallel reporting")
    {
        // no real test, because console output can't be easily tested. but at least we are test compiling the interface.
        const auto tasks = std::vector<int>(151);
        auto pi = ProgressIndicator(tasks.size());
        auto monitoring_thread = pi.start_monitoring();
        std::for_each(std::execution::par, tasks.begin(), tasks.end(), [&](const auto&) { std::this_thread::sleep_for(10ms); pi.task_finished(); });
        monitoring_thread.join();
        CHECK_THROWS(pi.task_finished());
    }
}
