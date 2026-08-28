#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "Error.h"

TEST_CASE("Error::fail creates an expected failure", "[error]")
{
    const Expected<void> result = Error::fail(Error::Code::Io, "open input");

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == Error::Code::Io);
    CHECK(result.error().to_string().find("open input") != std::string::npos);
}

TEST_CASE("Error::propagate adds a call-site frame", "[error]")
{
    Expected<int> source = Error::fail(Error::Code::Io, "open input");
    const Expected<void> result = Error::propagate(std::move(source));

    REQUIRE_FALSE(result.has_value());
    const std::string description = result.error().to_string();
    CHECK(description.find("propagated") != std::string::npos);
    CHECK(description.find("unittests/terrainlib/error.cpp") != std::string::npos);
    CHECK(description.find("open input") != std::string::npos);
}

TEST_CASE("Error::propagate uses a supplied context as its call-site frame", "[error]")
{
    Expected<int> source = Error::fail(Error::Code::ResourceExhausted, "decode payload");
    const Expected<void> result = Error::propagate(std::move(source), Error::Code::CorruptData, "read envelope");

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == Error::Code::CorruptData);
    const std::string description = result.error().to_string();
    CHECK(description.find("read envelope (reclassified ResourceExhausted -> CorruptData)") != std::string::npos);
    CHECK(description.find("unittests/terrainlib/error.cpp") != std::string::npos);
    CHECK(description.find("decode payload") != std::string::npos);
}
