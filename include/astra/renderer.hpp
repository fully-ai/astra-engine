#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "astra/assets.hpp"
#include "astra/ecs.hpp"
#include "astra/math.hpp"

namespace astra {

struct RenderStats {
  uint32_t draw_calls = 0;
  uint32_t triangles = 0;
  float frame_ms = 0.0f;
};

struct OverlayRect {
  float x, y, w, h;
  Vec4 color;
};

struct OverlayText {
  float x, y, scale;
  Vec3 color;
  std::string text;
};

class Renderer {
 public:
  ~Renderer();
  bool init(int width, int height, std::string* err = nullptr);
  void shutdown();

  void resize(int width, int height);
  void upload_mesh(const MeshData& mesh);
  void upload_texture(const TextureData& tex);

  void render_scene(const Scene& scene, const AssetManager& assets, const Mat4& view, const Mat4& proj, const Vec3& cam_pos, EntityId selected = kInvalidEntity, bool show_grid = true);
  void draw_overlay(const std::vector<OverlayRect>& rects, const std::vector<OverlayText>& texts);
  std::vector<uint8_t> read_center_pixel();

  const RenderStats& stats() const { return stats_; }

 private:
  struct GPUMesh {
    uint32_t vao = 0, vbo = 0, ebo = 0;
    uint32_t index_count = 0;
  };
  int width_ = 1280;
  int height_ = 720;
  uint32_t pbr_prog_ = 0;
  uint32_t shadow_prog_ = 0;
  uint32_t line_prog_ = 0;
  uint32_t ui_prog_ = 0;
  uint32_t shadow_fbo_ = 0;
  uint32_t shadow_tex_ = 0;
  uint32_t white_tex_ = 0;
  uint32_t grid_vao_ = 0, grid_vbo_ = 0;
  uint32_t grid_vert_count_ = 0;
  uint32_t ui_vao_ = 0, ui_vbo_ = 0;
  std::unordered_map<std::string, GPUMesh> gpu_meshes_;
  std::unordered_map<std::string, uint32_t> gpu_textures_;
  RenderStats stats_;
  bool initialized_ = false;
};

// Headless EGL offscreen OpenGL 3.3 Core helper for automated GPU tests
class HeadlessGLContext {
 public:
  ~HeadlessGLContext();
  bool create(int width, int height, std::string* err = nullptr);
  void destroy();
 private:
  void* display_ = nullptr;
  void* surface_ = nullptr;
  void* context_ = nullptr;
};

}  // namespace astra
