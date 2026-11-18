#include "astra/ecs.hpp"
#include "astra/json.hpp"

#include <algorithm>
#include <stdexcept>

namespace astra {

namespace {
json::Value vec3_to_json(const Vec3& v) {
  return json::Value(json::Array{json::Value(v.x), json::Value(v.y), json::Value(v.z)});
}
Vec3 json_to_vec3(const json::Value& v, const Vec3& def) {
  const auto& a = v.as_array();
  if (a.size() != 3) return def;
  return {static_cast<float>(a[0].as_number()), static_cast<float>(a[1].as_number()), static_cast<float>(a[2].as_number())};
}
}  // namespace

EntityId Scene::create_entity(const std::string& entity_name) {
  EntityId id = next_id_++;
  Entity e;
  e.id = id;
  e.name = entity_name;
  entities_[id] = std::move(e);
  order_.push_back(id);
  return id;
}

void Scene::destroy_entity(EntityId id) {
  entities_.erase(id);
  order_.erase(std::remove(order_.begin(), order_.end(), id), order_.end());
}

Entity* Scene::find(EntityId id) {
  auto it = entities_.find(id);
  return it == entities_.end() ? nullptr : &it->second;
}

const Entity* Scene::find(EntityId id) const {
  auto it = entities_.find(id);
  return it == entities_.end() ? nullptr : &it->second;
}

Entity* Scene::find_by_name(const std::string& entity_name) {
  for (EntityId id : order_) {
    auto it = entities_.find(id);
    if (it != entities_.end() && it->second.name == entity_name) return &it->second;
  }
  return nullptr;
}

Mat4 Scene::world_matrix(EntityId id) const {
  Mat4 acc = Mat4::identity();
  EntityId cur = id;
  for (int guard = 0; cur != kInvalidEntity && guard < 32; ++guard) {
    const Entity* e = find(cur);
    if (!e) break;
    Mat4 local = Mat4::translate(e->transform.position) *
                 Mat4::rotate_euler_deg(e->transform.rotation_deg) *
                 Mat4::scale(e->transform.scale);
    acc = local * acc;
    cur = e->transform.parent;
  }
  return acc;
}

Vec3 Scene::world_position(EntityId id) const {
  Mat4 m = world_matrix(id);
  return {m.m[12], m.m[13], m.m[14]};
}

void Scene::step_animations(float dt) {
  for (EntityId id : order_) {
    Entity* e = find(id);
    if (!e || !e->animation || !e->animation->playing || e->animation->keyframes.size() < 2) continue;
    auto& anim = *e->animation;
    anim.elapsed += dt;
    if (anim.loop && anim.duration > 0.0f) {
      while (anim.elapsed >= anim.duration) anim.elapsed -= anim.duration;
    } else if (anim.elapsed > anim.duration) {
      anim.elapsed = anim.duration;
    }
    const auto& kfs = anim.keyframes;
    size_t idx = 0;
    while (idx + 1 < kfs.size() && kfs[idx + 1].time <= anim.elapsed) ++idx;
    if (idx + 1 >= kfs.size()) {
      e->transform.position = kfs.back().position;
      e->transform.rotation_deg = kfs.back().rotation_deg;
      e->transform.scale = kfs.back().scale;
    } else {
      float span = std::max(1e-5f, kfs[idx + 1].time - kfs[idx].time);
      float alpha = std::clamp((anim.elapsed - kfs[idx].time) / span, 0.0f, 1.0f);
      e->transform.position = Vec3::lerp(kfs[idx].position, kfs[idx + 1].position, alpha);
      e->transform.rotation_deg = Vec3::lerp(kfs[idx].rotation_deg, kfs[idx + 1].rotation_deg, alpha);
      e->transform.scale = Vec3::lerp(kfs[idx].scale, kfs[idx + 1].scale, alpha);
    }
  }
}

std::string Scene::serialize_json() const {
  json::Array arr;
  for (EntityId id : order_) {
    const Entity* e = find(id);
    if (!e) continue;
    json::Object eo;
    eo["id"] = json::Value(static_cast<int>(e->id));
    eo["name"] = json::Value(e->name);
    eo["tag"] = json::Value(e->tag);
    json::Object tr;
    tr["pos"] = vec3_to_json(e->transform.position);
    tr["rot"] = vec3_to_json(e->transform.rotation_deg);
    tr["scale"] = vec3_to_json(e->transform.scale);
    tr["parent"] = json::Value(static_cast<int>(e->transform.parent));
    eo["transform"] = json::Value(std::move(tr));
    if (e->mesh) {
      json::Object mo;
      mo["mesh_id"] = json::Value(e->mesh->mesh_id);
      mo["albedo"] = vec3_to_json(e->mesh->material.albedo);
      mo["emissive"] = vec3_to_json(e->mesh->material.emissive);
      mo["metallic"] = json::Value(e->mesh->material.metallic);
      mo["roughness"] = json::Value(e->mesh->material.roughness);
      mo["cast_shadow"] = json::Value(e->mesh->cast_shadow);
      eo["mesh"] = json::Value(std::move(mo));
    }
    if (e->light) {
      json::Object lo;
      lo["kind"] = json::Value(static_cast<int>(e->light->kind));
      lo["color"] = vec3_to_json(e->light->color);
      lo["intensity"] = json::Value(e->light->intensity);
      lo["range"] = json::Value(e->light->range);
      eo["light"] = json::Value(std::move(lo));
    }
    if (e->rigidbody) {
      json::Object ro;
      ro["mass"] = json::Value(e->rigidbody->mass);
      ro["use_gravity"] = json::Value(e->rigidbody->use_gravity);
      ro["kinematic"] = json::Value(e->rigidbody->is_kinematic);
      ro["restitution"] = json::Value(e->rigidbody->restitution);
      eo["rigidbody"] = json::Value(std::move(ro));
    }
    if (e->collider) {
      json::Object co;
      co["shape"] = json::Value(static_cast<int>(e->collider->shape));
      co["half_extents"] = vec3_to_json(e->collider->half_extents);
      co["radius"] = json::Value(e->collider->radius);
      co["is_trigger"] = json::Value(e->collider->is_trigger);
      co["trigger_tag"] = json::Value(e->collider->trigger_tag);
      eo["collider"] = json::Value(std::move(co));
    }
    if (e->script) {
      json::Object so;
      so["path"] = json::Value(e->script->script_path);
      so["inline"] = json::Value(e->script->inline_source);
      eo["script"] = json::Value(std::move(so));
    }
    if (e->audio) {
      json::Object ao;
      ao["wav_id"] = json::Value(e->audio->wav_id);
      ao["play_on_start"] = json::Value(e->audio->play_on_start);
      eo["audio"] = json::Value(std::move(ao));
    }
    arr.emplace_back(std::move(eo));
  }
  json::Object root;
  root["name"] = json::Value(name);
  root["entities"] = json::Value(std::move(arr));
  return json::stringify(json::Value(std::move(root)), 2);
}

bool Scene::deserialize_json(const std::string& raw, std::string* err) {
  try {
    json::Value root = json::parse(raw);
    name = root.get("name").as_string("UntitledScene");
    order_.clear();
    entities_.clear();
    next_id_ = 1;
    for (const auto& ev : root.get("entities").as_array()) {
      Entity e;
      e.id = static_cast<EntityId>(ev.get("id").as_int(static_cast<int>(next_id_)));
      next_id_ = std::max(next_id_, e.id + 1);
      e.name = ev.get("name").as_string("Entity");
      e.tag = ev.get("tag").as_string("default");
      const auto& tr = ev.get("transform");
      e.transform.position = json_to_vec3(tr.get("pos"), {0, 0, 0});
      e.transform.rotation_deg = json_to_vec3(tr.get("rot"), {0, 0, 0});
      e.transform.scale = json_to_vec3(tr.get("scale"), {1, 1, 1});
      e.transform.parent = static_cast<EntityId>(tr.get("parent").as_int(0));
      if (ev.get("mesh").is_object()) {
        const auto& m = ev.get("mesh");
        MeshRendererComponent mr;
        mr.mesh_id = m.get("mesh_id").as_string("cube");
        mr.material.albedo = json_to_vec3(m.get("albedo"), {0.8f, 0.8f, 0.85f});
        mr.material.emissive = json_to_vec3(m.get("emissive"), {0, 0, 0});
        mr.material.metallic = static_cast<float>(m.get("metallic").as_number(0.1));
        mr.material.roughness = static_cast<float>(m.get("roughness").as_number(0.5));
        mr.cast_shadow = m.get("cast_shadow").as_bool(true);
        e.mesh = mr;
      }
      if (ev.get("light").is_object()) {
        const auto& l = ev.get("light");
        LightComponent lc;
        lc.kind = static_cast<LightKind>(l.get("kind").as_int(0));
        lc.color = json_to_vec3(l.get("color"), {1, 1, 1});
        lc.intensity = static_cast<float>(l.get("intensity").as_number(2.0));
        lc.range = static_cast<float>(l.get("range").as_number(25.0));
        e.light = lc;
      }
      if (ev.get("rigidbody").is_object()) {
        const auto& r = ev.get("rigidbody");
        RigidBodyComponent rb;
        rb.mass = static_cast<float>(r.get("mass").as_number(1.0));
        rb.use_gravity = r.get("use_gravity").as_bool(true);
        rb.is_kinematic = r.get("kinematic").as_bool(false);
        rb.restitution = static_cast<float>(r.get("restitution").as_number(0.2));
        e.rigidbody = rb;
      }
      if (ev.get("collider").is_object()) {
        const auto& c = ev.get("collider");
        ColliderComponent col;
        col.shape = static_cast<ColliderShape>(c.get("shape").as_int(0));
        col.half_extents = json_to_vec3(c.get("half_extents"), {0.5f, 0.5f, 0.5f});
        col.radius = static_cast<float>(c.get("radius").as_number(0.5));
        col.is_trigger = c.get("is_trigger").as_bool(false);
        col.trigger_tag = c.get("trigger_tag").as_string("");
        e.collider = col;
      }
      if (ev.get("script").is_object()) {
        const auto& s = ev.get("script");
        e.script = ScriptComponent{s.get("path").as_string(""), s.get("inline").as_string("")};
      }
      if (ev.get("audio").is_object()) {
        const auto& a = ev.get("audio");
        e.audio = AudioSourceComponent{a.get("wav_id").as_string(""), a.get("play_on_start").as_bool(false)};
      }
      order_.push_back(e.id);
      entities_[e.id] = std::move(e);
    }
    return true;
  } catch (const std::exception& ex) {
    if (err) *err = ex.what();
    return false;
  }
}

EntityId Scene::instantiate_prefab(const std::string& prefab_json, const Vec3& at_pos) {
  Scene tmp;
  if (!tmp.deserialize_json(prefab_json) || tmp.order().empty()) return kInvalidEntity;
  Entity src = *tmp.find(tmp.order().front());
  EntityId new_id = create_entity(src.name);
  src.id = new_id;
  src.transform.position = at_pos;
  src.transform.parent = kInvalidEntity;
  entities_[new_id] = std::move(src);
  return new_id;
}

}  // namespace astra
