#pragma once

#include "merge.h"
#include <CLI/CLI.hpp>

namespace rf_merger::cli {
void configure(CLI::App& app, merge::Options& options);
} // namespace rf_merger::cli
