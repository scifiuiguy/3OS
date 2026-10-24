#pragma once

#include "core/types.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace threeos {

/// ~3 inch voxel edge length in meters (OpenXR / SI); Trivariate high-throughput default.
constexpr float kDefaultVoxelSizeMeters = 0.0762f;

/// Far-world meters represented by one meter in a near-field proxy (e.g. 100 => 1:100).
constexpr float kDefaultWorldToProxyScale = 100.f;

struct Aabb {
  Vec3 min{};
  Vec3 max{};
};

/// Invisible voodoo-drop field proxy (API name DomeState is historical; never rendered).
struct DomeState {
  bool active = false;
  float scale_ratio = kDefaultWorldToProxyScale;
  Pose pose{};       // proxy origin in world meters (comfortable reach ahead of head)
  float radius_m = 0.35f;
};

struct DoubleProxyState {
  EntityId id = 0;
  bool active = false;
  float scale_ratio = 1.f;  // far extent / near extent (uniform)
  Aabb far_aabb{};
  Aabb near_aabb{};
};

/// Multiscale workspace: voxels, invisible voodoo-drop proxy, Double-Proxy lattices, portals.
class TopologyWorkspace {
 public:
  explicit TopologyWorkspace(float voxel_size_m = kDefaultVoxelSizeMeters,
                             float world_to_proxy_scale = kDefaultWorldToProxyScale);

  float voxel_size() const { return voxel_size_m_; }
  float world_to_proxy_scale() const { return world_to_proxy_scale_; }
  void set_world_to_proxy_scale(float ratio);

  VoxelCoord world_to_voxel(const Vec3& world) const;
  Vec3 voxel_center_to_world(const VoxelCoord& vc) const;

  /// Open the invisible voodoo-drop proxy in comfortable reach ahead of the head pose.
  void open_dome(const Pose& head_pose);
  void close_dome();
  const DomeState& dome() const { return dome_; }

  /// Far world ↔ drop-proxy local (relative to proxy pose, proxy meters).
  Vec3 world_to_dome_local(const Vec3& world) const;
  Vec3 dome_local_to_world(const Vec3& local) const;

  /// Voodoo drop: proxy-local hit → far-field world position (Y preserved from local).
  Pose dome_drop(const Pose& dome_local_pose) const;

  /// Open a double-proxy lattice for a far AABB; near volume sits in front of the head.
  bool open_double_proxy(EntityId id, const Aabb& far_aabb, const Pose& head_pose);
  bool close_double_proxy(EntityId id);
  const DoubleProxyState* find_double_proxy(EntityId id) const;
  const std::vector<DoubleProxyState>& double_proxies() const { return proxies_; }

  /// Select a far voxel slab via the drop proxy and open a double-proxy lattice for it.
  bool select_dome_region(EntityId proxy_id, const VoxelCoord& far_min, const VoxelCoord& far_max,
                          const Pose& head_pose);

  Vec3 far_to_near(EntityId proxy_id, const Vec3& far_world) const;
  Vec3 near_to_far(EntityId proxy_id, const Vec3& near_world) const;

  /// Voodoo portal: near pose inside proxy ↔ far pose (orientation copied).
  std::optional<Pose> portal_near_to_far(EntityId proxy_id, const Pose& near_pose) const;
  std::optional<Pose> portal_far_to_near(EntityId proxy_id, const Pose& far_pose) const;

  /// Cross-proxy: near pose in `from_id` maps to far, then into `to_id` far via near drop in `to_id`.
  /// Convenience: treat `near_in_to` as the drop location inside the destination near volume.
  std::optional<Pose> portal_cross(EntityId from_id, EntityId to_id, const Pose& near_in_to) const;

  static float aabb_extent_max(const Aabb& a);
  static Vec3 aabb_center(const Aabb& a);
  static bool aabb_contains(const Aabb& a, const Vec3& p);
  static Vec3 remap_aabb(const Vec3& p, const Aabb& src, const Aabb& dst);

 private:
  float voxel_size_m_;
  float world_to_proxy_scale_;
  DomeState dome_{};
  std::vector<DoubleProxyState> proxies_;

  DoubleProxyState* find_mutable(EntityId id);
};

}  // namespace threeos
