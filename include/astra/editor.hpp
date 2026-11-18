#pragma once

#include <string>

#include "astra/assets.hpp"
#include "astra/audio.hpp"
#include "astra/ecs.hpp"
#include "astra/physics.hpp"
#include "astra/renderer.hpp"
#include "astra/scripting.hpp"

namespace astra {

class EditorApp {
 public:
  bool init(int width = 1280, int height = 760, const std::string& scene_path = "");
  void run();
  void shutdown();

  // Exposed for deterministic non-windowed integration testing
  Scene& scene() { return scene_; }
  AssetManager& assets() { return assets_; }

 private:
  Scene scene_;
  AssetManager assets_;
  PhysicsWorld physics_;
  ScriptVM vm_;
  ScriptRuntimeState script_state_;
  AudioEngine audio_;
  Renderer renderer_;
  std::string scene_path_ = "project_scene.astra.json";
  EntityId selected_ = kInvalidEntity;
  bool playing_ = false;
  float cam_yaw_ = -35.0f;
  float cam_pitch_ = -22.0f;
  float cam_dist_ = 11.0f;
  Vec3 cam_target_{0.0f, 1.0f, 0.0f};
  std::string console_msg_ = "Ready. Left-click select, Right-drag orbit, P Play/Stop, Ctrl+S Save";
  void* window_ = nullptr;
  void* gl_ctx_ = nullptr;
};

}  // namespace astra
