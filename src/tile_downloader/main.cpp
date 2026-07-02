#include <algorithm>
#include <memory>
#include <string>

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

    std::unique_ptr<TileUrlBuilder> url_builder;
    std::string provider = args.provider;
    std::transform(provider.begin(), provider.end(), provider.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (provider == "basemap") {
        url_builder = std::make_unique<BasemapTileUrlBuilder>(args.layer, args.style);
    } else {
        url_builder = std::make_unique<GatakiTileUrlBuilder>();
    }

    const radix::tile::Id root_id = {args.zoom, {args.x, args.y}, args.scheme};

    TileDownloader downloader(*url_builder, args.output, args.early_skip, args.max_zoom_level);
    downloader.download_recursive(root_id);

    return 0;
}
