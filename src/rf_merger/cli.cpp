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

#include "cli.h"

namespace rf_merger::cli {
void configure(CLI::App& app, merge::Options& options)
{
    app.add_option("--left", options.left, "Published RF snapshot; the right input wins remaining ties")->required();
    app.add_option("--right", options.right, "Published RF snapshot")->required();
    app.add_option("--priorities", options.priorities, "JSON array of attribution indices, highest priority first")->required();
    app.add_option("--output", options.output, "New final snapshot path; log appends to <output>.log")->required();
    app.add_option("--jobs", options.jobs, "Concurrent tile workers")->check(CLI::PositiveNumber)->default_val(1);
    app.add_option_function<std::string>(
        "--cache", [&options](const auto& path) { options.cache = path; }, "Compatible incomplete .part merger snapshot to reuse");
    app.add_option_function<std::string>(
           "--compression",
           [&options](const auto& value) {
               using io::envelope::CompressionAlgorithm;
               options.compression_algorithm = value == "none" ? CompressionAlgorithm::None
                   : value == "zstd-best"                      ? CompressionAlgorithm::ZstdBestCompression
                                                               : CompressionAlgorithm::ZstdDefaultCompression;
           },
           "Output tile compression: zstd (default), zstd-best, or none")
        ->check(CLI::IsMember({ "zstd", "zstd-best", "none" }));
    app.footer(R"(Example:
  rf-merger --left rf/a --right rf/b --priorities priority.json --output rf/merged --jobs 8

priority.json lists attribution indices, highest priority first, e.g. [7, 3, 12].
Unlisted nonzero indices share one rank below all listed indices.
)");
}
} // namespace rf_merger::cli
