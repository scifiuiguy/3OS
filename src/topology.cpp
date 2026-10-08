#include "core/topology.hpp"

#include <algorithm>
#include <cmath>

namespace threeos {
namespace {

float clampf(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }

Vec3 rotate_by_quat(const Quat& q, const Vec3& v) {
  // q * (0,v) * q^{-1} for unit quaternion
  const float ix = q.w * v.x + q.y * v.z - q.z * v.y;
  const float iy = q.w * v.y + q.z * v.x - q.x * v.z;
  const float iz = q.w * v.z + q.x * v.y - q.y * v.x;
  const float iw = -q.x * v.x - q.y * v.y - q.z * v.z;
  return Vec3{
      ix * q.w + iw * -q.x + iy * -q.z - iz * -q.y,
      iy * q.w + iw * -q.y + iz * -q.x - ix * -q.z,
      iz * q.w + iw * -q.z + ix * -q.y - iy * -q.x,
  };
}

Quat yaw_from_forward_xz(const Quat& head_orientation) {
  const Vec3 fwd = rotate_by_quat(head_orientation, Vec3{0.f, 0.f, -1.f});  // OpenXR -Z forward
  const float yaw = std::atan2(fwd.x, -fwd.z);
  const float half = yaw * 0.5f;
  return Quat{0.f, std::sin(half), 0.f, std::cos(half)};
}

}  // namespace

TopologyWorkspace::TopologyWorkspace(float voxel_size_m, float world_to_proxy_scale)
    : voxel_size_m_(voxel_size_m > 0.f ? voxel_size_m : kDefaultVoxelSizeMeters),
      world_to_proxy_scale_(world_to_proxy_scale > 0.f ? world_to_proxy_scale
                                                      : kDefaultWorldToProxyScale) {}

void TopologyWorkspace::set_world_to_proxy_scale(float ratio) {
  if (ratio > 0.f) {
    world_to_proxy_scale_ = ratio;
    dome_.scale_ratio = ratio;
  }
}

VoxelCoord TopologyWorkspace::world_to_voxel(const Vec3& world) const {
  return VoxelCoord{
      static_cast<int32_t>(std::floor(world.x / voxel_size_m_)),
      static_cast<int32_t>(std::floor(world.y / voxel_size_m_)),
      static_cast<int32_t>(std::floor(world.z / voxel_size_m_)),
  };
}

Vec3 TopologyWorkspace::voxel_center_to_world(const VoxelCoord& vc) const {
  const float h = voxel_size_m_ * 0.5f;
  return Vec3{
      static_cast<float>(vc.x) * voxel_size_m_ + h,
      static_cast<float>(vc.y) * voxel_size_m_ + h,
      static_cast<float>(vc.z) * voxel_size_m_ + h,
  };
}

float TopologyWorkspace::aabb_extent_max(const Aabb& a) {
  return std::max({std::abs(a.max.x - a.min.x), std::abs(a.max.y - a.min.y),
                   std::abs(a.max.z - a.min.z)});
}

Vec3 TopologyWorkspace::aabb_center(const Aabb& a) {
  return Vec3{(a.min.x + a.max.x) * 0.5f, (a.min.y + a.max.y) * 0.5f,
              (a.min.z + a.max.z) * 0.5f};
}

bool TopologyWorkspace::aabb_contains(const Aabb& a, const Vec3& p) {
  return p.x >= a.min.x && p.x <= a.max.x && p.y >= a.min.y && p.y <= a.max.y && p.z >= a.min.z &&
         p.z <= a.max.z;
}

Vec3 TopologyWorkspace::remap_aabb(const Vec3& p, const Aabb& src, const Aabb& dst) {
  auto axis = [](float v, float s0, float s1, float d0, float d1) {
    const float denom = s1 - s0;
    if (std::abs(denom) < 1e-8f) {
      return (d0 + d1) * 0.5f;
    }
    const float t = (v - s0) / denom;
    return d0 + clampf(t, 0.f, 1.f) * (d1 - d0);
  };
  return Vec3{
      axis(p.x, src.min.x, src.max.x, dst.min.x, dst.max.x),
      axis(p.y, src.min.y, src.max.y, dst.min.y, dst.max.y),
      axis(p.z, src.min.z, src.max.z, dst.min.z, dst.max.z),
  };
}

void TopologyWorkspace::open_dome(const Pose& head_pose) {
  const Quat yaw = yaw_from_forward_xz(head_pose.orientation);
  const Vec3 forward = rotate_by_quat(yaw, Vec3{0.f, 0.f, -1.f});
  dome_.active = true;
  dome_.scale_ratio = world_to_proxy_scale_;
  dome_.radius_m = 0.35f;
  dome_.pose.orientation = yaw;
  dome_.pose.position = Vec3{
      head_pose.position.x + forward.x * 0.55f,
      head_pose.position.y - 0.15f,
      head_pose.position.z + forward.z * 0.55f,
  };
}

void TopologyWorkspace::close_dome() { dome_ = DomeState{}; }

Vec3 TopologyWorkspace::world_to_dome_local(const Vec3& world) const {
  const Vec3 delta{world.x - dome_.pose.position.x, world.y - dome_.pose.position.y,
                   world.z - dome_.pose.position.z};
  // Inverse yaw: rotate world delta into dome space, then scale down.
  const Quat inv{ -dome_.pose.orientation.x, -dome_.pose.orientation.y, -dome_.pose.orientation.z,
                  dome_.pose.orientation.w };
  const Vec3 local = rotate_by_quat(inv, delta);
  const float s = dome_.scale_ratio > 0.f ? dome_.scale_ratio : world_to_proxy_scale_;
  return Vec3{local.x / s, local.y / s, local.z / s};
}

Vec3 TopologyWorkspace::dome_local_to_world(const Vec3& local) const {
  const float s = dome_.scale_ratio > 0.f ? dome_.scale_ratio : world_to_proxy_scale_;
  const Vec3 scaled{local.x * s, local.y * s, local.z * s};
  const Vec3 world_delta = rotate_by_quat(dome_.pose.orientation, scaled);
  return Vec3{dome_.pose.position.x + world_delta.x, dome_.pose.position.y + world_delta.y,
              dome_.pose.position.z + world_delta.z};
}

Pose TopologyWorkspace::dome_drop(const Pose& dome_local_pose) const {
  Pose out = dome_local_pose;
  out.position = dome_local_to_world(dome_local_pose.position);
  return out;
}

DoubleProxyState* TopologyWorkspace::find_mutable(EntityId id) {
  for (auto& p : proxies_) {
    if (p.id == id) {
      return &p;
    }
  }
  return nullptr;
}

const DoubleProxyState* TopologyWorkspace::find_double_proxy(EntityId id) const {
  for (const auto& p : proxies_) {
    if (p.id == id) {
      return &p;
    }
  }
  return nullptr;
}

bool TopologyWorkspace::open_double_proxy(EntityId id, const Aabb& far_aabb, const Pose& head_pose) {
  if (id == 0) {
    return false;
  }

  const float far_ext = std::max(aabb_extent_max(far_aabb), voxel_size_m_);
  constexpr float kNearExtent = 0.30f;
  const float scale = far_ext / kNearExtent;

  const Quat yaw = yaw_from_forward_xz(head_pose.orientation);
  const Vec3 forward = rotate_by_quat(yaw, Vec3{0.f, 0.f, -1.f});
  const Vec3 near_center{
      head_pose.position.x + forward.x * 0.45f,
      head_pose.position.y - 0.05f,
      head_pose.position.z + forward.z * 0.45f,
  };
  const float h = kNearExtent * 0.5f;
  Aabb near_aabb{
      Vec3{near_center.x - h, near_center.y - h, near_center.z - h},
      Vec3{near_center.x + h, near_center.y + h, near_center.z + h},
  };

  if (auto* existing = find_mutable(id)) {
    existing->active = true;
    existing->scale_ratio = scale;
    existing->far_aabb = far_aabb;
    existing->near_aabb = near_aabb;
    return true;
  }

  DoubleProxyState state;
  state.id = id;
  state.active = true;
  state.scale_ratio = scale;
  state.far_aabb = far_aabb;
  state.near_aabb = near_aabb;
  proxies_.push_back(state);
  return true;
}

bool TopologyWorkspace::close_double_proxy(EntityId id) {
  const auto it = std::remove_if(proxies_.begin(), proxies_.end(),
                                 [id](const DoubleProxyState& p) { return p.id == id; });
  if (it == proxies_.end()) {
    return false;
  }
  proxies_.erase(it, proxies_.end());
  return true;
}

bool TopologyWorkspace::select_dome_region(EntityId proxy_id, const VoxelCoord& far_min,
                                           const VoxelCoord& far_max, const Pose& head_pose) {
  if (!dome_.active) {
    return false;
  }
  const Vec3 w0 = voxel_center_to_world(far_min);
  const Vec3 w1 = voxel_center_to_world(far_max);
  Aabb far{
      Vec3{std::min(w0.x, w1.x) - voxel_size_m_ * 0.5f,
           std::min(w0.y, w1.y) - voxel_size_m_ * 0.5f,
           std::min(w0.z, w1.z) - voxel_size_m_ * 0.5f},
      Vec3{std::max(w0.x, w1.x) + voxel_size_m_ * 0.5f,
           std::max(w0.y, w1.y) + voxel_size_m_ * 0.5f,
           std::max(w0.z, w1.z) + voxel_size_m_ * 0.5f},
  };
  return open_double_proxy(proxy_id, far, head_pose);
}

Vec3 TopologyWorkspace::far_to_near(EntityId proxy_id, const Vec3& far_world) const {
  const auto* p = find_double_proxy(proxy_id);
  if (p == nullptr || !p->active) {
    return far_world;
  }
  return remap_aabb(far_world, p->far_aabb, p->near_aabb);
}

Vec3 TopologyWorkspace::near_to_far(EntityId proxy_id, const Vec3& near_world) const {
  const auto* p = find_double_proxy(proxy_id);
  if (p == nullptr || !p->active) {
    return near_world;
  }
  return remap_aabb(near_world, p->near_aabb, p->far_aabb);
}

std::optional<Pose> TopologyWorkspace::portal_near_to_far(EntityId proxy_id,
                                                         const Pose& near_pose) const {
  const auto* p = find_double_proxy(proxy_id);
  if (p == nullptr || !p->active) {
    return std::nullopt;
  }
  if (!aabb_contains(p->near_aabb, near_pose.position)) {
    return std::nullopt;
  }
  Pose out = near_pose;
  out.position = near_to_far(proxy_id, near_pose.position);
  return out;
}

std::optional<Pose> TopologyWorkspace::portal_far_to_near(EntityId proxy_id,
                                                         const Pose& far_pose) const {
  const auto* p = find_double_proxy(proxy_id);
  if (p == nullptr || !p->active) {
    return std::nullopt;
  }
  if (!aabb_contains(p->far_aabb, far_pose.position)) {
    return std::nullopt;
  }
  Pose out = far_pose;
  out.position = far_to_near(proxy_id, far_pose.position);
  return out;
}

std::optional<Pose> TopologyWorkspace::portal_cross(EntityId from_id, EntityId to_id,
                                                    const Pose& near_in_to) const {
  (void)from_id;  // Presence of source proxy is required for a valid cross-drag UX;
  if (find_double_proxy(from_id) == nullptr) {
    return std::nullopt;
  }
  return portal_near_to_far(to_id, near_in_to);
}

}  // namespace threeos
