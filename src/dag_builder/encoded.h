#pragma once

#include <filesystem>

#include <expected>

#include "io/Error.h"
#include "io/serialize.h"
#include "serialization.h"

inline std::expected<void, ::io::Error> save_clustering(
    const Clustering &clustering,
    const std::filesystem::path &path,
    const bool make_dirs = true) {
    return ::io::write_to_path(clustering, path, make_dirs);
}

inline std::expected<Clustering, ::io::Error> load_clustering(
    const std::filesystem::path &path) {
    return ::io::read_from_path<Clustering>(path);
}
