#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace astra {

constexpr float PI = 3.14159265358979323846f;
inline float radians(float deg) { return deg * (PI / 180.0f); }

struct Vec2 {
  float x = 0.0f, y = 0.0f;
  Vec2() = default;
  Vec2(float x_, float y_) : x(x_), y(y_) {}
  Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
  Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
  Vec2 operator*(float s) const { return {x * s, y * s}; }
};

struct Vec3 {
  float x = 0.0f, y = 0.0f, z = 0.0f;
  Vec3() = default;
  Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
  Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vec3 operator-() const { return {-x, -y, -z}; }
  Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
  Vec3 operator*(const Vec3& o) const { return {x * o.x, y * o.y, z * o.z}; }
  Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
  Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }

  float length_sq() const { return x * x + y * y + z * z; }
  float length() const { return std::sqrt(length_sq()); }
  Vec3 normalized() const {
    float l = length();
    return l > 1e-6f ? Vec3{x / l, y / l, z / l} : Vec3{0, 0, 0};
  }
  static float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
  static Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
  }
  static Vec3 lerp(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }
};

struct Vec4 {
  float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
  Vec4() = default;
  Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
};

struct Mat4 {
  // Column-major 4x4 matrix matching OpenGL conventions
  std::array<float, 16> m{};

  static Mat4 identity() {
    Mat4 r;
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
  }

  static Mat4 translate(const Vec3& v) {
    Mat4 r = identity();
    r.m[12] = v.x;
    r.m[13] = v.y;
    r.m[14] = v.z;
    return r;
  }

  static Mat4 scale(const Vec3& s) {
    Mat4 r;
    r.m[0] = s.x;
    r.m[5] = s.y;
    r.m[10] = s.z;
    r.m[15] = 1.0f;
    return r;
  }

  static Mat4 rotate_euler_deg(const Vec3& deg) {
    float cx = std::cos(radians(deg.x)), sx = std::sin(radians(deg.x));
    float cy = std::cos(radians(deg.y)), sy = std::sin(radians(deg.y));
    float cz = std::cos(radians(deg.z)), sz = std::sin(radians(deg.z));

    Mat4 rx = identity();
    rx.m[5] = cx; rx.m[6] = sx; rx.m[9] = -sx; rx.m[10] = cx;
    Mat4 ry = identity();
    ry.m[0] = cy; ry.m[2] = -sy; ry.m[8] = sy; ry.m[10] = cy;
    Mat4 rz = identity();
    rz.m[0] = cz; rz.m[1] = sz; rz.m[4] = -sz; rz.m[5] = cz;
    return rz * ry * rx;
  }

  Mat4 operator*(const Mat4& o) const {
    Mat4 out;
    for (int col = 0; col < 4; ++col) {
      for (int row = 0; row < 4; ++row) {
        float sum = 0.0f;
        for (int k = 0; k < 4; ++k) {
          sum += m[k * 4 + row] * o.m[col * 4 + k];
        }
        out.m[col * 4 + row] = sum;
      }
    }
    return out;
  }

  Vec4 operator*(const Vec4& v) const {
    return {
        m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12] * v.w,
        m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13] * v.w,
        m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
        m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w,
    };
  }

  static Mat4 perspective(float fov_deg, float aspect, float near_z, float far_z) {
    float tan_half = std::tan(radians(fov_deg) * 0.5f);
    Mat4 r;
    r.m[0] = 1.0f / (std::max(0.01f, aspect) * tan_half);
    r.m[5] = 1.0f / tan_half;
    r.m[10] = -(far_z + near_z) / (far_z - near_z);
    r.m[11] = -1.0f;
    r.m[14] = -(2.0f * far_z * near_z) / (far_z - near_z);
    return r;
  }

  static Mat4 ortho(float l, float r_, float b, float t, float n, float f) {
    Mat4 out = identity();
    out.m[0] = 2.0f / (r_ - l);
    out.m[5] = 2.0f / (t - b);
    out.m[10] = -2.0f / (f - n);
    out.m[12] = -(r_ + l) / (r_ - l);
    out.m[13] = -(t + b) / (t - b);
    out.m[14] = -(f + n) / (f - n);
    return out;
  }

  static Mat4 look_at(const Vec3& eye, const Vec3& center, const Vec3& up) {
    Vec3 f = (center - eye).normalized();
    Vec3 s = Vec3::cross(f, up).normalized();
    Vec3 u = Vec3::cross(s, f);
    Mat4 r = identity();
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -Vec3::dot(s, eye);
    r.m[13] = -Vec3::dot(u, eye);
    r.m[14] = Vec3::dot(f, eye);
    return r;
  }
};

struct AABB {
  Vec3 min;
  Vec3 max;
  bool intersects(const AABB& o) const {
    return (min.x <= o.max.x && max.x >= o.min.x) &&
           (min.y <= o.max.y && max.y >= o.min.y) &&
           (min.z <= o.max.z && max.z >= o.min.z);
  }
};

struct Ray {
  Vec3 origin;
  Vec3 dir;
};

inline bool intersect_ray_aabb(const Ray& ray, const AABB& box, float max_dist, float& out_t) {
  float tmin = 0.0f;
  float tmax = max_dist;
  auto slab = [&](float orig, float d, float bmin, float bmax) -> bool {
    if (std::fabs(d) < 1e-7f) {
      return orig >= bmin && orig <= bmax;
    }
    float inv = 1.0f / d;
    float t1 = (bmin - orig) * inv;
    float t2 = (bmax - orig) * inv;
    if (t1 > t2) std::swap(t1, t2);
    tmin = std::max(tmin, t1);
    tmax = std::min(tmax, t2);
    return tmin <= tmax;
  };
  if (!slab(ray.origin.x, ray.dir.x, box.min.x, box.max.x)) return false;
  if (!slab(ray.origin.y, ray.dir.y, box.min.y, box.max.y)) return false;
  if (!slab(ray.origin.z, ray.dir.z, box.min.z, box.max.z)) return false;
  out_t = tmin;
  return true;
}

}  // namespace astra
