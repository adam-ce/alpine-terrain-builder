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
        : _path(std::filesystem::temp_directory_path() / name) {
        std::error_code error;
        std::filesystem::remove_all(_path, error);
        std::filesystem::remove(staging_path(), error);
    }

    ~TemporaryOutput() {
        std::error_code error;
        std::filesystem::remove_all(_path, error);
        std::filesystem::remove(staging_path(), error);
    }

    [[nodiscard]] const std::filesystem::path &path() const {
        return _path;
    }

    [[nodiscard]] std::filesystem::path staging_path() const {
        auto staging = _path;
        staging += ".part";
        return staging;
    }

private:
    std::filesystem::path _path;
};

}

TEST_CASE("checked file writer persists the complete response")
{
    const TemporaryOutput output("atb-write-file-success.bin");
    const std::vector<char> expected{'t', 'i', 'l', 'e'};

    write_file_checked(output.path(), expected);

    std::ifstream input(output.path(), std::ios::binary);
    const auto begin = std::istreambuf_iterator<char>(input);
    const std::vector<char> actual(begin, std::istreambuf_iterator<char>{});
    CHECK(actual == expected);
    CHECK_FALSE(std::filesystem::exists(output.staging_path()));
}

TEST_CASE("checked file writer preserves the final path when promotion fails")
{
    const TemporaryOutput output("atb-write-file-promotion-failure");
    REQUIRE(std::filesystem::create_directory(output.path()));

    CHECK_THROWS_AS(
        write_file_checked(output.path(), std::vector<char>{'t', 'i', 'l', 'e'}),
        std::filesystem::filesystem_error);

    CHECK(std::filesystem::is_directory(output.path()));
    CHECK_FALSE(std::filesystem::exists(output.staging_path()));
}

#if defined(__linux__)
TEST_CASE("checked file writer reports persistence failures")
{
    const std::vector<char> data(64 * 1024, 'x');

    CHECK_THROWS_AS(write_file_checked("/dev/full", data), std::runtime_error);
    CHECK(std::filesystem::is_character_file("/dev/full"));
    CHECK_FALSE(std::filesystem::exists("/dev/full.part"));
}
#endif
