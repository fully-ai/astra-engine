#include "astra/scripting.hpp"

#include <cmath>
#include <fstream>
#include <sstream>

namespace astra {

namespace {

std::string trim(const std::string& s) {
  size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

std::vector<float> parse_floats(const std::string& args) {
  std::vector<float> out;
  std::stringstream ss(args);
  std::string item;
  while (std::getline(ss, item, ',')) {
    try { out.push_back(std::stof(trim(item))); } catch (...) { out.push_back(0.0f); }
  }
  return out;
}

std::string read_text(const std::string& path) {
  std::ifstream f(path);
  if (!f) return "";
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

}  // namespace

void ScriptVM::load_entity_scripts(Scene& scene) {
  for (EntityId id : scene.order()) {
    Entity* e = scene.find(id);
    if (!e || !e->script) continue;
    LoadedScript ls;
    if (!e->script->script_path.empty()) {
      ls.source = read_text(e->script->script_path);
      std::error_code ec;
      ls.mtime = std::filesystem::last_write_time(e->script->script_path, ec);
    } else {
      ls.source = e->script->inline_source;
    }
    cache_[id] = std::move(ls);
  }
}

void ScriptVM::check_hot_reload(Scene& scene) {
  for (EntityId id : scene.order()) {
    Entity* e = scene.find(id);
    if (!e || !e->script || e->script->script_path.empty()) continue;
    std::error_code ec;
    auto mtime = std::filesystem::last_write_time(e->script->script_path, ec);
    if (ec) continue;
    auto it = cache_.find(id);
    if (it == cache_.end() || it->second.mtime != mtime) {
      std::string src = read_text(e->script->script_path);
      if (!src.empty()) {
        cache_[id] = {std::move(src), mtime, false};
      }
    }
  }
}

void ScriptVM::call_start(Scene& scene, ScriptRuntimeState& state) {
  load_entity_scripts(scene);
  for (EntityId id : scene.order()) {
    auto it = cache_.find(id);
    if (it != cache_.end() && !it->second.started) {
      it->second.started = true;
      eval_inline(scene, id, it->second.source, "on_start", 0.0f, "", state);
    }
  }
}

void ScriptVM::call_update(Scene& scene, float dt, ScriptRuntimeState& state) {
  for (EntityId id : scene.order()) {
    auto it = cache_.find(id);
    if (it != cache_.end()) {
      eval_inline(scene, id, it->second.source, "on_update", dt, "", state);
    }
  }
}

void ScriptVM::call_trigger(Scene& scene, EntityId entity, const std::string& tag, ScriptRuntimeState& state) {
  auto it = cache_.find(entity);
  if (it != cache_.end()) {
    eval_inline(scene, entity, it->second.source, "on_trigger", 0.0f, tag, state);
  }
}

bool ScriptVM::eval_inline(Scene& scene, EntityId self, const std::string& source, const std::string& fn, float dt, const std::string& arg, ScriptRuntimeState& state, std::string* err) {
  std::istringstream iss(source);
  std::string line;
  bool in_fn = false;
  int ops = 0;

  while (std::getline(iss, line)) {
    if (++ops > 10000) {
      if (err) *err = "script step budget exceeded";
      return false;
    }
    std::string t = trim(line);
    if (t.empty() || t.rfind("//", 0) == 0 || t[0] == '#') continue;

    if (!in_fn) {
      if (t.rfind("fn " + fn + "(", 0) == 0) in_fn = true;
      continue;
    }
    if (t == "end" || t == "}") break;

    Entity* ent = scene.find(self);
    if (!ent) return true;

    // Optional single-line conditional prefix: `if key("SPACE") -> jump(9.5)` or `if trigger("orb") -> ...`
    if (t.rfind("if ", 0) == 0) {
      size_t arrow = t.find("->");
      if (arrow == std::string::npos) continue;
      std::string cond = trim(t.substr(3, arrow - 3));
      std::string stmt = trim(t.substr(arrow + 2));
      bool pass = false;
      if (cond.rfind("key(\"", 0) == 0 && cond.size() > 7) {
        std::string k = cond.substr(5, cond.find('"', 5) - 5);
        pass = state.keys_down[k];
      } else if (cond.rfind("trigger(\"", 0) == 0 && cond.size() > 11) {
        std::string tg = cond.substr(9, cond.find('"', 9) - 9);
        pass = (arg == tg);
      } else if (cond == "grounded") {
        pass = ent->rigidbody && ent->rigidbody->grounded;
      }
      if (!pass) continue;
      t = stmt;
    }

    size_t lp = t.find('(');
    size_t rp = t.rfind(')');
    if (lp == std::string::npos || rp == std::string::npos || rp < lp) continue;
    std::string cmd = trim(t.substr(0, lp));
    std::string params = trim(t.substr(lp + 1, rp - lp - 1));

    if (cmd == "rotate") {
      auto v = parse_floats(params);
      if (v.size() >= 3) ent->transform.rotation_deg += Vec3{v[0], v[1], v[2]} * dt;
    } else if (cmd == "move") {
      auto v = parse_floats(params);
      if (v.size() >= 3) ent->transform.position += Vec3{v[0], v[1], v[2]} * dt;
    } else if (cmd == "set_pos") {
      auto v = parse_floats(params);
      if (v.size() >= 3) ent->transform.position = {v[0], v[1], v[2]};
    } else if (cmd == "set_vel" && ent->rigidbody) {
      auto v = parse_floats(params);
      if (v.size() >= 3) {
        ent->rigidbody->velocity.x = v[0];
        ent->rigidbody->velocity.z = v[2];
      }
    } else if (cmd == "jump" && ent->rigidbody && ent->rigidbody->grounded) {
      auto v = parse_floats(params);
      if (!v.empty()) {
        ent->rigidbody->velocity.y = v[0];
        ent->rigidbody->grounded = false;
      }
    } else if (cmd == "patrol_x") {
      auto v = parse_floats(params);
      if (v.size() >= 2) {
        ent->transform.position.x = std::sin(ent->transform.rotation_deg.y * 0.02f + v[1]) * v[0];
        ent->transform.rotation_deg.y += 90.0f * dt;
      }
    } else if (cmd == "add_score") {
      auto v = parse_floats(params);
      if (!v.empty()) state.score += static_cast<int>(v[0]);
    } else if (cmd == "play_sound") {
      std::string s = params;
      if (s.size() >= 2 && s.front() == '"' && s.back() == '"') s = s.substr(1, s.size() - 2);
      if (state.play_sound) state.play_sound(s);
    } else if (cmd == "win") {
      state.won = true;
    } else if (cmd == "load_scene") {
      std::string s = params;
      if (s.size() >= 2 && s.front() == '"' && s.back() == '"') s = s.substr(1, s.size() - 2);
      state.next_scene = s;
    } else if (cmd == "destroy_self") {
      scene.destroy_entity(self);
      return true;
    } else if (cmd == "log") {
      state.logs.push_back(params);
    }
  }
  return true;
}

}  // namespace astra
