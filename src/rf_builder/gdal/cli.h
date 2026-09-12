#pragma once
#include "build.h"
#include <CLI/CLI.hpp>
namespace rf_builder::gdal::cli {
void configure(CLI::App& app, Options& options);
}
