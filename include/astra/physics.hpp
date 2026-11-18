#pragma once

#include <string>
#include <vector>

#include "astra/ecs.hpp"

namespace astra {

struct TriggerHit {
  EntityId self = kInvalidEntity;
  EntityId other = kInvalidEntity;
  std::string tag;
};

struct RaycastHit {
  bool hit = false;
  EntityId entity = kInvalidEntity;
  float distance = 0.0f;
  Vec3 point;
};

class PhysicsWorld {
 public:
  Vec3 gravity{0.0f, -18.0f, 0.0f};

  std::vector<TriggerHit> step(Scene& scene, float dt);
  RaycastHit raycast(const Scene& scene, const Ray& ray, float max_dist, EntityId ignore = kInvalidEntity) const;
  static AABB world_aabb(const Scene& scene, const Entity& ent);
};

}  // namespace astra
