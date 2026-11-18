#include "astra/renderer.hpp"

#include <GL/glew.h>
#ifdef ASTRA_HAS_EGL
#include <EGL/egl.h>
#endif

#include <chrono>
#include <cstring>

namespace astra {

namespace {

const char* kShadowVert = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 uLightVP;
uniform mat4 uModel;
void main() {
  gl_Position = uLightVP * uModel * vec4(aPos, 1.0);
}
)GLSL";

const char* kShadowFrag = R"GLSL(
#version 330 core
void main() {}
)GLSL";

const char* kPBRVert = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNorm;
layout (location = 2) in vec2 aUV;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uLightVP;
out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out vec4 vLightSpace;
void main() {
  vec4 wp = uModel * vec4(aPos, 1.0);
  vWorldPos = wp.xyz;
  vNormal = normalize(mat3(uModel) * aNorm);
  vUV = aUV;
  vLightSpace = uLightVP * wp;
  gl_Position = uProj * uView * wp;
}
)GLSL";

const char* kPBRFrag = R"GLSL(
#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
in vec4 vLightSpace;
out vec4 FragColor;

uniform vec3 uCamPos;
uniform vec3 uAlbedo;
uniform vec3 uEmissive;
uniform float uMetallic;
uniform float uRoughness;
uniform int uUseTex;
uniform sampler2D uTex;
uniform sampler2D uShadowMap;

uniform vec3 uDirLightDir;
uniform vec3 uDirLightColor;
uniform int uNumPoints;
uniform vec3 uPointPos[4];
uniform vec3 uPointColor[4];
uniform float uPointRange[4];

float shadow_calc(vec4 lsPos, vec3 n, vec3 ldir) {
  vec3 proj = lsPos.xyz / max(lsPos.w, 0.0001);
  proj = proj * 0.5 + 0.5;
  if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) return 0.0;
  float bias = max(0.003 * (1.0 - dot(n, ldir)), 0.0008);
  float depth = proj.z;
  float shadow = 0.0;
  vec2 texel = 1.0 / vec2(1024.0, 1024.0);
  for (int x = -1; x <= 1; ++x) {
    for (int y = -1; y <= 1; ++y) {
      float pcf = texture(uShadowMap, proj.xy + vec2(x, y) * texel).r;
      shadow += (depth - bias > pcf) ? 1.0 : 0.0;
    }
  }
  return shadow / 9.0;
}

vec3 calc_light(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 baseCol) {
  vec3 H = normalize(V + L);
  float ndl = max(dot(N, L), 0.0);
  float ndh = max(dot(N, H), 0.0);
  float specPow = mix(96.0, 8.0, clamp(uRoughness, 0.05, 1.0));
  float spec = pow(ndh, specPow) * mix(0.25, 1.0, uMetallic);
  vec3 diff = baseCol * (1.0 - uMetallic * 0.7);
  return (diff + vec3(spec)) * radiance * ndl;
}

void main() {
  vec3 base = uAlbedo;
  if (uUseTex == 1) base *= texture(uTex, vUV).rgb;
  vec3 N = normalize(vNormal);
  vec3 V = normalize(uCamPos - vWorldPos);
  vec3 L = normalize(-uDirLightDir);
  float sh = shadow_calc(vLightSpace, N, L);

  vec3 lit = vec3(0.14, 0.17, 0.24) * base;
  lit += (1.0 - sh * 0.78) * calc_light(N, V, L, uDirLightColor, base);

  for (int i = 0; i < uNumPoints; ++i) {
    vec3 toL = uPointPos[i] - vWorldPos;
    float d = length(toL);
    float att = clamp(1.0 - d / max(uPointRange[i], 0.1), 0.0, 1.0);
    lit += calc_light(N, V, normalize(toL), uPointColor[i] * (att * att), base);
  }
  lit += uEmissive;
  FragColor = vec4(lit, 1.0);
}
)GLSL";

const char* kLineVert = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 uVP;
void main() { gl_Position = uVP * vec4(aPos, 1.0); }
)GLSL";

const char* kLineFrag = R"GLSL(
#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() { FragColor = uColor; }
)GLSL";

const char* kUIVert = R"GLSL(
#version 330 core
layout (location = 0) in vec2 aPos;
uniform vec2 uViewport;
void main() {
  vec2 ndc = (aPos / uViewport) * 2.0 - 1.0;
  ndc.y = -ndc.y;
  gl_Position = vec4(ndc, 0.0, 1.0);
}
)GLSL";

uint32_t compile_program(const char* vs_src, const char* fs_src, std::string* err) {
  auto comp = [&](GLenum type, const char* src) -> uint32_t {
    uint32_t s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
      char buf[512];
      glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
      if (err) *err = buf;
      glDeleteShader(s);
      return 0;
    }
    return s;
  };
  uint32_t vs = comp(GL_VERTEX_SHADER, vs_src);
  uint32_t fs = comp(GL_FRAGMENT_SHADER, fs_src);
  if (!vs || !fs) { if (vs) glDeleteShader(vs); if (fs) glDeleteShader(fs); return 0; }
  uint32_t prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);
  glDeleteShader(vs);
  glDeleteShader(fs);
  int ok = 0;
  glGetProgramiv(prog, GL_LINK_STATUS, &ok);
  if (!ok) {
    char buf[512];
    glGetProgramInfoLog(prog, sizeof(buf), nullptr, buf);
    if (err) *err = buf;
    glDeleteProgram(prog);
    return 0;
  }
  return prog;
}

// Compact 5x7 ASCII glyph rows for crisp HUD/Editor rendering without font file deps
std::array<uint8_t, 5> glyph_5x7(char c) {
  c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  switch (c) {
    case 'A': return {0x7E,0x11,0x11,0x11,0x7E}; case 'B': return {0x7F,0x49,0x49,0x49,0x36};
    case 'C': return {0x3E,0x41,0x41,0x41,0x22}; case 'D': return {0x7F,0x41,0x41,0x22,0x1C};
    case 'E': return {0x7F,0x49,0x49,0x49,0x41}; case 'F': return {0x7F,0x09,0x09,0x09,0x01};
    case 'G': return {0x3E,0x41,0x49,0x49,0x7A}; case 'H': return {0x7F,0x08,0x08,0x08,0x7F};
    case 'I': return {0x00,0x41,0x7F,0x41,0x00}; case 'J': return {0x20,0x40,0x41,0x3F,0x01};
    case 'K': return {0x7F,0x08,0x14,0x22,0x41}; case 'L': return {0x7F,0x40,0x40,0x40,0x40};
    case 'M': return {0x7F,0x02,0x0C,0x02,0x7F}; case 'N': return {0x7F,0x04,0x08,0x10,0x7F};
    case 'O': return {0x3E,0x41,0x41,0x41,0x3E}; case 'P': return {0x7F,0x09,0x09,0x09,0x06};
    case 'Q': return {0x3E,0x41,0x51,0x21,0x5E}; case 'R': return {0x7F,0x09,0x19,0x29,0x46};
    case 'S': return {0x46,0x49,0x49,0x49,0x31}; case 'T': return {0x01,0x01,0x7F,0x01,0x01};
    case 'U': return {0x3F,0x40,0x40,0x40,0x3F}; case 'V': return {0x1F,0x20,0x40,0x20,0x1F};
    case 'W': return {0x3F,0x40,0x38,0x40,0x3F}; case 'X': return {0x63,0x14,0x08,0x14,0x63};
    case 'Y': return {0x07,0x08,0x70,0x08,0x07}; case 'Z': return {0x61,0x51,0x49,0x45,0x43};
    case '0': return {0x3E,0x51,0x49,0x45,0x3E}; case '1': return {0x00,0x42,0x7F,0x40,0x00};
    case '2': return {0x42,0x61,0x51,0x49,0x46}; case '3': return {0x21,0x41,0x45,0x4B,0x31};
    case '4': return {0x18,0x14,0x12,0x7F,0x10}; case '5': return {0x27,0x45,0x45,0x45,0x39};
    case '6': return {0x3C,0x4A,0x49,0x49,0x30}; case '7': return {0x01,0x71,0x09,0x05,0x03};
    case '8': return {0x36,0x49,0x49,0x49,0x36}; case '9': return {0x06,0x49,0x49,0x29,0x1E};
    case ':': return {0x00,0x36,0x36,0x00,0x00}; case '.': return {0x00,0x60,0x60,0x00,0x00};
    case '-': return {0x08,0x08,0x08,0x08,0x08}; case '/': return {0x20,0x10,0x08,0x04,0x02};
    case '[': return {0x00,0x7F,0x41,0x41,0x00}; case ']': return {0x00,0x41,0x41,0x7F,0x00};
    case '(': return {0x00,0x1C,0x22,0x41,0x00}; case ')': return {0x00,0x41,0x22,0x1C,0x00};
    default:  return {0x00,0x00,0x00,0x00,0x00};
  }
}

}  // namespace

Renderer::~Renderer() { shutdown(); }

bool Renderer::init(int width, int height, std::string* err) {
  width_ = width;
  height_ = height;
  glewExperimental = GL_TRUE;
  GLenum gerr = glewInit();
  if (gerr != GLEW_OK && gerr != GLEW_ERROR_NO_GLX_DISPLAY) {
    // Clear benign GLEW init GL error on core profiles
    if (err) *err = reinterpret_cast<const char*>(glewGetErrorString(gerr));
    return false;
  }
  while (glGetError() != GL_NO_ERROR) {}

  pbr_prog_ = compile_program(kPBRVert, kPBRFrag, err);
  shadow_prog_ = compile_program(kShadowVert, kShadowFrag, err);
  line_prog_ = compile_program(kLineVert, kLineFrag, err);
  ui_prog_ = compile_program(kUIVert, kLineFrag, err);
  if (!pbr_prog_ || !shadow_prog_ || !line_prog_ || !ui_prog_) return false;

  // 1024x1024 shadow FBO
  glGenFramebuffers(1, &shadow_fbo_);
  glGenTextures(1, &shadow_tex_);
  glBindTexture(GL_TEXTURE_2D, shadow_tex_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 1024, 1024, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  float border[] = {1.0f, 1.0f, 1.0f, 1.0f};
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
  glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_tex_, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // Default 1x1 white texture
  glGenTextures(1, &white_tex_);
  glBindTexture(GL_TEXTURE_2D, white_tex_);
  uint8_t white[4] = {255, 255, 255, 255};
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);

  // Build world reference grid
  std::vector<float> grid_pts;
  for (int i = -12; i <= 12; ++i) {
    float p = static_cast<float>(i);
    grid_pts.insert(grid_pts.end(), {p, 0, -12.0f, p, 0, 12.0f});
    grid_pts.insert(grid_pts.end(), {-12.0f, 0, p, 12.0f, 0, p});
  }
  grid_vert_count_ = static_cast<uint32_t>(grid_pts.size() / 3);
  glGenVertexArrays(1, &grid_vao_);
  glGenBuffers(1, &grid_vbo_);
  glBindVertexArray(grid_vao_);
  glBindBuffer(GL_ARRAY_BUFFER, grid_vbo_);
  glBufferData(GL_ARRAY_BUFFER, grid_pts.size() * sizeof(float), grid_pts.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, nullptr);
  glEnableVertexAttribArray(0);

  glGenVertexArrays(1, &ui_vao_);
  glGenBuffers(1, &ui_vbo_);

  glEnable(GL_DEPTH_TEST);
  initialized_ = true;
  return true;
}

void Renderer::shutdown() {
  if (!initialized_) return;
  for (auto& [_, m] : gpu_meshes_) {
    glDeleteVertexArrays(1, &m.vao);
    glDeleteBuffers(1, &m.vbo);
    glDeleteBuffers(1, &m.ebo);
  }
  gpu_meshes_.clear();
  for (auto& [_, t] : gpu_textures_) glDeleteTextures(1, &t);
  gpu_textures_.clear();
  if (shadow_fbo_) glDeleteFramebuffers(1, &shadow_fbo_);
  if (shadow_tex_) glDeleteTextures(1, &shadow_tex_);
  if (white_tex_) glDeleteTextures(1, &white_tex_);
  if (grid_vao_) glDeleteVertexArrays(1, &grid_vao_);
  if (grid_vbo_) glDeleteBuffers(1, &grid_vbo_);
  if (ui_vao_) glDeleteVertexArrays(1, &ui_vao_);
  if (ui_vbo_) glDeleteBuffers(1, &ui_vbo_);
  if (pbr_prog_) glDeleteProgram(pbr_prog_);
  if (shadow_prog_) glDeleteProgram(shadow_prog_);
  if (line_prog_) glDeleteProgram(line_prog_);
  if (ui_prog_) glDeleteProgram(ui_prog_);
  initialized_ = false;
}

void Renderer::resize(int width, int height) {
  width_ = std::max(1, width);
  height_ = std::max(1, height);
}

void Renderer::upload_mesh(const MeshData& mesh) {
  if (gpu_meshes_.count(mesh.id)) return;
  GPUMesh gm;
  gm.index_count = static_cast<uint32_t>(mesh.indices.size());
  glGenVertexArrays(1, &gm.vao);
  glGenBuffers(1, &gm.vbo);
  glGenBuffers(1, &gm.ebo);
  glBindVertexArray(gm.vao);
  glBindBuffer(GL_ARRAY_BUFFER, gm.vbo);
  glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(Vertex), mesh.vertices.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gm.ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(uint32_t), mesh.indices.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(0));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));
  glEnableVertexAttribArray(2);
  gpu_meshes_[mesh.id] = gm;
}

void Renderer::upload_texture(const TextureData& tex) {
  if (gpu_textures_.count(tex.id)) return;
  uint32_t tid = 0;
  glGenTextures(1, &tid);
  glBindTexture(GL_TEXTURE_2D, tid);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex.width, tex.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.rgba.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gpu_textures_[tex.id] = tid;
}

void Renderer::render_scene(const Scene& scene, const AssetManager& assets, const Mat4& view, const Mat4& proj, const Vec3& cam_pos, EntityId selected, bool show_grid) {
  auto t0 = std::chrono::steady_clock::now();
  stats_ = {};

  Vec3 dir_dir{-0.45f, -0.85f, -0.35f};
  Vec3 dir_col{1.0f, 0.96f, 0.9f};
  Vec3 p_pos[4]{};
  Vec3 p_col[4]{};
  float p_rng[4]{};
  int p_cnt = 0;

  for (EntityId id : scene.order()) {
    const Entity* e = scene.find(id);
    if (!e || !e->light) continue;
    if (e->light->kind == LightKind::Directional) {
      dir_dir = (Vec3{0,0,0} - e->transform.position).normalized();
      if (dir_dir.length_sq() < 0.01f) dir_dir = {-0.45f, -0.85f, -0.35f};
      dir_col = e->light->color * e->light->intensity;
    } else if (p_cnt < 4) {
      p_pos[p_cnt] = scene.world_position(id);
      p_col[p_cnt] = e->light->color * e->light->intensity;
      p_rng[p_cnt] = e->light->range;
      ++p_cnt;
    }
  }

  Mat4 light_vp = Mat4::ortho(-18, 18, -18, 18, 1.0f, 60.0f) *
                  Mat4::look_at(-dir_dir * 20.0f, {0, 0, 0}, {0, 1, 0});

  // 1. Directional Shadow Map Pass
  glViewport(0, 0, 1024, 1024);
  glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo_);
  glClear(GL_DEPTH_BUFFER_BIT);
  glUseProgram(shadow_prog_);
  glUniformMatrix4fv(glGetUniformLocation(shadow_prog_, "uLightVP"), 1, GL_FALSE, light_vp.m.data());
  for (EntityId id : scene.order()) {
    const Entity* e = scene.find(id);
    if (!e || !e->mesh || !e->mesh->visible || !e->mesh->cast_shadow) continue;
    if (const auto* md = assets.get_mesh(e->mesh->mesh_id)) upload_mesh(*md);
    auto it = gpu_meshes_.find(e->mesh->mesh_id);
    if (it == gpu_meshes_.end()) continue;
    Mat4 model = scene.world_matrix(id);
    glUniformMatrix4fv(glGetUniformLocation(shadow_prog_, "uModel"), 1, GL_FALSE, model.m.data());
    glBindVertexArray(it->second.vao);
    glDrawElements(GL_TRIANGLES, it->second.index_count, GL_UNSIGNED_INT, nullptr);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // 2. Main PBR Scene Pass
  glViewport(0, 0, width_, height_);
  glClearColor(0.05f, 0.08f, 0.14f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (show_grid) {
    Mat4 vp = proj * view;
    glUseProgram(line_prog_);
    glUniformMatrix4fv(glGetUniformLocation(line_prog_, "uVP"), 1, GL_FALSE, vp.m.data());
    glUniform4f(glGetUniformLocation(line_prog_, "uColor"), 0.2f, 0.28f, 0.42f, 1.0f);
    glBindVertexArray(grid_vao_);
    glDrawArrays(GL_LINES, 0, grid_vert_count_);
  }

  glUseProgram(pbr_prog_);
  glUniformMatrix4fv(glGetUniformLocation(pbr_prog_, "uView"), 1, GL_FALSE, view.m.data());
  glUniformMatrix4fv(glGetUniformLocation(pbr_prog_, "uProj"), 1, GL_FALSE, proj.m.data());
  glUniformMatrix4fv(glGetUniformLocation(pbr_prog_, "uLightVP"), 1, GL_FALSE, light_vp.m.data());
  glUniform3fv(glGetUniformLocation(pbr_prog_, "uCamPos"), 1, &cam_pos.x);
  glUniform3fv(glGetUniformLocation(pbr_prog_, "uDirLightDir"), 1, &dir_dir.x);
  glUniform3fv(glGetUniformLocation(pbr_prog_, "uDirLightColor"), 1, &dir_col.x);
  glUniform1i(glGetUniformLocation(pbr_prog_, "uNumPoints"), p_cnt);
  if (p_cnt > 0) {
    glUniform3fv(glGetUniformLocation(pbr_prog_, "uPointPos"), p_cnt, &p_pos[0].x);
    glUniform3fv(glGetUniformLocation(pbr_prog_, "uPointColor"), p_cnt, &p_col[0].x);
    glUniform1fv(glGetUniformLocation(pbr_prog_, "uPointRange"), p_cnt, p_rng);
  }
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, shadow_tex_);
  glUniform1i(glGetUniformLocation(pbr_prog_, "uShadowMap"), 1);

  for (EntityId id : scene.order()) {
    const Entity* e = scene.find(id);
    if (!e || !e->mesh || !e->mesh->visible) continue;
    if (const auto* md = assets.get_mesh(e->mesh->mesh_id)) upload_mesh(*md);
    auto it = gpu_meshes_.find(e->mesh->mesh_id);
    if (it == gpu_meshes_.end()) continue;

    Mat4 model = scene.world_matrix(id);
    glUniformMatrix4fv(glGetUniformLocation(pbr_prog_, "uModel"), 1, GL_FALSE, model.m.data());
    Vec3 albedo = e->mesh->material.albedo;
    Vec3 emis = e->mesh->material.emissive;
    if (id == selected) emis += Vec3{0.18f, 0.25f, 0.35f};
    glUniform3fv(glGetUniformLocation(pbr_prog_, "uAlbedo"), 1, &albedo.x);
    glUniform3fv(glGetUniformLocation(pbr_prog_, "uEmissive"), 1, &emis.x);
    glUniform1f(glGetUniformLocation(pbr_prog_, "uMetallic"), e->mesh->material.metallic);
    glUniform1f(glGetUniformLocation(pbr_prog_, "uRoughness"), e->mesh->material.roughness);

    int use_tex = 0;
    glActiveTexture(GL_TEXTURE0);
    if (!e->mesh->material.texture_id.empty()) {
      if (const auto* td = assets.get_texture(e->mesh->material.texture_id)) upload_texture(*td);
      if (auto tit = gpu_textures_.find(e->mesh->material.texture_id); tit != gpu_textures_.end()) {
        glBindTexture(GL_TEXTURE_2D, tit->second);
        use_tex = 1;
      }
    }
    if (!use_tex) glBindTexture(GL_TEXTURE_2D, white_tex_);
    glUniform1i(glGetUniformLocation(pbr_prog_, "uTex"), 0);
    glUniform1i(glGetUniformLocation(pbr_prog_, "uUseTex"), use_tex);

    glBindVertexArray(it->second.vao);
    glDrawElements(GL_TRIANGLES, it->second.index_count, GL_UNSIGNED_INT, nullptr);
    stats_.draw_calls++;
    stats_.triangles += it->second.index_count / 3;
  }

  auto t1 = std::chrono::steady_clock::now();
  stats_.frame_ms = std::chrono::duration<float, std::milli>(t1 - t0).count();
}

void Renderer::draw_overlay(const std::vector<OverlayRect>& rects, const std::vector<OverlayText>& texts) {
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(ui_prog_);
  glUniform2f(glGetUniformLocation(ui_prog_, "uViewport"), static_cast<float>(width_), static_cast<float>(height_));
  glBindVertexArray(ui_vao_);
  glBindBuffer(GL_ARRAY_BUFFER, ui_vbo_);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, nullptr);
  glEnableVertexAttribArray(0);

  auto draw_quad = [&](float x, float y, float w, float h, const Vec4& c) {
    float q[] = {x,y, x+w,y, x+w,y+h, x,y, x+w,y+h, x,y+h};
    glUniform4f(glGetUniformLocation(ui_prog_, "uColor"), c.x, c.y, c.z, c.w);
    glBufferData(GL_ARRAY_BUFFER, sizeof(q), q, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
  };

  for (const auto& r : rects) draw_quad(r.x, r.y, r.w, r.h, r.color);

  for (const auto& t : texts) {
    float cx = t.x;
    float s = std::max(1.0f, t.scale);
    for (char ch : t.text) {
      auto cols = glyph_5x7(ch);
      for (int c = 0; c < 5; ++c) {
        for (int r = 0; r < 7; ++r) {
          if (cols[c] & (1 << r)) draw_quad(cx + c * s, t.y + r * s, s, s, {t.color.x, t.color.y, t.color.z, 1.0f});
        }
      }
      cx += 6.0f * s;
    }
  }
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
}

std::vector<uint8_t> Renderer::read_center_pixel() {
  std::vector<uint8_t> px(4, 0);
  glReadPixels(width_ / 2, height_ / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
  return px;
}

HeadlessGLContext::~HeadlessGLContext() { destroy(); }

bool HeadlessGLContext::create(int width, int height, std::string* err) {
#ifdef ASTRA_HAS_EGL
  EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (dpy == EGL_NO_DISPLAY) { if (err) *err = "eglGetDisplay failed"; return false; }
  if (!eglInitialize(dpy, nullptr, nullptr)) { if (err) *err = "eglInitialize failed"; return false; }
  if (!eglBindAPI(EGL_OPENGL_API)) { if (err) *err = "eglBindAPI OpenGL failed"; return false; }
  EGLint cfg_attrs[] = {
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 24,
    EGL_NONE
  };
  EGLConfig cfg = nullptr;
  EGLint ncfg = 0;
  if (!eglChooseConfig(dpy, cfg_attrs, &cfg, 1, &ncfg) || ncfg == 0) {
    if (err) *err = "eglChooseConfig failed";
    return false;
  }
  EGLint pbuf_attrs[] = {EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE};
  EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pbuf_attrs);
  if (surf == EGL_NO_SURFACE) { if (err) *err = "eglCreatePbufferSurface failed"; return false; }
  EGLint ctx_attrs[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 3, EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
  EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attrs);
  if (ctx == EGL_NO_CONTEXT) { if (err) *err = "eglCreateContext 3.3 failed"; return false; }
  if (!eglMakeCurrent(dpy, surf, surf, ctx)) { if (err) *err = "eglMakeCurrent failed"; return false; }
  display_ = dpy;
  surface_ = surf;
  context_ = ctx;
  return true;
#else
  (void)width; (void)height;
  if (err) *err = "EGL not available at build time";
  return false;
#endif
}

void HeadlessGLContext::destroy() {
#ifdef ASTRA_HAS_EGL
  if (display_) {
    EGLDisplay dpy = static_cast<EGLDisplay>(display_);
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (context_) eglDestroyContext(dpy, static_cast<EGLContext>(context_));
    if (surface_) eglDestroySurface(dpy, static_cast<EGLSurface>(surface_));
    eglTerminate(dpy);
  }
#endif
  display_ = surface_ = context_ = nullptr;
}

}  // namespace astra
