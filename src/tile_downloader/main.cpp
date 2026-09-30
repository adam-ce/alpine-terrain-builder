/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2023 Martin Braunsperger
 * Copyright (C) 2023 Adam Celarek-Litofcenko
 * Copyright (C) 2023 Adrian Gawor
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

#include "TileDownloader.h"
#include "TileUrlBuilder.h"
#include "cli.h"
#include "log.h"

int main(int argc, char *argv[]) {
    const auto args = cli::parse(argc, argv);
    Log::init(args.log_level);

    if (args.srs != 3857) {
        LOG_ERROR_AND_EXIT("unsupported srs EPSG \"{}\"", args.srs);
    }

    const auto provider_config = args.provider.has_value()
        ? tile_provider_config(*args.provider)
        : TileProviderConfig { *args.url_pattern, args.url_y_direction };
    const TileUrlBuilder url_builder(provider_config);

    const radix::tile::Id root_id = {args.zoom, {args.x, args.y}};

    TileDownloader downloader(url_builder, args.output, args.max_zoom_level, root_id.zoom_level);
    return downloader.download_recursive(root_id) ? 0 : 1;
}
