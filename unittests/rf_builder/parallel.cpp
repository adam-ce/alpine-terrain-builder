#include "TilePool.h"
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
    rf_builder::TilePool<unsigned> pool(2, [&](unsigned, const Key& key) -> Expected<unsigned> {
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
    rf_builder::TilePool<unsigned> pool(2, [&](unsigned, const Key& key) -> Expected<unsigned> {
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
        CHECK_FALSE(pool.submit({ 3, { 4, 0 } }));
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

TEST_CASE("RF retains the first worker error when later failures occupy earlier slots", "[rf-builder][parallel]")
{
    std::promise<void> release_first_slot, release_second_slot;
    const auto first_gate = release_first_slot.get_future().share();
    const auto second_gate = release_second_slot.get_future().share();
    std::atomic_uint started = 0;
    rf_builder::TilePool<unsigned> pool(2, [&](unsigned, const Key& key) -> Expected<unsigned> {
        ++started;
        if (key.coords.x == 0) {
            first_gate.wait();
            return Error::fail(Error::Code::Io, "later failure");
        }
        second_gate.wait();
        return Error::fail(Error::Code::Io, "first failure");
    });
    Release first { release_first_slot }, second { release_second_slot };
    REQUIRE(pool.submit({ 3, { 0, 0 } }));
    REQUIRE(pool.submit({ 3, { 1, 0 } }));
    REQUIRE(wait_for([&] { return started == 2; }));
    second.open();
    REQUIRE(wait_for([&] { return pool.failure().has_value(); }));
    first.open();
    pool.join();
    auto done = pool.take(0ms);
    REQUIRE(done);
    CHECK(done->key.coords.x == 0);
    CHECK(done->result.error().to_string().find("later failure") != std::string::npos);
    CHECK(pool.failure()->to_string().find("first failure") != std::string::npos);
}

TEST_CASE("RF worker errors and exceptions stop queued work and join active workers", "[rf-builder][parallel]")
{
    const bool throws = GENERATE(false, true);
    std::promise<void> release;
    const auto gate = release.get_future().share();
    std::atomic_uint started = 0;
    rf_builder::TilePool<unsigned> pool(2, [&](unsigned, const Key& key) -> Expected<unsigned> {
        ++started;
        gate.wait();
        if (throws) {
            throw std::runtime_error("injected worker failure");
        }
        return Error::fail(Error::Code::Io, "injected failure for " + to_string(key));
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
    CHECK_FALSE(first->result);
    REQUIRE(pool.failure());
    CHECK(pool.failure()->code() == (throws ? Error::Code::Internal : Error::Code::Io));
    auto second = pool.take(3s);
    REQUIRE(second);
    CHECK_FALSE(second->result);
    pool.join();
    CHECK(started == 2);
    CHECK(pool.outstanding() == 0);
}
