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

TEST_CASE("Error::propagate adds supplied context to an Error", "[error]")
{
    Error source = Error::make(Error::Code::Io, "open input");
    const Expected<void> result = Error::propagate(std::move(source), "read input");

    REQUIRE_FALSE(result.has_value());
    const std::string description = result.error().to_string();
    CHECK(description.find("read input") != std::string::npos);
    CHECK(description.find("unittests/terrainlib/error.cpp") != std::string::npos);
    CHECK(description.find("open input") != std::string::npos);
}

TEST_CASE("Error::raise throws an exception carrying the error", "[error]")
{
    try {
        Error::raise(Error::Code::NotFound, "find input");
        FAIL("raise returned");
    } catch (const Error::Exception& exception) {
        CHECK(exception.error().code() == Error::Code::NotFound);
        CHECK(std::string(exception.what()) == exception.error().to_string());
        CHECK(std::string(exception.what()).find("find input") != std::string::npos);
    }
}

TEST_CASE("Error::throwing_unwrap returns values and throws failures", "[error]")
{
    CHECK(Error::throwing_unwrap(Expected<int>(7)) == 7);
    Error::throwing_unwrap(Expected<void>());

    const auto thrown = [](Expected<int> result, std::string message) {
        try {
            Error::throwing_unwrap(std::move(result), std::move(message));
        } catch (const Error::Exception& exception) {
            return exception.error();
        }
        FAIL("throwing_unwrap returned");
        return Error::make(Error::Code::Internal, "unreachable");
    };
    const auto plain = thrown(Error::fail(Error::Code::Io, "open input"), {});
    CHECK(plain.code() == Error::Code::Io);
    CHECK(plain.to_string().find("caused by") == std::string::npos);
    const auto context = thrown(Error::fail(Error::Code::Io, "open input"), "read tile");
    CHECK(context.to_string().find("read tile") != std::string::npos);
    CHECK(context.to_string().find("caused by: open input") != std::string::npos);
}

TEST_CASE("Error keeps the stack trace where it was made", "[error]")
{
    Expected<int> source = Error::fail(Error::Code::Io, "open input");
    const auto origin = source.error().stacktrace();
    CHECK_FALSE(origin.empty());
    const Expected<void> propagated = Error::propagate(std::move(source), "read input");
    CHECK(propagated.error().stacktrace() == origin);
    try {
        Error::throwing_unwrap(Expected<void>(propagated), "decode input");
    } catch (const Error::Exception& exception) {
        CHECK(exception.error().stacktrace() == origin);
    }
}
