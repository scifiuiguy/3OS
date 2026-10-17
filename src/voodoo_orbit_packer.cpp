#include "core/voodoo_orbit_packer.hpp"

#include "core/mind_palace_layout.hpp"

#include "core/voodoo_volume.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

namespace threeos {
namespace {

float clampf(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }

float dist3(const Vec3& a, const Vec3& b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  const float dz = a.z - b.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

float length2_xz(const Vec3& v) { return std::sqrt(v.x * v.x + v.z * v.z); }

}  // namespace

Vec3 VoodooOrbitPacker::effective_portal_pos(const VoodooPortal& portal) {
  return (portal.bump_t < 1.f) ? portal.bump_to : portal.near_bb.pose.position;
}


void VoodooOrbitPacker::set_portal_orbit_target(VoodooPortal& portal, const Vec3& to,
                                                 const Pose& head, bool head_valid) {
  portal.bump_from = portal.near_bb.pose.position;
  portal.bump_to = to;
  if (dist3(portal.bump_from, to) < 1e-4f) {
    portal.near_bb.pose.position = to;
    portal.bump_t = 1.f;
  } else {
    portal.bump_t = 0.f;
  }
  portal.near_bb.pose.orientation = VoodooVolumeEngine::yaw_billboard_toward(to, head, head_valid);
  engine_.refresh_ghost_near_poses(portal);
}


bool VoodooOrbitPacker::walk_orbit(const Vec3& body, const Vec3& fwd, const Vec3& right, float az0,
                                    float y, const std::function<bool(const Vec3&)>& pred,
                                    Vec3* out) const {
  if (out == nullptr || !pred) {
    return false;
  }
  constexpr float kStep = 0.06f;
  for (int dir = 0; dir < 2; ++dir) {
    const float sign = dir == 0 ? 1.f : -1.f;
    for (int step = 1; step <= 48; ++step) {
      const float az = VoodooVolumeEngine::wrap_pi(az0 + sign * kStep * static_cast<float>(step));
      const Vec3 cand = pos_on_orbit_ellipse(body, fwd, right, az, y);
      if (pred(cand)) {
        *out = cand;
        return true;
      }
    }
  }
  return false;
}


float VoodooOrbitPacker::near_footprint_radius_m() const {
  const Vec3 e = engine_.near_extents_m();
  // Horizontal half-diagonal so yaw-facing corners cannot kiss.
  return 0.5f * std::sqrt(e.x * e.x + e.z * e.z) + VoodooVolumeEngine::kNearClearGapM * 0.5f;
}


float VoodooOrbitPacker::portal_near_footprint_radius(const VoodooBoundingBox& near_bb) {
  const Vec3& e = near_bb.extents;
  return 0.5f * std::sqrt(e.x * e.x + e.z * e.z) + VoodooVolumeEngine::kNearClearGapM * 0.5f;
}


uint32_t VoodooOrbitPacker::max_orbit_slots() const {
  // Pack along the front↔behind ellipse (avg radius). Extra pad for clearance.
  const float need = 2.f * near_footprint_radius_m() + 0.02f;
  const float r = 0.5f * (VoodooVolumeEngine::kNearRestForwardM + VoodooVolumeEngine::kOrbitBehindM);
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
  return std::max(1u, std::min(VoodooVolumeEngine::kMaxNearPortals, geo));
}


float VoodooOrbitPacker::orbit_radius_at_az(float az) const {
  const float c = std::cos(az);
  const float r_lat = std::max(VoodooVolumeEngine::kNearRestForwardM, VoodooVolumeEngine::kOrbitBehindM);
  const float r_z = (c >= 0.f) ? VoodooVolumeEngine::kNearRestForwardM : VoodooVolumeEngine::kOrbitBehindM;
  const float s = std::sin(az);
  return std::sqrt((r_lat * s) * (r_lat * s) + (r_z * c) * (r_z * c));
}


Vec3 VoodooOrbitPacker::pos_on_orbit_ellipse(const Vec3& body, const Vec3& fwd, const Vec3& right,
                                               float az, float y) const {
  const float s = std::sin(az);
  const float c = std::cos(az);
  const float r_lat = std::max(VoodooVolumeEngine::kNearRestForwardM, VoodooVolumeEngine::kOrbitBehindM);
  const float r_z = (c >= 0.f) ? VoodooVolumeEngine::kNearRestForwardM : VoodooVolumeEngine::kOrbitBehindM;
  return Vec3{body.x + right.x * (r_lat * s) + fwd.x * (r_z * c), y,
              body.z + right.z * (r_lat * s) + fwd.z * (r_z * c)};
}


float VoodooOrbitPacker::azimuth_on_orbit(const Vec3& body, const Vec3& fwd, const Vec3& right,
                                           const Vec3& p) const {
  const float dx = p.x - body.x;
  const float dz = p.z - body.z;
  const float along_fwd = dx * fwd.x + dz * fwd.z;
  const float along_right = dx * right.x + dz * right.z;
  return std::atan2(along_right, along_fwd);
}


bool VoodooOrbitPacker::near_targets_clear() const {
  std::vector<size_t> all(engine_.portals_.size());
  for (size_t i = 0; i < engine_.portals_.size(); ++i) {
    all[i] = i;
  }
  return near_targets_clear_indices(all);
}


bool VoodooOrbitPacker::near_targets_clear_indices(const std::vector<size_t>& idxs) const {
  for (size_t a = 0; a < idxs.size(); ++a) {
    for (size_t b = a + 1; b < idxs.size(); ++b) {
      const auto& pa = engine_.portals_[idxs[a]];
      const auto& pb = engine_.portals_[idxs[b]];
      VoodooBoundingBox ba = pa.near_bb;
      VoodooBoundingBox bb = pb.near_bb;
      ba.pose.position = pa.bump_t < 1.f ? pa.bump_to : pa.near_bb.pose.position;
      bb.pose.position = pb.bump_t < 1.f ? pb.bump_to : pb.near_bb.pose.position;
      if (near_bbs_overlap_xz(ba, bb, VoodooVolumeEngine::kNearClearGapM)) {
        return false;
      }
    }
  }
  return true;
}


void VoodooOrbitPacker::merge_groups(uint64_t from_id, uint64_t into_id) {
  if (from_id == 0 || into_id == 0 || from_id == into_id) {
    return;
  }
  for (auto& p : engine_.portals_) {
    if (p.group_id == from_id) {
      p.group_id = into_id;
    }
  }
}


size_t VoodooOrbitPacker::find_overlap_portal(const VoodooBoundingBox& candidate,
                                              size_t skip_index) const {
  for (size_t i = 0; i < engine_.portals_.size(); ++i) {
    if (i == skip_index || !engine_.portals_[i].near_bb.open) {
      continue;
    }
    VoodooBoundingBox other = engine_.portals_[i].near_bb;
    other.pose.position = engine_.portals_[i].bump_t < 1.f ? engine_.portals_[i].bump_to
                                                   : engine_.portals_[i].near_bb.pose.position;
    if (near_bbs_overlap_xz(candidate, other, VoodooVolumeEngine::kNearClearGapM)) {
      return i;
    }
  }
  return static_cast<size_t>(-1);
}


bool VoodooOrbitPacker::group_is_full_circle(uint64_t group_id, const Pose& head,
                                             bool head_valid) const {
  (void)head;
  (void)head_valid;
  if (group_id == 0) {
    return false;
  }
  uint32_t n = 0;
  for (const auto& p : engine_.portals_) {
    if (p.group_id == group_id && p.near_bb.open) {
      ++n;
    }
  }
  return n >= max_orbit_slots();
}


bool VoodooOrbitPacker::near_bbs_overlap_xz(const VoodooBoundingBox& a, const VoodooBoundingBox& b,
                                             float gap_m) {
  // Conservative circle vs circle on footprint radii.
  const float ra = 0.5f * std::sqrt(a.extents.x * a.extents.x + a.extents.z * a.extents.z);
  const float rb = 0.5f * std::sqrt(b.extents.x * b.extents.x + b.extents.z * b.extents.z);
  const float need = ra + rb + gap_m;
  const float dx = a.pose.position.x - b.pose.position.x;
  const float dz = a.pose.position.z - b.pose.position.z;
  return (dx * dx + dz * dz) < (need * need);
}


bool VoodooOrbitPacker::orbit_near_bbs_clear_of_paste(const Vec3& paste_c, float paste_r) {
  const Pose& head = engine_.last_head_;
  const bool head_valid = engine_.last_head_valid_;
  const Vec3 body{head_valid ? head.position.x : 0.f, 0.f, head_valid ? head.position.z : 0.f};
  const Vec3 fwd = VoodooVolumeEngine::flat_forward(head, head_valid);
  const Vec3 right{fwd.z, 0.f, -fwd.x};

  auto paste_need = [&](const VoodooPortal& p) {
    return paste_r + portal_near_footprint_radius(p.near_bb) + VoodooVolumeEngine::kNearClearGapM;
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
    return ra + rb + VoodooVolumeEngine::kNearClearGapM;
  };

  // Pass 1: orbit every near BB that intersects the paste footprint.
  for (auto& p : engine_.portals_) {
    if (!p.near_bb.open) {
      continue;
    }
    const Vec3 from = effective_portal_pos(p);
    if (clear_of_paste(from, p)) {
      continue;
    }
    const float az0 = azimuth_on_orbit(body, fwd, right, from);
    Vec3 target = from;
    if (!walk_orbit(body, fwd, right, az0, from.y,
                    [&](const Vec3& cand) { return clear_of_paste(cand, p); }, &target)) {
      return false;
    }
    set_portal_orbit_target(p, target, head, head_valid);
  }

  // Pass 2+: cascade sibling overlaps. A paste-bumped BB that lands on a neighbor must
  // push that neighbor onward (and so on), without anyone walking back into the paste.
  for (int pass = 0; pass < 64; ++pass) {
    bool moved = false;
    for (size_t i = 0; i < engine_.portals_.size(); ++i) {
      if (!engine_.portals_[i].near_bb.open) {
        continue;
      }
      for (size_t j = i + 1; j < engine_.portals_.size(); ++j) {
        if (!engine_.portals_[j].near_bb.open) {
          continue;
        }
        auto& a = engine_.portals_[i];
        auto& b = engine_.portals_[j];
        Vec3 pa = effective_portal_pos(a);
        Vec3 pb = effective_portal_pos(b);
        const float need = pair_need(a, b);
        float dx = pb.x - pa.x;
        float dz = pb.z - pa.z;
        float dist = std::sqrt(dx * dx + dz * dz);
        if (dist + 1e-4f >= need) {
          continue;
        }

        float az_a = azimuth_on_orbit(body, fwd, right, pa);
        float az_b = azimuth_on_orbit(body, fwd, right, pb);
        const float daz = VoodooVolumeEngine::shortest_daz(az_a, az_b);
        const float sign = daz >= 0.f ? 1.f : -1.f;
        constexpr float kSep = 0.025f;
        bool separated = false;
        for (int step = 0; step < 80; ++step) {
          // Prefer moving B (later seed) when A was the paste bumper; both may move.
          az_b = VoodooVolumeEngine::wrap_pi(az_b + sign * kSep);
          az_a = VoodooVolumeEngine::wrap_pi(az_a - sign * kSep * 0.35f);
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
          pa = effective_portal_pos(a);
          pb = effective_portal_pos(b);
          az_b = azimuth_on_orbit(body, fwd, right, pb);
          Vec3 target = pb;
          const bool ok = walk_orbit(
              body, fwd, right, az_b, pb.y,
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
          set_portal_orbit_target(b, target, head, head_valid);
          moved = true;
          continue;
        }
        set_portal_orbit_target(a, pa, head, head_valid);
        set_portal_orbit_target(b, pb, head, head_valid);
        moved = true;
      }
    }

    // Re-clear anyone who drifted into paste during sibling pushes.
    for (auto& p : engine_.portals_) {
      if (!p.near_bb.open) {
        continue;
      }
      const Vec3 from = effective_portal_pos(p);
      if (clear_of_paste(from, p)) {
        continue;
      }
      const float az0 = azimuth_on_orbit(body, fwd, right, from);
      Vec3 target = from;
      if (!walk_orbit(body, fwd, right, az0, from.y,
                      [&](const Vec3& cand) { return clear_of_paste(cand, p); }, &target)) {
        return false;
      }
      set_portal_orbit_target(p, target, head, head_valid);
      moved = true;
    }

    if (!moved) {
      break;
    }
  }

  // Final world check: paste clear + sibling targets clear.
  for (const auto& p : engine_.portals_) {
    if (p.near_bb.open && !clear_of_paste(effective_portal_pos(p), p)) {
      return false;
    }
  }
  return near_targets_clear();
}


Vec3 VoodooOrbitPacker::resolve_cut_paste_origin(const Vec3& pinch_world, float cluster_radius_m) {
  const Pose& head = engine_.last_head_;
  const bool head_valid = engine_.last_head_valid_;
  const Vec3 body{head_valid ? head.position.x : 0.f, 0.f, head_valid ? head.position.z : 0.f};
  const Vec3 fwd = VoodooVolumeEngine::flat_forward(head, head_valid);
  const Vec3 right{fwd.z, 0.f, -fwd.x};
  const float paste_r = std::max(cluster_radius_m, VoodooVolumeEngine::kGlyphHalfM) + VoodooVolumeEngine::kNearClearGapM;

  auto footprint_clear = [&](const Vec3& c) {
    for (const auto& p : engine_.portals_) {
      if (!p.near_bb.open) {
        continue;
      }
      const Vec3 pc = effective_portal_pos(p);
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
    Vec3 nudged = pinch_world;
    // Wider step than packing walks — paste origin nudge can travel farther.
    constexpr float kNudgeStep = 0.1f;
    bool found = false;
    for (int dir = 0; dir < 2 && !found; ++dir) {
      const float sign = dir == 0 ? 1.f : -1.f;
      for (int step = 1; step <= 32; ++step) {
        const float az = VoodooVolumeEngine::wrap_pi(az0 + sign * kNudgeStep * static_cast<float>(step));
        const Vec3 cand = pos_on_orbit_ellipse(body, fwd, right, az, pinch_world.y);
        if (footprint_clear(cand)) {
          nudged = cand;
          found = true;
          break;
        }
      }
    }
    if (found) {
      dest = nudged;
      orbit_near_bbs_clear_of_paste(dest, paste_r);
    }
  }
  return dest;
}


Pose VoodooOrbitPacker::orbit_near_to_hand(const Pose& seed, const Pose& head, bool head_valid,
                                             const Pose& hand_pose, bool hand_valid,
                                             uint32_t hand) const {
  const float body_x = head_valid ? head.position.x : 0.f;
  const float body_z = head_valid ? head.position.z : 0.f;
  const Vec3 body{body_x, 0.f, body_z};
  const Vec3 fwd = VoodooVolumeEngine::flat_forward(head, head_valid);
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
  out.orientation = VoodooVolumeEngine::yaw_billboard_toward(out.position, head, head_valid);
  return out;
}


bool VoodooOrbitPacker::bump_resolve(const Pose& head, bool head_valid, size_t pinned_index) {
  if (pinned_index >= engine_.portals_.size()) {
    return false;
  }
  if (engine_.portals_.size() < 2) {
    engine_.refresh_ghost_near_poses(engine_.portals_[pinned_index]);
    return true;
  }
  const float body_x = head_valid ? head.position.x : 0.f;
  const float body_z = head_valid ? head.position.z : 0.f;
  const Vec3 body{body_x, 0.f, body_z};
  const Vec3 fwd = VoodooVolumeEngine::flat_forward(head, head_valid);
  const Vec3 right = Vec3{fwd.z, 0.f, -fwd.x};
  // Use the largest near-BB footprint in the active group (near can grow with far).
  float foot = near_footprint_radius_m();
  for (const auto& p : engine_.portals_) {
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
  std::vector<Snapshot> snap(engine_.portals_.size());
  for (size_t i = 0; i < engine_.portals_.size(); ++i) {
    snap[i].pos = engine_.portals_[i].near_bb.pose.position;
    snap[i].bump_from = engine_.portals_[i].bump_from;
    snap[i].bump_to = engine_.portals_[i].bump_to;
    snap[i].bump_t = engine_.portals_[i].bump_t;
    snap[i].group_id = engine_.portals_[i].group_id;
    snap[i].orientation = engine_.portals_[i].near_bb.pose.orientation;
  }
  auto restore_all = [&]() {
    for (size_t i = 0; i < engine_.portals_.size() && i < snap.size(); ++i) {
      engine_.portals_[i].near_bb.pose.position = snap[i].pos;
      engine_.portals_[i].near_bb.pose.orientation = snap[i].orientation;
      engine_.portals_[i].bump_from = snap[i].bump_from;
      engine_.portals_[i].bump_to = snap[i].bump_to;
      engine_.portals_[i].bump_t = snap[i].bump_t;
      engine_.portals_[i].group_id = snap[i].group_id;
    }
  };

  uint64_t active = engine_.portals_[pinned_index].group_id;
  if (active == 0) {
    active = engine_.next_group_id_++;
    engine_.portals_[pinned_index].group_id = active;
  }

  struct Slot {
    size_t idx = 0;
    float az = 0.f;
    float y = 0.f;
    bool pinned = false;
  };

  auto rebuild_slots = [&](std::vector<Slot>* out) {
    out->clear();
    for (size_t i = 0; i < engine_.portals_.size(); ++i) {
      if (engine_.portals_[i].group_id != active) {
        continue;
      }
      Slot s;
      s.idx = i;
      s.pinned = (i == pinned_index);
      // Prefer settled/target pose — never sample a mid-chord lerp for azimuth.
      const Vec3 src = effective_portal_pos(engine_.portals_[i]);
      s.az = azimuth_on_orbit(body, fwd, right, src);
      s.y = src.y;
      if (s.pinned) {
        // Bumper stays put: never snap the pinned near BB onto the ellipse.
        engine_.portals_[i].bump_from = engine_.portals_[i].near_bb.pose.position;
        engine_.portals_[i].bump_to = engine_.portals_[i].near_bb.pose.position;
        engine_.portals_[i].bump_t = 1.f;
      } else {
        const Vec3 on_arc = pos_on_orbit_ellipse(body, fwd, right, s.az, s.y);
        engine_.portals_[i].bump_from = on_arc;
        if (engine_.portals_[i].bump_t >= 1.f) {
          engine_.portals_[i].near_bb.pose.position = on_arc;
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
          const float daz = VoodooVolumeEngine::shortest_daz(a.az, b.az);
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
              b.az = VoodooVolumeEngine::wrap_pi(b.az + sign * kStep);
            } else if (b.pinned) {
              a.az = VoodooVolumeEngine::wrap_pi(a.az - sign * kStep);
            } else {
              a.az = VoodooVolumeEngine::wrap_pi(a.az - sign * kStep * 0.5f);
              b.az = VoodooVolumeEngine::wrap_pi(b.az + sign * kStep * 0.5f);
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
      auto& portal = engine_.portals_[s.idx];
      if (s.pinned) {
        // Bumper (e.g. near BB being resized): never translate or re-yaw.
        portal.bump_to = portal.near_bb.pose.position;
        portal.bump_t = 1.f;
        continue;
      }
      // Preserve rebuild_slots bump_from (ellipse seat), then animate to resolved az.
      const Vec3 from = portal.bump_from;
      const Vec3 to = pos_on_orbit_ellipse(body, fwd, right, s.az, s.y);
      portal.near_bb.pose.position = from;
      set_portal_orbit_target(portal, to, head, head_valid);
      // set_portal_orbit_target overwrote bump_from with `from`; keep explicit.
      portal.bump_from = from;
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
      VoodooBoundingBox cand = engine_.portals_[s.idx].near_bb;
      cand.pose.position = engine_.portals_[s.idx].bump_t < 1.f ? engine_.portals_[s.idx].bump_to
                                                        : engine_.portals_[s.idx].near_bb.pose.position;
      const size_t hit = find_overlap_portal(cand, s.idx);
      if (hit == static_cast<size_t>(-1)) {
        continue;
      }
      if (engine_.portals_[hit].group_id == active) {
        continue;
      }
      merge_groups(engine_.portals_[hit].group_id, active);
      engine_.portals_[hit].group_id = active;
      merged = true;
    }
    if (!merged) {
      if (near_targets_clear()) {
        for (auto& portal : engine_.portals_) {
          if (portal.group_id == active) {
            engine_.refresh_ghost_near_poses(portal);
          }
        }
        return true;
      }
      // Absorb any still-overlapping outsider (including peer-group pairs) into
      // the facing group and keep resolving until the world is clear.
      for (size_t i = 0; i < engine_.portals_.size() && !merged; ++i) {
        VoodooBoundingBox cand = engine_.portals_[i].near_bb;
        cand.pose.position = engine_.portals_[i].bump_t < 1.f ? engine_.portals_[i].bump_to
                                                      : engine_.portals_[i].near_bb.pose.position;
        const size_t hit = find_overlap_portal(cand, i);
        if (hit == static_cast<size_t>(-1)) {
          continue;
        }
        if (engine_.portals_[i].group_id != active) {
          merge_groups(engine_.portals_[i].group_id, active);
          engine_.portals_[i].group_id = active;
        }
        if (engine_.portals_[hit].group_id != active) {
          merge_groups(engine_.portals_[hit].group_id, active);
          engine_.portals_[hit].group_id = active;
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


void VoodooOrbitPacker::tick_portals(float dt_seconds, const Pose& head, bool head_valid) {
  engine_.last_head_ = head;
  engine_.last_head_valid_ = head_valid;
  const float dt = clampf(dt_seconds, 1e-4f, 0.1f);
  const float body_x = head_valid ? head.position.x : 0.f;
  const float body_z = head_valid ? head.position.z : 0.f;
  const Vec3 body{body_x, 0.f, body_z};
  const Vec3 fwd = VoodooVolumeEngine::flat_forward(head, head_valid);
  const Vec3 right = Vec3{fwd.z, 0.f, -fwd.x};
  for (auto& p : engine_.portals_) {
    if (p.bump_t >= 1.f) {
      continue;
    }
    p.bump_t = clampf(p.bump_t + dt / VoodooVolumeEngine::kBumpDurationS, 0.f, 1.f);
    if (p.bump_t >= 1.f) {
      // Snap exactly to target so ellipse re-parameterization cannot drift.
      p.near_bb.pose.position = p.bump_to;
      p.near_bb.pose.orientation = VoodooVolumeEngine::yaw_billboard_toward(p.bump_to, head, head_valid);
    } else {
      // Smoothstep along the shortest ellipse arc (front rest ↔ 2ft behind).
      const float u = p.bump_t;
      const float s = u * u * (3.f - 2.f * u);
      const float az0 = azimuth_on_orbit(body, fwd, right, p.bump_from);
      const float az1 = azimuth_on_orbit(body, fwd, right, p.bump_to);
      const float daz = VoodooVolumeEngine::shortest_daz(az0, az1);
      const float az = VoodooVolumeEngine::wrap_pi(az0 + daz * s);
      const float y = p.bump_from.y + (p.bump_to.y - p.bump_from.y) * s;
      p.near_bb.pose.position = pos_on_orbit_ellipse(body, fwd, right, az, y);
      p.near_bb.pose.orientation =
          VoodooVolumeEngine::yaw_billboard_toward(p.near_bb.pose.position, head, head_valid);
    }
    engine_.refresh_ghost_near_poses(p);
  }

  // Keep parked hand-session BB snapshots aligned with their live portal (bump / dismiss).
  for (uint32_t h = 0; h < 2u; ++h) {
    auto& hr = engine_.hands_[h];
    if (hr.session.phase != VoodooSessionPhase::Parked || hr.parked_portal_id_ == 0) {
      continue;
    }
    bool found = false;
    for (const auto& p : engine_.portals_) {
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


void VoodooOrbitPacker::bump_near_field_clutter(const VoodooPortal& portal,
                                                 Workspace& workspace, const Pose& head,
                                                 bool head_valid, bool animate) {
  if (!portal.near_bb.open) {
    return;
  }
  // Clear anything inside the near OBB *or* its XZ footprint cylinder (same Y band).
  const float foot = portal_near_footprint_radius(portal.near_bb) + VoodooVolumeEngine::kNearClearGapM;
  const Vec3& c = portal.near_bb.pose.position;
  const float y_half = portal.near_bb.extents.y * 0.5f + VoodooVolumeEngine::kNearClearGapM;
  const Vec3 body{head_valid ? head.position.x : 0.f, 0.f, head_valid ? head.position.z : 0.f};
  const Vec3 fwd = VoodooVolumeEngine::flat_forward(head, head_valid);
  const Vec3 right{fwd.z, 0.f, -fwd.x};
  // Beyond near reach → spill into far field (just past arm-reach capsule).
  const float far_spill_r = engine_.params_.arm_reach_m * 2.5f + foot;

  const auto snapshot = workspace.entries();
  bool any_bumped = false;
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
        VoodooVolumeEngine::point_in_obb(e.pose.position, portal.near_bb.pose, portal.near_bb.extents);
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
        const float az = VoodooVolumeEngine::wrap_pi(az0 + sign * kStep * static_cast<float>(step));
        const Vec3 cand = pos_on_orbit_ellipse(body, fwd, right, az, y);
        const float cdx = cand.x - c.x;
        const float cdz = cand.z - c.z;
        const float ch = std::sqrt(cdx * cdx + cdz * cdz);
        if (ch >= foot && !VoodooVolumeEngine::point_in_obb(cand, portal.near_bb.pose, portal.near_bb.extents)) {
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

    any_bumped = true;
    if (animate) {
      Pose to = we->pose;
      to.position = target;
      engine_.begin_pose_lerp(e.id, to, workspace);
    } else {
      we->pose.position = target;
    }
  }

  // Only resolve when clutter actually moved — never re-pack the whole field matrix.
  if (any_bumped) {
    resolve_mutual_object_overlaps(workspace, head, head_valid, workspace.field_spacing_m());
  }
}


void VoodooOrbitPacker::bump_siblings_min_clear(size_t pinned_index) {
  if (pinned_index >= engine_.portals_.size() || engine_.portals_.size() < 2) {
    return;
  }
  const auto& bumper = engine_.portals_[pinned_index];
  if (!bumper.near_bb.open) {
    return;
  }
  const Vec3& pc = bumper.near_bb.pose.position;
  const float r_pin = portal_near_footprint_radius(bumper.near_bb);

  for (size_t i = 0; i < engine_.portals_.size(); ++i) {
    if (i == pinned_index) {
      continue;
    }
    auto& other = engine_.portals_[i];
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
      const Vec3 fwd = VoodooVolumeEngine::flat_forward(engine_.last_head_, engine_.last_head_valid_);
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
        VoodooVolumeEngine::yaw_billboard_toward(to, engine_.last_head_, engine_.last_head_valid_);
    engine_.refresh_ghost_near_poses(other);
  }
}

}  // namespace threeos
