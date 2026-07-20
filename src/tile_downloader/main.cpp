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

    TileDownloader downloader(url_builder, args.output, args.early_skip, args.max_zoom_level);
    downloader.download_recursive(root_id);

    return 0;
}
