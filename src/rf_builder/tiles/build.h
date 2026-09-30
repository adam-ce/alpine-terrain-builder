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
