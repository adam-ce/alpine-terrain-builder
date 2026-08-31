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
