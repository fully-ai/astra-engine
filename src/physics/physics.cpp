#include "astra/physics.hpp"

#include <algorithm>
#include <cmath>

namespace astra {

AABB PhysicsWorld::world_aabb(const Scene& scene, const Entity& ent) {
  Vec3 wp = scene.world_position(ent.id);
  Vec3 he{0.5f, 0.5f, 0.5f};
  if (ent.collider) {
    if (ent.collider->shape == ColliderShape::Sphere) {
      float r = ent.collider->radius;
      he = {r, r, r};
    } else {
      he = ent.collider->half_extents * ent.transform.scale;
    }
  }
  return {wp - he, wp + he};
}

std::vector<TriggerHit> PhysicsWorld::step(Scene& scene, float dt) {
  std::vector<TriggerHit> triggers;
  if (dt <= 0.0f) return triggers;
  dt = std::min(dt, 0.05f);

  // 1. Integrate dynamic rigid bodies
  for (EntityId id : scene.order()) {
    Entity* e = scene.find(id);
    if (!e || !e->rigidbody || e->rigidbody->is_kinematic) continue;
    auto& rb = *e->rigidbody;
    if (rb.use_gravity) rb.velocity += gravity * dt;
    e->transform.position += rb.velocity * dt;
    rb.grounded = false;
  }

  // 2. Pairwise AABB narrowphase & impulse/trigger detection
  const auto& ids = scene.order();
  for (size_t i = 0; i < ids.size(); ++i) {
    Entity* a = scene.find(ids[i]);
    if (!a || !a->collider) continue;
    for (size_t j = i + 1; j < ids.size(); ++j) {
      Entity* b = scene.find(ids[j]);
      if (!b || !b->collider) continue;
      AABB ba = world_aabb(scene, *a);
      AABB bb = world_aabb(scene, *b);
      if (!ba.intersects(bb)) continue;

      if (a->collider->is_trigger || b->collider->is_trigger) {
        if (a->collider->is_trigger) triggers.push_back({a->id, b->id, a->collider->trigger_tag});
        if (b->collider->is_trigger) triggers.push_back({b->id, a->id, b->collider->trigger_tag});
        continue;
      }

      // Solid collision resolution along minimum penetration axis
      bool dyn_a = a->rigidbody && !a->rigidbody->is_kinematic;
      bool dyn_b = b->rigidbody && !b->rigidbody->is_kinematic;
      if (!dyn_a && !dyn_b) continue;

      Vec3 ca = (ba.min + ba.max) * 0.5f;
      Vec3 cb = (bb.min + bb.max) * 0.5f;
      float ox = std::min(ba.max.x, bb.max.x) - std::max(ba.min.x, bb.min.x);
      float oy = std::min(ba.max.y, bb.max.y) - std::max(ba.min.y, bb.min.y);
      float oz = std::min(ba.max.z, bb.max.z) - std::max(ba.min.z, bb.min.z);

      Vec3 normal{0, 1, 0};
      float pen = oy;
      if (ox < pen && ox <= oz) {
        pen = ox;
        normal = {ca.x < cb.x ? -1.0f : 1.0f, 0, 0};
      } else if (oz < pen && oz <= ox) {
        pen = oz;
        normal = {0, 0, ca.z < cb.z ? -1.0f : 1.0f};
      } else {
        normal = {0, ca.y < cb.y ? -1.0f : 1.0f, 0};
      }

      if (dyn_a && !dyn_b) {
        a->transform.position += normal * pen;
        float vn = Vec3::dot(a->rigidbody->velocity, normal);
        if (vn < 0) a->rigidbody->velocity -= normal * (1.0f + a->rigidbody->restitution) * vn;
        if (normal.y > 0.5f) a->rigidbody->grounded = true;
      } else if (dyn_b && !dyn_a) {
        b->transform.position -= normal * pen;
        float vn = Vec3::dot(b->rigidbody->velocity, normal);
        if (vn > 0) b->rigidbody->velocity -= normal * (1.0f + b->rigidbody->restitution) * vn;
        if (normal.y < -0.5f) b->rigidbody->grounded = true;
      } else {
        a->transform.position += normal * (pen * 0.5f);
        b->transform.position -= normal * (pen * 0.5f);
      }
    }
  }
  return triggers;
}

RaycastHit PhysicsWorld::raycast(const Scene& scene, const Ray& ray, float max_dist, EntityId ignore) const {
  RaycastHit best;
  best.distance = max_dist;
  for (EntityId id : scene.order()) {
    if (id == ignore) continue;
    const Entity* e = scene.find(id);
    if (!e || (!e->collider && !e->mesh)) continue;
    AABB box = world_aabb(scene, *e);
    float t = 0.0f;
    if (intersect_ray_aabb(ray, box, best.distance, t)) {
      best.hit = true;
      best.entity = id;
      best.distance = t;
      best.point = ray.origin + ray.dir * t;
    }
  }
  return best;
}

}  // namespace astra
