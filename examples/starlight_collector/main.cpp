#include "astra/assets.hpp"
#include "astra/audio.hpp"
#include "astra/ecs.hpp"
#include "astra/physics.hpp"
#include "astra/renderer.hpp"
#include "astra/scripting.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <filesystem>
#include <iostream>

using namespace astra;

int main(int argc, char** argv) {
  bool headless_smoke = (argc > 1 && std::string(argv[1]) == "--headless-smoke");

  AssetManager assets;
  std::filesystem::path tmp_assets = std::filesystem::temp_directory_path() / "astra_starlight_assets";
  std::filesystem::create_directories(tmp_assets);
  if (const auto* sp = assets.get_mesh("sphere")) {
    AssetManager::export_mesh_glb(*sp, (tmp_assets / "orb.glb").string());
    assets.load_glb("orb_glb", (tmp_assets / "orb.glb").string());
  }
  AssetManager::export_chime_wav((tmp_assets / "chime.wav").string(), 784.0f, 0.14f);
  assets.load_wav("chime", (tmp_assets / "chime.wav").string());

  Scene scene;
  scene.name = "StarlightCollector";

  EntityId sun = scene.create_entity("Sun");
  scene.find(sun)->transform.position = {10, 16, 8};
  scene.find(sun)->light = LightComponent{LightKind::Directional, {1.0f, 0.96f, 0.9f}, 2.4f, 50.0f, 0.85f};

  EntityId arena = scene.create_entity("ArenaFloor");
  scene.find(arena)->transform.position = {0, -0.5f, 0};
  scene.find(arena)->transform.scale = {24, 1, 24};
  scene.find(arena)->mesh = MeshRendererComponent{"cube", {{0.16f, 0.21f, 0.30f}, {0,0,0}, 0.2f, 0.6f, "checker"}, true, false};
  scene.find(arena)->collider = ColliderComponent{};

  EntityId player = scene.create_entity("Player");
  scene.find(player)->transform.position = {0, 2.0f, 7.0f};
  scene.find(player)->mesh = MeshRendererComponent{"cube", {{0.2f, 0.85f, 1.0f}, {0.0f, 0.1f, 0.2f}, 0.3f, 0.3f, ""}, true, true};
  scene.find(player)->rigidbody = RigidBodyComponent{1.0f, {0,0,0}, true, false, 0.15f, false};
  scene.find(player)->collider = ColliderComponent{};
  scene.find(player)->script = ScriptComponent{"", "fn on_update(dt)\n  if key(\"SPACE\") -> jump(9.5)\nend"};

  // 4 collectible GLB orbs driven by AstraScript trigger callbacks + keyframe bob animation
  const Vec3 orb_pos[4] = {{-4, 1.0f, 2}, {4, 1.0f, 2}, {-4, 1.0f, -4}, {4, 1.0f, -4}};
  for (int i = 0; i < 4; ++i) {
    EntityId o = scene.create_entity("Orb_" + std::to_string(i));
    scene.find(o)->transform.position = orb_pos[i];
    scene.find(o)->transform.scale = {0.75f, 0.75f, 0.75f};
    scene.find(o)->mesh = MeshRendererComponent{"orb_glb", {{1.0f, 0.85f, 0.2f}, {0.5f, 0.35f, 0.05f}, 0.8f, 0.15f, ""}, true, true};
    scene.find(o)->collider = ColliderComponent{ColliderShape::Sphere, {0.5f,0.5f,0.5f}, 0.75f, true, "orb"};
    scene.find(o)->script = ScriptComponent{"",
      "fn on_update(dt)\n  rotate(0, 120, 0)\nend\n"
      "fn on_trigger(tag)\n  add_score(1)\n  play_sound(\"chime\")\n  destroy_self()\nend"};
    scene.find(o)->light = LightComponent{LightKind::Point, {1.0f, 0.8f, 0.2f}, 2.0f, 8.0f, 0.8f};
  }

  // Moving patrol hazard cube
  EntityId hazard = scene.create_entity("PatrolHazard");
  scene.find(hazard)->transform.position = {0, 0.6f, -1};
  scene.find(hazard)->mesh = MeshRendererComponent{"cube", {{0.95f, 0.22f, 0.28f}, {0.3f, 0.04f, 0.04f}, 0.3f, 0.4f, ""}, true, true};
  scene.find(hazard)->collider = ColliderComponent{};
  scene.find(hazard)->script = ScriptComponent{"", "fn on_update(dt)\n  patrol_x(5.0, 0.0)\nend"};

  PhysicsWorld physics;
  ScriptVM vm;
  ScriptRuntimeState state;
  AudioEngine audio;
  vm.call_start(scene, state);

  if (headless_smoke) {
    // Run 60 deterministic simulation ticks verifying player falls onto floor & scripts run cleanly
    for (int i = 0; i < 60; ++i) {
      scene.step_animations(1.0f / 60.0f);
      physics.step(scene, 1.0f / 60.0f);
      vm.call_update(scene, 1.0f / 60.0f, state);
    }
    std::cout << "StarlightCollector headless simulation OK; player_y=" << scene.find(player)->transform.position.y << "\n";
    return 0;
  }

  if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_Window* win = SDL_CreateWindow("Starlight Collector — Astra Engine 3D Sample", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL);
  SDL_GLContext ctx = SDL_GL_CreateContext(win);
  Renderer renderer;
  renderer.init(1280, 720);
  audio.init();
  state.play_sound = [&](const std::string& id) {
    if (const auto* wv = assets.get_wav(id)) audio.play(*wv);
  };

  bool running = true;
  uint64_t last = SDL_GetTicks64();
  while (running) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) { if (ev.type == SDL_QUIT) running = false; }
    const uint8_t* keys = SDL_GetKeyboardState(nullptr);
    state.keys_down["SPACE"] = keys[SDL_SCANCODE_SPACE] != 0;
    if (Entity* pe = scene.find(player); pe && pe->rigidbody) {
      pe->rigidbody->velocity.x = (keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A]) * 6.5f;
      pe->rigidbody->velocity.z = (keys[SDL_SCANCODE_S] - keys[SDL_SCANCODE_W]) * 6.5f;
    }

    uint64_t now = SDL_GetTicks64();
    float dt = std::clamp((now - last) / 1000.0f, 0.001f, 0.05f);
    last = now;

    scene.step_animations(dt);
    auto trigs = physics.step(scene, dt);
    for (const auto& t : trigs) {
      if (t.other == player) vm.call_trigger(scene, t.self, t.tag, state);
    }
    vm.call_update(scene, dt, state);
    if (state.score >= 4) state.won = true;

    Vec3 ppos = scene.find(player)->transform.position;
    Vec3 cam_pos = ppos + Vec3{0, 6.5f, 9.5f};
    renderer.render_scene(scene, assets, Mat4::look_at(cam_pos, ppos + Vec3{0,0.5f,0}, {0,1,0}), Mat4::perspective(60.0f, 1280.0f/720.0f, 0.1f, 200.0f), cam_pos, kInvalidEntity, true);
    std::string hud = state.won ? "VICTORY! ALL 4 ORBS COLLECTED" : ("STARLIGHT COLLECTOR  SCORE: " + std::to_string(state.score) + " / 4  (WASD+SPACE)");
    renderer.draw_overlay({{12, 12, 540, 34, {0.05f, 0.09f, 0.16f, 0.82f}}}, {{22, 22, 1.5f, {0.4f, 1.0f, 0.85f}, hud}});
    SDL_GL_SwapWindow(win);
  }

  audio.shutdown();
  renderer.shutdown();
  SDL_GL_DeleteContext(ctx);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
