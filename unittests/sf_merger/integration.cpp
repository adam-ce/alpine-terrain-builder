#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

#include "mesh/io.h"

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        _path = std::filesystem::temp_directory_path() /
                ("alpine-terrain-builder-sf-merge-" + std::to_string(suffix));
        std::filesystem::create_directories(_path);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(_path, error);
    }

    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

    [[nodiscard]] const std::filesystem::path &path() const {
        return _path;
    }

private:
    std::filesystem::path _path;
};

std::string shell_quote(const std::filesystem::path &path) {
    std::string quoted = "'";
    for (const char character : path.string()) {
        if (character == '\'') {
            quoted += "'\\''";
        } else {
            quoted += character;
        }
    }
    return quoted + "'";
}

int build_sf(
    const std::filesystem::path &dataset,
    const std::filesystem::path &textures,
    const std::filesystem::path &output) {
    const std::string command =
        shell_quote(ALP_SF_BUILDER_PATH) +
        " --dataset " + shell_quote(dataset) +
        " --textures " + shell_quote(textures) +
        " --min-texture-level 12 --max-texture-level 17"
        " --mesh-srs EPSG:4978 --verbosity warn"
        " batch --target-level 15 --output " + shell_quote(output) +
        " --format .terrain --threads 1";
    return std::system(command.c_str());
}

} // namespace

TEST_CASE("SF builders reproduce working and malformed mask-border merges", "[integration][sf-builder][sf-merger]") {
    const std::filesystem::path fixture =
        std::filesystem::path(ALP_TEST_DATA_DIR) / "sf_builder_merge_border";
    const TemporaryDirectory temporary_directory;
    const std::filesystem::path base_output = temporary_directory.path() / "basemap-gs";
    const std::filesystem::path new_output = temporary_directory.path() / "gataki-gt";
    const std::filesystem::path merged_output = temporary_directory.path() / "merged";

    REQUIRE(build_sf(
                fixture / "elevation/gs.tif",
                fixture / "orthophoto/basemap",
                base_output) == 0);
    REQUIRE(build_sf(
                fixture / "elevation/gt.tif",
                fixture / "orthophoto/gataki",
                new_output) == 0);

    const std::string merge_command =
        shell_quote(ALP_SF_MERGER_PATH) +
        " merge --base " + shell_quote(base_output) +
        " --new " + shell_quote(new_output) +
        " --mask " + shell_quote(fixture / "mask/tirol.shp") +
        " --output " + shell_quote(merged_output) +
        " --verbosity warn";
    REQUIRE(std::system(merge_command.c_str()) == 0);

    const std::filesystem::path working_relative = "15/26291/18610/27235.terrain";
    const std::filesystem::path malformed_relative = "15/26290/18610/27235.terrain";
    const std::filesystem::path working_path = merged_output / working_relative;
    const std::filesystem::path malformed_path = merged_output / malformed_relative;

    REQUIRE(std::filesystem::exists(working_path));
    CHECK_FALSE(std::filesystem::equivalent(working_path, base_output / working_relative));
    CHECK_FALSE(std::filesystem::equivalent(working_path, new_output / working_relative));

    const auto working_mesh = mesh::io::load_from_path(working_path);
    REQUIRE(working_mesh.has_value());
    CHECK_FALSE(working_mesh->is_empty());

    REQUIRE(std::filesystem::exists(malformed_path));
    CHECK_FALSE(std::filesystem::equivalent(malformed_path, base_output / malformed_relative));
    CHECK_FALSE(std::filesystem::equivalent(malformed_path, new_output / malformed_relative));

    const auto malformed_mesh = mesh::io::load_from_path(malformed_path);
    const std::string malformed_error = malformed_mesh.has_value()
                                            ? std::string()
                                            : malformed_mesh.error().description();
    INFO("Malformed mask-border merge: " << malformed_error);
    REQUIRE(malformed_mesh.has_value());
    CHECK_FALSE(malformed_mesh->is_empty());
}
