#include "core/Application.h"
#include "utils/log/Log.h"

int main() {
  Log::Init(spdlog::level::trace);

  Application app("Alpenite Browser", 1280, 720);

  app.run();

  return 0;
}