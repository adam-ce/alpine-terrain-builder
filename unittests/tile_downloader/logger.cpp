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

    logger.skipped(radix::tile::Id{0, {0, 0}});
    session.finish();
}
