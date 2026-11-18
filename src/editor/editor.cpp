#include "astra/editor.hpp"

#include <SDL2/SDL.h>
#include <cmath>
#include <fstream>
#include <sstream>

namespace astra {

bool EditorApp::init(int width, int height, const std::string& scene_path) {
  if (!scene_path.empty()) scene_path_ = scene_path;

  std::ifstream ifs(scene_path_);
  if (ifs) {
    std::ostringstream ss;
    ss << ifs.rdbuf();
    scene_.deserialize_json(ss.str());
  } else {
    scene_.name = "AstraEditorProject";
    EntityId sun = scene_.create_entity("SunLight");
    scene_.find(sun)->transform.position = {8, 14, 6};
    scene_.find(sun)->light = LightComponent{LightKind::Directional, {1.0f, 0.96f, 0.90f}, 2.4f, 40.0f, 0.85f};

    EntityId floor = scene_.create_entity("Floor");
    scene_.find(floor)->transform.scale = {14.0f, 0.5f, 14.0f};
    scene_.find(floor)->transform.position = {0.0f, -0.25f, 0.0f};
    scene_.find(floor)->mesh = MeshRendererComponent{"cube", {{0.24f, 0.28f, 0.36f}, {0,0,0}, 0.1f, 0.7f, "checker"}, true, false};
    scene_.find(floor)->collider = ColliderComponent{ColliderShape::Box, {0.5f, 0.5f, 0.5f}, 0.5f, false, ""};

    EntityId hero = scene_.create_entity("HeroCube");
    scene_.find(hero)->transform.position = {0.0f, 2.5f, 0.0f};
    scene_.find(hero)->mesh = MeshRendererComponent{"cube", {{0.25f, 0.75f, 0.98f}, {0,0,0}, 0.25f, 0.35f, ""}, true, true};
    scene_.find(hero)->rigidbody = RigidBodyComponent{1.0f, {0,0,0}, true, false, 0.25f, false};
    scene_.find(hero)->collider = ColliderComponent{ColliderShape::Box, {0.5f, 0.5f, 0.5f}, 0.5f, false, ""};
    scene_.find(hero)->script = ScriptComponent{"", "fn on_update(dt)\n  rotate(0, 45, 0)\nend"};
    selected_ = hero;
  }

  if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_Window* win = SDL_CreateWindow("Astra Engine — 3D Visual Editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (!win) return false;
  SDL_GLContext ctx = SDL_GL_CreateContext(win);
  if (!ctx) return false;
  window_ = win;
  gl_ctx_ = ctx;

  std::string err;
  if (!renderer_.init(width, height, &err)) {
    console_msg_ = "Renderer init failed: " + err;
    return false;
  }
  audio_.init();
  return true;
}

void EditorApp::run() {
  SDL_Window* win = static_cast<SDL_Window*>(window_);
  bool running = true;
  bool dragging = false;
  int w = 1280, h = 760;
  uint64_t last_ticks = SDL_GetTicks64();
  std::string saved_play_snapshot;

  while (running) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) running = false;
      else if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        w = ev.window.data1; h = ev.window.data2;
        renderer_.resize(w, h);
      } else if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_RIGHT) {
        dragging = true;
      } else if (ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_RIGHT) {
        dragging = false;
      } else if (ev.type == SDL_MOUSEMOTION && dragging) {
        cam_yaw_ += ev.motion.xrel * 0.35f;
        cam_pitch_ = std::clamp(cam_pitch_ - ev.motion.yrel * 0.35f, -80.0f, 80.0f);
      } else if (ev.type == SDL_MOUSEWHEEL) {
        cam_dist_ = std::clamp(cam_dist_ - ev.wheel.y * 0.8f, 2.5f, 40.0f);
      } else if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
        // Raycast picking inside viewport
        float nx = (2.0f * ev.button.x) / static_cast<float>(w) - 1.0f;
        float ny = 1.0f - (2.0f * ev.button.y) / static_cast<float>(h);
        float yr = radians(cam_yaw_), pr = radians(cam_pitch_);
        Vec3 cam_pos = cam_target_ + Vec3{std::cos(pr)*std::cos(yr), std::sin(-pr), std::cos(pr)*std::sin(yr)} * cam_dist_;
        Vec3 fwd = (cam_target_ - cam_pos).normalized();
        Vec3 right = Vec3::cross(fwd, {0,1,0}).normalized();
        Vec3 up = Vec3::cross(right, fwd);
        float asp = static_cast<float>(w) / std::max(1, h);
        Vec3 dir = (fwd + right * (nx * asp * 0.577f) + up * (ny * 0.577f)).normalized();
        RaycastHit hit = physics_.raycast(scene_, {cam_pos, dir}, 100.0f);
        if (hit.hit) {
          selected_ = hit.entity;
          if (const auto* se = scene_.find(selected_)) console_msg_ = "Selected: " + se->name;
        }
      } else if (ev.type == SDL_KEYDOWN) {
        auto sym = ev.key.keysym.sym;
        bool ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;
        if (sym == SDLK_p) {
          playing_ = !playing_;
          if (playing_) {
            saved_play_snapshot = scene_.serialize_json();
            vm_.call_start(scene_, script_state_);
            console_msg_ = "PLAY MODE ACTIVE";
          } else {
            if (!saved_play_snapshot.empty()) scene_.deserialize_json(saved_play_snapshot);
            console_msg_ = "STOPPED -> SCENE RESTORED";
          }
        } else if (ctrl && sym == SDLK_s) {
          std::ofstream ofs(scene_path_);
          ofs << scene_.serialize_json();
          console_msg_ = "Saved scene to " + scene_path_;
        } else if (sym == SDLK_1) {
          EntityId id = scene_.create_entity("Cube_" + std::to_string(scene_.order().size()));
          scene_.find(id)->transform.position = {0, 3, 0};
          scene_.find(id)->mesh = MeshRendererComponent{"cube", {{0.95f, 0.45f, 0.25f}, {0,0,0}, 0.2f, 0.4f, ""}, true, true};
          scene_.find(id)->rigidbody = RigidBodyComponent{1.0f, {0,0,0}, true, false, 0.2f, false};
          scene_.find(id)->collider = ColliderComponent{};
          selected_ = id;
          console_msg_ = "Spawned Cube";
        } else if (sym == SDLK_2) {
          EntityId id = scene_.create_entity("Sphere_" + std::to_string(scene_.order().size()));
          scene_.find(id)->transform.position = {1.5f, 3.0f, 0};
          scene_.find(id)->mesh = MeshRendererComponent{"sphere", {{0.4f, 0.9f, 0.55f}, {0,0,0}, 0.4f, 0.25f, ""}, true, true};
          scene_.find(id)->collider = ColliderComponent{ColliderShape::Sphere, {0.5f,0.5f,0.5f}, 0.5f, false, ""};
          selected_ = id;
          console_msg_ = "Spawned Sphere";
        } else if (selected_ != kInvalidEntity) {
          if (Entity* se = scene_.find(selected_)) {
            if (sym == SDLK_LEFT) se->transform.position.x -= 0.25f;
            if (sym == SDLK_RIGHT) se->transform.position.x += 0.25f;
            if (sym == SDLK_UP) se->transform.position.z -= 0.25f;
            if (sym == SDLK_DOWN) se->transform.position.z += 0.25f;
            if (sym == SDLK_PAGEUP) se->transform.position.y += 0.25f;
            if (sym == SDLK_PAGEDOWN) se->transform.position.y -= 0.25f;
          }
        }
      }
    }

    uint64_t now = SDL_GetTicks64();
    float dt = std::clamp((now - last_ticks) / 1000.0f, 0.001f, 0.05f);
    last_ticks = now;

    if (playing_) {
      vm_.check_hot_reload(scene_);
      scene_.step_animations(dt);
      auto triggers = physics_.step(scene_, dt);
      for (const auto& tr : triggers) vm_.call_trigger(scene_, tr.self, tr.tag, script_state_);
      vm_.call_update(scene_, dt, script_state_);
    }

    float yr = radians(cam_yaw_), pr = radians(cam_pitch_);
    Vec3 cam_pos = cam_target_ + Vec3{std::cos(pr)*std::cos(yr), std::sin(-pr), std::cos(pr)*std::sin(yr)} * cam_dist_;
    Mat4 view = Mat4::look_at(cam_pos, cam_target_, {0, 1, 0});
    Mat4 proj = Mat4::perspective(60.0f, static_cast<float>(w) / std::max(1, h), 0.1f, 200.0f);
    renderer_.render_scene(scene_, assets_, view, proj, cam_pos, selected_, true);

    // Editor Overlay UI Panels (Hierarchy + Inspector + Console + Profiler)
    std::vector<OverlayRect> rects = {
      {10, 10, 235, 360, {0.04f, 0.07f, 0.12f, 0.85f}},
      {static_cast<float>(w - 285), 10, 275, 230, {0.04f, 0.07f, 0.12f, 0.85f}},
      {10, static_cast<float>(h - 52), static_cast<float>(w - 20), 42, {0.04f, 0.07f, 0.12f, 0.88f}},
    };
    std::vector<OverlayText> texts = {
      {20, 20, 1.5f, {0.4f, 0.9f, 1.0f}, playing_ ? "ASTRA EDITOR [PLAY]" : "ASTRA EDITOR [EDIT]"},
      {20, 42, 1.2f, {0.7f, 0.8f, 0.9f}, "HIERARCHY (1 CUBE 2 SPH)"},
    };
    float hy = 66.0f;
    for (EntityId eid : scene_.order()) {
      const Entity* e = scene_.find(eid);
      if (!e || hy > 340.0f) continue;
      Vec3 col = (eid == selected_) ? Vec3{0.2f, 1.0f, 0.7f} : Vec3{0.85f, 0.85f, 0.9f};
      texts.push_back({24, hy, 1.2f, col, (eid == selected_ ? "> " : "  ") + e->name});
      hy += 18.0f;
    }
    const auto& rs = renderer_.stats();
    texts.push_back({static_cast<float>(w - 270), 20, 1.4f, {0.8f, 0.6f, 1.0f}, "INSPECTOR & PROFILER"});
    texts.push_back({static_cast<float>(w - 270), 44, 1.2f, {0.9f, 0.9f, 0.9f}, "DRAWS:" + std::to_string(rs.draw_calls) + " TRIS:" + std::to_string(rs.triangles)});
    texts.push_back({static_cast<float>(w - 270), 62, 1.2f, {0.9f, 0.9f, 0.9f}, "FRAME MS:" + std::to_string(static_cast<int>(rs.frame_ms))});
    if (const Entity* se = scene_.find(selected_)) {
      texts.push_back({static_cast<float>(w - 270), 92, 1.2f, {0.4f, 1.0f, 0.8f}, "SEL: " + se->name});
      texts.push_back({static_cast<float>(w - 270), 112, 1.2f, {0.8f, 0.9f, 1.0f},
        "POS: " + std::to_string(static_cast<int>(se->transform.position.x*10)/10.0f).substr(0,4) + " " +
        std::to_string(static_cast<int>(se->transform.position.y*10)/10.0f).substr(0,4) + " " +
        std::to_string(static_cast<int>(se->transform.position.z*10)/10.0f).substr(0,4)});
    }
    texts.push_back({22, static_cast<float>(h - 36), 1.3f, {0.9f, 0.95f, 1.0f}, console_msg_});
    renderer_.draw_overlay(rects, texts);

    SDL_GL_SwapWindow(win);
  }
}

void EditorApp::shutdown() {
  audio_.shutdown();
  renderer_.shutdown();
  if (gl_ctx_) { SDL_GL_DeleteContext(static_cast<SDL_GLContext>(gl_ctx_)); gl_ctx_ = nullptr; }
  if (window_) { SDL_DestroyWindow(static_cast<SDL_Window*>(window_)); window_ = nullptr; }
  SDL_Quit();
}

}  // namespace astra
