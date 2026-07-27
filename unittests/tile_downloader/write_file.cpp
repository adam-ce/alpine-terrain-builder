#include "write_file.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace {

class TemporaryOutput {
public:
    TemporaryOutput()
        : _path(std::filesystem::temp_directory_path() / "atb-write-file-test.bin") {}

    ~TemporaryOutput() {
        std::error_code error;
        std::filesystem::remove(_path, error);
    }

    [[nodiscard]] const std::filesystem::path &path() const {
        return _path;
    }

private:
    std::filesystem::path _path;
};

}

TEST_CASE("checked file writer persists the complete response")
{
    const TemporaryOutput output;
    const std::vector<char> expected{'t', 'i', 'l', 'e'};

    write_file_checked(output.path(), expected);

    std::ifstream input(output.path(), std::ios::binary);
    const auto begin = std::istreambuf_iterator<char>(input);
    const std::vector<char> actual(begin, std::istreambuf_iterator<char>{});
    CHECK(actual == expected);
}

#if defined(__linux__)
TEST_CASE("checked file writer reports persistence failures")
{
    const std::vector<char> data(64 * 1024, 'x');

    CHECK_THROWS_AS(write_file_checked("/dev/full", data), std::runtime_error);
}
#endif
