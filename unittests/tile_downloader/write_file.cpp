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

#include "write_file.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace {

class TemporaryOutput {
public:
    explicit TemporaryOutput(std::string_view name)
        : m_path(std::filesystem::temp_directory_path() / name)
    {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
        std::filesystem::remove(partial_path(), error);
        std::filesystem::remove(pending_path(), error);
    }

    ~TemporaryOutput()
    {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
        std::filesystem::remove(partial_path(), error);
        std::filesystem::remove(pending_path(), error);
    }

    [[nodiscard]] const std::filesystem::path& path() const { return m_path; }

    [[nodiscard]] std::filesystem::path partial_path() const { return partial_tile_path(m_path); }

    [[nodiscard]] std::filesystem::path pending_path() const { return children_pending_tile_path(m_path); }

private:
    std::filesystem::path m_path;
};

} // namespace

TEST_CASE("checked file writer persists the complete response")
{
    const TemporaryOutput output("atb-write-file-success.bin");
    const std::vector<char> expected { 't', 'i', 'l', 'e' };

    write_file_children_pending(output.path(), expected);

    CHECK_FALSE(std::filesystem::exists(output.path()));
    CHECK_FALSE(std::filesystem::exists(output.partial_path()));
    REQUIRE(std::filesystem::exists(output.pending_path()));

    std::ifstream input(output.pending_path(), std::ios::binary);
    const auto begin = std::istreambuf_iterator<char>(input);
    const std::vector<char> actual(begin, std::istreambuf_iterator<char> {});
    CHECK(actual == expected);

    mark_tile_children_complete(output.path());
    CHECK(std::filesystem::exists(output.path()));
    CHECK_FALSE(std::filesystem::exists(output.pending_path()));
}

TEST_CASE("checked file writer preserves the final path when promotion fails")
{
    const TemporaryOutput output("atb-write-file-promotion-failure");
    REQUIRE(std::filesystem::create_directory(output.path()));
    write_file_children_pending(output.path(), std::vector<char> { 't', 'i', 'l', 'e' });

    CHECK_THROWS_AS(mark_tile_children_complete(output.path()), std::filesystem::filesystem_error);

    CHECK(std::filesystem::is_directory(output.path()));
    CHECK(std::filesystem::exists(output.pending_path()));
}

#if defined(__linux__)
TEST_CASE("checked file writer reports persistence failures")
{
    const std::vector<char> data(64 * 1024, 'x');

    CHECK_THROWS_AS(write_file_children_pending("/dev/full", data), std::runtime_error);
    CHECK(std::filesystem::is_character_file("/dev/full"));
    CHECK_FALSE(std::filesystem::exists("/dev/full.part"));
    CHECK_FALSE(std::filesystem::exists("/dev/full.children-pending"));
}
#endif
