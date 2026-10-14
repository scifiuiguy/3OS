#pragma once

#include "core/hand_interaction.hpp"
#include "core/types.hpp"

#include <cmath>

namespace threeos {
namespace voodoo_geom {

inline float wrap_pi(float a) {
  constexpr float kPi = 3.14159265f;
  constexpr float kTwoPi = 6.2831853f;
  while (a > kPi) {
    a -= kTwoPi;
  }
  while (a < -kPi) {
    a += kTwoPi;
  }
  return a;
}

inline float shortest_daz(float from_az, float to_az) { return wrap_pi(to_az - from_az); }

inline float dist3(const Vec3& a, const Vec3& b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  const float dz = a.z - b.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

inline Quat quat_from_yaw(float yaw) {
  const float half = yaw * 0.5f;
  return Quat{0.f, std::sin(half), 0.f, std::cos(half)};
}

inline Vec3 flat_forward(const Pose& head, bool head_valid) {
  if (!head_valid) {
    return Vec3{0.f, 0.f, 1.f};
  }
  return HandInteractionEngine::floor_forward(head);
}

inline Quat quat_inverse_yaw(const Quat& q) { return Quat{0.f, -q.y, 0.f, q.w}; }

inline Vec3 quat_rotate_yaw(const Quat& q, const Vec3& v) {
  const float y = q.y;
  const float w = q.w;
  const float x2 = v.x * (1.f - 2.f * y * y) + v.z * (2.f * w * y);
  const float z2 = v.z * (1.f - 2.f * y * y) - v.x * (2.f * w * y);
  return Vec3{x2, v.y, z2};
}

inline Quat yaw_billboard_toward(const Vec3& from, const Pose& head, bool head_valid) {
  Vec3 to{};
  if (head_valid) {
    to = Vec3{head.position.x - from.x, 0.f, head.position.z - from.z};
  } else {
    to = Vec3{0.f, 0.f, -1.f};
  }
  const float len = std::sqrt(to.x * to.x + to.z * to.z);
  if (len < 1e-4f) {
    const Vec3 fwd = flat_forward(head, head_valid);
    to = Vec3{-fwd.x, 0.f, -fwd.z};
  }
  return quat_from_yaw(std::atan2(to.x, to.z));
}

inline bool point_in_obb(const Vec3& p, const Pose& box_pose, const Vec3& full_extents) {
  const Vec3 d{p.x - box_pose.position.x, p.y - box_pose.position.y, p.z - box_pose.position.z};
  const Vec3 local = quat_rotate_yaw(quat_inverse_yaw(box_pose.orientation), d);
  const Vec3 half{full_extents.x * 0.5f, full_extents.y * 0.5f, full_extents.z * 0.5f};
  return std::abs(local.x) <= half.x + 1e-4f && std::abs(local.y) <= half.y + 1e-4f &&
         std::abs(local.z) <= half.z + 1e-4f;
}

}  // namespace voodoo_geom
}  // namespace threeos
