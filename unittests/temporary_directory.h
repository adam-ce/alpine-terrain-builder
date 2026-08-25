#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>

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

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
    }

    const std::filesystem::path& path() const { return m_path; }

private:
    std::filesystem::path m_path;
};

} // namespace test
