#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "astra/ecs.hpp"
#include "astra/math.hpp"

namespace astra {

struct Vertex {
  Vec3 pos;
  Vec3 normal;
  Vec2 uv;
};

struct MeshData {
  std::string id;
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  Material default_material;
  AABB local_bounds;
};

struct TextureData {
  std::string id;
  int width = 0;
  int height = 0;
  std::vector<uint8_t> rgba;
};

struct WavData {
  std::string id;
  uint32_t sample_rate = 22050;
  uint16_t channels = 1;
  std::vector<int16_t> samples;
};

class AssetManager {
 public:
  AssetManager();

  void register_builtin_primitives();
  bool load_glb(const std::string& id, const std::string& path, std::string* err = nullptr);
  bool load_gltf(const std::string& id, const std::string& path, std::string* err = nullptr);
  bool load_texture_tga_or_ppm(const std::string& id, const std::string& path, std::string* err = nullptr);
  bool load_wav(const std::string& id, const std::string& path, std::string* err = nullptr);

  // Generates a valid binary glTF 2.0 (.glb) file for portable sample assets/tests
  static bool export_mesh_glb(const MeshData& mesh, const std::string& out_path);
  static bool export_chime_wav(const std::string& out_path, float freq_hz = 660.0f, float duration_sec = 0.16f);

  const MeshData* get_mesh(const std::string& id) const;
  const TextureData* get_texture(const std::string& id) const;
  const WavData* get_wav(const std::string& id) const;
  std::vector<std::string> list_mesh_ids() const;

  void add_mesh(MeshData mesh);

 private:
  std::unordered_map<std::string, MeshData> meshes_;
  std::unordered_map<std::string, TextureData> textures_;
  std::unordered_map<std::string, WavData> wavs_;
};

}  // namespace astra
