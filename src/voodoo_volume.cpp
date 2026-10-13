#include "core/voodoo_volume.hpp"

#include <algorithm>

namespace threeos {
namespace {

Quat quat_from_yaw(float yaw) {
  const float half = yaw * 0.5f;
  return Quat{0.f, std::sin(half), 0.f, std::cos(half)};
}

float clampf(float v, float lo, float hi) {
  return std::max(lo, std::min(hi, v));
}

float dist3(const Vec3& a, const Vec3& b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  const float dz = a.z - b.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

float length2_xz(const Vec3& v) {
  return std::sqrt(v.x * v.x + v.z * v.z);
}

float azimuth_of(const Vec3& from_body, const Vec3& p) {
  return std::atan2(p.x - from_body.x, p.z - from_body.z);
}

Vec3 pos_from_az(const Vec3& body, float az, float radius, float y) {
  return Vec3{body.x + std::sin(az) * radius, y, body.z + std::cos(az) * radius};
}

}  // namespace

float VoodooVolumeEngine::wrap_pi(float a) {
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

float VoodooVolumeEngine::shortest_daz(float from_az, float to_az) {
  return wrap_pi(to_az - from_az);
}

float VoodooVolumeEngine::near_footprint_radius_m() const {
  const Vec3 e = near_extents_m();
  // Horizontal half-diagonal so yaw-facing corners cannot kiss.
  return 0.5f * std::sqrt(e.x * e.x + e.z * e.z) + kNearClearGapM * 0.5f;
}

uint32_t VoodooVolumeEngine::max_orbit_slots() const {
  // Pack along the front↔behind ellipse (avg radius). Extra pad for clearance.
  const float need = 2.f * near_footprint_radius_m() + 0.02f;
  const float r = 0.5f * (kNearRestForwardM + kOrbitBehindM);
  if (!(need > 1e-4f) || !(r > 1e-4f)) {
    return 1u;
  }
  const float ratio = clampf(need / (2.f * r), 0.f, 1.f);
  const float need_ang = 2.f * std::asin(ratio);
  if (!(need_ang > 1e-4f)) {
    return 1u;
  }
  constexpr float kTwoPi = 6.2831853f;
  const auto geo = static_cast<uint32_t>(std::floor(kTwoPi / need_ang + 1e-4f));
  return std::max(1u, std::min(kMaxNearPortals, geo));
}

float VoodooVolumeEngine::orbit_radius_at_az(float az) const {
  const float c = std::cos(az);
  const float r_lat = std::max(kNearRestForwardM, kOrbitBehindM);
  const float r_z = (c >= 0.f) ? kNearRestForwardM : kOrbitBehindM;
  const float s = std::sin(az);
  return std::sqrt((r_lat * s) * (r_lat * s) + (r_z * c) * (r_z * c));
}

Vec3 VoodooVolumeEngine::pos_on_orbit_ellipse(const Vec3& body, const Vec3& fwd, const Vec3& right,
                                               float az, float y) const {
  const float s = std::sin(az);
  const float c = std::cos(az);
  const float r_lat = std::max(kNearRestForwardM, kOrbitBehindM);
  const float r_z = (c >= 0.f) ? kNearRestForwardM : kOrbitBehindM;
  return Vec3{body.x + right.x * (r_lat * s) + fwd.x * (r_z * c), y,
              body.z + right.z * (r_lat * s) + fwd.z * (r_z * c)};
}

float VoodooVolumeEngine::azimuth_on_orbit(const Vec3& body, const Vec3& fwd, const Vec3& right,
                                           const Vec3& p) const {
  const float dx = p.x - body.x;
  const float dz = p.z - body.z;
  const float along_fwd = dx * fwd.x + dz * fwd.z;
  const float along_right = dx * right.x + dz * right.z;
  return std::atan2(along_right, along_fwd);
}

bool VoodooVolumeEngine::near_targets_clear() const {
  std::vector<size_t> all(portals_.size());
  for (size_t i = 0; i < portals_.size(); ++i) {
    all[i] = i;
  }
  return near_targets_clear_indices(all);
}

bool VoodooVolumeEngine::near_targets_clear_indices(const std::vector<size_t>& idxs) const {
  for (size_t a = 0; a < idxs.size(); ++a) {
    for (size_t b = a + 1; b < idxs.size(); ++b) {
      const auto& pa = portals_[idxs[a]];
      const auto& pb = portals_[idxs[b]];
      VoodooBoundingBox ba = pa.near_bb;
      VoodooBoundingBox bb = pb.near_bb;
      ba.pose.position = pa.bump_t < 1.f ? pa.bump_to : pa.near_bb.pose.position;
      bb.pose.position = pb.bump_t < 1.f ? pb.bump_to : pb.near_bb.pose.position;
      if (near_bbs_overlap_xz(ba, bb, kNearClearGapM)) {
        return false;
      }
    }
  }
  return true;
}

void VoodooVolumeEngine::merge_groups(uint64_t from_id, uint64_t into_id) {
  if (from_id == 0 || into_id == 0 || from_id == into_id) {
    return;
  }
  for (auto& p : portals_) {
    if (p.group_id == from_id) {
      p.group_id = into_id;
    }
  }
}

size_t VoodooVolumeEngine::find_overlap_portal(const VoodooBoundingBox& candidate,
                                              size_t skip_index) const {
  for (size_t i = 0; i < portals_.size(); ++i) {
    if (i == skip_index || !portals_[i].near_bb.open) {
      continue;
    }
    VoodooBoundingBox other = portals_[i].near_bb;
    other.pose.position = portals_[i].bump_t < 1.f ? portals_[i].bump_to
                                                   : portals_[i].near_bb.pose.position;
    if (near_bbs_overlap_xz(candidate, other, kNearClearGapM)) {
      return i;
    }
  }
  return static_cast<size_t>(-1);
}

bool VoodooVolumeEngine::group_is_full_circle(uint64_t group_id, const Pose& head,
                                             bool head_valid) const {
  (void)head;
  (void)head_valid;
  if (group_id == 0) {
    return false;
  }
  uint32_t n = 0;
  for (const auto& p : portals_) {
    if (p.group_id == group_id && p.near_bb.open) {
      ++n;
    }
  }
  return n >= max_orbit_slots();
}

bool VoodooVolumeEngine::near_bbs_overlap_xz(const VoodooBoundingBox& a, const VoodooBoundingBox& b,
                                             float gap_m) {
  // Conservative circle vs circle on footprint radii.
  const float ra = 0.5f * std::sqrt(a.extents.x * a.extents.x + a.extents.z * a.extents.z);
  const float rb = 0.5f * std::sqrt(b.extents.x * b.extents.x + b.extents.z * b.extents.z);
  const float need = ra + rb + gap_m;
  const float dx = a.pose.position.x - b.pose.position.x;
  const float dz = a.pose.position.z - b.pose.position.z;
  return (dx * dx + dz * dz) < (need * need);
}

bool VoodooVolumeEngine::set_params(const VoodooFieldParams& p) {
  if (!(p.voxel_size_m > 1e-4f) || p.voxel_size_m > 1.f) {
    return false;
  }
  if (p.near_bb_voxels_x < 1 || p.near_bb_voxels_y < 1 || p.near_bb_voxels_z < 1 ||
      p.near_bb_voxels_x > 64 || p.near_bb_voxels_y > 64 || p.near_bb_voxels_z > 64) {
    return false;
  }
  if (!(p.field_radius_m > 0.5f) || p.field_radius_m > 200.f) {
    return false;
  }
  if (!(p.field_height_m > 0.5f) || p.field_height_m > 100.f) {
    return false;
  }
  if (!(p.arm_reach_m > 0.05f) || p.arm_reach_m > 2.f) {
    return false;
  }
  params_ = p;
  for (uint32_t h = 0; h < 2u; ++h) {
    auto& s = hands_[h].session;
    if (s.far_bb.open && s.snapped_centroid_id == 0) {
      apply_far_start_extents(s.far_bb);
    }
    if (s.near_bb.open) {
      apply_near_extents(s.near_bb);
    }
  }
  for (auto& portal : portals_) {
    apply_near_extents(portal.near_bb);
  }
  return true;
}

void VoodooVolumeEngine::reset() {
  params_ = VoodooFieldParams{};
  portals_.clear();
  next_portal_id_ = 1;
  next_group_id_ = 1;
  clear_hand_session(0);
  clear_hand_session(1);
}

void VoodooVolumeEngine::clear_hand_session(uint32_t hand) {
  if (hand >= 2u) {
    return;
  }
  hands_[hand] = HandRuntime{};
  hands_[hand].session.hand = hand;
}

void VoodooVolumeEngine::cancel_drag(uint32_t hand) {
  clear_hand_session(hand);
}

bool VoodooVolumeEngine::dismiss_portal(uint64_t portal_id) {
  const auto it = std::remove_if(portals_.begin(), portals_.end(),
                                 [portal_id](const VoodooPortal& p) { return p.id == portal_id; });
  if (it == portals_.end()) {
    return false;
  }
  portals_.erase(it, portals_.end());
  return true;
}

void VoodooVolumeEngine::apply_near_extents(VoodooBoundingBox& bb) const {
  bb.extents = near_extents_m();
  bb.aspect_x = params_.near_bb_voxels_x;
  bb.aspect_y = params_.near_bb_voxels_y;
  bb.aspect_z = params_.near_bb_voxels_z;
}

void VoodooVolumeEngine::apply_far_start_extents(VoodooBoundingBox& bb) const {
  bb.extents = far_start_extents_m();
  bb.aspect_x = params_.near_bb_voxels_x;
  bb.aspect_y = params_.near_bb_voxels_y;
  bb.aspect_z = params_.near_bb_voxels_z;
}

Vec3 VoodooVolumeEngine::fit_extents_for_aabb(const Vec3& aabb_min, const Vec3& aabb_max) const {
  const Vec3 size{std::max(1e-4f, aabb_max.x - aabb_min.x),
                  std::max(1e-4f, aabb_max.y - aabb_min.y),
                  std::max(1e-4f, aabb_max.z - aabb_min.z)};
  const Vec3 aspect = near_extents_m();
  const float sx = size.x / std::max(aspect.x, 1e-6f);
  const float sy = size.y / std::max(aspect.y, 1e-6f);
  const float sz = size.z / std::max(aspect.z, 1e-6f);
  const float s = std::max({sx, sy, sz, kFarStartScale});
  return Vec3{aspect.x * s, aspect.y * s, aspect.z * s};
}

Vec3 VoodooVolumeEngine::flat_forward(const Pose& head, bool head_valid) {
  if (!head_valid) {
    return Vec3{0.f, 0.f, 1.f};
  }
  return HandInteractionEngine::floor_forward(head);
}

Quat VoodooVolumeEngine::yaw_billboard_toward(const Vec3& from, const Pose& head, bool head_valid) {
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

Vec3 VoodooVolumeEngine::quat_rotate_yaw(const Quat& q, const Vec3& v) {
  // yaw-only: q = (0, sin, 0, cos)
  const float y = q.y;
  const float w = q.w;
  const float x2 = v.x * (1.f - 2.f * y * y) + v.z * (2.f * w * y);
  const float z2 = v.z * (1.f - 2.f * y * y) - v.x * (2.f * w * y);
  return Vec3{x2, v.y, z2};
}

Quat VoodooVolumeEngine::quat_inverse_yaw(const Quat& q) {
  return Quat{0.f, -q.y, 0.f, q.w};
}

bool VoodooVolumeEngine::point_in_obb(const Vec3& p, const Pose& box_pose, const Vec3& full_extents) {
  const Vec3 d{p.x - box_pose.position.x, p.y - box_pose.position.y, p.z - box_pose.position.z};
  const Vec3 local = quat_rotate_yaw(quat_inverse_yaw(box_pose.orientation), d);
  const Vec3 half{full_extents.x * 0.5f, full_extents.y * 0.5f, full_extents.z * 0.5f};
  return std::abs(local.x) <= half.x + 1e-4f && std::abs(local.y) <= half.y + 1e-4f &&
         std::abs(local.z) <= half.z + 1e-4f;
}

bool VoodooVolumeEngine::point_in_any_near_bb(const Vec3& p) const {
  for (const auto& portal : portals_) {
    if (portal.near_bb.open && point_in_obb(p, portal.near_bb.pose, portal.near_bb.extents)) {
      return true;
    }
  }
  for (uint32_t h = 0; h < 2u; ++h) {
    const auto& s = hands_[h].session;
    if (s.near_bb.open && point_in_obb(p, s.near_bb.pose, s.near_bb.extents)) {
      return true;
    }
  }
  return false;
}

Pose VoodooVolumeEngine::far_rest_pose(const Pose& head, bool head_valid) const {
  Pose p{};
  const Vec3 fwd = flat_forward(head, head_valid);
  const float navel_y = head_valid ? (head.position.y - kNavelBelowHeadM) : 1.25f;
  const float hx = head_valid ? head.position.x : 0.f;
  const float hz = head_valid ? head.position.z : 0.f;
  p.position = Vec3{hx + fwd.x * kFarRestForwardM, navel_y, hz + fwd.z * kFarRestForwardM};
  p.orientation = yaw_billboard_toward(p.position, head, head_valid);
  return p;
}

Pose VoodooVolumeEngine::near_rest_pose(const Pose& head, bool head_valid) const {
  Pose p{};
  const Vec3 fwd = flat_forward(head, head_valid);
  const float navel_y = head_valid ? (head.position.y - kNavelBelowHeadM) : 1.25f;
  const float hx = head_valid ? head.position.x : 0.f;
  const float hz = head_valid ? head.position.z : 0.f;
  const float half_h = near_extents_m().y * 0.5f;
  const float bottom_y = navel_y - kNearRestYBelowNavelM;
  p.position =
      Vec3{hx + fwd.x * kNearRestForwardM, bottom_y + half_h, hz + fwd.z * kNearRestForwardM};
  p.orientation = yaw_billboard_toward(p.position, head, head_valid);
  return p;
}

Pose VoodooVolumeEngine::orbit_near_to_hand(const Pose& seed, const Pose& head, bool head_valid,
                                             const Pose& hand_pose, bool hand_valid,
                                             uint32_t hand) const {
  const float body_x = head_valid ? head.position.x : 0.f;
  const float body_z = head_valid ? head.position.z : 0.f;
  const Vec3 body{body_x, 0.f, body_z};
  const Vec3 fwd = flat_forward(head, head_valid);
  const Vec3 right = Vec3{fwd.z, 0.f, -fwd.x};

  Vec3 aim{};
  if (hand_valid) {
    aim = Vec3{hand_pose.position.x - body_x, 0.f, hand_pose.position.z - body_z};
  }
  if (length2_xz(aim) < 0.05f) {
    // L biases −right, R +right when aim is ambiguous.
    const float sign = hand == 0u ? -1.f : 1.f;
    aim = Vec3{fwd.x + right.x * 0.35f * sign, 0.f, fwd.z + right.z * 0.35f * sign};
  }
  const float az = azimuth_on_orbit(body, fwd, right, Vec3{body_x + aim.x, seed.position.y, body_z + aim.z});

  Pose out = seed;
  out.position = pos_on_orbit_ellipse(body, fwd, right, az, seed.position.y);
  out.orientation = yaw_billboard_toward(out.position, head, head_valid);
  return out;
}

void VoodooVolumeEngine::open_drag(uint32_t hand, VoodooMode mode, const Pose& hand_pose,
                                   const Pose& head, bool head_valid) {
  auto& hr = hands_[hand];
  hr.session = VoodooSessionState{};
  hr.session.hand = hand;
  // Capacity full, or cursor already inside a parked near BB → no new voodoo.
  if (!creation_allowed() || point_in_any_near_bb(hand_pose.position)) {
    hr.session.creation_refused = true;
    return;
  }
  hr.session.mode = mode;
  hr.session.open = true;
  hr.session.phase = VoodooSessionPhase::Dragging;
  hr.rest_far_ = far_rest_pose(head, head_valid);
  hr.session.far_bb.open = true;
  hr.session.far_bb.pose = hr.rest_far_;
  hr.session.far_bb.linked_centroid_id = 0;
  apply_far_start_extents(hr.session.far_bb);
  hr.far_smoothed_ = hr.rest_far_.position;
  hr.extents_smoothed_ = hr.session.far_bb.extents;
  hr.have_far_smoothed_ = true;
  hr.session.near_bb = {};
  hr.start_hand_ = hand_pose;
  hr.have_start_hand_ = true;
  hr.deadzone_armed_ = false;
  hr.deadzone_left_once_ = false;
  hr.session.arrow = {};
  hr.session.snap_nodes.clear();
  hr.session.snapped_centroid_id = 0;
  hr.drag_hold_s_ = 0.f;
  update_arrow(hand, hand_pose, head, head_valid, nullptr);
}

Vec3 VoodooVolumeEngine::near_bb_bottom_center(const VoodooBoundingBox& bb) {
  // Center-bottom-center of the near OBB (parent-node / temp centroid seat).
  return Vec3{bb.pose.position.x, bb.pose.position.y - bb.extents.y * 0.5f, bb.pose.position.z};
}

void VoodooVolumeEngine::refresh_ghost_near_poses(VoodooPortal& portal) const {
  for (auto& g : portal.ghosts) {
    if (g.is_parent_box) {
      g.near_pose.position = near_bb_bottom_center(portal.near_bb);
      g.near_pose.orientation = portal.near_bb.pose.orientation;
      continue;
    }
    const Vec3 d{g.far_pose.position.x - portal.far_bb.pose.position.x,
                 g.far_pose.position.y - portal.far_bb.pose.position.y,
                 g.far_pose.position.z - portal.far_bb.pose.position.z};
    const Vec3 local = quat_rotate_yaw(quat_inverse_yaw(portal.far_bb.pose.orientation), d);
    const Vec3 scaled{local.x * g.scale, local.y * g.scale, local.z * g.scale};
    const Vec3 world = quat_rotate_yaw(portal.near_bb.pose.orientation, scaled);
    g.near_pose.position = Vec3{portal.near_bb.pose.position.x + world.x,
                                portal.near_bb.pose.position.y + world.y,
                                portal.near_bb.pose.position.z + world.z};
    g.near_pose.orientation = portal.near_bb.pose.orientation;
  }
}

void VoodooVolumeEngine::rebuild_ghosts(VoodooPortal& portal, const CentroidRegistry& centroids,
                                        const Workspace& workspace) {
  portal.ghosts.clear();
  const float near_w = std::max(portal.near_bb.extents.x, 1e-4f);
  const float far_w = std::max(portal.far_bb.extents.x, 1e-4f);
  const float scale = clampf(near_w / far_w, 0.05f, 1.f);

  auto push_ghost = [&](const WorkspaceEntry& e, bool parent_box) {
    VoodooGhost g;
    g.entity_id = e.id;
    g.is_parent_box = parent_box;
    g.far_pose = e.pose;
    g.scale = scale;
    portal.ghosts.push_back(g);
  };

  if (portal.mode == VoodooMode::Snap && portal.persistent_centroid_id != 0) {
    const CentroidState* c = centroids.find(portal.persistent_centroid_id);
    if (c != nullptr) {
      for (const auto& e : workspace.entries()) {
        if (!e.in_field) {
          continue;
        }
        const bool match = (c->kind == CentroidKind::God)
                               ? (e.parent_box == 0 && e.god_root_id == c->id)
                               : (e.parent_box == c->id);
        if (match) {
          push_ghost(e, false);
        }
      }
      if (c->kind == CentroidKind::Box) {
        if (const WorkspaceEntry* box = workspace.find(c->id)) {
          push_ghost(*box, true);
        }
      }
    }
  } else if (portal.mode == VoodooMode::FreeRange) {
    for (const auto& e : workspace.entries()) {
      if (!e.in_field) {
        continue;
      }
      // Encapsulated if center (+ glyph pad) lies in far OBB.
      if (point_in_obb(e.pose.position, portal.far_bb.pose, portal.far_bb.extents)) {
        push_ghost(e, false);
      }
    }
  }
  refresh_ghost_near_poses(portal);
}

bool VoodooVolumeEngine::bump_resolve(const Pose& head, bool head_valid, size_t pinned_index) {
  if (pinned_index >= portals_.size()) {
    return false;
  }
  if (portals_.size() < 2) {
    refresh_ghost_near_poses(portals_[pinned_index]);
    return true;
  }
  const float body_x = head_valid ? head.position.x : 0.f;
  const float body_z = head_valid ? head.position.z : 0.f;
  const Vec3 body{body_x, 0.f, body_z};
  const Vec3 fwd = flat_forward(head, head_valid);
  const Vec3 right = Vec3{fwd.z, 0.f, -fwd.x};
  const float foot = near_footprint_radius_m();
  // Slight pad so post-resolve footprint tests don't flicker on float edges.
  const float need_center = 2.f * foot + 0.015f;

  // Snapshot pre-resolve state for full rollback on failure (including in-flight bumps).
  struct Snapshot {
    Vec3 pos{};
    Vec3 bump_from{};
    Vec3 bump_to{};
    float bump_t = 1.f;
    uint64_t group_id = 0;
    Quat orientation{};
  };
  std::vector<Snapshot> snap(portals_.size());
  for (size_t i = 0; i < portals_.size(); ++i) {
    snap[i].pos = portals_[i].near_bb.pose.position;
    snap[i].bump_from = portals_[i].bump_from;
    snap[i].bump_to = portals_[i].bump_to;
    snap[i].bump_t = portals_[i].bump_t;
    snap[i].group_id = portals_[i].group_id;
    snap[i].orientation = portals_[i].near_bb.pose.orientation;
  }
  auto restore_all = [&]() {
    for (size_t i = 0; i < portals_.size() && i < snap.size(); ++i) {
      portals_[i].near_bb.pose.position = snap[i].pos;
      portals_[i].near_bb.pose.orientation = snap[i].orientation;
      portals_[i].bump_from = snap[i].bump_from;
      portals_[i].bump_to = snap[i].bump_to;
      portals_[i].bump_t = snap[i].bump_t;
      portals_[i].group_id = snap[i].group_id;
    }
  };

  uint64_t active = portals_[pinned_index].group_id;
  if (active == 0) {
    active = next_group_id_++;
    portals_[pinned_index].group_id = active;
  }

  struct Slot {
    size_t idx = 0;
    float az = 0.f;
    float y = 0.f;
    bool pinned = false;
  };

  auto rebuild_slots = [&](std::vector<Slot>* out) {
    out->clear();
    for (size_t i = 0; i < portals_.size(); ++i) {
      if (portals_[i].group_id != active) {
        continue;
      }
      Slot s;
      s.idx = i;
      // Prefer settled/target pose — never sample a mid-chord lerp for azimuth.
      const Vec3 src = (portals_[i].bump_t < 1.f) ? portals_[i].bump_to
                                                   : portals_[i].near_bb.pose.position;
      s.az = azimuth_on_orbit(body, fwd, right, src);
      s.y = src.y;
      s.pinned = (i == pinned_index);
      const Vec3 on_arc = pos_on_orbit_ellipse(body, fwd, right, s.az, s.y);
      portals_[i].bump_from = on_arc;
      if (portals_[i].bump_t >= 1.f) {
        portals_[i].near_bb.pose.position = on_arc;
      }
      out->push_back(s);
    }
  };

  auto resolve_slots = [&](std::vector<Slot>& slots) {
    // Euclidean separation on the ellipse (angular chord math is wrong on ovals).
    constexpr float kStep = 0.025f;
    for (int pass = 0; pass < 48; ++pass) {
      bool moved = false;
      for (size_t i = 0; i < slots.size(); ++i) {
        for (size_t j = i + 1; j < slots.size(); ++j) {
          auto& a = slots[i];
          auto& b = slots[j];
          if (a.pinned && b.pinned) {
            continue;
          }
          const Vec3 pa = pos_on_orbit_ellipse(body, fwd, right, a.az, a.y);
          const Vec3 pb = pos_on_orbit_ellipse(body, fwd, right, b.az, b.y);
          const float dx = pb.x - pa.x;
          const float dz = pb.z - pa.z;
          const float dist = std::sqrt(dx * dx + dz * dz);
          if (dist + 1e-4f >= need_center) {
            continue;
          }
          const float daz = shortest_daz(a.az, b.az);
          const float sign = daz >= 0.f ? 1.f : -1.f;
          // Walk apart along the short arc until clear or step budget.
          for (int step = 0; step < 64; ++step) {
            const Vec3 qa = pos_on_orbit_ellipse(body, fwd, right, a.az, a.y);
            const Vec3 qb = pos_on_orbit_ellipse(body, fwd, right, b.az, b.y);
            const float ddx = qb.x - qa.x;
            const float ddz = qb.z - qa.z;
            if (std::sqrt(ddx * ddx + ddz * ddz) + 1e-4f >= need_center) {
              break;
            }
            if (a.pinned) {
              b.az = wrap_pi(b.az + sign * kStep);
            } else if (b.pinned) {
              a.az = wrap_pi(a.az - sign * kStep);
            } else {
              a.az = wrap_pi(a.az - sign * kStep * 0.5f);
              b.az = wrap_pi(b.az + sign * kStep * 0.5f);
            }
            moved = true;
          }
        }
      }
      if (!moved) {
        break;
      }
    }
    for (const auto& s : slots) {
      auto& portal = portals_[s.idx];
      const Vec3 to = pos_on_orbit_ellipse(body, fwd, right, s.az, s.y);
      portal.bump_to = to;
      const float moved = dist3(portal.bump_from, to);
      if (s.pinned || moved < 1e-4f) {
        portal.near_bb.pose.position = to;
        portal.bump_t = 1.f;
      } else {
        portal.near_bb.pose.position = portal.bump_from;
        portal.bump_t = 0.f;
      }
      portal.near_bb.pose.orientation = yaw_billboard_toward(to, head, head_valid);
    }
  };

  // Grow the facing group: resolve, then absorb any outside portal we still hit.
  for (int grow = 0; grow < 16; ++grow) {
    std::vector<Slot> slots;
    rebuild_slots(&slots);
    if (slots.size() > max_orbit_slots()) {
      restore_all();
      return false;
    }
    resolve_slots(slots);

    std::vector<size_t> active_idxs;
    active_idxs.reserve(slots.size());
    for (const auto& s : slots) {
      active_idxs.push_back(s.idx);
    }
    // One more resolve pass if footprints still kiss.
    if (!near_targets_clear_indices(active_idxs)) {
      resolve_slots(slots);
    }
    if (!near_targets_clear_indices(active_idxs)) {
      restore_all();
      return false;
    }

    // Merge any outside group we now touch.
    bool merged = false;
    for (const auto& s : slots) {
      VoodooBoundingBox cand = portals_[s.idx].near_bb;
      cand.pose.position = portals_[s.idx].bump_t < 1.f ? portals_[s.idx].bump_to
                                                        : portals_[s.idx].near_bb.pose.position;
      const size_t hit = find_overlap_portal(cand, s.idx);
      if (hit == static_cast<size_t>(-1)) {
        continue;
      }
      if (portals_[hit].group_id == active) {
        continue;
      }
      merge_groups(portals_[hit].group_id, active);
      portals_[hit].group_id = active;
      merged = true;
    }
    if (!merged) {
      if (near_targets_clear()) {
        for (auto& portal : portals_) {
          if (portal.group_id == active) {
            refresh_ghost_near_poses(portal);
          }
        }
        return true;
      }
      // Absorb any still-overlapping outsider (including peer-group pairs) into
      // the facing group and keep resolving until the world is clear.
      for (size_t i = 0; i < portals_.size() && !merged; ++i) {
        VoodooBoundingBox cand = portals_[i].near_bb;
        cand.pose.position = portals_[i].bump_t < 1.f ? portals_[i].bump_to
                                                      : portals_[i].near_bb.pose.position;
        const size_t hit = find_overlap_portal(cand, i);
        if (hit == static_cast<size_t>(-1)) {
          continue;
        }
        if (portals_[i].group_id != active) {
          merge_groups(portals_[i].group_id, active);
          portals_[i].group_id = active;
        }
        if (portals_[hit].group_id != active) {
          merge_groups(portals_[hit].group_id, active);
          portals_[hit].group_id = active;
        }
        merged = true;
      }
      if (!merged) {
        // Overlap persists inside a single group — fail.
        return false;
      }
    }
  }

  restore_all();
  return false;
}

void VoodooVolumeEngine::tick_portals(float dt_seconds, const Pose& head, bool head_valid) {
  const float dt = clampf(dt_seconds, 1e-4f, 0.1f);
  const float body_x = head_valid ? head.position.x : 0.f;
  const float body_z = head_valid ? head.position.z : 0.f;
  const Vec3 body{body_x, 0.f, body_z};
  const Vec3 fwd = flat_forward(head, head_valid);
  const Vec3 right = Vec3{fwd.z, 0.f, -fwd.x};
  for (auto& p : portals_) {
    if (p.bump_t >= 1.f) {
      continue;
    }
    p.bump_t = clampf(p.bump_t + dt / kBumpDurationS, 0.f, 1.f);
    if (p.bump_t >= 1.f) {
      // Snap exactly to target so ellipse re-parameterization cannot drift.
      p.near_bb.pose.position = p.bump_to;
      p.near_bb.pose.orientation = yaw_billboard_toward(p.bump_to, head, head_valid);
    } else {
      // Smoothstep along the shortest ellipse arc (front rest ↔ 2ft behind).
      const float u = p.bump_t;
      const float s = u * u * (3.f - 2.f * u);
      const float az0 = azimuth_on_orbit(body, fwd, right, p.bump_from);
      const float az1 = azimuth_on_orbit(body, fwd, right, p.bump_to);
      const float daz = shortest_daz(az0, az1);
      const float az = wrap_pi(az0 + daz * s);
      const float y = p.bump_from.y + (p.bump_to.y - p.bump_from.y) * s;
      p.near_bb.pose.position = pos_on_orbit_ellipse(body, fwd, right, az, y);
      p.near_bb.pose.orientation =
          yaw_billboard_toward(p.near_bb.pose.position, head, head_valid);
    }
    refresh_ghost_near_poses(p);
  }
}

void VoodooVolumeEngine::park(uint32_t hand, const Pose& hand_pose, bool hand_valid,
                              const Pose& head, bool head_valid, CentroidRegistry& centroids,
                              const Workspace& workspace) {
  auto& hr = hands_[hand];
  if (!hr.session.open || hr.session.phase != VoodooSessionPhase::Dragging) {
    return;
  }
  if (!creation_allowed()) {
    // Hard portal cap: cancel both BBs for this gesture.
    clear_hand_session(hand);
    return;
  }

  const Pose seed = near_rest_pose(head, head_valid);
  const Pose near_target =
      orbit_near_to_hand(seed, head, head_valid, hand_pose, hand_valid, hand);

  VoodooBoundingBox probe{};
  probe.open = true;
  probe.pose = near_target;
  apply_near_extents(probe);

  // Collect groups we'd collide with at the park pose.
  std::vector<uint64_t> hit_groups;
  for (size_t i = 0; i < portals_.size(); ++i) {
    if (!portals_[i].near_bb.open) {
      continue;
    }
    VoodooBoundingBox other = portals_[i].near_bb;
    other.pose.position = portals_[i].bump_t < 1.f ? portals_[i].bump_to
                                                   : portals_[i].near_bb.pose.position;
    if (!near_bbs_overlap_xz(probe, other, kNearClearGapM)) {
      continue;
    }
    const uint64_t gid = portals_[i].group_id;
    if (gid == 0) {
      continue;
    }
    if (std::find(hit_groups.begin(), hit_groups.end(), gid) == hit_groups.end()) {
      hit_groups.push_back(gid);
    }
  }

  // Full-ring group: refuse only colliding parks; non-colliding still allowed.
  for (uint64_t gid : hit_groups) {
    if (group_is_full_circle(gid, head, head_valid)) {
      clear_hand_session(hand);
      return;
    }
  }

  uint64_t gid = 0;
  if (hit_groups.empty()) {
    gid = next_group_id_++;
  } else {
    gid = hit_groups[0];
    for (size_t i = 1; i < hit_groups.size(); ++i) {
      merge_groups(hit_groups[i], gid);
    }
  }

  VoodooPortal portal{};
  portal.id = next_portal_id_++;
  portal.mode = hr.session.mode;
  portal.creating_hand = hand;
  portal.group_id = gid;
  portal.far_bb = hr.session.far_bb;
  portal.far_bb.linked_centroid_id = hr.session.snapped_centroid_id;
  portal.far_bb.pose.orientation =
      yaw_billboard_toward(portal.far_bb.pose.position, head, head_valid);
  portal.near_bb.open = true;
  portal.near_bb.pose = near_target;
  portal.near_bb.linked_centroid_id = hr.session.snapped_centroid_id;
  apply_near_extents(portal.near_bb);
  portal.persistent_centroid_id = hr.session.snapped_centroid_id;
  portal.temporary_centroid_id =
      centroids.register_temporary("voodoo_temp", near_bb_bottom_center(portal.near_bb));
  portal.bump_from = near_target.position;
  portal.bump_to = near_target.position;
  portal.bump_t = 1.f;

  rebuild_ghosts(portal, centroids, workspace);
  portals_.push_back(std::move(portal));
  // Seat on the front↔behind ellipse, then bump/merge so solos that land close join.
  {
    const Vec3 body{head_valid ? head.position.x : 0.f, 0.f, head_valid ? head.position.z : 0.f};
    const Vec3 fwd = flat_forward(head, head_valid);
    const Vec3 right = Vec3{fwd.z, 0.f, -fwd.x};
    auto& p = portals_.back();
    const float az = azimuth_on_orbit(body, fwd, right, p.near_bb.pose.position);
    p.near_bb.pose.position =
        pos_on_orbit_ellipse(body, fwd, right, az, p.near_bb.pose.position.y);
    p.near_bb.pose.orientation =
        yaw_billboard_toward(p.near_bb.pose.position, head, head_valid);
    p.bump_from = p.near_bb.pose.position;
    p.bump_to = p.near_bb.pose.position;
    p.bump_t = 1.f;
    refresh_ghost_near_poses(p);
    if (p.temporary_centroid_id != 0) {
      centroids.set_temporary_position(p.temporary_centroid_id, near_bb_bottom_center(p.near_bb));
    }
  }
  if (portals_.size() >= 2 && !bump_resolve(head, head_valid, portals_.size() - 1)) {
    // Group cannot clear (arc filled toward a circle) — roll back.
    const EntityId temp = portals_.back().temporary_centroid_id;
    portals_.pop_back();
    if (temp != 0) {
      centroids.clear_temporary(temp);
    }
    clear_hand_session(hand);
    return;
  }
  // Final ghost + temp-centroid sync after bump (pinned portals skip tick lerp).
  for (auto& p : portals_) {
    refresh_ghost_near_poses(p);
    if (p.temporary_centroid_id != 0) {
      centroids.set_temporary_position(p.temporary_centroid_id, near_bb_bottom_center(p.near_bb));
    }
  }

  // Hand session mirrors latest portal for ABI get_session compat.
  hr.session.phase = VoodooSessionPhase::Parked;
  hr.session.arrow = {};
  hr.session.snap_nodes.clear();
  hr.have_start_hand_ = false;
  hr.deadzone_armed_ = false;
  hr.deadzone_left_once_ = false;
  hr.drag_hold_s_ = 0.f;
  hr.session.near_bb = portals_.back().near_bb;
  hr.session.far_bb = portals_.back().far_bb;
  hr.session.far_bb.linked_centroid_id = hr.session.snapped_centroid_id;
}

void VoodooVolumeEngine::rebuild_snap_nodes(uint32_t hand, const CentroidRegistry& centroids) {
  auto& hr = hands_[hand];
  hr.session.snap_nodes.clear();
  if (hr.session.mode != VoodooMode::Snap || !hr.have_start_hand_) {
    return;
  }
  const float reach = std::max(params_.arm_reach_m, 1e-3f);
  const float gain_h = params_.field_radius_m / reach;
  const float gain_v = params_.field_height_m / reach;

  Vec3 origin = hr.session.arrow.ray_origin;
  if (!hr.session.arrow.active) {
    origin = hr.start_hand_.position;
  }

  for (const auto& c : centroids.centroids()) {
    if (c.kind == CentroidKind::Temporary) {
      continue;
    }
    VoodooSnapNode n;
    n.centroid_id = c.id;
    n.kind = c.kind;
    n.field_position = c.position;
    n.hand_offset = Vec3{(c.position.x - hr.rest_far_.position.x) / gain_h,
                         (c.position.y - hr.rest_far_.position.y) / gain_v,
                         (c.position.z - hr.rest_far_.position.z) / gain_h};
    n.world_position = Vec3{origin.x + n.hand_offset.x, origin.y + n.hand_offset.y,
                            origin.z + n.hand_offset.z};
    n.radius_m = kSnapNodeRadiusM;
    n.snapped = (hr.session.snapped_centroid_id == c.id);
    hr.session.snap_nodes.push_back(n);
  }
}

void VoodooVolumeEngine::update_far_while_dragging(uint32_t hand, const Pose& hand_pose,
                                                   const Pose& head, bool head_valid,
                                                   float dt_seconds,
                                                   const CentroidRegistry& centroids,
                                                   const Workspace& workspace) {
  auto& hr = hands_[hand];
  if (!hr.session.far_bb.open || !hr.have_start_hand_) {
    return;
  }

  const float reach = std::max(params_.arm_reach_m, 1e-3f);
  const float gain_h = params_.field_radius_m / reach;
  const float gain_v = params_.field_height_m / reach;
  Vec3 stick{hand_pose.position.x - hr.start_hand_.position.x,
             hand_pose.position.y - hr.start_hand_.position.y,
             hand_pose.position.z - hr.start_hand_.position.z};

  EntityId snapped = 0;
  if (hr.session.mode == VoodooMode::Snap) {
    const Vec3 flat = flat_forward(head, head_valid);
    const Vec3 origin{hr.start_hand_.position.x + flat.x * kStickVizForwardOffsetM,
                      hr.start_hand_.position.y + kStickVizUpOffsetM,
                      hr.start_hand_.position.z + flat.z * kStickVizForwardOffsetM};
    hr.session.arrow.ray_origin = origin;

    rebuild_snap_nodes(hand, centroids);
    for (auto& n : hr.session.snap_nodes) {
      n.world_position = Vec3{origin.x + n.hand_offset.x, origin.y + n.hand_offset.y,
                              origin.z + n.hand_offset.z};
    }

    float best_d = 1e9f;
    int best_i = -1;
    for (int i = 0; i < static_cast<int>(hr.session.snap_nodes.size()); ++i) {
      const float d = dist3(stick, hr.session.snap_nodes[static_cast<size_t>(i)].hand_offset);
      if (d < best_d) {
        best_d = d;
        best_i = i;
      }
    }
    const float pull_r = kSnapNodeRadiusM * kSnapPullMul;
    if (best_i >= 0 && best_d < pull_r) {
      auto& n = hr.session.snap_nodes[static_cast<size_t>(best_i)];
      if (best_d <= kSnapNodeRadiusM) {
        stick = n.hand_offset;
        snapped = n.centroid_id;
        n.snapped = true;
      } else {
        const float t = 1.f - (best_d - kSnapNodeRadiusM) / (pull_r - kSnapNodeRadiusM);
        const float u = clampf(t * t, 0.f, 1.f);
        stick.x += (n.hand_offset.x - stick.x) * u;
        stick.y += (n.hand_offset.y - stick.y) * u;
        stick.z += (n.hand_offset.z - stick.z) * u;
      }
    }
  } else {
    hr.session.snap_nodes.clear();
  }
  hr.session.snapped_centroid_id = snapped;
  hr.session.far_bb.linked_centroid_id = snapped;

  Vec3 target{};
  Vec3 target_ext = far_start_extents_m();
  if (snapped != 0) {
    if (const auto* c = centroids.find(snapped)) {
      target = c->position;
    } else {
      target = hr.rest_far_.position;
    }
    Vec3 mn{};
    Vec3 mx{};
    if (centroids.members_aabb(workspace, snapped, &mn, &mx)) {
      target_ext = fit_extents_for_aabb(mn, mx);
      target = Vec3{0.5f * (mn.x + mx.x), 0.5f * (mn.y + mx.y), 0.5f * (mn.z + mx.z)};
    }
  } else {
    target = Vec3{hr.rest_far_.position.x + stick.x * gain_h,
                  hr.rest_far_.position.y + stick.y * gain_v,
                  hr.rest_far_.position.z + stick.z * gain_h};
    const float ox = head_valid ? head.position.x : 0.f;
    const float oz = head_valid ? head.position.z : 0.f;
    const float navel_y =
        head_valid ? (head.position.y - kNavelBelowHeadM) : hr.rest_far_.position.y;
    float dx = target.x - ox;
    float dz = target.z - oz;
    const float horiz = std::sqrt(dx * dx + dz * dz);
    if (horiz > params_.field_radius_m && horiz > 1e-6f) {
      const float s = params_.field_radius_m / horiz;
      target.x = ox + dx * s;
      target.z = oz + dz * s;
    }
    target.y =
        clampf(target.y, navel_y - params_.field_height_m, navel_y + params_.field_height_m);
  }

  const float dt = clampf(dt_seconds, 1e-4f, 0.1f);
  if (!hr.have_far_smoothed_) {
    hr.far_smoothed_ = target;
    hr.extents_smoothed_ = target_ext;
    hr.have_far_smoothed_ = true;
  } else {
    const float tau = snapped != 0 ? 0.02f : kFarDampTauS;
    const float alpha = 1.f - std::exp(-dt / tau);
    hr.far_smoothed_.x += (target.x - hr.far_smoothed_.x) * alpha;
    hr.far_smoothed_.y += (target.y - hr.far_smoothed_.y) * alpha;
    hr.far_smoothed_.z += (target.z - hr.far_smoothed_.z) * alpha;
    const float ea = 1.f - std::exp(-dt * kExtentsLerpPerSec);
    hr.extents_smoothed_.x += (target_ext.x - hr.extents_smoothed_.x) * ea;
    hr.extents_smoothed_.y += (target_ext.y - hr.extents_smoothed_.y) * ea;
    hr.extents_smoothed_.z += (target_ext.z - hr.extents_smoothed_.z) * ea;
  }

  hr.session.far_bb.pose.position = hr.far_smoothed_;
  hr.session.far_bb.pose.orientation =
      yaw_billboard_toward(hr.far_smoothed_, head, head_valid);
  hr.session.far_bb.extents = hr.extents_smoothed_;
  hr.session.far_bb.aspect_x = params_.near_bb_voxels_x;
  hr.session.far_bb.aspect_y = params_.near_bb_voxels_y;
  hr.session.far_bb.aspect_z = params_.near_bb_voxels_z;

  update_arrow(hand, hand_pose, head, head_valid, &stick);
}

void VoodooVolumeEngine::update_arrow(uint32_t hand, const Pose& hand_pose, const Pose& head,
                                      bool head_valid, const Vec3* stick_override) {
  auto& hr = hands_[hand];
  if (!hr.session.open || hr.session.phase != VoodooSessionPhase::Dragging ||
      !hr.have_start_hand_) {
    hr.session.arrow = {};
    return;
  }
  Vec3 stick = stick_override != nullptr
                   ? *stick_override
                   : Vec3{hand_pose.position.x - hr.start_hand_.position.x,
                          hand_pose.position.y - hr.start_hand_.position.y,
                          hand_pose.position.z - hr.start_hand_.position.z};
  const float mag = std::sqrt(stick.x * stick.x + stick.y * stick.y + stick.z * stick.z);
  const Vec3 flat = flat_forward(head, head_valid);
  const Vec3 origin{hr.start_hand_.position.x + flat.x * kStickVizForwardOffsetM,
                    hr.start_hand_.position.y + kStickVizUpOffsetM,
                    hr.start_hand_.position.z + flat.z * kStickVizForwardOffsetM};

  if (!hr.deadzone_armed_) {
    if (mag >= deadzone_m_) {
      hr.deadzone_left_once_ = true;
    }
    if (hr.deadzone_left_once_ && mag < deadzone_m_) {
      hr.deadzone_armed_ = true;
    }
  }

  hr.session.arrow.active = true;
  hr.session.arrow.deadzone_armed = hr.deadzone_armed_;
  hr.session.arrow.in_deadzone = hr.deadzone_armed_ && mag < deadzone_m_;
  hr.session.arrow.magnitude = mag;
  hr.session.arrow.stick = stick;
  hr.session.arrow.ray_origin = origin;
  hr.session.arrow.ray_tip = Vec3{origin.x + stick.x, origin.y + stick.y, origin.z + stick.z};

  for (auto& n : hr.session.snap_nodes) {
    n.world_position =
        Vec3{origin.x + n.hand_offset.x, origin.y + n.hand_offset.y, origin.z + n.hand_offset.z};
    n.snapped = (hr.session.snapped_centroid_id == n.centroid_id);
  }
}

void VoodooVolumeEngine::tick_hand(uint32_t hand, const InteractionIntents& intents,
                                   const Pose& hand_pose, bool hand_valid, const Pose& head,
                                   bool head_valid, float dt_seconds,
                                   CentroidRegistry& centroids, const Workspace& workspace) {
  if (hand >= 2u) {
    return;
  }
  auto& hr = hands_[hand];
  const bool want_snap = has_flag(intents.flags, InteractionIntentFlags::VoodooSnap);
  const bool want_free = has_flag(intents.flags, InteractionIntentFlags::VoodooFree);
  const bool want_any = want_snap || want_free;

  if (hr.session.phase == VoodooSessionPhase::Parked) {
    if (want_any && hand_valid) {
      // Keep portals; start a new drag on this hand (refused if inside a near BB).
      clear_hand_session(hand);
      const VoodooMode mode = want_snap ? VoodooMode::Snap : VoodooMode::FreeRange;
      open_drag(hand, mode, hand_pose, head, head_valid);
      if (hr.session.open) {
        hr.drag_hold_s_ += std::max(0.f, dt_seconds);
        update_far_while_dragging(hand, hand_pose, head, head_valid, dt_seconds, centroids,
                                  workspace);
      }
    }
    return;
  }

  if (hr.session.phase != VoodooSessionPhase::Dragging) {
    if (!want_any || !hand_valid) {
      return;
    }
    const VoodooMode mode = want_snap ? VoodooMode::Snap : VoodooMode::FreeRange;
    open_drag(hand, mode, hand_pose, head, head_valid);
    if (hr.session.open) {
      hr.drag_hold_s_ += std::max(0.f, dt_seconds);
      update_far_while_dragging(hand, hand_pose, head, head_valid, dt_seconds, centroids,
                                workspace);
    }
    return;
  }

  if (!hand_valid) {
    cancel_drag(hand);
    return;
  }

  const bool hold_ok =
      (hr.session.mode == VoodooMode::Snap && want_snap) ||
      (hr.session.mode == VoodooMode::FreeRange && want_free);
  if (!hold_ok) {
    // Accidental taps: hold < 0.25 s cancels far+near for this gesture.
    if (hr.drag_hold_s_ < kMinHoldToParkS) {
      cancel_drag(hand);
    } else {
      park(hand, hand_pose, hand_valid, head, head_valid, centroids, workspace);
    }
    return;
  }

  hr.drag_hold_s_ += std::max(0.f, dt_seconds);
  update_far_while_dragging(hand, hand_pose, head, head_valid, dt_seconds, centroids, workspace);
}

}  // namespace threeos
