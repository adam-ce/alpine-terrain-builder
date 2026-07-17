#include "core/Application.h"
#include <log.h>

int main()
{
#ifdef DEBUG
    Log::init(spdlog::level::trace);
#else
    Log::init(spdlog::level::info);
#endif

    Application app("Alpenite Browser", 1280, 720);

    app.run();

    return 0;
}