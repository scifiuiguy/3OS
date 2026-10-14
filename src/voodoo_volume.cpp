#include "core/voodoo_volume.hpp"

#include <algorithm>
#include <functional>
#include <string>

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

float VoodooVolumeEngine::portal_near_footprint_radius(const VoodooBoundingBox& near_bb) {
  const Vec3& e = near_bb.extents;
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
  cut_portal_id_ = 0;
  clear_hand_session(0);
  clear_hand_session(1);
  ghost_drags_[0] = {};
  ghost_drags_[1] = {};
  pose_lerps_.clear();
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

void VoodooVolumeEngine::cancel_open_session(uint32_t hand) {
  if (hand >= 2u) {
    return;
  }
  if (hands_[hand].session.phase == VoodooSessionPhase::Dragging) {
    cancel_drag(hand);
  }
}

bool VoodooVolumeEngine::dismiss_portal(uint64_t portal_id) {
  const auto it = std::remove_if(portals_.begin(), portals_.end(),
                                 [portal_id](const VoodooPortal& p) { return p.id == portal_id; });
  if (it == portals_.end()) {
    return false;
  }
  // Drop any ghost/audition drag that was targeting this portal.
  for (uint32_t h = 0; h < 2u; ++h) {
    if (ghost_drags_[h].active && ghost_drags_[h].portal_id == portal_id) {
      ghost_drags_[h] = {};
    }
  }
  if (cut_portal_id_ == portal_id) {
    cut_portal_id_ = 0;
  }
  portals_.erase(it, portals_.end());
  return true;
}

bool VoodooVolumeEngine::cut_arm(uint64_t portal_id) {
  if (portal_id == 0) {
    return false;
  }
  for (const auto& p : portals_) {
    if (p.id == portal_id && p.near_bb.open) {
      cut_portal_id_ = portal_id;
      return true;
    }
  }
  return false;
}

void VoodooVolumeEngine::cut_cancel() {
  cut_portal_id_ = 0;
}

std::vector<EntityId> VoodooVolumeEngine::cut_entity_ids() const {
  std::vector<EntityId> ids;
  if (cut_portal_id_ == 0) {
    return ids;
  }
  for (const auto& p : portals_) {
    if (p.id != cut_portal_id_) {
      continue;
    }
    for (const auto& g : p.ghosts) {
      if (!g.is_parent_box && g.entity_id != 0) {
        ids.push_back(g.entity_id);
      }
    }
    break;
  }
  return ids;
}

bool VoodooVolumeEngine::orbit_near_bbs_clear_of_paste(const Vec3& paste_c, float paste_r) {
  const Pose& head = last_head_;
  const bool head_valid = last_head_valid_;
  const Vec3 body{head_valid ? head.position.x : 0.f, 0.f, head_valid ? head.position.z : 0.f};
  const Vec3 fwd = flat_forward(head, head_valid);
  const Vec3 right{fwd.z, 0.f, -fwd.x};

  auto eff_pos = [](const VoodooPortal& p) {
    return (p.bump_t < 1.f) ? p.bump_to : p.near_bb.pose.position;
  };
  auto set_orbit_target = [&](VoodooPortal& p, const Vec3& to) {
    p.bump_from = p.near_bb.pose.position;
    p.bump_to = to;
    if (dist3(p.bump_from, to) < 1e-4f) {
      p.near_bb.pose.position = to;
      p.bump_t = 1.f;
    } else {
      p.bump_t = 0.f;
    }
    p.near_bb.pose.orientation = yaw_billboard_toward(to, head, head_valid);
    refresh_ghost_near_poses(p);
  };
  auto paste_need = [&](const VoodooPortal& p) {
    return paste_r + portal_near_footprint_radius(p.near_bb) + kNearClearGapM;
  };
  auto clear_of_paste = [&](const Vec3& pos, const VoodooPortal& p) {
    const float need = paste_need(p);
    const float dx = pos.x - paste_c.x;
    const float dz = pos.z - paste_c.z;
    return std::sqrt(dx * dx + dz * dz) + 1e-4f >= need;
  };
  auto pair_need = [](const VoodooPortal& a, const VoodooPortal& b) {
    // Match near_bbs_overlap_xz / near_targets_clear (half-diagonals + gap).
    const auto& ea = a.near_bb.extents;
    const auto& eb = b.near_bb.extents;
    const float ra = 0.5f * std::sqrt(ea.x * ea.x + ea.z * ea.z);
    const float rb = 0.5f * std::sqrt(eb.x * eb.x + eb.z * eb.z);
    return ra + rb + kNearClearGapM;
  };

  // Walk `az0` along ±orbit until `pred(cand)` is true. Returns false if no slot.
  auto walk_orbit = [&](float az0, float y, const std::function<bool(const Vec3&)>& pred,
                        Vec3* out) {
    constexpr float kStep = 0.06f;
    for (int dir = 0; dir < 2; ++dir) {
      const float sign = dir == 0 ? 1.f : -1.f;
      for (int step = 1; step <= 48; ++step) {
        const float az = wrap_pi(az0 + sign * kStep * static_cast<float>(step));
        const Vec3 cand = pos_on_orbit_ellipse(body, fwd, right, az, y);
        if (pred(cand)) {
          *out = cand;
          return true;
        }
      }
    }
    return false;
  };

  // Pass 1: orbit every near BB that intersects the paste footprint.
  for (auto& p : portals_) {
    if (!p.near_bb.open) {
      continue;
    }
    const Vec3 from = eff_pos(p);
    if (clear_of_paste(from, p)) {
      continue;
    }
    const float az0 = azimuth_on_orbit(body, fwd, right, from);
    Vec3 target = from;
    if (!walk_orbit(az0, from.y, [&](const Vec3& cand) { return clear_of_paste(cand, p); },
                    &target)) {
      return false;
    }
    set_orbit_target(p, target);
  }

  // Pass 2+: cascade sibling overlaps. A paste-bumped BB that lands on a neighbor must
  // push that neighbor onward (and so on), without anyone walking back into the paste.
  for (int pass = 0; pass < 64; ++pass) {
    bool moved = false;
    for (size_t i = 0; i < portals_.size(); ++i) {
      if (!portals_[i].near_bb.open) {
        continue;
      }
      for (size_t j = i + 1; j < portals_.size(); ++j) {
        if (!portals_[j].near_bb.open) {
          continue;
        }
        auto& a = portals_[i];
        auto& b = portals_[j];
        Vec3 pa = eff_pos(a);
        Vec3 pb = eff_pos(b);
        const float need = pair_need(a, b);
        float dx = pb.x - pa.x;
        float dz = pb.z - pa.z;
        float dist = std::sqrt(dx * dx + dz * dz);
        if (dist + 1e-4f >= need) {
          continue;
        }

        float az_a = azimuth_on_orbit(body, fwd, right, pa);
        float az_b = azimuth_on_orbit(body, fwd, right, pb);
        const float daz = shortest_daz(az_a, az_b);
        const float sign = daz >= 0.f ? 1.f : -1.f;
        constexpr float kSep = 0.025f;
        bool separated = false;
        for (int step = 0; step < 80; ++step) {
          // Prefer moving B (later seed) when A was the paste bumper; both may move.
          az_b = wrap_pi(az_b + sign * kSep);
          az_a = wrap_pi(az_a - sign * kSep * 0.35f);
          pa = pos_on_orbit_ellipse(body, fwd, right, az_a, pa.y);
          pb = pos_on_orbit_ellipse(body, fwd, right, az_b, pb.y);
          // Never re-enter the paste footprint while separating.
          if (!clear_of_paste(pa, a) || !clear_of_paste(pb, b)) {
            continue;
          }
          dx = pb.x - pa.x;
          dz = pb.z - pa.z;
          if (std::sqrt(dx * dx + dz * dz) + 1e-4f >= need) {
            separated = true;
            break;
          }
        }
        if (!separated) {
          // One-sided yield: park A, push B clear of both A and paste.
          pa = eff_pos(a);
          pb = eff_pos(b);
          az_b = azimuth_on_orbit(body, fwd, right, pb);
          Vec3 target = pb;
          const bool ok = walk_orbit(
              az_b, pb.y,
              [&](const Vec3& cand) {
                if (!clear_of_paste(cand, b)) {
                  return false;
                }
                const float cdx = cand.x - pa.x;
                const float cdz = cand.z - pa.z;
                return std::sqrt(cdx * cdx + cdz * cdz) + 1e-4f >= need;
              },
              &target);
          if (!ok) {
            return false;
          }
          set_orbit_target(b, target);
          moved = true;
          continue;
        }
        set_orbit_target(a, pa);
        set_orbit_target(b, pb);
        moved = true;
      }
    }

    // Re-clear anyone who drifted into paste during sibling pushes.
    for (auto& p : portals_) {
      if (!p.near_bb.open) {
        continue;
      }
      const Vec3 from = eff_pos(p);
      if (clear_of_paste(from, p)) {
        continue;
      }
      const float az0 = azimuth_on_orbit(body, fwd, right, from);
      Vec3 target = from;
      if (!walk_orbit(az0, from.y, [&](const Vec3& cand) { return clear_of_paste(cand, p); },
                      &target)) {
        return false;
      }
      set_orbit_target(p, target);
      moved = true;
    }

    if (!moved) {
      break;
    }
  }

  // Final world check: paste clear + sibling targets clear.
  for (const auto& p : portals_) {
    if (p.near_bb.open && !clear_of_paste(eff_pos(p), p)) {
      return false;
    }
  }
  return near_targets_clear();
}

Vec3 VoodooVolumeEngine::resolve_cut_paste_origin(const Vec3& pinch_world, float cluster_radius_m) {
  const Pose& head = last_head_;
  const bool head_valid = last_head_valid_;
  const Vec3 body{head_valid ? head.position.x : 0.f, 0.f, head_valid ? head.position.z : 0.f};
  const Vec3 fwd = flat_forward(head, head_valid);
  const Vec3 right{fwd.z, 0.f, -fwd.x};
  const float paste_r = std::max(cluster_radius_m, kGlyphHalfM) + kNearClearGapM;

  auto footprint_clear = [&](const Vec3& c) {
    for (const auto& p : portals_) {
      if (!p.near_bb.open) {
        continue;
      }
      const Vec3 pc = (p.bump_t < 1.f) ? p.bump_to : p.near_bb.pose.position;
      const float need = paste_r + portal_near_footprint_radius(p.near_bb);
      const float dx = pc.x - c.x;
      const float dz = pc.z - c.z;
      if (std::sqrt(dx * dx + dz * dz) + 1e-4f < need) {
        return false;
      }
    }
    return true;
  };

  Vec3 dest = pinch_world;
  // Prefer orbiting near BBs clear of the pinch; only nudge paste origin if the ring is full.
  if (!orbit_near_bbs_clear_of_paste(dest, paste_r) || !footprint_clear(dest)) {
    const float az0 = azimuth_on_orbit(body, fwd, right, pinch_world);
    const float y = pinch_world.y;
    constexpr float kStep = 0.1f;
    bool nudged = false;
    for (int dir = 0; dir < 2 && !nudged; ++dir) {
      const float sign = dir == 0 ? 1.f : -1.f;
      for (int step = 1; step <= 32; ++step) {
        const float az = wrap_pi(az0 + sign * kStep * static_cast<float>(step));
        const Vec3 cand = pos_on_orbit_ellipse(body, fwd, right, az, y);
        if (footprint_clear(cand)) {
          dest = cand;
          nudged = true;
          break;
        }
      }
    }
    if (nudged) {
      // After nudging paste origin, orbit any leftover overlaps (should be rare).
      orbit_near_bbs_clear_of_paste(dest, paste_r);
    }
  }
  return dest;
}

bool VoodooVolumeEngine::cut_paste_at(const Vec3& pinch_world, Workspace& workspace) {
  if (cut_portal_id_ == 0) {
    return false;
  }
  VoodooPortal* home = nullptr;
  for (auto& p : portals_) {
    if (p.id == cut_portal_id_) {
      home = &p;
      break;
    }
  }
  if (home == nullptr) {
    cut_portal_id_ = 0;
    return false;
  }
  // Pinch inside source near BB is cancel (host); refuse paste here.
  if (point_in_obb(pinch_world, home->near_bb.pose, home->near_bb.extents)) {
    return false;
  }

  struct CutItem {
    EntityId id = 0;
    Pose far_pose{};
    Pose near_pose{};
    float scale = 1.f;
  };
  std::vector<CutItem> items;
  items.reserve(home->ghosts.size());
  for (const auto& g : home->ghosts) {
    if (!g.is_parent_box && g.entity_id != 0) {
      items.push_back(CutItem{g.entity_id, g.far_pose, g.near_pose, g.scale});
    }
  }
  if (items.empty()) {
    cut_portal_id_ = 0;
    return false;
  }

  const uint64_t src = cut_portal_id_;
  cut_portal_id_ = 0;

  const float n = static_cast<float>(items.size());
  const uint64_t pinch_portal = portal_at_near_point(pinch_world);
  VoodooPortal* target = nullptr;
  if (pinch_portal != 0 && pinch_portal != src) {
    for (auto& p : portals_) {
      if (p.id == pinch_portal) {
        target = &p;
        break;
      }
    }
  }

  bool any = false;
  if (target != nullptr) {
    // Paste into another near BB: cluster relative to near-pose centroid, billboarded to target.
    Vec3 near_centroid{};
    for (const auto& it : items) {
      near_centroid.x += it.near_pose.position.x;
      near_centroid.y += it.near_pose.position.y;
      near_centroid.z += it.near_pose.position.z;
    }
    near_centroid.x /= n;
    near_centroid.y /= n;
    near_centroid.z /= n;
    const Quat src_yaw = home->near_bb.pose.orientation;
    const Quat dest_yaw = target->near_bb.pose.orientation;
    const Vec3 dest_centroid = pinch_world;

    for (const auto& it : items) {
      const Vec3 d{it.near_pose.position.x - near_centroid.x,
                   it.near_pose.position.y - near_centroid.y,
                   it.near_pose.position.z - near_centroid.z};
      const Vec3 local = quat_rotate_yaw(quat_inverse_yaw(src_yaw), d);
      const Vec3 world_off = quat_rotate_yaw(dest_yaw, local);
      Pose near_pose{};
      near_pose.position = Vec3{dest_centroid.x + world_off.x, dest_centroid.y + world_off.y,
                                dest_centroid.z + world_off.z};
      near_pose.orientation = dest_yaw;
      near_pose.position = clamp_point_to_near_obb(*target, near_pose.position);

      const float near_w = std::max(target->near_bb.extents.x, 1e-4f);
      const float far_w = std::max(target->far_bb.extents.x, 1e-4f);
      const float scale = clampf(near_w / far_w, 0.05f, 1.f);
      const Pose far_pose = map_near_to_far_pose(*target, near_pose, scale);

      const EntityId eid = it.id;
      home->ghosts.erase(std::remove_if(home->ghosts.begin(), home->ghosts.end(),
                                        [eid](const VoodooGhost& g) {
                                          return g.entity_id == eid && !g.is_parent_box;
                                        }),
                         home->ghosts.end());
      remove_ghost_from_other_portals(target->id, eid);

      VoodooGhost ng;
      ng.entity_id = it.id;
      ng.is_parent_box = false;
      ng.scale = scale;
      ng.far_pose = far_pose;
      ng.near_pose = near_pose;
      target->ghosts.push_back(ng);
      begin_pose_lerp(it.id, far_pose, workspace);
      any = true;
    }
  } else {
    // Open-air: paste the far-pose centroid cluster, billboarded toward the user.
    Vec3 far_centroid{};
    for (const auto& it : items) {
      far_centroid.x += it.far_pose.position.x;
      far_centroid.y += it.far_pose.position.y;
      far_centroid.z += it.far_pose.position.z;
    }
    far_centroid.x /= n;
    far_centroid.y /= n;
    far_centroid.z /= n;
    const Quat src_yaw = home->far_bb.pose.orientation;

    float cluster_r = kGlyphHalfM;
    std::vector<Vec3> local_offs;
    local_offs.reserve(items.size());
    for (const auto& it : items) {
      const Vec3 d{it.far_pose.position.x - far_centroid.x, it.far_pose.position.y - far_centroid.y,
                   it.far_pose.position.z - far_centroid.z};
      const Vec3 local = quat_rotate_yaw(quat_inverse_yaw(src_yaw), d);
      local_offs.push_back(local);
      const float horiz = std::sqrt(local.x * local.x + local.z * local.z);
      cluster_r = std::max(cluster_r, horiz + kGlyphHalfM);
    }

    const Vec3 dest_centroid = resolve_cut_paste_origin(pinch_world, cluster_r);
    const Quat dest_yaw = yaw_billboard_toward(dest_centroid, last_head_, last_head_valid_);

    for (size_t i = 0; i < items.size(); ++i) {
      const auto& it = items[i];
      const Vec3 world_off = quat_rotate_yaw(dest_yaw, local_offs[i]);
      Pose dest{};
      dest.position = Vec3{dest_centroid.x + world_off.x, dest_centroid.y + world_off.y,
                           dest_centroid.z + world_off.z};
      // Re-yaw entity orientation by the same billboard delta as the cluster.
      const float src_y = src_yaw.y;
      const float src_w = src_yaw.w;
      const float dst_y = dest_yaw.y;
      const float dst_w = dest_yaw.w;
      // delta = dest * inverse(src) for yaw-only quats.
      const float inv_y = -src_y;
      const float inv_w = src_w;
      const float dy = dst_w * inv_y + dst_y * inv_w;
      const float dw = dst_w * inv_w - dst_y * inv_y;
      const float ey = it.far_pose.orientation.y;
      const float ew = it.far_pose.orientation.w;
      dest.orientation = Quat{0.f, dw * ey + dy * ew, 0.f, dw * ew - dy * ey};

      const EntityId eid = it.id;
      home->ghosts.erase(std::remove_if(home->ghosts.begin(), home->ghosts.end(),
                                        [eid](const VoodooGhost& g) {
                                          return g.entity_id == eid && !g.is_parent_box;
                                        }),
                         home->ghosts.end());
      remove_ghost_from_other_portals(0, eid);
      begin_pose_lerp(eid, dest, workspace);
      any = true;
    }

    // Bump near-field clutter (non-BB objects) out of the pasted cluster footprint.
    VoodooPortal synth{};
    synth.near_bb.open = true;
    synth.near_bb.pose.position = dest_centroid;
    synth.near_bb.pose.orientation = dest_yaw;
    const float ext = std::max(cluster_r * 2.f, near_extents_m().x);
    synth.near_bb.extents = Vec3{ext, near_extents_m().y, ext};
    bump_near_field_clutter(synth, workspace, last_head_, last_head_valid_, /*animate=*/true);
  }

  // Ensure content ghosts are gone from source even if a path early-out.
  for (auto& p : portals_) {
    if (p.id != src) {
      continue;
    }
    p.ghosts.erase(std::remove_if(p.ghosts.begin(), p.ghosts.end(),
                                  [](const VoodooGhost& g) { return !g.is_parent_box; }),
                   p.ghosts.end());
    // Intentionally no far/near refit — leave empty shell at park size.
    break;
  }
  return any;
}

bool VoodooVolumeEngine::cut_paste_into(EntityId box_id, Workspace& workspace) {
  if (cut_portal_id_ == 0 || box_id == 0) {
    return false;
  }
  VoodooPortal* home = nullptr;
  for (auto& p : portals_) {
    if (p.id == cut_portal_id_) {
      home = &p;
      break;
    }
  }
  if (home == nullptr) {
    cut_portal_id_ = 0;
    return false;
  }
  // Refuse paste into any ghost belonging to the cut portal (content or parent).
  for (const auto& g : home->ghosts) {
    if (g.entity_id == box_id) {
      return false;
    }
  }

  const auto ids = cut_entity_ids();
  if (ids.empty()) {
    cut_portal_id_ = 0;
    return false;
  }

  const uint64_t src = cut_portal_id_;
  cut_portal_id_ = 0;
  bool any = false;
  for (EntityId id : ids) {
    if (id == box_id) {
      continue;
    }
    std::string err;
    if (workspace.insert_into(id, box_id, &err)) {
      any = true;
      remove_content_ghost(id);
      // Also drop pose lerps for inserted entity.
      pose_lerps_.erase(std::remove_if(pose_lerps_.begin(), pose_lerps_.end(),
                                       [id](const PoseLerpState& l) { return l.entity_id == id; }),
                        pose_lerps_.end());
    }
  }
  for (auto& p : portals_) {
    if (p.id != src) {
      continue;
    }
    p.ghosts.erase(std::remove_if(p.ghosts.begin(), p.ghosts.end(),
                                  [](const VoodooGhost& g) { return !g.is_parent_box; }),
                   p.ghosts.end());
    break;
  }
  return any;
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
  // Live parked portals own near-BB hit testing. Do NOT also test hand-session
  // near_bb after Parked: park copies a snapshot that never tracks bump-lerp /
  // dismiss, which left phantom blue-cursor volumes at the old pose.
  for (const auto& portal : portals_) {
    if (portal.near_bb.open && point_in_obb(p, portal.near_bb.pose, portal.near_bb.extents)) {
      return true;
    }
  }
  for (uint32_t h = 0; h < 2u; ++h) {
    const auto& s = hands_[h].session;
    // Dragging only (near opens on release today; keep the gate for future live near).
    if (s.phase != VoodooSessionPhase::Dragging || !s.near_bb.open) {
      continue;
    }
    if (point_in_obb(p, s.near_bb.pose, s.near_bb.extents)) {
      return true;
    }
  }
  return false;
}

uint64_t VoodooVolumeEngine::portal_at_near_point(const Vec3& p) const {
  for (const auto& portal : portals_) {
    if (portal.near_bb.open && point_in_obb(p, portal.near_bb.pose, portal.near_bb.extents)) {
      return portal.id;
    }
  }
  return 0;
}

uint64_t VoodooVolumeEngine::portal_for_ghost_entity(EntityId entity_id) const {
  const VoodooPortal* portal = nullptr;
  const VoodooGhost* ghost = nullptr;
  if (!find_ghost(entity_id, &portal, &ghost) || portal == nullptr) {
    return 0;
  }
  return portal->id;
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
  if (portal.lock_scale < 1e-4f) {
    portal.lock_scale = clampf(near_w / far_w, 0.05f, 1.f);
  }
  const float scale = portal.lock_scale;

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
  // Use the largest near-BB footprint in the active group (near can grow with far).
  float foot = near_footprint_radius_m();
  for (const auto& p : portals_) {
    foot = std::max(foot, portal_near_footprint_radius(p.near_bb));
  }
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
      s.pinned = (i == pinned_index);
      // Prefer settled/target pose — never sample a mid-chord lerp for azimuth.
      const Vec3 src = (portals_[i].bump_t < 1.f) ? portals_[i].bump_to
                                                   : portals_[i].near_bb.pose.position;
      s.az = azimuth_on_orbit(body, fwd, right, src);
      s.y = src.y;
      if (s.pinned) {
        // Bumper stays put: never snap the pinned near BB onto the ellipse.
        portals_[i].bump_from = portals_[i].near_bb.pose.position;
        portals_[i].bump_to = portals_[i].near_bb.pose.position;
        portals_[i].bump_t = 1.f;
      } else {
        const Vec3 on_arc = pos_on_orbit_ellipse(body, fwd, right, s.az, s.y);
        portals_[i].bump_from = on_arc;
        if (portals_[i].bump_t >= 1.f) {
          portals_[i].near_bb.pose.position = on_arc;
        }
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
      if (s.pinned) {
        // Bumper (e.g. near BB being resized): never translate or re-yaw.
        portal.bump_to = portal.near_bb.pose.position;
        portal.bump_t = 1.f;
        continue;
      }
      const Vec3 to = pos_on_orbit_ellipse(body, fwd, right, s.az, s.y);
      portal.bump_to = to;
      const float moved = dist3(portal.bump_from, to);
      if (moved < 1e-4f) {
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
  last_head_ = head;
  last_head_valid_ = head_valid;
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

  // Keep parked hand-session BB snapshots aligned with their live portal (bump / dismiss).
  for (uint32_t h = 0; h < 2u; ++h) {
    auto& hr = hands_[h];
    if (hr.session.phase != VoodooSessionPhase::Parked || hr.parked_portal_id_ == 0) {
      continue;
    }
    bool found = false;
    for (const auto& p : portals_) {
      if (p.id != hr.parked_portal_id_) {
        continue;
      }
      hr.session.near_bb = p.near_bb;
      hr.session.far_bb = p.far_bb;
      found = true;
      break;
    }
    if (!found) {
      // Portal dismissed — drop the stale parked session entirely.
      const uint32_t hand = hr.session.hand;
      hr.session = VoodooSessionState{};
      hr.session.hand = hand;
      hr.parked_portal_id_ = 0;
    }
  }
}

void VoodooVolumeEngine::park(uint32_t hand, const Pose& hand_pose, bool hand_valid,
                              const Pose& head, bool head_valid, CentroidRegistry& centroids,
                              Workspace& workspace) {
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
  {
    const float near_w = std::max(portal.near_bb.extents.x, 1e-4f);
    const float far_w = std::max(portal.far_bb.extents.x, 1e-4f);
    portal.lock_scale = clampf(near_w / far_w, 0.05f, 1.f);
  }
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

  // Task 6.6: non-voodoo near clutter yields to the new portal (orbit bump-lerp).
  bump_near_field_clutter(portals_.back(), workspace, head, head_valid, /*animate=*/true);

  // Hand session mirrors latest portal for ABI get_session compat.
  hr.session.phase = VoodooSessionPhase::Parked;
  hr.session.arrow = {};
  hr.session.snap_nodes.clear();
  hr.have_start_hand_ = false;
  hr.deadzone_armed_ = false;
  hr.deadzone_left_once_ = false;
  hr.drag_hold_s_ = 0.f;
  hr.parked_portal_id_ = portals_.back().id;
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
  const Vec3 origin{hr.start_hand_.position.x - flat.x * kStickVizForwardOffsetM,
                    hr.start_hand_.position.y - flat.y * kStickVizForwardOffsetM +
                        kStickVizUpOffsetM,
                    hr.start_hand_.position.z - flat.z * kStickVizForwardOffsetM};
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
  const Vec3 origin{hr.start_hand_.position.x - flat.x * kStickVizForwardOffsetM,
                    hr.start_hand_.position.y - flat.y * kStickVizForwardOffsetM +
                        kStickVizUpOffsetM,
                    hr.start_hand_.position.z - flat.z * kStickVizForwardOffsetM};

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
                                   CentroidRegistry& centroids, Workspace& workspace) {
  if (hand >= 2u) {
    return;
  }
  last_head_ = head;
  last_head_valid_ = head_valid;
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

bool VoodooVolumeEngine::find_ghost(EntityId entity_id, const VoodooPortal** out_portal,
                                    const VoodooGhost** out_ghost) const {
  if (out_portal != nullptr) {
    *out_portal = nullptr;
  }
  if (out_ghost != nullptr) {
    *out_ghost = nullptr;
  }
  if (entity_id == 0) {
    return false;
  }
  for (const auto& portal : portals_) {
    for (const auto& g : portal.ghosts) {
      if (g.entity_id == entity_id && !g.is_parent_box) {
        if (out_portal != nullptr) {
          *out_portal = &portal;
        }
        if (out_ghost != nullptr) {
          *out_ghost = &g;
        }
        return true;
      }
    }
  }
  return false;
}

bool VoodooVolumeEngine::find_ghost_mut(EntityId entity_id, VoodooPortal** out_portal,
                                        VoodooGhost** out_ghost) {
  if (out_portal != nullptr) {
    *out_portal = nullptr;
  }
  if (out_ghost != nullptr) {
    *out_ghost = nullptr;
  }
  if (entity_id == 0) {
    return false;
  }
  for (auto& portal : portals_) {
    for (auto& g : portal.ghosts) {
      if (g.entity_id == entity_id && !g.is_parent_box) {
        if (out_portal != nullptr) {
          *out_portal = &portal;
        }
        if (out_ghost != nullptr) {
          *out_ghost = &g;
        }
        return true;
      }
    }
  }
  return false;
}

Pose VoodooVolumeEngine::map_far_to_near_pose(const VoodooPortal& portal, const Pose& far_pose,
                                              float scale) const {
  Pose out = far_pose;
  const Vec3 d{far_pose.position.x - portal.far_bb.pose.position.x,
               far_pose.position.y - portal.far_bb.pose.position.y,
               far_pose.position.z - portal.far_bb.pose.position.z};
  const Vec3 local = quat_rotate_yaw(quat_inverse_yaw(portal.far_bb.pose.orientation), d);
  const Vec3 scaled{local.x * scale, local.y * scale, local.z * scale};
  const Vec3 world = quat_rotate_yaw(portal.near_bb.pose.orientation, scaled);
  out.position = Vec3{portal.near_bb.pose.position.x + world.x,
                      portal.near_bb.pose.position.y + world.y,
                      portal.near_bb.pose.position.z + world.z};
  out.orientation = portal.near_bb.pose.orientation;
  return out;
}

Pose VoodooVolumeEngine::map_near_to_far_pose(const VoodooPortal& portal, const Pose& near_pose,
                                              float scale) const {
  Pose out = near_pose;
  const float inv = 1.f / std::max(scale, 1e-4f);
  const Vec3 d{near_pose.position.x - portal.near_bb.pose.position.x,
               near_pose.position.y - portal.near_bb.pose.position.y,
               near_pose.position.z - portal.near_bb.pose.position.z};
  const Vec3 local = quat_rotate_yaw(quat_inverse_yaw(portal.near_bb.pose.orientation), d);
  const Vec3 unscaled{local.x * inv, local.y * inv, local.z * inv};
  const Vec3 world = quat_rotate_yaw(portal.far_bb.pose.orientation, unscaled);
  out.position = Vec3{portal.far_bb.pose.position.x + world.x,
                      portal.far_bb.pose.position.y + world.y,
                      portal.far_bb.pose.position.z + world.z};
  out.orientation = portal.far_bb.pose.orientation;
  return out;
}

Vec3 VoodooVolumeEngine::clamp_point_to_near_obb(const VoodooPortal& portal, const Vec3& p) const {
  const Vec3 d{p.x - portal.near_bb.pose.position.x, p.y - portal.near_bb.pose.position.y,
               p.z - portal.near_bb.pose.position.z};
  Vec3 local = quat_rotate_yaw(quat_inverse_yaw(portal.near_bb.pose.orientation), d);
  const Vec3 half{portal.near_bb.extents.x * 0.5f, portal.near_bb.extents.y * 0.5f,
                  portal.near_bb.extents.z * 0.5f};
  local.x = clampf(local.x, -half.x, half.x);
  local.y = clampf(local.y, -half.y, half.y);
  local.z = clampf(local.z, -half.z, half.z);
  const Vec3 world = quat_rotate_yaw(portal.near_bb.pose.orientation, local);
  return Vec3{portal.near_bb.pose.position.x + world.x, portal.near_bb.pose.position.y + world.y,
              portal.near_bb.pose.position.z + world.z};
}

void VoodooVolumeEngine::update_ghost_drag_arrow(uint32_t hand, const Pose& hand_pose,
                                                 const Pose& head, bool head_valid,
                                                 const Vec3& stick) {
  (void)hand_pose;
  auto& gd = ghost_drags_[hand];
  gd.near_stick_visible = false;
  if (gd.direct_follow) {
    gd.arrow = {};
    gd.far_arrow = {};
    return;
  }

  if (gd.kind == GhostDragKind::Audition) {
    gd.near_stick_visible = gd.audition_inside;
  } else {
    VoodooPortal* portal = nullptr;
    VoodooGhost* ghost = nullptr;
    gd.near_stick_visible =
        find_ghost_mut(gd.entity_id, &portal, &ghost) && portal != nullptr && ghost != nullptr &&
        portal->id == gd.portal_id;
  }

  const float mag = std::sqrt(stick.x * stick.x + stick.y * stick.y + stick.z * stick.z);
  const Vec3 flat = flat_forward(head, head_valid);
  // Match kinematics stick viz: subtract flat_forward so the ray sits just
  // forward of the body (kernel -Z forward under Unity's X-only map) + 4" up.
  const Vec3 hand_origin{gd.start_hand.position.x - flat.x * kStickVizForwardOffsetM,
                         gd.start_hand.position.y - flat.y * kStickVizForwardOffsetM +
                             kStickVizUpOffsetM,
                         gd.start_hand.position.z - flat.z * kStickVizForwardOffsetM};

  const VoodooPortal* portal = nullptr;
  for (const auto& p : portals_) {
    if (p.id == gd.portal_id) {
      portal = &p;
      break;
    }
  }
  const float scale =
      std::max(gd.scale > 1e-4f ? gd.scale : (portal != nullptr ? portal->lock_scale : 1.f), 1e-4f);

  auto fill = [&](VoodooDragArrow& a, const Vec3& o, const Vec3& s, float mag_m) {
    a.active = true;
    a.deadzone_armed = gd.deadzone_armed;
    a.in_deadzone = gd.deadzone_armed && mag < deadzone_m_;
    a.magnitude = mag_m;
    a.stick = s;
    a.ray_origin = o;
    a.ray_tip = Vec3{o.x + s.x, o.y + s.y, o.z + s.z};
  };

  // Outside near BB: yellow only, seated at the hand (same offset as possess stick).
  // Inside near BB: blue at hand + yellow twin mapped into far space.
  if (gd.near_stick_visible) {
    fill(gd.arrow, hand_origin, stick, mag);
    Vec3 far_origin = hand_origin;
    if (portal != nullptr) {
      Pose near_o{};
      near_o.position = hand_origin;
      near_o.orientation = portal->near_bb.pose.orientation;
      far_origin = map_near_to_far_pose(*portal, near_o, scale).position;
    }
    const Vec3 far_stick{stick.x / scale, stick.y / scale, stick.z / scale};
    fill(gd.far_arrow, far_origin, far_stick, mag / scale);
  } else {
    gd.arrow = {};
    fill(gd.far_arrow, hand_origin, stick, mag);
  }
}

void VoodooVolumeEngine::refresh_ghost_drag_arrows(uint32_t hand, const Pose& hand_pose,
                                                   const Pose& head, bool head_valid) {
  if (hand >= 2u) {
    return;
  }
  const auto& gd = ghost_drags_[hand];
  if (!gd.active || !gd.have_start_hand) {
    return;
  }
  const Vec3 stick{hand_pose.position.x - gd.start_hand.position.x,
                   hand_pose.position.y - gd.start_hand.position.y,
                   hand_pose.position.z - gd.start_hand.position.z};
  update_ghost_drag_arrow(hand, hand_pose, head, head_valid, stick);
}

void VoodooVolumeEngine::remove_ghost_from_other_portals(uint64_t keep_portal_id,
                                                        EntityId entity_id) {
  for (auto& portal : portals_) {
    if (portal.id == keep_portal_id) {
      continue;
    }
    portal.ghosts.erase(std::remove_if(portal.ghosts.begin(), portal.ghosts.end(),
                                       [entity_id](const VoodooGhost& g) {
                                         return g.entity_id == entity_id && !g.is_parent_box;
                                       }),
                        portal.ghosts.end());
  }
}

bool VoodooVolumeEngine::ensure_ghost_in_portal(VoodooPortal& portal, EntityId entity_id,
                                               const Workspace& workspace) {
  for (auto& g : portal.ghosts) {
    if (g.entity_id == entity_id && !g.is_parent_box) {
      return true;
    }
  }
  const WorkspaceEntry* e = workspace.find(entity_id);
  if (e == nullptr) {
    return false;
  }
  const float near_w = std::max(portal.near_bb.extents.x, 1e-4f);
  const float far_w = std::max(portal.far_bb.extents.x, 1e-4f);
  if (portal.lock_scale < 1e-4f) {
    portal.lock_scale = clampf(near_w / far_w, 0.05f, 1.f);
  }
  VoodooGhost g;
  g.entity_id = entity_id;
  g.is_parent_box = false;
  g.far_pose = e->pose;
  g.scale = portal.lock_scale;
  g.near_pose = map_far_to_near_pose(portal, g.far_pose, g.scale);
  portal.ghosts.push_back(g);
  remove_ghost_from_other_portals(portal.id, entity_id);
  return true;
}

void VoodooVolumeEngine::bump_near_field_clutter(const VoodooPortal& portal,
                                                 Workspace& workspace, const Pose& head,
                                                 bool head_valid, bool animate) {
  if (!portal.near_bb.open) {
    return;
  }
  // Clear anything inside the near OBB *or* its XZ footprint cylinder (same Y band).
  const float foot = portal_near_footprint_radius(portal.near_bb) + kNearClearGapM;
  const Vec3& c = portal.near_bb.pose.position;
  const float y_half = portal.near_bb.extents.y * 0.5f + kNearClearGapM;
  const Vec3 body{head_valid ? head.position.x : 0.f, 0.f, head_valid ? head.position.z : 0.f};
  const Vec3 fwd = flat_forward(head, head_valid);
  const Vec3 right{fwd.z, 0.f, -fwd.x};
  // Beyond near reach → spill into far field (just past arm-reach capsule).
  const float far_spill_r = params_.arm_reach_m * 2.5f + foot;

  const auto snapshot = workspace.entries();
  for (const auto& e : snapshot) {
    if (!e.in_field || e.id == 0) {
      continue;
    }
    bool is_ghost = false;
    for (const auto& g : portal.ghosts) {
      if (!g.is_parent_box && g.entity_id == e.id) {
        is_ghost = true;
        break;
      }
    }
    if (is_ghost) {
      continue;
    }
    float dx = e.pose.position.x - c.x;
    float dz = e.pose.position.z - c.z;
    float horiz = std::sqrt(dx * dx + dz * dz);
    const bool in_obb =
        point_in_obb(e.pose.position, portal.near_bb.pose, portal.near_bb.extents);
    const bool in_foot_band = horiz < foot && std::abs(e.pose.position.y - c.y) <= y_half;
    if (!in_obb && !in_foot_band) {
      continue;
    }
    auto* we = workspace.find_mut(e.id);
    if (we == nullptr) {
      continue;
    }

    // Prefer body-orbit L/R: walk azimuth until clear of the portal footprint.
    const float az0 = azimuth_on_orbit(body, fwd, right, we->pose.position);
    const float y = we->pose.position.y;
    Vec3 target = we->pose.position;
    bool found = false;
    constexpr float kStep = 0.12f;  // radians
    for (int dir = 0; dir < 2 && !found; ++dir) {
      const float sign = dir == 0 ? 1.f : -1.f;
      for (int step = 1; step <= 24; ++step) {
        const float az = wrap_pi(az0 + sign * kStep * static_cast<float>(step));
        const Vec3 cand = pos_on_orbit_ellipse(body, fwd, right, az, y);
        const float cdx = cand.x - c.x;
        const float cdz = cand.z - c.z;
        const float ch = std::sqrt(cdx * cdx + cdz * cdz);
        if (ch >= foot && !point_in_obb(cand, portal.near_bb.pose, portal.near_bb.extents)) {
          target = cand;
          found = true;
          break;
        }
      }
    }
    if (!found) {
      // Orbital slot blocked — push radially beyond reach into the far field.
      dx = we->pose.position.x - body.x;
      dz = we->pose.position.z - body.z;
      horiz = std::sqrt(dx * dx + dz * dz);
      if (horiz < 1e-4f) {
        dx = fwd.x;
        dz = fwd.z;
        horiz = std::sqrt(dx * dx + dz * dz);
        if (horiz < 1e-4f) {
          dx = 1.f;
          dz = 0.f;
          horiz = 1.f;
        }
      }
      const float s = far_spill_r / horiz;
      target = Vec3{body.x + dx * s, y, body.z + dz * s};
    }

    if (animate) {
      Pose to = we->pose;
      to.position = target;
      begin_pose_lerp(e.id, to, workspace);
    } else {
      we->pose.position = target;
    }
  }
}

void VoodooVolumeEngine::refit_far_bb_to_content_ghosts(VoodooPortal& portal,
                                                       Workspace& workspace,
                                                       bool settle_bump) {
  bool any = false;
  Vec3 mn{};
  Vec3 mx{};
  for (const auto& g : portal.ghosts) {
    if (g.is_parent_box || g.entity_id == 0) {
      continue;
    }
    const Vec3& p = g.far_pose.position;
    const float pad = kGlyphHalfM;
    if (!any) {
      mn = Vec3{p.x - pad, p.y - pad, p.z - pad};
      mx = Vec3{p.x + pad, p.y + pad, p.z + pad};
      any = true;
    } else {
      mn.x = std::min(mn.x, p.x - pad);
      mn.y = std::min(mn.y, p.y - pad);
      mn.z = std::min(mn.z, p.z - pad);
      mx.x = std::max(mx.x, p.x + pad);
      mx.y = std::max(mx.y, p.y + pad);
      mx.z = std::max(mx.z, p.z + pad);
    }
  }
  if (!any) {
    return;
  }

  const float lock = portal.lock_scale > 1e-4f ? portal.lock_scale : 0.5f;
  Vec3 fitted{};
  if (settle_bump) {
    // Settle: recenter far on content, then orbit-bump / clutter.
    fitted = fit_extents_for_aabb(mn, mx);
    portal.far_bb.pose.position =
        Vec3{0.5f * (mn.x + mx.x), 0.5f * (mn.y + mx.y), 0.5f * (mn.z + mx.z)};
  } else {
    // Live drag: expand extents about the *fixed* far center. Moving the center
    // mid-drag remaps near↔far and feedback-loops with boundary clamp (10× blow-up).
    // This portal stays put (bumper); siblings may orbit-bump after extents grow.
    const Vec3& c = portal.far_bb.pose.position;
    const Vec3 half{std::max(std::abs(mn.x - c.x), std::abs(mx.x - c.x)),
                    std::max(std::abs(mn.y - c.y), std::abs(mx.y - c.y)),
                    std::max(std::abs(mn.z - c.z), std::abs(mx.z - c.z))};
    fitted = fit_extents_for_aabb(Vec3{c.x - half.x, c.y - half.y, c.z - half.z},
                                  Vec3{c.x + half.x, c.y + half.y, c.z + half.z});
    // Don't shrink mid-drag (avoids jitter when content is near a face).
    fitted.x = std::max(fitted.x, portal.far_bb.extents.x);
    fitted.y = std::max(fitted.y, portal.far_bb.extents.y);
    fitted.z = std::max(fitted.z, portal.far_bb.extents.z);
  }
  portal.far_bb.extents = fitted;
  portal.far_bb.aspect_x = params_.near_bb_voxels_x;
  portal.far_bb.aspect_y = params_.near_bb_voxels_y;
  portal.far_bb.aspect_z = params_.near_bb_voxels_z;

  // Lock park-time ratio: near extents track far; this portal's pose (pos+yaw) stays put.
  portal.near_bb.extents =
      Vec3{fitted.x * lock, fitted.y * lock, fitted.z * lock};
  portal.near_bb.aspect_x = params_.near_bb_voxels_x;
  portal.near_bb.aspect_y = params_.near_bb_voxels_y;
  portal.near_bb.aspect_z = params_.near_bb_voxels_z;
  for (auto& g : portal.ghosts) {
    if (!g.is_parent_box) {
      g.scale = lock;
    }
  }
  refresh_ghost_near_poses(portal);

  // Resizing portal is always the bumper: siblings nudge clear; it never moves/re-yaws.
  // Use min-clear (not full orbit resolve) so live grow cannot ratchet siblings behind the user.
  size_t pinned = portals_.size();
  for (size_t i = 0; i < portals_.size(); ++i) {
    if (portals_[i].id == portal.id) {
      pinned = i;
      break;
    }
  }
  if (pinned < portals_.size()) {
    bump_siblings_min_clear(pinned);
    refresh_ghost_near_poses(portals_[pinned]);
  }
  bump_near_field_clutter(portal, workspace, last_head_, last_head_valid_, /*animate=*/false);
}

void VoodooVolumeEngine::bump_siblings_min_clear(size_t pinned_index) {
  if (pinned_index >= portals_.size() || portals_.size() < 2) {
    return;
  }
  const auto& bumper = portals_[pinned_index];
  if (!bumper.near_bb.open) {
    return;
  }
  const Vec3& pc = bumper.near_bb.pose.position;
  const float r_pin = portal_near_footprint_radius(bumper.near_bb);

  for (size_t i = 0; i < portals_.size(); ++i) {
    if (i == pinned_index) {
      continue;
    }
    auto& other = portals_[i];
    if (!other.near_bb.open) {
      continue;
    }
    // Prefer settled / target pose so a mid-lerp sibling is not re-measured from a chord.
    const Vec3 oc =
        (other.bump_t < 1.f) ? other.bump_to : other.near_bb.pose.position;
    const float r_o = portal_near_footprint_radius(other.near_bb);
    const float need = r_pin + r_o;
    float dx = oc.x - pc.x;
    float dz = oc.z - pc.z;
    float dist = std::sqrt(dx * dx + dz * dz);
    if (dist + 1e-4f >= need) {
      continue;
    }
    if (dist < 1e-4f) {
      const Vec3 fwd = flat_forward(last_head_, last_head_valid_);
      // Prefer lateral push so we don't shove along the user's forward axis.
      dx = fwd.z;
      dz = -fwd.x;
      dist = std::sqrt(dx * dx + dz * dz);
      if (dist < 1e-4f) {
        dx = 1.f;
        dz = 0.f;
        dist = 1.f;
      }
    }
    const float scale = need / dist;
    const Vec3 to{pc.x + dx * scale, oc.y, pc.z + dz * scale};
    // Apply immediately in world XZ — do not start an ellipse-arc bump_t lerp
    // (that path can swing a sibling behind the user when only inches were needed).
    other.near_bb.pose.position = to;
    other.bump_from = to;
    other.bump_to = to;
    other.bump_t = 1.f;
    other.near_bb.pose.orientation =
        yaw_billboard_toward(to, last_head_, last_head_valid_);
    refresh_ghost_near_poses(other);
  }
}

bool VoodooVolumeEngine::apply_near_ghost_snap_hold(EntityId object_id, EntityId box_id,
                                                    float /*hold_above_m*/, Workspace& workspace) {
  if (object_id == 0 || box_id == 0 || object_id == box_id) {
    return false;
  }
  VoodooPortal* portal = nullptr;
  VoodooGhost* drag_g = nullptr;
  VoodooGhost* box_g = nullptr;
  for (auto& p : portals_) {
    VoodooGhost* d = nullptr;
    VoodooGhost* b = nullptr;
    for (auto& g : p.ghosts) {
      if (g.is_parent_box) {
        continue;
      }
      if (g.entity_id == object_id) {
        d = &g;
      }
      if (g.entity_id == box_id) {
        b = &g;
      }
    }
    if (d != nullptr && b != nullptr) {
      portal = &p;
      drag_g = d;
      box_g = b;
      break;
    }
  }
  if (portal == nullptr || drag_g == nullptr || box_g == nullptr) {
    return false;
  }
  // Split the difference between the clipped 0.04 m seat and the gappy 0.14 m far hold.
  constexpr float kNearHoldAboveM = 0.09f;
  drag_g->near_pose = box_g->near_pose;
  drag_g->near_pose.position.y += kNearHoldAboveM;
  drag_g->near_pose.orientation = portal->near_bb.pose.orientation;
  drag_g->scale = std::max(drag_g->scale, 1e-4f);
  drag_g->far_pose = map_near_to_far_pose(*portal, drag_g->near_pose, drag_g->scale);
  if (auto* we = workspace.find_mut(object_id)) {
    we->pose = drag_g->far_pose;
  }
  return true;
}

void VoodooVolumeEngine::sync_ghosts_from_workspace(const Workspace& workspace) {
  auto entity_ghost_dragging = [this](EntityId id) {
    for (uint32_t h = 0; h < 2u; ++h) {
      if (ghost_drags_[h].active && ghost_drags_[h].entity_id == id) {
        return true;
      }
    }
    return false;
  };

  for (auto& portal : portals_) {
    if (!portal.far_bb.open) {
      continue;
    }

    // Prune / update: content ghost lives only while its far pose stays in the
    // far BB. Skip mid ghost-drag (committed drag grows the BB instead) and
    // pose-lerp. Possess / far velocity still deallocs immediately on exit.
    portal.ghosts.erase(
        std::remove_if(portal.ghosts.begin(), portal.ghosts.end(),
                       [&](VoodooGhost& g) {
                         if (g.is_parent_box) {
                           return false;
                         }
                         if (entity_ghost_dragging(g.entity_id) || pose_lerp_active(g.entity_id)) {
                           return false;
                         }
                         const WorkspaceEntry* e = workspace.find(g.entity_id);
                         if (e == nullptr || !e->in_field ||
                             !point_in_obb(e->pose.position, portal.far_bb.pose,
                                           portal.far_bb.extents)) {
                           return true;  // dealloc
                         }
                         g.far_pose = e->pose;
                         return false;
                       }),
        portal.ghosts.end());

    // Alloc: any in-field Object/Box that enters this far BB gets a ghost —
    // including velocity re-entry and objects that did not originate here.
    for (const auto& e : workspace.entries()) {
      if (!e.in_field || e.id == 0) {
        continue;
      }
      if (e.kind != StorageKind::Object && e.kind != StorageKind::Box) {
        continue;
      }
      if (entity_ghost_dragging(e.id) || pose_lerp_active(e.id)) {
        continue;
      }
      if (!point_in_obb(e.pose.position, portal.far_bb.pose, portal.far_bb.extents)) {
        continue;
      }
      bool have = false;
      for (const auto& g : portal.ghosts) {
        if (!g.is_parent_box && g.entity_id == e.id) {
          have = true;
          break;
        }
      }
      if (!have) {
        (void)ensure_ghost_in_portal(portal, e.id, workspace);
      }
    }

    refresh_ghost_near_poses(portal);
  }
}

bool VoodooVolumeEngine::ghost_drag_active(uint32_t hand) const {
  return hand < 2u && ghost_drags_[hand].active;
}

EntityId VoodooVolumeEngine::ghost_drag_entity(uint32_t hand) const {
  return hand < 2u && ghost_drags_[hand].active ? ghost_drags_[hand].entity_id : 0;
}

GhostDragKind VoodooVolumeEngine::ghost_drag_kind(uint32_t hand) const {
  return hand < 2u && ghost_drags_[hand].active ? ghost_drags_[hand].kind
                                                : GhostDragKind::Committed;
}

const VoodooDragArrow& VoodooVolumeEngine::ghost_drag_arrow(uint32_t hand) const {
  static const VoodooDragArrow kEmpty{};
  return hand < 2u ? ghost_drags_[hand].arrow : kEmpty;
}

const VoodooDragArrow& VoodooVolumeEngine::ghost_drag_far_arrow(uint32_t hand) const {
  static const VoodooDragArrow kEmpty{};
  return hand < 2u ? ghost_drags_[hand].far_arrow : kEmpty;
}

bool VoodooVolumeEngine::pose_lerp_active(EntityId entity_id) const {
  for (const auto& l : pose_lerps_) {
    if (l.active && l.entity_id == entity_id) {
      return true;
    }
  }
  return false;
}

void VoodooVolumeEngine::begin_pose_lerp(EntityId entity_id, const Pose& to, Workspace& workspace) {
  if (entity_id == 0) {
    return;
  }
  Pose from{};
  if (const WorkspaceEntry* e = workspace.find(entity_id)) {
    from = e->pose;
  } else {
    from = to;
  }
  // Replace any in-flight lerp for this entity.
  for (auto& l : pose_lerps_) {
    if (l.active && l.entity_id == entity_id) {
      l.from = from;
      l.to = to;
      l.t = 0.f;
      return;
    }
  }
  PoseLerpState l;
  l.active = true;
  l.entity_id = entity_id;
  l.from = from;
  l.to = to;
  l.t = 0.f;
  pose_lerps_.push_back(l);
}

void VoodooVolumeEngine::tick_pose_lerps(float dt_seconds, Workspace& workspace) {
  const float dt = std::max(0.f, dt_seconds);
  for (auto& l : pose_lerps_) {
    if (!l.active) {
      continue;
    }
    l.t += dt / std::max(kSummonLerpS, 1e-3f);
    if (l.t >= 1.f) {
      l.t = 1.f;
      if (auto* we = workspace.find_mut(l.entity_id)) {
        we->pose = l.to;
      }
      // Sync ghost far pose if still a portal member.
      VoodooPortal* portal = nullptr;
      VoodooGhost* ghost = nullptr;
      if (find_ghost_mut(l.entity_id, &portal, &ghost) && ghost != nullptr) {
        ghost->far_pose = l.to;
        refresh_ghost_near_poses(*portal);
      }
      l.active = false;
      continue;
    }
    const float u = l.t * l.t * (3.f - 2.f * l.t);  // smoothstep
    Pose p = l.to;
    p.position = Vec3{l.from.position.x + (l.to.position.x - l.from.position.x) * u,
                      l.from.position.y + (l.to.position.y - l.from.position.y) * u,
                      l.from.position.z + (l.to.position.z - l.from.position.z) * u};
    if (auto* we = workspace.find_mut(l.entity_id)) {
      we->pose = p;
    }
    VoodooPortal* portal = nullptr;
    VoodooGhost* ghost = nullptr;
    if (find_ghost_mut(l.entity_id, &portal, &ghost) && ghost != nullptr) {
      ghost->far_pose = p;
      refresh_ghost_near_poses(*portal);
    }
  }
  pose_lerps_.erase(std::remove_if(pose_lerps_.begin(), pose_lerps_.end(),
                                   [](const PoseLerpState& l) { return !l.active; }),
                    pose_lerps_.end());
}

void VoodooVolumeEngine::audition_enter_portal(GhostDragState& gd, VoodooPortal& portal,
                                               const Pose& hand_pose, Workspace& workspace) {
  if (!ensure_ghost_in_portal(portal, gd.entity_id, workspace)) {
    return;
  }
  VoodooGhost* ghost = nullptr;
  VoodooPortal* p = &portal;
  if (!find_ghost_mut(gd.entity_id, &p, &ghost) || ghost == nullptr) {
    return;
  }
  gd.portal_id = portal.id;
  gd.audition_inside = true;
  gd.scale = std::max(ghost->scale, 1e-4f);
  const Vec3 near_pos = gd.direct_follow ? Vec3{hand_pose.position.x + gd.grab_offset.x,
                                                hand_pose.position.y + gd.grab_offset.y,
                                                hand_pose.position.z + gd.grab_offset.z}
                                         : hand_pose.position;
  ghost->near_pose.position = near_pos;
  ghost->near_pose.orientation = portal.near_bb.pose.orientation;
  ghost->far_pose = map_near_to_far_pose(portal, ghost->near_pose, gd.scale);
  if (auto* we = workspace.find_mut(gd.entity_id)) {
    we->pose = ghost->far_pose;
  }
}

void VoodooVolumeEngine::audition_exit_portal(GhostDragState& gd, const Pose& hand_pose,
                                              Workspace& workspace) {
  for (auto& portal : portals_) {
    if (portal.id != gd.portal_id) {
      continue;
    }
    portal.ghosts.erase(std::remove_if(portal.ghosts.begin(), portal.ghosts.end(),
                                       [&gd](const VoodooGhost& g) {
                                         return g.entity_id == gd.entity_id && !g.is_parent_box;
                                       }),
                        portal.ghosts.end());
    break;
  }
  gd.audition_inside = false;
  gd.portal_id = 0;
  if (auto* we = workspace.find_mut(gd.entity_id)) {
    we->pose.position = gd.direct_follow ? Vec3{hand_pose.position.x + gd.grab_offset.x,
                                                hand_pose.position.y + gd.grab_offset.y,
                                                hand_pose.position.z + gd.grab_offset.z}
                                         : hand_pose.position;
  }
}

void VoodooVolumeEngine::audition_apply_matrix_snap(GhostDragState& gd, const Pose& hand_pose,
                                                    Workspace& workspace) {
  if (gd.audition_inside || gd.entity_id == 0) {
    return;
  }
  auto* we = workspace.find_mut(gd.entity_id);
  if (we == nullptr || !we->in_field) {
    return;
  }
  const bool pending_before =
      workspace.snap().pending && workspace.snap().object_id == gd.entity_id;

  // Free/escape pose for sticky tests — never use the seated hold as escape, or the
  // snap can never clear (and never re-anchor every frame; that resets escape too).
  Pose free = we->pose;
  if (pending_before && gd.have_snap_escape) {
    if (gd.direct_follow) {
      gd.snap_escape =
          Vec3{hand_pose.position.x + gd.grab_offset.x, hand_pose.position.y + gd.grab_offset.y,
               hand_pose.position.z + gd.grab_offset.z};
    }
    free.position = gd.snap_escape;
  }

  (void)workspace.update_snap(gd.entity_id, free, /*apply_pose=*/true);

  const bool pending_after =
      workspace.snap().pending && workspace.snap().object_id == gd.entity_id;
  if (!pending_after) {
    gd.have_snap_escape = false;
    return;
  }

  if (!pending_before) {
    // Snap enter only: seat is on hold; start escape at hold and re-anchor grab/stick.
    gd.have_snap_escape = true;
    gd.snap_escape = we->pose.position;
    if (gd.direct_follow) {
      gd.grab_offset =
          Vec3{we->pose.position.x - hand_pose.position.x, we->pose.position.y - hand_pose.position.y,
               we->pose.position.z - hand_pose.position.z};
    } else {
      gd.start_hand = hand_pose;
      gd.deadzone_armed = false;
      gd.deadzone_left_once = false;
      gd.velocity = {};
    }
  }
}

bool VoodooVolumeEngine::ghost_drag_begin(uint32_t hand, EntityId entity_id, const Pose& hand_pose,
                                         Workspace& workspace) {
  if (hand >= 2u || entity_id == 0) {
    return false;
  }
  ghost_drags_[hand] = {};

  VoodooPortal* portal = nullptr;
  VoodooGhost* ghost = nullptr;
  const bool has_ghost =
      find_ghost_mut(entity_id, &portal, &ghost) && portal != nullptr && ghost != nullptr;

  auto& gd = ghost_drags_[hand];
  gd.active = true;
  gd.hand = hand;
  gd.entity_id = entity_id;
  gd.start_hand = hand_pose;
  gd.have_start_hand = true;
  gd.velocity = {};
  gd.deadzone_armed = false;
  gd.deadzone_left_once = false;
  gd.clamped = false;
  gd.arrow = {};
  gd.audition_inside = false;

  if (has_ghost) {
    // Committed portal ghost: must start inside home near BB.
    if (!point_in_obb(hand_pose.position, portal->near_bb.pose, portal->near_bb.extents)) {
      ghost_drags_[hand] = {};
      return false;
    }
    gd.kind = GhostDragKind::Committed;
    gd.portal_id = portal->id;
    gd.scale = std::max(ghost->scale, 1e-4f);
    gd.start_far = ghost->far_pose;
    if (const WorkspaceEntry* e = workspace.find(entity_id)) {
      gd.start_far = e->pose;
      ghost->far_pose = e->pose;
    }
    refresh_ghost_near_poses(*portal);
    // Cursor inside the near ghost glyph → direct translation sync (same as near reals).
    // Unity floors ghost visual scale at 0.15; match that so hover-in-glyph works.
    const float grab_r = kDirectGrabRadiusM * std::max(ghost->scale, 0.15f);
    const float dx = ghost->near_pose.position.x - hand_pose.position.x;
    const float dy = ghost->near_pose.position.y - hand_pose.position.y;
    const float dz = ghost->near_pose.position.z - hand_pose.position.z;
    const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (dist <= grab_r) {
      gd.direct_follow = true;
      gd.grab_offset = Vec3{dx, dy, dz};
    }
    return true;
  }

  // Near-field real: audition drag (may start outside or already inside a portal).
  gd.kind = GhostDragKind::Audition;
  if (const WorkspaceEntry* e = workspace.find(entity_id)) {
    gd.start_far = e->pose;
    const float dx = e->pose.position.x - hand_pose.position.x;
    const float dy = e->pose.position.y - hand_pose.position.y;
    const float dz = e->pose.position.z - hand_pose.position.z;
    const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    // Cursor inside the glyph → direct translation sync; otherwise velocity stick.
    if (dist <= kDirectGrabRadiusM) {
      gd.direct_follow = true;
      gd.grab_offset = Vec3{dx, dy, dz};
    }
  }
  const uint64_t pid = portal_at_near_point(hand_pose.position);
  if (pid != 0) {
    for (auto& p : portals_) {
      if (p.id == pid) {
        audition_enter_portal(gd, p, hand_pose, workspace);
        break;
      }
    }
  }
  return true;
}

void VoodooVolumeEngine::ghost_drag_tick(uint32_t hand, const Pose& hand_pose, bool hand_valid,
                                         const Pose& head, bool head_valid, float dt_seconds,
                                         Workspace& workspace) {
  if (hand >= 2u) {
    return;
  }
  auto& gd = ghost_drags_[hand];
  if (!gd.active) {
    return;
  }
  if (!hand_valid || !gd.have_start_hand) {
    gd.velocity = {};
    gd.arrow = {};
    gd.far_arrow = {};
    return;
  }

  const Vec3 stick{hand_pose.position.x - gd.start_hand.position.x,
                   hand_pose.position.y - gd.start_hand.position.y,
                   hand_pose.position.z - gd.start_hand.position.z};
  const float mag = std::sqrt(stick.x * stick.x + stick.y * stick.y + stick.z * stick.z);

  if (!gd.deadzone_armed) {
    if (mag >= deadzone_m_) {
      gd.deadzone_left_once = true;
    }
    if (gd.deadzone_left_once && mag < deadzone_m_) {
      gd.deadzone_armed = true;
    }
  }

  // --- Audition: enter/exit portals freely; no bound trap until commit ---
  if (gd.kind == GhostDragKind::Audition) {
    const uint64_t pid = portal_at_near_point(hand_pose.position);
    if (gd.audition_inside) {
      if (pid == 0 || pid != gd.portal_id) {
        audition_exit_portal(gd, hand_pose, workspace);
        if (pid != 0) {
          for (auto& p : portals_) {
            if (p.id == pid) {
              audition_enter_portal(gd, p, hand_pose, workspace);
              break;
            }
          }
        }
      }
    } else if (pid != 0) {
      for (auto& p : portals_) {
        if (p.id == pid) {
          audition_enter_portal(gd, p, hand_pose, workspace);
          break;
        }
      }
    }

    // Direct grab: no deadzone / velocity — object stays translation-synced to cursor.
    if (gd.direct_follow) {
      const Vec3 synced{hand_pose.position.x + gd.grab_offset.x,
                        hand_pose.position.y + gd.grab_offset.y,
                        hand_pose.position.z + gd.grab_offset.z};
      gd.velocity = {};
      if (gd.audition_inside) {
        VoodooPortal* portal = nullptr;
        VoodooGhost* ghost = nullptr;
        if (!find_ghost_mut(gd.entity_id, &portal, &ghost) || portal == nullptr ||
            ghost == nullptr) {
          gd.audition_inside = false;
          return;
        }
        ghost->near_pose.position = synced;
        ghost->near_pose.orientation = portal->near_bb.pose.orientation;
        gd.scale = std::max(ghost->scale, 1e-4f);
        ghost->far_pose = map_near_to_far_pose(*portal, ghost->near_pose, gd.scale);
        if (auto* we = workspace.find_mut(gd.entity_id)) {
          we->pose = ghost->far_pose;
        }
        gd.clamped = false;
      } else if (auto* we = workspace.find_mut(gd.entity_id)) {
        we->pose.position = synced;
        audition_apply_matrix_snap(gd, hand_pose, workspace);
      }
      return;
    }

    if (mag < deadzone_m_) {
      gd.velocity = {};
      if (!gd.audition_inside) {
        audition_apply_matrix_snap(gd, hand_pose, workspace);
      }
      return;
    }

    if (gd.audition_inside) {
      VoodooPortal* portal = nullptr;
      VoodooGhost* ghost = nullptr;
      if (!find_ghost_mut(gd.entity_id, &portal, &ghost) || portal == nullptr || ghost == nullptr) {
        gd.audition_inside = false;
        return;
      }
      // Velocity audition inside: ghost tracks cursor; real tracks mapped far.
      ghost->near_pose.position = hand_pose.position;
      ghost->near_pose.orientation = portal->near_bb.pose.orientation;
      gd.scale = std::max(ghost->scale, 1e-4f);
      ghost->far_pose = map_near_to_far_pose(*portal, ghost->near_pose, gd.scale);
      if (auto* we = workspace.find_mut(gd.entity_id)) {
        we->pose = ghost->far_pose;
      }
      gd.clamped = false;
    } else {
      // Outside: velocity stick (cursor was not in the glyph at grab).
      const float eff = (mag - deadzone_m_) / mag;
      gd.velocity =
          Vec3{kGhostDragGain * stick.x * eff, kGhostDragGain * stick.y * eff,
               kGhostDragGain * stick.z * eff};
      const float dt = std::max(0.f, dt_seconds);
      if (auto* we = workspace.find_mut(gd.entity_id)) {
        const bool snapped =
            workspace.snap().pending && workspace.snap().object_id == gd.entity_id;
        if (snapped && gd.have_snap_escape) {
          // Advance escape only — display hold is applied inside matrix snap.
          gd.snap_escape.x += gd.velocity.x * dt;
          gd.snap_escape.y += gd.velocity.y * dt;
          gd.snap_escape.z += gd.velocity.z * dt;
        } else {
          we->pose.position.x += gd.velocity.x * dt;
          we->pose.position.y += gd.velocity.y * dt;
          we->pose.position.z += gd.velocity.z * dt;
        }
      }
      audition_apply_matrix_snap(gd, hand_pose, workspace);
    }
    return;
  }

  // --- Committed ghost drag: clamp near proxy + live far/near BB refit ---
  VoodooPortal* portal = nullptr;
  VoodooGhost* ghost = nullptr;
  if (!find_ghost_mut(gd.entity_id, &portal, &ghost) || portal == nullptr || ghost == nullptr ||
      portal->id != gd.portal_id) {
    gd = {};
    return;
  }

  // Direct grab: ghost stays on/inside the near face; far follows the *unclamped*
  // cursor so overshoot grows far+near (same contract as velocity drag). Mapping
  // from the clamped near alone pinned far on the face and made adjacent near-field
  // objects look like impenetrable walls.
  if (gd.direct_follow) {
    gd.velocity = {};
    const Vec3 synced{hand_pose.position.x + gd.grab_offset.x,
                      hand_pose.position.y + gd.grab_offset.y,
                      hand_pose.position.z + gd.grab_offset.z};
    const Vec3 clamped = clamp_point_to_near_obb(*portal, synced);
    gd.clamped = dist3(synced, clamped) > 1e-4f;
    gd.scale = std::max(ghost->scale, 1e-4f);
    Pose near_drive{};
    near_drive.position = synced;
    near_drive.orientation = portal->near_bb.pose.orientation;
    ghost->far_pose = map_near_to_far_pose(*portal, near_drive, gd.scale);
    if (auto* we = workspace.find_mut(gd.entity_id)) {
      we->pose = ghost->far_pose;
    }
    refit_far_bb_to_content_ghosts(*portal, workspace, /*settle_bump=*/false);
    gd.scale = std::max(ghost->scale, 1e-4f);
    ghost->near_pose = map_far_to_near_pose(*portal, ghost->far_pose, ghost->scale);
    ghost->near_pose.position = clamp_point_to_near_obb(*portal, ghost->near_pose.position);
    ghost->near_pose.orientation = portal->near_bb.pose.orientation;
    return;
  }

  if (mag < deadzone_m_) {
    gd.velocity = {};
    return;
  }

  const float eff = (mag - deadzone_m_) / mag;
  gd.scale = std::max(ghost->scale, 1e-4f);
  const float gain_far = kGhostDragGain / gd.scale;
  gd.velocity = Vec3{gain_far * stick.x * eff, gain_far * stick.y * eff, gain_far * stick.z * eff};
  const float dt = std::max(0.f, dt_seconds);
  Pose far = ghost->far_pose;
  far.position.x += gd.velocity.x * dt;
  far.position.y += gd.velocity.y * dt;
  far.position.z += gd.velocity.z * dt;
  ghost->far_pose = far;

  if (auto* we = workspace.find_mut(gd.entity_id)) {
    we->pose = far;
  }

  {
    const Pose near_unc = map_far_to_near_pose(*portal, far, ghost->scale);
    gd.clamped = !point_in_obb(near_unc.position, portal->near_bb.pose, portal->near_bb.extents);
  }

  // Grow far+near at lock_scale so dragging past a face expands the portal
  // instead of ejecting the ghost. Possess/far velocity still prunes via sync.
  refit_far_bb_to_content_ghosts(*portal, workspace, /*settle_bump=*/false);
  gd.scale = std::max(ghost->scale, 1e-4f);
  ghost->near_pose.position = clamp_point_to_near_obb(*portal, ghost->near_pose.position);
  ghost->near_pose.orientation = portal->near_bb.pose.orientation;
}

void VoodooVolumeEngine::ghost_drag_end(uint32_t hand, bool release_inside_near_bb,
                                        Workspace& workspace) {
  if (hand >= 2u) {
    return;
  }
  auto& gd = ghost_drags_[hand];
  if (!gd.active) {
    return;
  }

  if (gd.kind == GhostDragKind::Audition) {
    if (gd.audition_inside && release_inside_near_bb) {
      // Commit: keep ghost; workspace stays at mapped far; now refit far BB.
      VoodooPortal* portal = nullptr;
      VoodooGhost* ghost = nullptr;
      if (find_ghost_mut(gd.entity_id, &portal, &ghost) && portal != nullptr && ghost != nullptr) {
        if (auto* we = workspace.find_mut(gd.entity_id)) {
          we->pose = ghost->far_pose;
        }
        refit_far_bb_to_content_ghosts(*portal, workspace, /*settle_bump=*/true);
      }
    } else if (gd.audition_inside) {
      // Released outside (or cancelled): destroy ghost; real returns to last near pose.
      Pose leave{};
      leave.position = gd.start_hand.position;
      VoodooPortal* portal = nullptr;
      VoodooGhost* ghost = nullptr;
      if (find_ghost_mut(gd.entity_id, &portal, &ghost) && ghost != nullptr) {
        leave = ghost->near_pose;
      }
      audition_exit_portal(gd, leave, workspace);
      if (auto* we = workspace.find_mut(gd.entity_id)) {
        we->pose = leave;
      }
    }
    // Already outside: workspace is the near-field real — nothing to clean up.
    gd = {};
    return;
  }

  VoodooPortal* portal = nullptr;
  VoodooGhost* ghost = nullptr;
  find_ghost_mut(gd.entity_id, &portal, &ghost);
  if (portal != nullptr && ghost != nullptr) {
    if (auto* we = workspace.find_mut(gd.entity_id)) {
      we->pose = ghost->far_pose;
    }
    refit_far_bb_to_content_ghosts(*portal, workspace, /*settle_bump=*/true);
    // Refit can land exactly on the OBB face; keep the ghost inside the near volume.
    ghost->near_pose.position =
        clamp_point_to_near_obb(*portal, ghost->near_pose.position);
    ghost->near_pose.orientation = portal->near_bb.pose.orientation;
  }

  gd = {};
}

void VoodooVolumeEngine::remove_content_ghost(EntityId entity_id) {
  if (entity_id == 0) {
    return;
  }
  for (auto& portal : portals_) {
    // Strip content and parent-box ghosts — insert/put-away must not leave a
    // stale parent marker for an out-of-field entity.
    portal.ghosts.erase(std::remove_if(portal.ghosts.begin(), portal.ghosts.end(),
                                       [entity_id](const VoodooGhost& g) {
                                         return g.entity_id == entity_id;
                                       }),
                        portal.ghosts.end());
  }
  for (uint32_t h = 0; h < 2u; ++h) {
    if (ghost_drags_[h].active && ghost_drags_[h].entity_id == entity_id) {
      ghost_drags_[h] = {};
    }
  }
  pose_lerps_.erase(std::remove_if(pose_lerps_.begin(), pose_lerps_.end(),
                                   [entity_id](const PoseLerpState& l) {
                                     return l.active && l.entity_id == entity_id;
                                   }),
                    pose_lerps_.end());
}

std::vector<EntityId> VoodooVolumeEngine::on_entity_inserted(EntityId entity_id) {
  std::vector<EntityId> temps_to_clear;
  if (entity_id == 0) {
    return temps_to_clear;
  }
  remove_content_ghost(entity_id);

  // Snap-root box was put away — dismiss its portal (near BB + ghosts) so we
  // never tick a volume whose persistent centroid no longer exists in-field.
  std::vector<uint64_t> doomed;
  doomed.reserve(portals_.size());
  for (const auto& p : portals_) {
    if (p.persistent_centroid_id == entity_id) {
      doomed.push_back(p.id);
      if (p.temporary_centroid_id != 0) {
        temps_to_clear.push_back(p.temporary_centroid_id);
      }
    }
  }
  for (uint64_t pid : doomed) {
    (void)dismiss_portal(pid);
  }
  return temps_to_clear;
}

bool VoodooVolumeEngine::summon_selected(EntityId entity_id, const Vec3& pinch_world,
                                         Workspace& workspace) {
  if (entity_id == 0) {
    return false;
  }

  VoodooPortal* home = nullptr;
  VoodooGhost* ghost = nullptr;
  const bool has_ghost =
      find_ghost_mut(entity_id, &home, &ghost) && home != nullptr && ghost != nullptr;
  const uint64_t pinch_portal = portal_at_near_point(pinch_world);

  // Insert: selected near-field real + pinch inside a near BB → lerp real to mapped far.
  // Open-air: entity already without a ghost (left far BB / pruned) → lerp real to pinch.
  if (!has_ghost) {
    if (pinch_portal == 0) {
      const WorkspaceEntry* e = workspace.find(entity_id);
      if (e == nullptr || !e->in_field) {
        return false;
      }
      Pose p{};
      p.position = pinch_world;
      p.orientation = e->pose.orientation;
      begin_pose_lerp(entity_id, p, workspace);
      return true;
    }
    VoodooPortal* target = nullptr;
    for (auto& p : portals_) {
      if (p.id == pinch_portal) {
        target = &p;
        break;
      }
    }
    if (target == nullptr || !ensure_ghost_in_portal(*target, entity_id, workspace)) {
      return false;
    }
    VoodooGhost* ng = nullptr;
    if (!find_ghost_mut(entity_id, &target, &ng) || ng == nullptr) {
      return false;
    }
    ng->near_pose.position = pinch_world;
    ng->near_pose.orientation = target->near_bb.pose.orientation;
    const Pose far_pose = map_near_to_far_pose(*target, ng->near_pose, ng->scale);
    ng->far_pose = far_pose;
    begin_pose_lerp(entity_id, far_pose, workspace);
    return true;
  }

  // Pinch inside home near BB is deselect (host handles); refuse summon here.
  if (point_in_obb(pinch_world, home->near_bb.pose, home->near_bb.extents)) {
    return false;
  }

  if (pinch_portal != 0 && pinch_portal != home->id) {
    // Cross-portal: ghost jumps to pinch; real fast-lerps to mapped far.
    VoodooPortal* target = nullptr;
    for (auto& p : portals_) {
      if (p.id == pinch_portal) {
        target = &p;
        break;
      }
    }
    if (target == nullptr) {
      return false;
    }
    const float near_w = std::max(target->near_bb.extents.x, 1e-4f);
    const float far_w = std::max(target->far_bb.extents.x, 1e-4f);
    const float scale = clampf(near_w / far_w, 0.05f, 1.f);
    Pose near_pose{};
    near_pose.position = pinch_world;
    near_pose.orientation = target->near_bb.pose.orientation;
    const Pose far_pose = map_near_to_far_pose(*target, near_pose, scale);

    home->ghosts.erase(std::remove_if(home->ghosts.begin(), home->ghosts.end(),
                                      [entity_id](const VoodooGhost& g) {
                                        return g.entity_id == entity_id && !g.is_parent_box;
                                      }),
                       home->ghosts.end());

    VoodooGhost ng;
    ng.entity_id = entity_id;
    ng.is_parent_box = false;
    ng.scale = scale;
    ng.far_pose = far_pose;
    ng.near_pose = near_pose;
    target->ghosts.push_back(ng);
    begin_pose_lerp(entity_id, far_pose, workspace);
    return true;
  }

  // Open-air summon: remove ghost; real fast-lerps to world pinch.
  home->ghosts.erase(std::remove_if(home->ghosts.begin(), home->ghosts.end(),
                                    [entity_id](const VoodooGhost& g) {
                                      return g.entity_id == entity_id && !g.is_parent_box;
                                    }),
                     home->ghosts.end());
  Pose p{};
  p.position = pinch_world;
  if (const WorkspaceEntry* e = workspace.find(entity_id)) {
    p.orientation = e->pose.orientation;
  } else {
    p.orientation = home->near_bb.pose.orientation;
  }
  begin_pose_lerp(entity_id, p, workspace);
  return true;
}

}  // namespace threeos
