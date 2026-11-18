#include "astra/editor.hpp"
#include <iostream>

int main(int argc, char** argv) {
  astra::EditorApp app;
  std::string scene_path = argc > 1 ? argv[1] : "";
  if (!app.init(1280, 760, scene_path)) {
    std::cerr << "Failed to initialize Astra Editor.\n";
    return 1;
  }
  app.run();
  app.shutdown();
  return 0;
}
