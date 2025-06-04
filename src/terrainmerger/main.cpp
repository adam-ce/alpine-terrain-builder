#include <string>

#include "log.h"
#include "cli.h"
#include "merge.h"

void run(const cli::Args &args) {
    if (!std::filesystem::exists(args.output_path)) {
        std::filesystem::create_directories(args.output_path);
    }

    octree::Storage output_storage = octree::open_folder(args.output_path);
    std::vector<octree::Storage> input_storages;
    for (const auto &input_path : args.input_paths) {
        if (!std::filesystem::exists(input_path)) {
            LOG_ERROR("Input path '{}' does not exist.", input_path.string());
            return;
        }
        auto input_storage = octree::open_folder(input_path);
        input_storages.push_back(std::move(input_storage));
    }

    merge_datasets(output_storage, input_storages);
}

int main(int argc, char **argv) {
    const cli::Args args = cli::parse(argc, argv);
    Log::init(args.log_level);

    const std::string arg_str = std::accumulate(argv, argv + argc, std::string(),
                                                [](const std::string &acc, const char *arg) {
                                                    return acc + (acc.empty() ? "" : " ") + arg;
                                                });
    LOG_DEBUG("Running with: {}", arg_str);

    run(args);
}

