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

#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

namespace test {

class TemporaryDirectory {
public:
    explicit TemporaryDirectory(const std::string_view label = {})
    {
        static std::atomic_uint64_t counter = 0;
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        std::string name = "atb-test";
        if (!label.empty()) {
            name += "-" + std::string(label);
        }
        name += "-" + std::to_string(timestamp) + "-" + std::to_string(counter++);
        m_path = std::filesystem::temp_directory_path() / name;
        REQUIRE(std::filesystem::create_directories(m_path));
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    TemporaryDirectory(TemporaryDirectory&&) = delete;
    TemporaryDirectory& operator=(TemporaryDirectory&&) = delete;

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
    }

    const std::filesystem::path& path() const { return m_path; }

private:
    std::filesystem::path m_path;
};

static_assert(!std::is_copy_constructible_v<TemporaryDirectory>);
static_assert(!std::is_copy_assignable_v<TemporaryDirectory>);
static_assert(!std::is_move_constructible_v<TemporaryDirectory>);
static_assert(!std::is_move_assignable_v<TemporaryDirectory>);

} // namespace test
