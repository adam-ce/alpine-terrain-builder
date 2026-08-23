#include "HttpClient.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include <catch2/catch_test_macros.hpp>

namespace {

class TemporaryFile {
public:
    TemporaryFile()
        : m_path(std::filesystem::temp_directory_path() / "atb-http-client-test.txt")
    {
        std::ofstream output(m_path, std::ios::binary);
        output << "response body";
        REQUIRE(output);
    }

    ~TemporaryFile()
    {
        std::error_code error;
        std::filesystem::remove(m_path, error);
    }

    [[nodiscard]] std::string url() const { return "file://" + m_path.string(); }

private:
    std::filesystem::path m_path;
};

class CallbackError : public std::runtime_error {
public:
    CallbackError()
        : std::runtime_error("callback failed")
    {
    }
};

} // namespace

TEST_CASE("http client propagates response writer exceptions")
{
    const TemporaryFile source;
    HttpClient client([](std::vector<char>&, const char*, size_t) { throw CallbackError(); });

    CHECK_THROWS_AS(client.get(source.url()), CallbackError);
}

TEST_CASE("http client propagates progress callback exceptions")
{
    const TemporaryFile source;
    HttpClient client;

    CHECK_THROWS_AS(client.get(source.url(), [](double) { throw CallbackError(); }), CallbackError);
}
