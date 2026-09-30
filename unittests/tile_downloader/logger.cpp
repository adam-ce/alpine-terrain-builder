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

#include "TileLogger.h"

#include <chrono>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

using namespace std::literals;

TEST_CASE("tile logger session stops monitoring during exceptional unwinding")
{
    TileLogger logger(0);

    const auto before_throw = std::chrono::steady_clock::now();
    CHECK_THROWS_AS(
        [&]() {
            auto session = logger.start();
            throw std::runtime_error("download failed");
        }(),
        std::runtime_error);
    const auto unwind_duration = std::chrono::steady_clock::now() - before_throw;

    CHECK(unwind_duration < 250ms);
}

TEST_CASE("tile logger session can finish normally")
{
    TileLogger logger(0);
    auto session = logger.start();

    logger.skipped(radix::tile::Id { 0, { 0, 0 } });
    session.finish();
}
