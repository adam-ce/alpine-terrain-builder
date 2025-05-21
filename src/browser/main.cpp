#include "core/Application.h"
#include "utils/log/Log.h"

int main() {
#ifdef DEBUG
  Log::Init(spdlog::level::trace);
#else
  Log::Init(spdlog::level::info);
#endif

  Application app("Alpenite Browser", 1280, 720);

  app.run();

  return 0;
}