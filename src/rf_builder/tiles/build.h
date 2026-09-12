#pragma once
#include "HttpClient.h"
#include "run.h"
namespace rf_builder::tiles {
struct Options {
    std::filesystem::path provider;
    std::string mask;
    run::Options output;
    // Internal execution settings; never included in pixel cache identity.
    RetryPolicy retry;
    std::size_t source_cache_bytes = 64 * 1024 * 1024;
};
Expected<run::Report> build(const Options& options, const std::function<bool()>& stop_requested = {});
} // namespace rf_builder::tiles
