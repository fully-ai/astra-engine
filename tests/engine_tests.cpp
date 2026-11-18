#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <thread>

#include "astra/assets.hpp"
#include "astra/ecs.hpp"
#include "astra/json.hpp"
#include "astra/math.hpp"
#include "astra/physics.hpp"
#include "astra/renderer.hpp"
#include "astra/scripting.hpp"

using namespace astra;

TEST(MathTests, MatrixAndRaycastIntersection) {
  Mat4 t = Mat4::translate({2.0f, 3.0f, 4.0f});
  Vec4 p = t * Vec4{1.0f, 0.0f, 0.0f, 1.0f};
  EXPECT_FLOAT_EQ(p.x, 3.0f);
  EXPECT_FLOAT_EQ(p.y, 3.0f);
  EXPECT_FLOAT_EQ(p.z, 4.0f);

  AABB box{{-1, -1, -1}, {1, 1, 1}};
  float hit_t = 0.0f;
  EXPECT_TRUE(intersect_ray_aabb({{0, 0, -5}, {0, 0, 1}}, box, 20.0f, hit_t));
  EXPECT_NEAR(hit_t, 4.0f, 1e-4f);
  EXPECT_FALSE(intersect_ray_aabb({{5, 0, -5}, {0, 0, 1}}, box, 20.0f, hit_t));
}

TEST(ECSTests, HierarchySerializationAndAnimation) {
  Scene s;
  s.name = "TestLevel";
  EntityId parent = s.create_entity("Parent");
  s.find(parent)->transform.position = {10, 0, 0};

  EntityId child = s.create_entity("Child");
  s.find(child)->transform.parent = parent;
  s.find(child)->transform.position = {0, 2, 0};
  s.find(child)->mesh = MeshRendererComponent{"cube", {{1, 0, 0}, {0,0,0}, 0.5f, 0.3f, ""}, true, true};
  s.find(child)->animation = AnimationComponent{
      {{0.0f, {0, 2, 0}, {0, 0, 0}, {1, 1, 1}}, {1.0f, {0, 4, 0}, {0, 0, 0}, {1, 1, 1}}},
      1.0f, 0.0f, true, true};

  Vec3 wp = s.world_position(child);
  EXPECT_FLOAT_EQ(wp.x, 10.0f);
  EXPECT_FLOAT_EQ(wp.y, 2.0f);

  s.step_animations(0.5f);
  EXPECT_NEAR(s.find(child)->transform.position.y, 3.0f, 1e-4f);

  std::string raw = s.serialize_json();
  Scene s2;
  std::string err;
  ASSERT_TRUE(s2.deserialize_json(raw, &err)) << err;
  EXPECT_EQ(s2.name, "TestLevel");
  ASSERT_NE(s2.find_by_name("Child"), nullptr);
  EXPECT_EQ(s2.find_by_name("Child")->mesh->mesh_id, "cube");
}

TEST(AssetTests, GLBExportAndImportRoundtripPlusWAV) {
  AssetManager am;
  const MeshData* cube = am.get_mesh("cube");
  ASSERT_NE(cube, nullptr);

  auto tmp = std::filesystem::temp_directory_path() / "astra_test_assets";
  std::filesystem::create_directories(tmp);
  std::string glb_path = (tmp / "cube.glb").string();
  ASSERT_TRUE(AssetManager::export_mesh_glb(*cube, glb_path));

  std::string err;
  ASSERT_TRUE(am.load_glb("imported_cube", glb_path, &err)) << err;
  const MeshData* imp = am.get_mesh("imported_cube");
  ASSERT_NE(imp, nullptr);
  EXPECT_EQ(imp->vertices.size(), cube->vertices.size());
  EXPECT_EQ(imp->indices.size(), cube->indices.size());

  std::string wav_path = (tmp / "chime.wav").string();
  ASSERT_TRUE(AssetManager::export_chime_wav(wav_path, 440.0f, 0.1f));
  ASSERT_TRUE(am.load_wav("chime", wav_path, &err)) << err;
  EXPECT_GT(am.get_wav("chime")->samples.size(), 1000u);

  std::filesystem::remove_all(tmp);
}

TEST(PhysicsTests, GravityCollisionTriggerAndRaycast) {
  Scene s;
  EntityId floor = s.create_entity("Floor");
  s.find(floor)->transform.position = {0, -0.5f, 0};
  s.find(floor)->transform.scale = {10, 1, 10};
  s.find(floor)->collider = ColliderComponent{ColliderShape::Box, {0.5f, 0.5f, 0.5f}, 0.5f, false, ""};

  EntityId box = s.create_entity("Box");
  s.find(box)->transform.position = {0, 2.0f, 0};
  s.find(box)->rigidbody = RigidBodyComponent{1.0f, {0, 0, 0}, true, false, 0.0f, false};
  s.find(box)->collider = ColliderComponent{ColliderShape::Box, {0.5f, 0.5f, 0.5f}, 0.5f, false, ""};

  EntityId goal = s.create_entity("GoalTrigger");
  s.find(goal)->transform.position = {0, 0.5f, 0};
  s.find(goal)->collider = ColliderComponent{ColliderShape::Sphere, {0.5f, 0.5f, 0.5f}, 0.6f, true, "goal"};

  PhysicsWorld pw;
  bool triggered = false;
  for (int i = 0; i < 60; ++i) {
    auto hits = pw.step(s, 1.0f / 60.0f);
    for (const auto& h : hits) {
      if (h.tag == "goal") triggered = true;
    }
  }
  EXPECT_TRUE(s.find(box)->rigidbody->grounded);
  EXPECT_TRUE(triggered);

  RaycastHit rh = pw.raycast(s, {{0, 10, 0}, {0, -1, 0}}, 50.0f);
  EXPECT_TRUE(rh.hit);
}

TEST(ScriptingTests, LifecycleTriggerAndHotReload) {
  auto tmp = std::filesystem::temp_directory_path() / "astra_script_test";
  std::filesystem::create_directories(tmp);
  std::string spath = (tmp / "hero.as").string();
  {
    std::ofstream f(spath);
    f << "fn on_start()\n  log(\"init\")\nend\n"
      << "fn on_update(dt)\n  move(2, 0, 0)\nend\n"
      << "fn on_trigger(tag)\n  if trigger(\"orb\") -> add_score(5)\nend\n";
  }

  Scene s;
  EntityId hero = s.create_entity("Hero");
  s.find(hero)->script = ScriptComponent{spath, ""};

  ScriptVM vm;
  ScriptRuntimeState st;
  vm.call_start(s, st);
  vm.call_update(s, 0.5f, st);
  vm.call_trigger(s, hero, "orb", st);
  EXPECT_NEAR(s.find(hero)->transform.position.x, 1.0f, 1e-4f);
  EXPECT_EQ(st.score, 5);
  EXPECT_EQ(st.logs.size(), 1u);

  // Modify script file & verify hot reload
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  {
    std::ofstream f(spath);
    f << "fn on_update(dt)\n  move(0, 10, 0)\nend\n";
  }
  vm.check_hot_reload(s);
  vm.call_update(s, 1.0f, st);
  EXPECT_NEAR(s.find(hero)->transform.position.y, 10.0f, 1e-4f);

  std::filesystem::remove_all(tmp);
}

TEST(RenderTests, HeadlessEGLOpenGL33ShadowAndPBRPass) {
  HeadlessGLContext ctx;
  std::string err;
  if (!ctx.create(320, 240, &err)) {
    GTEST_SKIP() << "Headless EGL OpenGL 3.3 context unavailable on runner: " << err;
  }

  Renderer r;
  ASSERT_TRUE(r.init(320, 240, &err)) << err;
  AssetManager assets;
  Scene s;
  EntityId light = s.create_entity("Sun");
  s.find(light)->transform.position = {4, 8, 6};
  s.find(light)->light = LightComponent{};
  EntityId cube = s.create_entity("Cube");
  s.find(cube)->mesh = MeshRendererComponent{"cube", {{0.2f, 0.8f, 0.9f}, {0.1f, 0.1f, 0.1f}, 0.2f, 0.4f, ""}, true, true};

  Mat4 view = Mat4::look_at({0, 1.5f, 3.5f}, {0, 0, 0}, {0, 1, 0});
  Mat4 proj = Mat4::perspective(60.0f, 320.0f / 240.0f, 0.1f, 100.0f);
  r.render_scene(s, assets, view, proj, {0, 1.5f, 3.5f}, cube, true);

  EXPECT_EQ(r.stats().draw_calls, 1u);
  EXPECT_EQ(r.stats().triangles, 12u);
  auto px = r.read_center_pixel();
  EXPECT_EQ(px.size(), 4u);
  EXPECT_GT(px[0] + px[1] + px[2], 0);  // Rendered non-black shaded geometry!
  r.shutdown();
}
