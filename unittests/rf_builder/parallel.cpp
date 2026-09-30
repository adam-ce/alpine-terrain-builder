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

#include "raster_store/TilePool.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <future>

namespace {
using Key = radix::tile::Id;
using namespace std::chrono_literals;
struct Release {
    std::promise<void>& promise;
    bool opened = false;
    void open()
    {
        if (!std::exchange(opened, true)) {
            promise.set_value();
        }
    }
    ~Release() { open(); }
};
bool wait_for(const std::function<bool()>& predicate)
{
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    return predicate();
}
} // namespace

TEST_CASE("RF workers return ready tiles without waiting for the first tile", "[rf-builder][parallel]")
{
    std::promise<void> release;
    const auto gate = release.get_future().share();
    raster_store::TilePool<unsigned> pool(2, [&](unsigned, const Key& key) -> unsigned {
        if (key.coords.x == 0) {
            gate.wait();
        }
        return key.coords.x;
    });
    Release cleanup { release };
    REQUIRE(pool.submit({ 2, { 0, 0 } }));
    REQUIRE(pool.submit({ 2, { 1, 0 } }));
    auto done = pool.take(3s);
    REQUIRE(done);
    REQUIRE(done->result);
    CHECK(*done->result == 1);
}

TEST_CASE("RF cancellation drops queued tiles and finishes active tiles within the bound", "[rf-builder][parallel]")
{
    std::promise<void> release;
    const auto gate = release.get_future().share();
    std::atomic_uint started = 0;
    raster_store::TilePool<unsigned> pool(2, [&](unsigned, const Key& key) -> unsigned {
        ++started;
        gate.wait();
        return key.coords.x;
    });
    {
        Release cleanup { release };
        REQUIRE(pool.submit({ 3, { 0, 0 } }));
        REQUIRE(pool.submit({ 3, { 1, 0 } }));
        REQUIRE(wait_for([&] { return started == 2; }));
        REQUIRE(pool.submit({ 3, { 2, 0 } }));
        REQUIRE(pool.submit({ 3, { 3, 0 } }));
        CHECK(pool.outstanding() == 4);
        pool.stop();
        CHECK(pool.outstanding() == 2);
        CHECK_FALSE(pool.submit({ 3, { 5, 0 } }));
    }
    for (unsigned i = 0; i < 2; ++i) {
        auto done = pool.take(3s);
        REQUIRE(done);
        REQUIRE(done->result);
        CHECK(*done->result < 2);
    }
    pool.join();
    CHECK(started == 2);
    CHECK(pool.outstanding() == 0);
}

TEST_CASE("RF worker errors and exceptions stop queued work and join active workers", "[rf-builder][parallel]")
{
    const bool error_exception = GENERATE(true, false);
    std::promise<void> release;
    const auto gate = release.get_future().share();
    std::atomic_uint started = 0;
    raster_store::TilePool<unsigned> pool(2, [&](unsigned, const Key& key) -> unsigned {
        ++started;
        gate.wait();
        if (error_exception) {
            Error::raise(Error::Code::Io, "injected failure for " + to_string(key));
        }
        throw std::runtime_error("injected worker failure");
    });
    {
        Release cleanup { release };
        REQUIRE(pool.submit({ 3, { 0, 0 } }));
        REQUIRE(pool.submit({ 3, { 1, 0 } }));
        REQUIRE(wait_for([&] { return started == 2; }));
        REQUIRE(pool.submit({ 3, { 2, 0 } }));
        REQUIRE(pool.submit({ 3, { 3, 0 } }));
    }
    auto first = pool.take(3s);
    REQUIRE(first);
    REQUIRE_FALSE(first->result);
    CHECK(first->result.error().code() == (error_exception ? Error::Code::Io : Error::Code::Internal));
    CHECK(first->result.error().to_string().find("prepare RF tile " + to_string(first->key)) != std::string::npos);
    CHECK_FALSE(pool.submit({ 3, { 4, 0 } }));
    auto second = pool.take(3s);
    REQUIRE(second);
    CHECK_FALSE(second->result);
    pool.join();
    CHECK(started == 2);
    CHECK(pool.outstanding() == 0);
}
