#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "astra/ecs.hpp"

namespace astra {

struct ScriptRuntimeState {
  int score = 0;
  bool won = false;
  std::string next_scene;
  std::unordered_map<std::string, bool> keys_down;
  std::vector<std::string> logs;
  std::function<void(const std::string&)> play_sound;
};

class ScriptVM {
 public:
  void load_entity_scripts(Scene& scene);
  void check_hot_reload(Scene& scene);
  void call_start(Scene& scene, ScriptRuntimeState& state);
  void call_update(Scene& scene, float dt, ScriptRuntimeState& state);
  void call_trigger(Scene& scene, EntityId entity, const std::string& tag, ScriptRuntimeState& state);

  bool eval_inline(Scene& scene, EntityId self, const std::string& source, const std::string& fn, float dt, const std::string& arg, ScriptRuntimeState& state, std::string* err = nullptr);

 private:
  struct LoadedScript {
    std::string source;
    std::filesystem::file_time_type mtime{};
    bool started = false;
  };
  std::unordered_map<EntityId, LoadedScript> cache_;
};

}  // namespace astra
