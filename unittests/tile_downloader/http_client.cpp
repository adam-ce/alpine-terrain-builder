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
