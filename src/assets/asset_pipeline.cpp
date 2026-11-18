#include "astra/assets.hpp"
#include "astra/json.hpp"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace astra {

namespace {

void compute_bounds(MeshData& m) {
  if (m.vertices.empty()) return;
  Vec3 lo = m.vertices[0].pos, hi = m.vertices[0].pos;
  for (const auto& v : m.vertices) {
    lo.x = std::min(lo.x, v.pos.x); lo.y = std::min(lo.y, v.pos.y); lo.z = std::min(lo.z, v.pos.z);
    hi.x = std::max(hi.x, v.pos.x); hi.y = std::max(hi.y, v.pos.y); hi.z = std::max(hi.z, v.pos.z);
  }
  m.local_bounds = {lo, hi};
}

MeshData make_cube() {
  MeshData m;
  m.id = "cube";
  struct Face { Vec3 n; Vec3 p[4]; };
  const Face faces[6] = {
    {{0, 0, 1}, {{-0.5f,-0.5f,0.5f}, {0.5f,-0.5f,0.5f}, {0.5f,0.5f,0.5f}, {-0.5f,0.5f,0.5f}}},
    {{0, 0,-1}, {{0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f,-0.5f}, {-0.5f,0.5f,-0.5f}, {0.5f,0.5f,-0.5f}}},
    {{-1,0, 0}, {{-0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f,0.5f}, {-0.5f,0.5f,0.5f}, {-0.5f,0.5f,-0.5f}}},
    {{1, 0, 0}, {{0.5f,-0.5f,0.5f}, {0.5f,-0.5f,-0.5f}, {0.5f,0.5f,-0.5f}, {0.5f,0.5f,0.5f}}},
    {{0, 1, 0}, {{-0.5f,0.5f,0.5f}, {0.5f,0.5f,0.5f}, {0.5f,0.5f,-0.5f}, {-0.5f,0.5f,-0.5f}}},
    {{0,-1, 0}, {{-0.5f,-0.5f,-0.5f}, {0.5f,-0.5f,-0.5f}, {0.5f,-0.5f,0.5f}, {-0.5f,-0.5f,0.5f}}},
  };
  const Vec2 uvs[4] = {{0,0},{1,0},{1,1},{0,1}};
  for (const auto& f : faces) {
    uint32_t base = static_cast<uint32_t>(m.vertices.size());
    for (int i = 0; i < 4; ++i) m.vertices.push_back({f.p[i], f.n, uvs[i]});
    m.indices.insert(m.indices.end(), {base, base+1, base+2, base, base+2, base+3});
  }
  compute_bounds(m);
  return m;
}

MeshData make_plane() {
  MeshData m;
  m.id = "plane";
  m.vertices = {
    {{-0.5f, 0,  0.5f}, {0,1,0}, {0,0}},
    {{ 0.5f, 0,  0.5f}, {0,1,0}, {1,0}},
    {{ 0.5f, 0, -0.5f}, {0,1,0}, {1,1}},
    {{-0.5f, 0, -0.5f}, {0,1,0}, {0,1}},
  };
  m.indices = {0, 1, 2, 0, 2, 3};
  compute_bounds(m);
  return m;
}

MeshData make_sphere(int stacks = 12, int slices = 18) {
  MeshData m;
  m.id = "sphere";
  for (int i = 0; i <= stacks; ++i) {
    float v = static_cast<float>(i) / stacks;
    float phi = v * PI;
    for (int j = 0; j <= slices; ++j) {
      float u = static_cast<float>(j) / slices;
      float theta = u * 2.0f * PI;
      Vec3 n{std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta)};
      m.vertices.push_back({n * 0.5f, n.normalized(), {u, v}});
    }
  }
  for (int i = 0; i < stacks; ++i) {
    for (int j = 0; j < slices; ++j) {
      uint32_t a = i * (slices + 1) + j;
      uint32_t b = a + slices + 1;
      m.indices.insert(m.indices.end(), {a, b, a + 1, a + 1, b, b + 1});
    }
  }
  compute_bounds(m);
  return m;
}

bool read_file_bytes(const std::string& path, std::vector<uint8_t>& out) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f) return false;
  auto sz = f.tellg();
  if (sz < 0 || sz > 64 * 1024 * 1024) return false;
  out.resize(static_cast<size_t>(sz));
  f.seekg(0);
  return static_cast<bool>(f.read(reinterpret_cast<char*>(out.data()), sz));
}

bool parse_gltf_json_and_bin(const std::string& id, const json::Value& doc, const std::vector<uint8_t>& bin, MeshData& out, std::string* err) {
  const auto& meshes = doc.get("meshes").as_array();
  if (meshes.empty()) { if (err) *err = "glTF contains no meshes"; return false; }
  const auto& prims = meshes[0].get("primitives").as_array();
  if (prims.empty()) { if (err) *err = "glTF mesh contains no primitives"; return false; }

  const auto& prim = prims[0];
  const auto& attrs = prim.get("attributes");
  int pos_acc = attrs.get("POSITION").as_int(-1);
  if (pos_acc < 0) { if (err) *err = "glTF primitive missing POSITION"; return false; }
  int norm_acc = attrs.get("NORMAL").as_int(-1);
  int uv_acc = attrs.get("TEXCOORD_0").as_int(-1);
  int idx_acc = prim.get("indices").as_int(-1);

  const auto& accessors = doc.get("accessors").as_array();
  const auto& buffer_views = doc.get("bufferViews").as_array();

  auto read_accessor_bytes = [&](int acc_idx, size_t elem_size, size_t& out_count, int& out_comp_type, size_t& out_stride, size_t& out_off) -> bool {
    if (acc_idx < 0 || static_cast<size_t>(acc_idx) >= accessors.size()) return false;
    const auto& acc = accessors[acc_idx];
    int bv_idx = acc.get("bufferView").as_int(-1);
    if (bv_idx < 0 || static_cast<size_t>(bv_idx) >= buffer_views.size()) return false;
    const auto& bv = buffer_views[bv_idx];
    out_count = static_cast<size_t>(acc.get("count").as_int(0));
    out_comp_type = acc.get("componentType").as_int(0);
    size_t bv_off = static_cast<size_t>(bv.get("byteOffset").as_int(0));
    size_t acc_off = static_cast<size_t>(acc.get("byteOffset").as_int(0));
    out_off = bv_off + acc_off;
    size_t bv_stride = static_cast<size_t>(bv.get("byteStride").as_int(0));
    out_stride = bv_stride > 0 ? bv_stride : elem_size;
    if (out_count == 0) return false;
    size_t required = out_off + (out_count - 1) * out_stride + elem_size;
    return required <= bin.size();
  };

  size_t count = 0, stride = 0, off = 0;
  int ctype = 0;
  if (!read_accessor_bytes(pos_acc, 12, count, ctype, stride, off) || ctype != 5126) {
    if (err) *err = "invalid glTF POSITION accessor";
    return false;
  }

  out.id = id;
  out.vertices.resize(count);
  for (size_t i = 0; i < count; ++i) {
    std::memcpy(&out.vertices[i].pos, &bin[off + i * stride], 12);
    out.vertices[i].normal = {0, 1, 0};
  }
  if (norm_acc >= 0) {
    size_t nc = 0, ns = 0, no = 0;
    int nt = 0;
    if (read_accessor_bytes(norm_acc, 12, nc, nt, ns, no) && nt == 5126 && nc == count) {
      for (size_t i = 0; i < count; ++i) std::memcpy(&out.vertices[i].normal, &bin[no + i * ns], 12);
    }
  }
  if (uv_acc >= 0) {
    size_t uc = 0, us = 0, uo = 0;
    int ut = 0;
    if (read_accessor_bytes(uv_acc, 8, uc, ut, us, uo) && ut == 5126 && uc == count) {
      for (size_t i = 0; i < count; ++i) std::memcpy(&out.vertices[i].uv, &bin[uo + i * us], 8);
    }
  }
  if (idx_acc >= 0) {
    size_t ic = 0, is = 0, io = 0;
    int it = 0;
    int elem_sz = 4;
    if (static_cast<size_t>(idx_acc) < accessors.size() && accessors[idx_acc].get("componentType").as_int(0) == 5123) elem_sz = 2;
    if (read_accessor_bytes(idx_acc, elem_sz, ic, it, is, io)) {
      out.indices.resize(ic);
      for (size_t i = 0; i < ic; ++i) {
        if (it == 5123) {
          uint16_t v = 0;
          std::memcpy(&v, &bin[io + i * is], 2);
          out.indices[i] = v;
        } else {
          uint32_t v = 0;
          std::memcpy(&v, &bin[io + i * is], 4);
          out.indices[i] = v;
        }
      }
    }
  } else {
    for (size_t i = 0; i < count; ++i) out.indices.push_back(static_cast<uint32_t>(i));
  }

  // PBR Material color if present
  const auto& mats = doc.get("materials").as_array();
  if (!mats.empty()) {
    const auto& pbr = mats[0].get("pbrMetallicRoughness");
    const auto& col = pbr.get("baseColorFactor").as_array();
    if (col.size() >= 3) {
      out.default_material.albedo = {
          static_cast<float>(col[0].as_number(0.8)),
          static_cast<float>(col[1].as_number(0.8)),
          static_cast<float>(col[2].as_number(0.8)),
      };
    }
    out.default_material.metallic = static_cast<float>(pbr.get("metallicFactor").as_number(0.2));
    out.default_material.roughness = static_cast<float>(pbr.get("roughnessFactor").as_number(0.5));
  }
  compute_bounds(out);
  return true;
}

}  // namespace

AssetManager::AssetManager() { register_builtin_primitives(); }

void AssetManager::register_builtin_primitives() {
  add_mesh(make_cube());
  add_mesh(make_plane());
  add_mesh(make_sphere());

  // Builtin procedural checker texture
  TextureData chk;
  chk.id = "checker";
  chk.width = 64;
  chk.height = 64;
  chk.rgba.resize(64 * 64 * 4);
  for (int y = 0; y < 64; ++y) {
    for (int x = 0; x < 64; ++x) {
      bool odd = ((x / 8) + (y / 8)) % 2 == 1;
      uint8_t c = odd ? 235 : 175;
      size_t idx = (y * 64 + x) * 4;
      chk.rgba[idx] = c; chk.rgba[idx+1] = c; chk.rgba[idx+2] = odd ? 250 : 195; chk.rgba[idx+3] = 255;
    }
  }
  textures_[chk.id] = std::move(chk);
}

void AssetManager::add_mesh(MeshData mesh) {
  meshes_[mesh.id] = std::move(mesh);
}

bool AssetManager::load_glb(const std::string& id, const std::string& path, std::string* err) {
  std::vector<uint8_t> raw;
  if (!read_file_bytes(path, raw) || raw.size() < 20) {
    if (err) *err = "failed reading glb header";
    return false;
  }
  uint32_t magic = 0, version = 0, total_len = 0;
  std::memcpy(&magic, &raw[0], 4);
  std::memcpy(&version, &raw[4], 4);
  std::memcpy(&total_len, &raw[8], 4);
  if (magic != 0x46546C67 || version != 2 || total_len > raw.size()) {
    if (err) *err = "invalid GLB v2 header";
    return false;
  }
  size_t pos = 12;
  std::string json_str;
  std::vector<uint8_t> bin;
  while (pos + 8 <= raw.size()) {
    uint32_t clen = 0, ctype = 0;
    std::memcpy(&clen, &raw[pos], 4);
    std::memcpy(&ctype, &raw[pos + 4], 4);
    pos += 8;
    if (pos + clen > raw.size()) { if (err) *err = "truncated GLB chunk"; return false; }
    if (ctype == 0x4E4F534A) json_str.assign(reinterpret_cast<const char*>(&raw[pos]), clen);
    else if (ctype == 0x004E4942) bin.assign(raw.begin() + pos, raw.begin() + pos + clen);
    pos += clen;
  }
  try {
    json::Value doc = json::parse(json_str);
    MeshData mesh;
    if (!parse_gltf_json_and_bin(id, doc, bin, mesh, err)) return false;
    meshes_[id] = std::move(mesh);
    return true;
  } catch (const std::exception& ex) {
    if (err) *err = ex.what();
    return false;
  }
}

bool AssetManager::load_gltf(const std::string& id, const std::string& path, std::string* err) {
  std::vector<uint8_t> raw;
  if (!read_file_bytes(path, raw)) { if (err) *err = "cannot open gltf"; return false; }
  try {
    json::Value doc = json::parse(std::string(raw.begin(), raw.end()));
    const auto& bufs = doc.get("buffers").as_array();
    if (bufs.empty()) { if (err) *err = "no buffers in gltf"; return false; }
    std::string uri = bufs[0].get("uri").as_string("");
    std::filesystem::path bin_path = std::filesystem::path(path).parent_path() / uri;
    std::vector<uint8_t> bin;
    if (!read_file_bytes(bin_path.string(), bin)) { if (err) *err = "missing bin uri: " + uri; return false; }
    MeshData mesh;
    if (!parse_gltf_json_and_bin(id, doc, bin, mesh, err)) return false;
    meshes_[id] = std::move(mesh);
    return true;
  } catch (const std::exception& ex) {
    if (err) *err = ex.what();
    return false;
  }
}

bool AssetManager::export_mesh_glb(const MeshData& mesh, const std::string& out_path) {
  std::vector<uint8_t> bin;
  uint32_t pos_off = 0, pos_len = static_cast<uint32_t>(mesh.vertices.size() * 12);
  for (const auto& v : mesh.vertices) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&v.pos);
    bin.insert(bin.end(), p, p + 12);
  }
  uint32_t norm_off = static_cast<uint32_t>(bin.size()), norm_len = static_cast<uint32_t>(mesh.vertices.size() * 12);
  for (const auto& v : mesh.vertices) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&v.normal);
    bin.insert(bin.end(), p, p + 12);
  }
  uint32_t idx_off = static_cast<uint32_t>(bin.size()), idx_len = static_cast<uint32_t>(mesh.indices.size() * 4);
  for (uint32_t ix : mesh.indices) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&ix);
    bin.insert(bin.end(), p, p + 4);
  }
  while (bin.size() % 4 != 0) bin.push_back(0);

  json::Object root;
  root["asset"] = json::Value(json::Object{{"version", json::Value("2.0")}});
  root["buffers"] = json::Value(json::Array{json::Value(json::Object{{"byteLength", json::Value(static_cast<int>(bin.size()))}})});
  root["bufferViews"] = json::Value(json::Array{
    json::Value(json::Object{{"buffer", 0}, {"byteOffset", static_cast<int>(pos_off)}, {"byteLength", static_cast<int>(pos_len)}}),
    json::Value(json::Object{{"buffer", 0}, {"byteOffset", static_cast<int>(norm_off)}, {"byteLength", static_cast<int>(norm_len)}}),
    json::Value(json::Object{{"buffer", 0}, {"byteOffset", static_cast<int>(idx_off)}, {"byteLength", static_cast<int>(idx_len)}}),
  });
  root["accessors"] = json::Value(json::Array{
    json::Value(json::Object{{"bufferView", 0}, {"componentType", 5126}, {"count", static_cast<int>(mesh.vertices.size())}, {"type", "VEC3"}}),
    json::Value(json::Object{{"bufferView", 1}, {"componentType", 5126}, {"count", static_cast<int>(mesh.vertices.size())}, {"type", "VEC3"}}),
    json::Value(json::Object{{"bufferView", 2}, {"componentType", 5125}, {"count", static_cast<int>(mesh.indices.size())}, {"type", "SCALAR"}}),
  });
  root["meshes"] = json::Value(json::Array{json::Value(json::Object{
    {"primitives", json::Value(json::Array{json::Value(json::Object{
      {"attributes", json::Value(json::Object{{"POSITION", 0}, {"NORMAL", 1}})},
      {"indices", 2},
    })})},
  })});

  std::string jstr = json::stringify(json::Value(std::move(root)), 0);
  while (jstr.size() % 4 != 0) jstr.push_back(' ');

  uint32_t total_len = static_cast<uint32_t>(12 + 8 + jstr.size() + 8 + bin.size());
  std::ofstream out(out_path, std::ios::binary);
  if (!out) return false;
  auto w32 = [&](uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
  w32(0x46546C67); w32(2); w32(total_len);
  w32(static_cast<uint32_t>(jstr.size())); w32(0x4E4F534A); out.write(jstr.data(), jstr.size());
  w32(static_cast<uint32_t>(bin.size())); w32(0x004E4942); out.write(reinterpret_cast<const char*>(bin.data()), bin.size());
  return static_cast<bool>(out);
}

bool AssetManager::load_texture_tga_or_ppm(const std::string& id, const std::string& path, std::string* err) {
  std::vector<uint8_t> raw;
  if (!read_file_bytes(path, raw) || raw.size() < 18) { if (err) *err = "failed reading texture"; return false; }
  // Support uncompressed RGB/RGBA TGA (image_type == 2)
  if (raw[2] == 2 && (raw[16] == 24 || raw[16] == 32)) {
    int w = raw[12] | (raw[13] << 8);
    int h = raw[14] | (raw[15] << 8);
    int bpp = raw[16] / 8;
    size_t need = 18 + static_cast<size_t>(w) * h * bpp;
    if (w <= 0 || h <= 0 || need > raw.size()) { if (err) *err = "truncated TGA"; return false; }
    TextureData td{id, w, h, std::vector<uint8_t>(w * h * 4, 255)};
    for (int i = 0; i < w * h; ++i) {
      td.rgba[i*4+0] = raw[18 + i*bpp + 2];
      td.rgba[i*4+1] = raw[18 + i*bpp + 1];
      td.rgba[i*4+2] = raw[18 + i*bpp + 0];
      td.rgba[i*4+3] = (bpp == 4) ? raw[18 + i*bpp + 3] : 255;
    }
    textures_[id] = std::move(td);
    return true;
  }
  if (err) *err = "unsupported texture format (expected uncompressed TGA)";
  return false;
}

bool AssetManager::load_wav(const std::string& id, const std::string& path, std::string* err) {
  std::vector<uint8_t> raw;
  if (!read_file_bytes(path, raw) || raw.size() < 44 || std::memcmp(raw.data(), "RIFF", 4) != 0) {
    if (err) *err = "invalid RIFF WAV header";
    return false;
  }
  WavData wd;
  wd.id = id;
  std::memcpy(&wd.channels, &raw[22], 2);
  std::memcpy(&wd.sample_rate, &raw[24], 4);
  size_t pcm_bytes = (raw.size() - 44) & ~size_t(1);
  wd.samples.resize(pcm_bytes / 2);
  std::memcpy(wd.samples.data(), &raw[44], pcm_bytes);
  wavs_[id] = std::move(wd);
  return true;
}

bool AssetManager::export_chime_wav(const std::string& out_path, float freq_hz, float duration_sec) {
  uint32_t sr = 22050;
  uint32_t count = static_cast<uint32_t>(sr * duration_sec);
  uint32_t data_bytes = count * 2;
  std::ofstream out(out_path, std::ios::binary);
  if (!out) return false;
  auto w32 = [&](uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
  auto w16 = [&](uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };
  out.write("RIFF", 4); w32(36 + data_bytes); out.write("WAVEfmt ", 8);
  w32(16); w16(1); w16(1); w32(sr); w32(sr * 2); w16(2); w16(16);
  out.write("data", 4); w32(data_bytes);
  for (uint32_t i = 0; i < count; ++i) {
    float t = static_cast<float>(i) / sr;
    float env = 1.0f - (t / duration_sec);
    int16_t s = static_cast<int16_t>(std::sin(2.0f * PI * freq_hz * t) * env * 12000.0f);
    w16(static_cast<uint16_t>(s));
  }
  return static_cast<bool>(out);
}

const MeshData* AssetManager::get_mesh(const std::string& id) const {
  auto it = meshes_.find(id);
  return it == meshes_.end() ? nullptr : &it->second;
}

const TextureData* AssetManager::get_texture(const std::string& id) const {
  auto it = textures_.find(id);
  return it == textures_.end() ? nullptr : &it->second;
}

const WavData* AssetManager::get_wav(const std::string& id) const {
  auto it = wavs_.find(id);
  return it == wavs_.end() ? nullptr : &it->second;
}

std::vector<std::string> AssetManager::list_mesh_ids() const {
  std::vector<std::string> out;
  for (const auto& [k, _] : meshes_) out.push_back(k);
  return out;
}

}  // namespace astra
