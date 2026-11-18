#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "astra/math.hpp"

namespace astra {

using EntityId = uint32_t;
constexpr EntityId kInvalidEntity = 0;

struct Material {
  Vec3 albedo{0.8f, 0.8f, 0.85f};
  Vec3 emissive{0.0f, 0.0f, 0.0f};
  float metallic = 0.1f;
  float roughness = 0.5f;
  std::string texture_id;
};

struct TransformComponent {
  Vec3 position{0, 0, 0};
  Vec3 rotation_deg{0, 0, 0};
  Vec3 scale{1, 1, 1};
  EntityId parent = kInvalidEntity;
};

struct MeshRendererComponent {
  std::string mesh_id = "cube";
  Material material;
  bool visible = true;
  bool cast_shadow = true;
};

enum class LightKind { Directional, Point, Spot };

struct LightComponent {
  LightKind kind = LightKind::Directional;
  Vec3 color{1.0f, 0.97f, 0.92f};
  float intensity = 2.0f;
  float range = 25.0f;
  float spot_cos = 0.85f;
};

struct CameraComponent {
  float fov_deg = 60.0f;
  float near_z = 0.1f;
  float far_z = 250.0f;
  bool primary = true;
};

struct RigidBodyComponent {
  float mass = 1.0f;
  Vec3 velocity{0, 0, 0};
  bool use_gravity = true;
  bool is_kinematic = false;
  float restitution = 0.2f;
  bool grounded = false;
};

enum class ColliderShape { Box, Sphere };

struct ColliderComponent {
  ColliderShape shape = ColliderShape::Box;
  Vec3 half_extents{0.5f, 0.5f, 0.5f};
  float radius = 0.5f;
  bool is_trigger = false;
  std::string trigger_tag;
};

struct ScriptComponent {
  std::string script_path;
  std::string inline_source;
};

struct Keyframe {
  float time = 0.0f;
  Vec3 position{0, 0, 0};
  Vec3 rotation_deg{0, 0, 0};
  Vec3 scale{1, 1, 1};
};

struct AnimationComponent {
  std::vector<Keyframe> keyframes;
  float duration = 2.0f;
  float elapsed = 0.0f;
  bool loop = true;
  bool playing = true;
};

struct AudioSourceComponent {
  std::string wav_id;
  bool play_on_start = false;
};

struct Entity {
  EntityId id = kInvalidEntity;
  std::string name;
  std::string tag = "default";
  TransformComponent transform;
  std::optional<MeshRendererComponent> mesh;
  std::optional<LightComponent> light;
  std::optional<CameraComponent> camera;
  std::optional<RigidBodyComponent> rigidbody;
  std::optional<ColliderComponent> collider;
  std::optional<ScriptComponent> script;
  std::optional<AnimationComponent> animation;
  std::optional<AudioSourceComponent> audio;
};

class Scene {
 public:
  std::string name = "UntitledScene";

  EntityId create_entity(const std::string& entity_name);
  void destroy_entity(EntityId id);
  Entity* find(EntityId id);
  const Entity* find(EntityId id) const;
  Entity* find_by_name(const std::string& entity_name);

  Mat4 world_matrix(EntityId id) const;
  Vec3 world_position(EntityId id) const;

  void step_animations(float dt);
  EntityId instantiate_prefab(const std::string& prefab_json, const Vec3& at_pos);

  std::string serialize_json() const;
  bool deserialize_json(const std::string& raw, std::string* err = nullptr);

  const std::vector<EntityId>& order() const { return order_; }
  std::unordered_map<EntityId, Entity>& entities() { return entities_; }
  const std::unordered_map<EntityId, Entity>& entities() const { return entities_; }

 private:
  EntityId next_id_ = 1;
  std::vector<EntityId> order_;
  std::unordered_map<EntityId, Entity> entities_;
};

}  // namespace astra
