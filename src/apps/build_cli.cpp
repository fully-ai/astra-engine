#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cout << "Usage: astra-build <project_dir> <dist_dir>\n";
    return 1;
  }
  fs::path src = fs::weakly_canonical(argv[1]);
  fs::path dst = fs::weakly_canonical(argv[2]);
  if (!fs::exists(src)) {
    std::cerr << "Project dir not found: " << src << "\n";
    return 1;
  }
  fs::create_directories(dst);
  int count = 0;
  std::ofstream manifest(dst / "manifest.txt");
  for (const auto& entry : fs::recursive_directory_iterator(src)) {
    if (!entry.is_regular_file()) continue;
    auto ext = entry.path().extension().string();
    if (ext == ".json" || ext == ".glb" || ext == ".gltf" || ext == ".bin" || ext == ".wav" || ext == ".tga" || ext == ".as") {
      fs::path rel = fs::relative(entry.path(), src);
      fs::path out_file = dst / rel;
      fs::create_directories(out_file.parent_path());
      fs::copy_file(entry.path(), out_file, fs::copy_options::overwrite_existing);
      manifest << rel.generic_string() << " " << entry.file_size() << "\n";
      ++count;
    }
  }
  std::cout << "Packaged " << count << " Astra Engine asset/scene files into " << dst << "\n";
  return 0;
}
