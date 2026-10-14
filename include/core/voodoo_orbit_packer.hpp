#pragma once

#include "core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace threeos {

class VoodooVolumeEngine;
class Workspace;
struct VoodooPortal;
struct VoodooBoundingBox;
struct Pose;

/// Near-BB ellipse packing: bump-resolve, paste clear, sibling min-clear, clutter push.
class VoodooOrbitPacker {
 public:
  explicit VoodooOrbitPacker(VoodooVolumeEngine& engine) : engine_(engine) {}

  bool bump_resolve(const Pose& head, bool head_valid, size_t pinned_index);
  void bump_siblings_min_clear(size_t pinned_index);
  void bump_near_field_clutter(const VoodooPortal& portal, Workspace& workspace, const Pose& head,
                               bool head_valid, bool animate);
  bool orbit_near_bbs_clear_of_paste(const Vec3& paste_c, float paste_r);
  Vec3 resolve_cut_paste_origin(const Vec3& pinch_world, float cluster_radius_m);
  void tick_portals(float dt_seconds, const Pose& head, bool head_valid);

  bool walk_orbit(const Vec3& body, const Vec3& fwd, const Vec3& right, float az0, float y,
                  const std::function<bool(const Vec3&)>& pred, Vec3* out) const;
  void set_portal_orbit_target(VoodooPortal& portal, const Vec3& to, const Pose& head,
                               bool head_valid);
  static Vec3 effective_portal_pos(const VoodooPortal& portal);

  float near_footprint_radius_m() const;
  static float portal_near_footprint_radius(const VoodooBoundingBox& near_bb);
  uint32_t max_orbit_slots() const;
  float orbit_radius_at_az(float az) const;
  Vec3 pos_on_orbit_ellipse(const Vec3& body, const Vec3& fwd, const Vec3& right, float az,
                            float y) const;
  float azimuth_on_orbit(const Vec3& body, const Vec3& fwd, const Vec3& right, const Vec3& p) const;
  bool near_targets_clear() const;
  bool near_targets_clear_indices(const std::vector<size_t>& idxs) const;
  void merge_groups(uint64_t from_id, uint64_t into_id);
  size_t find_overlap_portal(const VoodooBoundingBox& candidate, size_t skip_index) const;
  bool group_is_full_circle(uint64_t group_id, const Pose& head, bool head_valid) const;
  static bool near_bbs_overlap_xz(const VoodooBoundingBox& a, const VoodooBoundingBox& b,
                                  float gap_m);
  Pose orbit_near_to_hand(const Pose& seed, const Pose& head, bool head_valid,
                          const Pose& hand_pose, bool hand_valid, uint32_t hand) const;

 private:
  VoodooVolumeEngine& engine_;
};

}  // namespace threeos
