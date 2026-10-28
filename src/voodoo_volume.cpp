#include "core/voodoo_volume.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <functional>
#include <string>

#if defined(__ANDROID__)
#include <android/log.h>
#endif

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

void voodoo_trace_log(const char* fmt, ...) {
#if defined(__ANDROID__)
  va_list ap;
  va_start(ap, fmt);
  __android_log_vprint(ANDROID_LOG_INFO, "3OS_Voodoo", fmt, ap);
  va_end(ap);
#else
  va_list ap;
  va_start(ap, fmt);
  std::vfprintf(stderr, fmt, ap);
  std::fprintf(stderr, "\n");
  va_end(ap);
#endif
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

// Far-field overlap separation (far space is the ground truth inside a voodoo volume; near
// ghosts just copy it). Pushes `a` (and `b`, when given) apart to min_gap. With b == nullptr,
// `fixed` is an immovable neighbor and only `a` moves. Coincident points split along a
// deterministic golden-angle heading.
bool separate_pair(Vec3& a, Vec3* b, float min_gap, size_t salt, const Vec3* fixed = nullptr) {
  const Vec3& other = b != nullptr ? *b : *fixed;
  Vec3 d{a.x - other.x, a.y - other.y, a.z - other.z};
  float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
  if (len >= min_gap) {
    return false;
  }
  if (len < 1e-4f) {
    const float ang = 2.3999632f * static_cast<float>(salt + 1);
    d = Vec3{std::cos(ang), 0.f, std::sin(ang)};
    len = 1.f;
    const float k0 = (b != nullptr ? 0.5f : 1.f) * min_gap;
    a.x += d.x * k0;
    a.z += d.z * k0;
    if (b != nullptr) {
      b->x -= d.x * k0;
      b->z -= d.z * k0;
    }
    return true;
  }
  const float k = (b != nullptr ? 0.5f : 1.f) * (min_gap - len) / len;
  a.x += d.x * k;
  a.y += d.y * k;
  a.z += d.z * k;
  if (b != nullptr) {
    b->x -= d.x * k;
    b->y -= d.y * k;
    b->z -= d.z * k;
  }
  return true;
}

}  // namespace

VoodooPortal* VoodooVolumeEngine::portal_mut(uint64_t portal_id) {
  if (portal_id == 0) {
    return nullptr;
  }
  for (auto& p : portals_) {
    if (p.id == portal_id) {
      return &p;
    }
  }
  return nullptr;
}

const VoodooPortal* VoodooVolumeEngine::portal_const(uint64_t portal_id) const {
  if (portal_id == 0) {
    return nullptr;
  }
  for (const auto& p : portals_) {
    if (p.id == portal_id) {
      return &p;
    }
  }
  return nullptr;
}

float VoodooVolumeEngine::content_scale_for_portal(const VoodooPortal& portal) {
  return PortalSpace::content_scale(portal);
}

void VoodooVolumeEngine::erase_content_ghost(VoodooPortal& portal, EntityId entity_id) {
  if (entity_id == 0) {
    return;
  }
  portal.ghosts.erase(std::remove_if(portal.ghosts.begin(), portal.ghosts.end(),
                                     [entity_id](const VoodooGhost& g) {
                                       return g.entity_id == entity_id && !g.is_parent_box;
                                     }),
                      portal.ghosts.end());
}

void VoodooVolumeEngine::erase_all_content_ghosts(VoodooPortal& portal) {
  portal.ghosts.erase(std::remove_if(portal.ghosts.begin(), portal.ghosts.end(),
                                     [](const VoodooGhost& g) { return !g.is_parent_box; }),
                      portal.ghosts.end());
}

void VoodooVolumeEngine::strip_content_ghost_everywhere(EntityId entity_id) {
  if (entity_id == 0) {
    return;
  }
  for (auto& portal : portals_) {
    erase_content_ghost(portal, entity_id);
  }
}

bool VoodooVolumeEngine::move_ghost_to_portal(EntityId entity_id, VoodooPortal& target,
                                              const Pose& near_pose,
                                              Workspace* workspace_for_lerp) {
  if (entity_id == 0) {
    return false;
  }
  // NOTE: this path follows a user drop (near_pose drives, far derives). It does not yet
  // resolve overlaps with existing ghosts in far space; see far-first convention on VoodooGhost.
  strip_content_ghost_everywhere(entity_id);
  remove_ghost_from_other_portals(target.id, entity_id);

  const float scale = content_scale_for_portal(target);
  Pose seated = near_pose;
  seated.position = clamp_point_to_near_obb(target, seated.position);
  seated.orientation = target.near_bb.pose.orientation;
  const Pose far_pose = map_near_to_far_pose(target, seated, scale);

  VoodooGhost ng;
  ng.entity_id = entity_id;
  ng.is_parent_box = false;
  ng.scale = scale;
  ng.far_pose = far_pose;
  ng.near_pose = seated;
  target.ghosts.push_back(ng);

  if (workspace_for_lerp != nullptr) {
    begin_pose_lerp(entity_id, far_pose, *workspace_for_lerp);
  }
  return true;
}

void VoodooVolumeEngine::arm_deadzone(bool& armed, bool& left_once, float mag, float deadzone_m) {
  StickViz::arm_deadzone(armed, left_once, mag, deadzone_m);
}

Vec3 VoodooVolumeEngine::stick_viz_origin(const Vec3& start_hand, const Pose& head,
                                         bool head_valid) {
  return StickViz::origin(start_hand, head, head_valid);
}

void VoodooVolumeEngine::fill_drag_arrow(VoodooDragArrow& arrow, const Vec3& origin,
                                         const Vec3& stick, float mag, bool deadzone_armed,
                                         float deadzone_m) {
  StickViz::fill(arrow, origin, stick, mag, deadzone_armed, deadzone_m);
}

void VoodooVolumeEngine::fill_drag_arrow_cursor(VoodooDragArrow& arrow, const Vec3& anchor,
                                                const Vec3& cursor, bool deadzone_armed,
                                                float deadzone_m) {
  StickViz::fill_cursor(arrow, anchor, cursor, deadzone_armed, deadzone_m);
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
  cut_.cancel();
  clear_hand_session(0);
  clear_hand_session(1);
  ghost_drags_[0] = {};
  ghost_drags_[1] = {};
  pose_lerps_.clear();
  pending_agent_ = {};
  enclosure_boxes_.clear();
  room_bounds_enabled_ = false;
}

namespace {

Vec3 cross3(const Vec3& a, const Vec3& b) {
  return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

Vec3 quat_rotate_full(const Quat& q, const Vec3& v) {
  const Vec3 u{q.x, q.y, q.z};
  const Vec3 uv = cross3(u, v);
  const Vec3 uuv = cross3(u, uv);
  return Vec3{v.x + 2.f * (q.w * uv.x + uuv.x), v.y + 2.f * (q.w * uv.y + uuv.y),
              v.z + 2.f * (q.w * uv.z + uuv.z)};
}

Quat quat_conj(const Quat& q) { return Quat{-q.x, -q.y, -q.z, q.w}; }

float aabb_support_on_axis(const Vec3& half, const Vec3& axis) {
  return std::abs(axis.x) * half.x + std::abs(axis.y) * half.y + std::abs(axis.z) * half.z;
}

void expanded_obb_extents(const VoodooVolumeEngine::EnclosureBox& box, const Vec3& half_aabb,
                          Vec3& out_ax, Vec3& out_ay, Vec3& out_az, float& ehx, float& ehy,
                          float& ehz) {
  const float hx = std::abs(box.half_extents.x);
  const float hy = std::abs(box.half_extents.y);
  const float hz = std::abs(box.half_extents.z);
  out_ax = quat_rotate_full(box.orientation, Vec3{1.f, 0.f, 0.f});
  out_ay = quat_rotate_full(box.orientation, Vec3{0.f, 1.f, 0.f});
  out_az = quat_rotate_full(box.orientation, Vec3{0.f, 0.f, 1.f});
  ehx = hx + aabb_support_on_axis(half_aabb, out_ax);
  ehy = hy + aabb_support_on_axis(half_aabb, out_ay);
  ehz = hz + aabb_support_on_axis(half_aabb, out_az);
}

bool aabb_overlaps_obb(const Vec3& center, const Vec3& half_aabb,
                       const VoodooVolumeEngine::EnclosureBox& box) {
  if (std::abs(box.half_extents.x) < 1e-6f && std::abs(box.half_extents.y) < 1e-6f &&
      std::abs(box.half_extents.z) < 1e-6f) {
    return false;
  }
  Vec3 ax, ay, az;
  float ehx, ehy, ehz;
  expanded_obb_extents(box, half_aabb, ax, ay, az, ehx, ehy, ehz);
  const Vec3 local = quat_rotate_full(quat_conj(box.orientation),
                                      Vec3{center.x - box.center.x, center.y - box.center.y,
                                           center.z - box.center.z});
  return std::abs(local.x) <= ehx && std::abs(local.y) <= ehy && std::abs(local.z) <= ehz;
}

/// Push world-AABB (center + half) out of OBB if overlapping. Returns push vector (world).
bool depenetrate_aabb_vs_obb(Vec3& center, const Vec3& half_aabb,
                             const VoodooVolumeEngine::EnclosureBox& box, Vec3& out_push) {
  out_push = {};
  if (!aabb_overlaps_obb(center, half_aabb, box)) {
    return false;
  }
  Vec3 ax, ay, az;
  float ehx, ehy, ehz;
  expanded_obb_extents(box, half_aabb, ax, ay, az, ehx, ehy, ehz);
  const Vec3 local = quat_rotate_full(quat_conj(box.orientation),
                                      Vec3{center.x - box.center.x, center.y - box.center.y,
                                           center.z - box.center.z});
  const float px = ehx - std::abs(local.x);
  const float py = ehy - std::abs(local.y);
  const float pz = ehz - std::abs(local.z);
  Vec3 push_local{};
  if (px <= py && px <= pz) {
    push_local.x = (local.x >= 0.f) ? px : -px;
  } else if (py <= px && py <= pz) {
    push_local.y = (local.y >= 0.f) ? py : -py;
  } else {
    push_local.z = (local.z >= 0.f) ? pz : -pz;
  }
  out_push = quat_rotate_full(box.orientation, push_local);
  center.x += out_push.x;
  center.y += out_push.y;
  center.z += out_push.z;
  return true;
}

/// Segment [0,1] from o toward o+d vs local AABB [-e,e]. Entry hit; outward normal in local.
bool ray_enter_aabb_local(const Vec3& o, const Vec3& d, float ehx, float ehy, float ehz,
                          float& t_hit, Vec3& n_local) {
  float t0 = 0.f;
  float t1 = 1.f;
  int enter_axis = -1;
  float enter_sign = 0.f;
  const float e[3] = {ehx, ehy, ehz};
  const float ro[3] = {o.x, o.y, o.z};
  const float rd[3] = {d.x, d.y, d.z};
  for (int a = 0; a < 3; ++a) {
    if (std::abs(rd[a]) < 1e-9f) {
      if (ro[a] < -e[a] || ro[a] > e[a]) {
        return false;
      }
      continue;
    }
    float inv = 1.f / rd[a];
    float t_near = (-e[a] - ro[a]) * inv;
    float t_far = (e[a] - ro[a]) * inv;
    float sign_near = -1.f;
    if (t_near > t_far) {
      std::swap(t_near, t_far);
      sign_near = 1.f;
    } else {
      sign_near = (rd[a] > 0.f) ? -1.f : 1.f;
    }
    if (t_near > t0) {
      t0 = t_near;
      enter_axis = a;
      enter_sign = sign_near;
    }
    if (t_far < t1) {
      t1 = t_far;
    }
    if (t0 > t1) {
      return false;
    }
  }
  if (enter_axis < 0 || t0 < 0.f || t0 > 1.f) {
    // Started inside or no entry on segment.
    return false;
  }
  t_hit = t0;
  n_local = {};
  if (enter_axis == 0) {
    n_local.x = enter_sign;
  } else if (enter_axis == 1) {
    n_local.y = enter_sign;
  } else {
    n_local.z = enter_sign;
  }
  return true;
}

/// Leaving an AABB interior along segment — first exit face; inward (into-volume) normal.
bool ray_exit_aabb_world(const Vec3& o, const Vec3& d, const Vec3& bmin, const Vec3& bmax,
                         float& t_hit, Vec3& n_inward) {
  float t0 = 0.f;
  float t1 = 1.f;
  int exit_axis = -1;
  float exit_sign = 0.f;
  const float ro[3] = {o.x, o.y, o.z};
  const float rd[3] = {d.x, d.y, d.z};
  const float mn[3] = {bmin.x, bmin.y, bmin.z};
  const float mx[3] = {bmax.x, bmax.y, bmax.z};
  for (int a = 0; a < 3; ++a) {
    if (std::abs(rd[a]) < 1e-9f) {
      if (ro[a] < mn[a] || ro[a] > mx[a]) {
        return false;
      }
      continue;
    }
    float inv = 1.f / rd[a];
    float t_near = (mn[a] - ro[a]) * inv;
    float t_far = (mx[a] - ro[a]) * inv;
    if (t_near > t_far) {
      std::swap(t_near, t_far);
    }
    if (t_near > t0) {
      t0 = t_near;
    }
    if (t_far < t1) {
      t1 = t_far;
      exit_axis = a;
      // Outward face normal is +axis if leaving through max, -axis through min.
      exit_sign = (rd[a] > 0.f) ? 1.f : -1.f;
    }
    if (t0 > t1) {
      return false;
    }
  }
  if (exit_axis < 0 || t1 < 0.f || t1 > 1.f) {
    return false;
  }
  t_hit = t1;
  // Into-room = opposite of outward exit.
  n_inward = {};
  if (exit_axis == 0) {
    n_inward.x = -exit_sign;
  } else if (exit_axis == 1) {
    n_inward.y = -exit_sign;
  } else {
    n_inward.z = -exit_sign;
  }
  return true;
}

}  // namespace

void VoodooVolumeEngine::set_enclosure_boxes(const EnclosureBox* boxes, uint32_t count) {
  enclosure_boxes_.clear();
  if (boxes == nullptr || count == 0) {
    return;
  }
  const uint32_t n = std::min(count, kMaxEnclosureBoxes);
  enclosure_boxes_.reserve(n);
  for (uint32_t i = 0; i < n; ++i) {
    EnclosureBox b = boxes[i];
    b.half_extents.x = std::abs(b.half_extents.x);
    b.half_extents.y = std::abs(b.half_extents.y);
    b.half_extents.z = std::abs(b.half_extents.z);
    const float qlen = std::sqrt(b.orientation.x * b.orientation.x + b.orientation.y * b.orientation.y +
                                 b.orientation.z * b.orientation.z + b.orientation.w * b.orientation.w);
    if (qlen < 1e-6f) {
      b.orientation = Quat{0.f, 0.f, 0.f, 1.f};
    } else {
      b.orientation.x /= qlen;
      b.orientation.y /= qlen;
      b.orientation.z /= qlen;
      b.orientation.w /= qlen;
    }
    enclosure_boxes_.push_back(b);
  }
}

void VoodooVolumeEngine::clear_enclosure_boxes() { enclosure_boxes_.clear(); }

void VoodooVolumeEngine::set_room_bounds(const Vec3& min_xyz, const Vec3& max_xyz, bool enabled) {
  if (!enabled) {
    room_bounds_enabled_ = false;
    return;
  }
  room_bounds_min_ = Vec3{std::min(min_xyz.x, max_xyz.x), std::min(min_xyz.y, max_xyz.y),
                          std::min(min_xyz.z, max_xyz.z)};
  room_bounds_max_ = Vec3{std::max(min_xyz.x, max_xyz.x), std::max(min_xyz.y, max_xyz.y),
                          std::max(min_xyz.z, max_xyz.z)};
  room_bounds_enabled_ = (room_bounds_max_.x - room_bounds_min_.x) > 0.05f &&
                         (room_bounds_max_.y - room_bounds_min_.y) > 0.05f &&
                         (room_bounds_max_.z - room_bounds_min_.z) > 0.05f;
}

void VoodooVolumeEngine::clear_room_bounds() { room_bounds_enabled_ = false; }

void VoodooVolumeEngine::set_mr_enclosure_proxy_half_m(float half_m) {
  mr_enclosure_proxy_half_m_ = std::clamp(half_m, 0.01f, 2.f);
}

bool VoodooVolumeEngine::constrain_object_center(Vec3& center, const Vec3& half_extents,
                                                 const Pose& head, bool head_valid,
                                                 Vec3* out_wall_into_room,
                                                 const Vec3* ray_origin) const {
  return constrain_far_center(center, half_extents, head, head_valid, out_wall_into_room,
                              ray_origin);
}

bool VoodooVolumeEngine::constrain_far_center(Vec3& center, const Vec3& extents, const Pose& head,
                                              bool head_valid, Vec3* out_wall_into_room,
                                              const Vec3* ray_origin) const {
  (void)head;
  (void)head_valid;
  if (out_wall_into_room != nullptr) {
    *out_wall_into_room = {};
  }
  if (!room_bounds_enabled_ && enclosure_boxes_.empty()) {
    return false;
  }

  const Vec3 half{0.5f * std::abs(extents.x), 0.5f * std::abs(extents.y),
                  0.5f * std::abs(extents.z)};

  auto inset_room = [&](Vec3& bmin, Vec3& bmax) -> bool {
    if (!room_bounds_enabled_) {
      return false;
    }
    bmin = Vec3{room_bounds_min_.x + half.x, room_bounds_min_.y + half.y,
                room_bounds_min_.z + half.z};
    bmax = Vec3{room_bounds_max_.x - half.x, room_bounds_max_.y - half.y,
                room_bounds_max_.z - half.z};
    return bmin.x <= bmax.x && bmin.y <= bmax.y && bmin.z <= bmax.z;
  };

  auto inside_free_volume = [&](const Vec3& c) {
    Vec3 bmin, bmax;
    if (inset_room(bmin, bmax)) {
      if (c.x < bmin.x || c.x > bmax.x || c.y < bmin.y || c.y > bmax.y || c.z < bmin.z ||
          c.z > bmax.z) {
        return false;
      }
    }
    for (const auto& box : enclosure_boxes_) {
      if (aabb_overlaps_obb(c, half, box)) {
        return false;
      }
    }
    return true;
  };

  // Ideal isometric target is free — drive 3D as usual.
  if (inside_free_volume(center)) {
    return false;
  }

  const Vec3 origin = ray_origin != nullptr ? *ray_origin : center;
  const Vec3 delta{center.x - origin.x, center.y - origin.y, center.z - origin.z};
  const float dlen = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
  if (dlen < 1e-6f) {
    return false;
  }

  float best_t = 2.f;
  Vec3 best_n{};
  bool have_hit = false;

  // WorldMesh solids: first entry into expanded OBB along start→ideal.
  for (const auto& box : enclosure_boxes_) {
    Vec3 ax, ay, az;
    float ehx, ehy, ehz;
    expanded_obb_extents(box, half, ax, ay, az, ehx, ehy, ehz);
    const Vec3 local_o = quat_rotate_full(quat_conj(box.orientation),
                                          Vec3{origin.x - box.center.x, origin.y - box.center.y,
                                               origin.z - box.center.z});
    const Vec3 local_d =
        quat_rotate_full(quat_conj(box.orientation), delta);
    float t = 0.f;
    Vec3 n_local{};
    if (!ray_enter_aabb_local(local_o, local_d, ehx, ehy, ehz, t, n_local)) {
      continue;
    }
    if (t < best_t) {
      best_t = t;
      best_n = quat_rotate_full(box.orientation, n_local);
      have_hit = true;
    }
  }

  // Room shell: first exit from inset free AABB.
  {
    Vec3 bmin, bmax;
    if (inset_room(bmin, bmax)) {
      float t = 0.f;
      Vec3 n_in{};
      if (ray_exit_aabb_world(origin, delta, bmin, bmax, t, n_in) && t < best_t) {
        best_t = t;
        best_n = n_in;
        have_hit = true;
      }
    }
  }

  if (!have_hit) {
    // Ideal blocked but no segment hit (e.g. started buried) — soft fallback nudge.
    for (const auto& box : enclosure_boxes_) {
      Vec3 push{};
      depenetrate_aabb_vs_obb(center, half, box, push);
    }
    Vec3 bmin, bmax;
    if (inset_room(bmin, bmax)) {
      center.x = clampf(center.x, bmin.x, bmax.x);
      center.y = clampf(center.y, bmin.y, bmax.y);
      center.z = clampf(center.z, bmin.z, bmax.z);
    }
    return false;
  }

  // Place BB center on ray hit, slightly toward start so we sit on the free side.
  const float back = std::min(0.002f / dlen, best_t);
  const float t_place = std::max(0.f, best_t - back);
  center = Vec3{origin.x + delta.x * t_place, origin.y + delta.y * t_place,
                origin.z + delta.z * t_place};

  // Corner: nudge out of any second overlapping collider without abandoning the ray hit.
  for (int pass = 0; pass < 4; ++pass) {
    bool any = false;
    for (const auto& box : enclosure_boxes_) {
      Vec3 push{};
      if (depenetrate_aabb_vs_obb(center, half, box, push)) {
        any = true;
      }
    }
    Vec3 bmin, bmax;
    if (inset_room(bmin, bmax)) {
      const float cx = center.x;
      const float cy = center.y;
      const float cz = center.z;
      center.x = clampf(center.x, bmin.x, bmax.x);
      center.y = clampf(center.y, bmin.y, bmax.y);
      center.z = clampf(center.z, bmin.z, bmax.z);
      if (center.x != cx || center.y != cy || center.z != cz) {
        any = true;
      }
    }
    if (!any) {
      break;
    }
  }

  // Contact normal (into room). Horizontalize for walls so slide keeps full vertical motion;
  // floor/ceiling keep the full normal so tangential XZ slide works in ghost drag.
  const float nlen = std::sqrt(best_n.x * best_n.x + best_n.y * best_n.y + best_n.z * best_n.z);
  if (nlen < 1e-6f) {
    return false;
  }
  Vec3 n{best_n.x / nlen, best_n.y / nlen, best_n.z / nlen};
  const float horiz = std::sqrt(n.x * n.x + n.z * n.z);
  if (horiz >= 0.35f) {
    n = Vec3{n.x / horiz, 0.f, n.z / horiz};
  }
  if (out_wall_into_room != nullptr) {
    *out_wall_into_room = n;
  }
  return true;
}

VoodooVolumeEngine::PendingAgentOpen VoodooVolumeEngine::consume_pending_agent_open() {
  const PendingAgentOpen out = pending_agent_;
  pending_agent_ = {};
  return out;
}

void VoodooVolumeEngine::clear_hand_session(uint32_t hand) {
  if (hand >= 2u) {
    return;
  }
  hands_[hand] = HandRuntime{};
  hands_[hand].session.hand = hand;
}

void VoodooVolumeEngine::cancel_drag(uint32_t hand) {
  const EntityId snap = hands_[hand].session.snapped_centroid_id;
  voodoo_trace_log("cancel_drag hand=%u snap=%llu hold=%.3f", hand,
                   static_cast<unsigned long long>(snap), hands_[hand].drag_hold_s_);
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
  cut_.disarm_portal(portal_id);
  portals_.erase(it, portals_.end());
  return true;
}

uint64_t VoodooVolumeEngine::dismiss_latest_for_hand(uint32_t hand) {
  if (hand > 1u || portals_.empty()) {
    return 0;
  }
  // Prefer highest id among portals created by this hand (most recent park).
  uint64_t best_id = 0;
  for (const auto& p : portals_) {
    if (p.creating_hand == hand && p.id >= best_id) {
      best_id = p.id;
    }
  }
  if (best_id == 0) {
    return 0;
  }
  return dismiss_portal(best_id) ? best_id : 0;
}

uint32_t VoodooVolumeEngine::dismiss_all_portals() {
  if (portals_.empty()) {
    return 0;
  }
  const uint32_t n = static_cast<uint32_t>(portals_.size());
  for (uint32_t h = 0; h < 2u; ++h) {
    if (ghost_drags_[h].active) {
      ghost_drags_[h] = {};
    }
  }
  cut_.cancel();
  portals_.clear();
  return n;
}

uint32_t VoodooVolumeEngine::dismiss_portals_for_target(EntityId target_id,
                                                        CentroidRegistry& centroids) {
  if (target_id == 0) {
    return 0;
  }
  std::vector<uint64_t> remove_ids;
  std::vector<EntityId> temps;
  for (const auto& p : portals_) {
    if (p.singleton_target_id == target_id || p.persistent_centroid_id == target_id ||
        p.far_bb.linked_centroid_id == target_id || p.near_bb.linked_centroid_id == target_id) {
      remove_ids.push_back(p.id);
      if (p.temporary_centroid_id != 0) {
        temps.push_back(p.temporary_centroid_id);
      }
    }
  }
  uint32_t n = 0;
  for (uint64_t pid : remove_ids) {
    if (dismiss_portal(pid)) {
      ++n;
    }
  }
  for (EntityId temp : temps) {
    centroids.clear_temporary(temp);
  }
  return n;
}


uint64_t VoodooVolumeEngine::cut_portal_id() const { return cut_.portal_id(); }
bool VoodooVolumeEngine::cut_is_armed(uint64_t portal_id) const { return cut_.is_armed(portal_id); }
bool VoodooVolumeEngine::cut_arm(uint64_t portal_id) { return cut_.arm(portal_id); }
void VoodooVolumeEngine::cut_cancel() { cut_.cancel(); }
bool VoodooVolumeEngine::cut_active() const { return cut_.active(); }
std::vector<EntityId> VoodooVolumeEngine::cut_entity_ids() const { return cut_.entity_ids(); }

bool VoodooVolumeEngine::place_items_at_pinch(VoodooPortal* home, const std::vector<PlaceItem>& items,
                                              const Vec3& pinch_world, Workspace& workspace,
                                              bool cluster_open_air, bool cube_matrix) {
  if (items.empty()) {
    return false;
  }
  const uint64_t home_id = home != nullptr ? home->id : 0;
  if (home != nullptr &&
      point_in_obb(pinch_world, home->near_bb.pose, home->near_bb.extents)) {
    // Pinch inside source near BB is cancel / deselect (host); refuse here.
    return false;
  }

  const uint64_t pinch_portal = portal_at_near_point(pinch_world);
  VoodooPortal* target = nullptr;
  if (pinch_portal != 0 && pinch_portal != home_id) {
    target = portal_mut(pinch_portal);
  }

  bool any = false;
  if (target != nullptr) {
    // Cross-portal: cluster relative to near-pose centroid, billboarded into target.
    Vec3 near_centroid{};
    for (const auto& it : items) {
      near_centroid.x += it.near_pose.position.x;
      near_centroid.y += it.near_pose.position.y;
      near_centroid.z += it.near_pose.position.z;
    }
    const float n = static_cast<float>(items.size());
    near_centroid.x /= n;
    near_centroid.y /= n;
    near_centroid.z /= n;
    const Quat src_yaw =
        home != nullptr ? home->near_bb.pose.orientation : target->near_bb.pose.orientation;
    const Quat dest_yaw = target->near_bb.pose.orientation;

    // Collision-avoidance convention: inside a voodoo volume the FAR field is the position and
    // orientation ground truth. Direct user input may drive the ghost (that is the point of the
    // ghost), but any other force, like this paste, must update far first, resolve overlaps in
    // far space, and let the near ghost copy the result. (Bump-lerping moves the voodoo volumes
    // themselves in the near field; it never decides where content sits inside one.) So resolve
    // overlaps in far space, let the far BB grow to the resolved positions, then refit the near
    // BB. Clamping to the current near BB instead would collapse a large cluster onto its faces.
    const float scale = content_scale_for_portal(*target);
    std::vector<Pose> far_poses;
    far_poses.reserve(items.size());
    for (const auto& it : items) {
      const Vec3 d{it.near_pose.position.x - near_centroid.x,
                   it.near_pose.position.y - near_centroid.y,
                   it.near_pose.position.z - near_centroid.z};
      const Vec3 local = quat_rotate_yaw(quat_inverse_yaw(src_yaw), d);
      const Vec3 world_off = quat_rotate_yaw(dest_yaw, local);
      Pose near_pose{};
      near_pose.position = Vec3{pinch_world.x + world_off.x, pinch_world.y + world_off.y,
                                pinch_world.z + world_off.z};
      near_pose.orientation = dest_yaw;
      far_poses.push_back(map_near_to_far_pose(*target, near_pose, scale));
    }

    std::vector<Vec3> fixed_far;
    for (const auto& g : target->ghosts) {
      if (g.is_parent_box || g.entity_id == 0) {
        continue;
      }
      bool moving = false;
      for (const auto& it : items) {
        if (it.id == g.entity_id) {
          moving = true;
          break;
        }
      }
      if (!moving) {
        fixed_far.push_back(g.far_pose.position);
      }
    }

    const float min_gap = 2.f * kGlyphHalfM + kNearClearGapM;
    for (int iter = 0; iter < 32; ++iter) {
      bool moved = false;
      for (size_t i = 0; i < far_poses.size(); ++i) {
        for (size_t j = i + 1; j < far_poses.size(); ++j) {
          moved |= separate_pair(far_poses[i].position, &far_poses[j].position, min_gap, i + j);
        }
        for (const Vec3& f : fixed_far) {
          Vec3 fixed = f;
          moved |= separate_pair(far_poses[i].position, nullptr, min_gap, i, &fixed);
        }
      }
      if (!moved) {
        break;
      }
    }

    for (size_t i = 0; i < items.size(); ++i) {
      const auto& it = items[i];
      strip_content_ghost_everywhere(it.id);
      remove_ghost_from_other_portals(target->id, it.id);
      VoodooGhost ng;
      ng.entity_id = it.id;
      ng.is_parent_box = false;
      ng.scale = scale;
      ng.far_pose = far_poses[i];
      target->ghosts.push_back(ng);
      begin_pose_lerp(it.id, far_poses[i], workspace);
      any = true;
    }
    // Settle: far BB expands to the real positions, then near BB rescales to fit.
    refit_far_bb_to_content_ghosts(*target, workspace, /*settle_bump=*/true);
  } else if (cube_matrix && items.size() > 1) {
    // Multi-summon: same cubic matrix as box expand (2³ / 3³ / …), billboarded at pinch.
    const int n = static_cast<int>(items.size());
    int side = 2;
    if (n > 8) {
      side = 3;
      while (side * side * side < n) {
        ++side;
      }
    }
    const float spacing = workspace.field_spacing_m();
    const float half = 0.5f * static_cast<float>(side - 1) * spacing;
    const Quat dest_yaw = yaw_billboard_toward(pinch_world, last_head_, last_head_valid_);
    float cluster_r = kGlyphHalfM;
    for (int i = 0; i < n; ++i) {
      const int ix = i % side;
      const int iy = (i / side) % side;
      const int iz = i / (side * side);
      const Vec3 local{static_cast<float>(ix) * spacing - half,
                       static_cast<float>(iy) * spacing - half,
                       static_cast<float>(iz) * spacing - half};
      const Vec3 world_off = quat_rotate_yaw(dest_yaw, local);
      const float horiz = std::sqrt(local.x * local.x + local.z * local.z);
      cluster_r = std::max(cluster_r, horiz + kGlyphHalfM);

      const auto& it = items[static_cast<size_t>(i)];
      Pose dest{};
      dest.position = Vec3{pinch_world.x + world_off.x, pinch_world.y + world_off.y,
                           pinch_world.z + world_off.z};
      dest.orientation = it.far_pose.orientation;
      strip_content_ghost_everywhere(it.id);
      begin_pose_lerp(it.id, dest, workspace);
      any = true;
    }

    VoodooPortal synth{};
    synth.near_bb.open = true;
    synth.near_bb.pose.position = pinch_world;
    synth.near_bb.pose.orientation = dest_yaw;
    const float ext = std::max(cluster_r * 2.f, near_extents_m().x);
    synth.near_bb.extents = Vec3{ext, near_extents_m().y, ext};
    bump_near_field_clutter(synth, workspace, last_head_, last_head_valid_, /*animate=*/true);
  } else if (!cluster_open_air) {
    // Summon open-air / single release: strip ghost and lerp real to pinch.
    for (const auto& it : items) {
      Pose dest{};
      dest.position = pinch_world;
      dest.orientation = it.far_pose.orientation;
      strip_content_ghost_everywhere(it.id);
      begin_pose_lerp(it.id, dest, workspace);
      any = true;
    }
  } else {
    // Cut open-air: far-pose centroid cluster, billboarded to user; orbit/bump footprint.
    Vec3 far_centroid{};
    for (const auto& it : items) {
      far_centroid.x += it.far_pose.position.x;
      far_centroid.y += it.far_pose.position.y;
      far_centroid.z += it.far_pose.position.z;
    }
    const float n = static_cast<float>(items.size());
    far_centroid.x /= n;
    far_centroid.y /= n;
    far_centroid.z /= n;
    const Quat src_yaw =
        home != nullptr ? home->far_bb.pose.orientation : Quat{0.f, 0.f, 0.f, 1.f};

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
      const float src_y = src_yaw.y;
      const float src_w = src_yaw.w;
      const float dst_y = dest_yaw.y;
      const float dst_w = dest_yaw.w;
      const float inv_y = -src_y;
      const float inv_w = src_w;
      const float dy = dst_w * inv_y + dst_y * inv_w;
      const float dw = dst_w * inv_w - dst_y * inv_y;
      const float ey = it.far_pose.orientation.y;
      const float ew = it.far_pose.orientation.w;
      dest.orientation = Quat{0.f, dw * ey + dy * ew, 0.f, dw * ew - dy * ey};

      strip_content_ghost_everywhere(it.id);
      begin_pose_lerp(it.id, dest, workspace);
      any = true;
    }

    VoodooPortal synth{};
    synth.near_bb.open = true;
    synth.near_bb.pose.position = dest_centroid;
    synth.near_bb.pose.orientation = dest_yaw;
    const float ext = std::max(cluster_r * 2.f, near_extents_m().x);
    synth.near_bb.extents = Vec3{ext, near_extents_m().y, ext};
    bump_near_field_clutter(synth, workspace, last_head_, last_head_valid_, /*animate=*/true);
  }

  if (home != nullptr) {
    erase_all_content_ghosts(*home);
  }
  return any;
}

bool VoodooVolumeEngine::cut_paste_at(const Vec3& pinch_world, Workspace& workspace) {
  return cut_.paste_at(pinch_world, workspace);
}

bool VoodooVolumeEngine::cut_paste_into(EntityId box_id, Workspace& workspace) {
  return cut_.paste_into(box_id, workspace);
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

bool VoodooVolumeEngine::snap_drag_co_target_allowed(uint32_t hand, const Pose& hand_pose,
                                                     const Pose& head, bool head_valid,
                                                     const CentroidRegistry& centroids) const {
  if (hand > 1u) {
    return true;
  }
  const uint32_t sib = hand ^ 1u;
  const auto& oh = hands_[sib];
  if (!oh.session.open || oh.session.phase != VoodooSessionPhase::Dragging ||
      oh.session.mode != VoodooMode::Snap || !oh.have_start_hand_) {
    return true;
  }
  EntityId sib_target = oh.resolved_snap_centroid_id_;
  if (sib_target == 0) {
    sib_target = oh.session.snapped_centroid_id;
  }
  if (sib_target == 0) {
    return true;
  }
  auto& mut = *const_cast<VoodooVolumeEngine*>(this);
  auto& hr = mut.hands_[hand];
  const bool had_start = hr.have_start_hand_;
  const Pose saved_start = hr.start_hand_;
  const Pose saved_rest = hr.rest_far_;
  hr.rest_far_ = far_rest_pose(head, head_valid);
  hr.start_hand_ = hand_pose;
  hr.have_start_hand_ = true;
  const Vec3 stick{};
  const SnapPick primary = mut.snap_pick_best(hand, stick, 0, centroids);
  const SnapPick alt =
      (primary.centroid_id != 0 && primary.centroid_id == sib_target)
          ? mut.snap_pick_best(hand, stick, sib_target, centroids)
          : SnapPick{};
  hr.have_start_hand_ = had_start;
  hr.start_hand_ = saved_start;
  hr.rest_far_ = saved_rest;
  if (primary.centroid_id == 0 || primary.centroid_id != sib_target) {
    return true;
  }
  return alt.centroid_id != 0;
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
  hr.release_arm_s_ = 0.f;
  hr.park_pinch_latched_ = false;
  hr.parked_portal_id_ = 0;
  update_arrow(hand, hand_pose, head, head_valid, nullptr);
}

Vec3 VoodooVolumeEngine::near_bb_bottom_center(const VoodooBoundingBox& bb) {
  // Center-bottom-center of the near OBB (parent-node / temp centroid seat).
  return Vec3{bb.pose.position.x, bb.pose.position.y - bb.extents.y * 0.5f, bb.pose.position.z};
}

void VoodooVolumeEngine::refresh_ghost_near_poses(VoodooPortal& portal) const {
  PortalSpace::refresh_ghost_near_poses(portal);
}

void VoodooVolumeEngine::rebuild_ghosts(VoodooPortal& portal, const CentroidRegistry& centroids,
                                        const Workspace& workspace) {
  portal.ghosts.clear();
  if (portal.lock_scale < 1e-4f) {
    portal.lock_scale = content_scale_for_portal(portal);
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
        const bool match = (c->kind == CentroidKind::MindPalace)
                               ? (e.parent_box == 0 && e.mind_palace_id == c->id)
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

void VoodooVolumeEngine::park(uint32_t hand, const Pose& hand_pose, bool hand_valid,
                              const Pose& head, bool head_valid, CentroidRegistry& centroids,
                              Workspace& workspace) {
  auto& hr = hands_[hand];
  if (!hr.session.open || hr.session.phase != VoodooSessionPhase::Dragging) {
    voodoo_trace_log("park_skip hand=%u open=%d phase=%u", hand, hr.session.open ? 1 : 0,
                     static_cast<unsigned>(hr.session.phase));
    return;
  }
  if (!creation_allowed()) {
    // Hard portal cap: cancel both BBs for this gesture.
    clear_hand_session(hand);
    return;
  }

  EntityId singleton_target = hr.session.snapped_centroid_id;
  if (singleton_target == 0) {
    singleton_target = hr.session.far_bb.linked_centroid_id;
  }
  if (singleton_target != 0) {
    (void)dismiss_portals_for_target(singleton_target, centroids);
  }

  const Pose seed = near_rest_pose(head, head_valid);
  // QA 346/352: after the first dual-hand park, the other hand often releases inside the
  // sibling near BB — hand-coupled orbit stacks on the same group and bump_resolve rolls back.
  Pose near_target{};
  if (point_in_any_near_bb(hand_pose.position)) {
    near_target = seed;
    near_target.orientation = yaw_billboard_toward(near_target.position, head, head_valid);
  } else {
    near_target = orbit_near_to_hand(seed, head, head_valid, hand_pose, hand_valid, hand);
  }

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
    // Different snap targets (Demo vs Demo2): do not merge — independent orbit seats.
    if (singleton_target != 0 && portals_[i].singleton_target_id != 0 &&
        singleton_target != portals_[i].singleton_target_id) {
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
  portal.lock_scale = content_scale_for_portal(portal);
  portal.persistent_centroid_id = hr.session.snapped_centroid_id;
  portal.singleton_target_id = singleton_target;
  portal.temporary_centroid_id =
      centroids.register_temporary("voodoo_temp", near_bb_bottom_center(portal.near_bb));
  portal.bump_from = near_target.position;
  portal.bump_to = near_target.position;
  portal.bump_t = 1.f;

  rebuild_ghosts(portal, centroids, workspace);
  portals_.push_back(std::move(portal));
  voodoo_trace_log("park ok hand=%u snap=%llu portals=%zu gid=%llu", hand,
                   static_cast<unsigned long long>(hr.session.snapped_centroid_id), portals_.size(),
                   static_cast<unsigned long long>(gid));
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
  const size_t pinned = portals_.size() - 1;
  orbit_.yield_orbit_for_incoming_park(pinned, head, head_valid);
  if (portals_.size() >= 2) {
    if (!bump_resolve(head, head_valid, pinned)) {
      bump_siblings_min_clear(pinned);
      if (!bump_resolve(head, head_valid, pinned)) {
        // Orbit full or unresolved overlap — roll back this park (never leave stacked near BBs).
        const EntityId temp = portals_.back().temporary_centroid_id;
        portals_.pop_back();
        if (temp != 0) {
          centroids.clear_temporary(temp);
        }
        clear_hand_session(hand);
        return;
      }
    } else {
      // Independent snap targets (Demo / Demo2 / Demo3): tight XZ nudge, not full-orbit merge.
      bump_siblings_min_clear(pinned);
    }
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
  hr.release_arm_s_ = 0.f;
  // Same continuous pinch/mode must not reopen a new drag (would reset the hold gate).
  hr.park_pinch_latched_ = true;
  hr.park_mode_latched_ = hr.session.mode;
  hr.parked_portal_id_ = portals_.back().id;
  hr.session.near_bb = portals_.back().near_bb;
  hr.session.far_bb = portals_.back().far_bb;
  hr.session.far_bb.linked_centroid_id = hr.session.snapped_centroid_id;
}

bool VoodooVolumeEngine::solo_resolve(EntityId id, const Workspace& workspace,
                                      const CentroidRegistry& centroids, EntityId* out_drop_id,
                                      Vec3* out_far_pos, Vec3* out_far_extents) const {
  if (out_drop_id == nullptr || out_far_pos == nullptr || out_far_extents == nullptr || id == 0) {
    return false;
  }
  *out_drop_id = 0;
  *out_far_pos = {};
  *out_far_extents = {};

  auto apply_members = [&](EntityId cid, const Vec3& fallback) {
    *out_drop_id = cid;
    Vec3 mn{};
    Vec3 mx{};
    if (centroids.members_aabb(workspace, cid, &mn, &mx)) {
      *out_far_pos =
          Vec3{0.5f * (mn.x + mx.x), 0.5f * (mn.y + mx.y), 0.5f * (mn.z + mx.z)};
      *out_far_extents = fit_extents_for_aabb(mn, mx);
    } else {
      *out_far_pos = fallback;
      *out_far_extents = near_extents_m();
    }
  };

  if (const CentroidState* c = centroids.find(id); c != nullptr && c->id != 0) {
    if (c->kind == CentroidKind::FocusAnchor) {
      *out_drop_id = id;
      *out_far_pos = c->position;
      *out_far_extents = near_extents_m();
      return true;
    }
    if (c->kind == CentroidKind::MindPalace || c->kind == CentroidKind::Box) {
      apply_members(id, c->position);
      return true;
    }
  }

  const WorkspaceEntry* e = workspace.find(id);
  if (e == nullptr || !e->in_field) {
    return false;
  }

  const EntityId cid = centroids.accessible_centroid_id(workspace, id);
  if (cid != 0) {
    if (const CentroidState* c = centroids.find(cid); c != nullptr) {
      const float dx = e->pose.position.x - c->position.x;
      const float dy = e->pose.position.y - c->position.y;
      const float dz = e->pose.position.z - c->position.z;
      const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
      if (dist <= kSoloClusterLockM) {
        apply_members(cid, c->position);
        return true;
      }
    }
  }

  *out_drop_id = id;
  *out_far_pos = e->pose.position;
  *out_far_extents = near_extents_m();
  return true;
}

bool VoodooVolumeEngine::solo_drop_open(uint32_t hand, EntityId entity_id,
                                        const Pose& hand_pose, bool hand_valid, const Pose& head,
                                        bool head_valid, CentroidRegistry& centroids,
                                        Workspace& workspace) {
  if (hand >= 2u || !hand_valid || !creation_allowed() ||
      point_in_any_near_bb(hand_pose.position)) {
    return false;
  }

  EntityId drop_id = 0;
  Vec3 far_pos{};
  Vec3 far_ext{};
  if (!solo_resolve(entity_id, workspace, centroids, &drop_id, &far_pos, &far_ext)) {
    return false;
  }

  // NPC FocusAnchor: agent Multi-Choice (no empty near BB). Demo-manual for 0.8.1.
  if (const CentroidState* c = centroids.find(drop_id);
      c != nullptr && c->kind == CentroidKind::FocusAnchor) {
    pending_agent_.pending = true;
    pending_agent_.hand = hand;
    pending_agent_.focus_id = drop_id;
    return true;
  }

  EntityId snap_centroid = 0;
  VoodooMode mode = VoodooMode::FreeRange;
  if (const CentroidState* c = centroids.find(drop_id);
      c != nullptr && (c->kind == CentroidKind::MindPalace || c->kind == CentroidKind::Box)) {
    snap_centroid = drop_id;
    mode = VoodooMode::Snap;
  }

  last_head_ = head;
  last_head_valid_ = head_valid;

  auto& hr = hands_[hand];
  hr = HandRuntime{};
  hr.session.hand = hand;
  hr.session.mode = mode;
  hr.session.open = true;
  hr.session.phase = VoodooSessionPhase::Dragging;
  hr.session.snapped_centroid_id = snap_centroid;
  hr.session.far_bb.open = true;
  hr.session.far_bb.pose.position = far_pos;
  hr.session.far_bb.pose.orientation = yaw_billboard_toward(far_pos, head, head_valid);
  hr.session.far_bb.linked_centroid_id = drop_id;
  hr.session.far_bb.extents = far_ext;
  hr.session.far_bb.aspect_x = params_.near_bb_voxels_x;
  hr.session.far_bb.aspect_y = params_.near_bb_voxels_y;
  hr.session.far_bb.aspect_z = params_.near_bb_voxels_z;
  park(hand, hand_pose, hand_valid, head, head_valid, centroids, workspace);
  return hr.session.phase == VoodooSessionPhase::Parked;
}

void VoodooVolumeEngine::rebuild_snap_nodes(uint32_t hand, const CentroidRegistry& centroids) {
  auto& hr = hands_[hand];
  hr.session.snap_nodes.clear();
  if (!hr.have_start_hand_) {
    return;
  }
  // Snap mode: all live centroids except Temporary.
  // Free-range: FocusAnchor only (NPC head → agent widget; Task 7.3).
  if (hr.session.mode != VoodooMode::Snap && hr.session.mode != VoodooMode::FreeRange) {
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
    if (hr.session.mode == VoodooMode::Snap && c.kind == CentroidKind::FocusAnchor &&
        !focus_anchor_snap_in_snap_mode_) {
      continue;
    }
    if (hr.session.mode == VoodooMode::FreeRange && c.kind != CentroidKind::FocusAnchor) {
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

float VoodooVolumeEngine::snap_lateral_preference_score(uint32_t hand, float hand_offset_x) {
  // Stick-space X: left hand should own left-side centroids (negative offset), right hand positive.
  return hand == 0u ? -hand_offset_x : hand_offset_x;
}

VoodooVolumeEngine::SnapPick VoodooVolumeEngine::snap_pick_best(uint32_t hand, const Vec3& stick,
                                                                EntityId exclude_centroid,
                                                                const CentroidRegistry& centroids) {
  SnapPick out{};
  auto& hr = hands_[hand];
  if (!hr.session.far_bb.open || !hr.have_start_hand_) {
    return out;
  }
  rebuild_snap_nodes(hand, centroids);
  const float pull_r = kSnapNodeRadiusM * kSnapPullMul;
  for (const auto& n : hr.session.snap_nodes) {
    if (exclude_centroid != 0 && n.centroid_id == exclude_centroid) {
      continue;
    }
    const float d = dist3(stick, n.hand_offset);
    if (d >= pull_r || d >= out.best_d) {
      continue;
    }
    out.best_d = d;
    out.centroid_id = n.centroid_id;
    out.hand_offset_x = n.hand_offset.x;
  }
  return out;
}

void VoodooVolumeEngine::refresh_dual_snap_claims(const Pose hand_pose[2], const bool hand_ok[2],
                                                  const Pose& head, bool head_valid,
                                                  const CentroidRegistry& centroids) {
  (void)head;
  (void)head_valid;
  hands_[0].resolved_snap_centroid_id_ = 0;
  hands_[1].resolved_snap_centroid_id_ = 0;

  auto dragging_snap = [&](uint32_t h) -> bool {
    const auto& s = hands_[h].session;
    return hand_ok[h] && s.open && s.phase == VoodooSessionPhase::Dragging &&
           s.mode == VoodooMode::Snap && s.far_bb.open && hands_[h].have_start_hand_;
  };

  const bool d0 = dragging_snap(0);
  const bool d1 = dragging_snap(1);
  if (!d0 && !d1) {
    return;
  }

  auto stick_for = [&](uint32_t h) -> Vec3 {
    const auto& hr = hands_[h];
    return Vec3{hand_pose[h].position.x - hr.start_hand_.position.x,
                hand_pose[h].position.y - hr.start_hand_.position.y,
                hand_pose[h].position.z - hr.start_hand_.position.z};
  };

  SnapPick p0{};
  SnapPick p1{};
  if (d0) {
    p0 = snap_pick_best(0, stick_for(0), 0, centroids);
  }
  if (d1) {
    p1 = snap_pick_best(1, stick_for(1), 0, centroids);
  }

  if (p0.centroid_id != 0 && p0.centroid_id == p1.centroid_id) {
    const float lat0 = snap_lateral_preference_score(0, p0.hand_offset_x);
    const float lat1 = snap_lateral_preference_score(1, p1.hand_offset_x);
    if (lat0 >= lat1) {
      hands_[0].resolved_snap_centroid_id_ = p0.centroid_id;
      p1 = snap_pick_best(1, stick_for(1), p0.centroid_id, centroids);
      hands_[1].resolved_snap_centroid_id_ = p1.centroid_id;
      voodoo_trace_log("snap_claim shared c=%llu L=%.3f R=%.3f -> L owns",
                       static_cast<unsigned long long>(p0.centroid_id), lat0, lat1);
    } else {
      hands_[1].resolved_snap_centroid_id_ = p1.centroid_id;
      p0 = snap_pick_best(0, stick_for(0), p1.centroid_id, centroids);
      hands_[0].resolved_snap_centroid_id_ = p0.centroid_id;
      voodoo_trace_log("snap_claim shared c=%llu L=%.3f R=%.3f -> R owns",
                       static_cast<unsigned long long>(p1.centroid_id), lat0, lat1);
    }
  } else {
    hands_[0].resolved_snap_centroid_id_ = p0.centroid_id;
    hands_[1].resolved_snap_centroid_id_ = p1.centroid_id;
  }

  // Product rule: while both snap-drag, the right hand never shares the left hand's centroid.
  if (d0 && d1 && hands_[0].resolved_snap_centroid_id_ != 0) {
    p1 = snap_pick_best(1, stick_for(1), hands_[0].resolved_snap_centroid_id_, centroids);
    hands_[1].resolved_snap_centroid_id_ = p1.centroid_id;
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
  // Snap gravity for Snap (all centroids) and FreeRange (FocusAnchor-only nodes).
  if (hr.session.mode == VoodooMode::Snap || hr.session.mode == VoodooMode::FreeRange) {
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

    const float pull_r = kSnapNodeRadiusM * kSnapPullMul;
    EntityId snap_pick = hr.resolved_snap_centroid_id_;
    if (hr.session.mode == VoodooMode::FreeRange) {
      snap_pick = 0;
      float best_d = 1e9f;
      int best_i = -1;
      for (int i = 0; i < static_cast<int>(hr.session.snap_nodes.size()); ++i) {
        const float d = dist3(stick, hr.session.snap_nodes[static_cast<size_t>(i)].hand_offset);
        if (d < best_d) {
          best_d = d;
          best_i = i;
        }
      }
      if (best_i >= 0 && best_d < pull_r) {
        snap_pick = hr.session.snap_nodes[static_cast<size_t>(best_i)].centroid_id;
      }
    }
    if (snap_pick != 0) {
      int pick_i = -1;
      float pick_d = 1e9f;
      for (int i = 0; i < static_cast<int>(hr.session.snap_nodes.size()); ++i) {
        const auto& n = hr.session.snap_nodes[static_cast<size_t>(i)];
        if (n.centroid_id != snap_pick) {
          continue;
        }
        const float d = dist3(stick, n.hand_offset);
        if (d < pick_d) {
          pick_d = d;
          pick_i = i;
        }
      }
      if (pick_i >= 0 && pick_d < pull_r) {
        auto& n = hr.session.snap_nodes[static_cast<size_t>(pick_i)];
        if (pick_d <= kSnapNodeRadiusM) {
          stick = n.hand_offset;
          snapped = n.centroid_id;
          n.snapped = true;
        } else {
          const float t = 1.f - (pick_d - kSnapNodeRadiusM) / (pull_r - kSnapNodeRadiusM);
          const float u = clampf(t * t, 0.f, 1.f);
          stick.x += (n.hand_offset.x - stick.x) * u;
          stick.y += (n.hand_offset.y - stick.y) * u;
          stick.z += (n.hand_offset.z - stick.z) * u;
        }
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

  // Always compute ideal isometric target first. If past WorldMesh, raycast from
  // drag-start far BB (near hand) → ideal and center on that hit (+ corner nudge).
  Vec3 wall_into_room{};
  const Vec3 ray_start = hr.rest_far_.position;
  const bool on_wall =
      constrain_far_center(target, target_ext, head, head_valid, &wall_into_room, &ray_start);

  const float dt = clampf(dt_seconds, 1e-4f, 0.1f);
  if (!hr.have_far_smoothed_) {
    hr.far_smoothed_ = target;
    hr.extents_smoothed_ = target_ext;
    hr.have_far_smoothed_ = true;
  } else {
    // Snap faster onto wall hits so the BB tracks the raycast surface cleanly.
    const float tau = (snapped != 0 || on_wall) ? 0.02f : kFarDampTauS;
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
  if (on_wall) {
    // Face into the room using the raycast hit normal (clean along the whole wall).
    Pose aim{};
    aim.position = Vec3{hr.far_smoothed_.x + wall_into_room.x, hr.far_smoothed_.y,
                        hr.far_smoothed_.z + wall_into_room.z};
    hr.session.far_bb.pose.orientation = yaw_billboard_toward(hr.far_smoothed_, aim, true);
  } else {
    hr.session.far_bb.pose.orientation =
        yaw_billboard_toward(hr.far_smoothed_, head, head_valid);
  }
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
  const Vec3 origin = stick_viz_origin(hr.start_hand_.position, head, head_valid);
  arm_deadzone(hr.deadzone_armed_, hr.deadzone_left_once_, mag, deadzone_m_);
  fill_drag_arrow(hr.session.arrow, origin, stick, mag, hr.deadzone_armed_, deadzone_m_);

  for (auto& n : hr.session.snap_nodes) {
    n.world_position =
        Vec3{origin.x + n.hand_offset.x, origin.y + n.hand_offset.y, origin.z + n.hand_offset.z};
    n.snapped = (hr.session.snapped_centroid_id == n.centroid_id);
  }
}

bool VoodooVolumeEngine::dragging_mature(uint32_t hand) const {
  if (hand >= 2u) {
    return false;
  }
  const auto& hr = hands_[hand];
  return hr.session.open && hr.session.phase == VoodooSessionPhase::Dragging &&
         hr.drag_hold_s_ >= kMinHoldToParkS;
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
    if (!want_any) {
      // Pinch fully released after park — next press may open a new drag.
      hr.park_pinch_latched_ = false;
      return;
    }
    if (!hand_valid) {
      return;
    }
    const VoodooMode mode = want_snap ? VoodooMode::Snap : VoodooMode::FreeRange;
    if (hr.park_pinch_latched_ && mode == hr.park_mode_latched_) {
      // Still holding the same-mode pinch that parked — do not reopen.
      return;
    }
    hr.park_pinch_latched_ = false;
    // Keep portals; start a new drag on this hand (refused if inside a near BB).
    clear_hand_session(hand);
    if (mode == VoodooMode::Snap &&
        !snap_drag_co_target_allowed(hand, hand_pose, head, head_valid, centroids)) {
      return;
    }
    open_drag(hand, mode, hand_pose, head, head_valid);
    if (hr.session.open) {
      hr.drag_hold_s_ += std::max(0.f, dt_seconds);
      update_far_while_dragging(hand, hand_pose, head, head_valid, dt_seconds, centroids,
                                workspace);
    }
    return;
  }

  if (hr.session.phase != VoodooSessionPhase::Dragging) {
    if (!want_any || !hand_valid) {
      return;
    }
    const VoodooMode mode = want_snap ? VoodooMode::Snap : VoodooMode::FreeRange;
    if (mode == VoodooMode::Snap &&
        !snap_drag_co_target_allowed(hand, hand_pose, head, head_valid, centroids)) {
      return;
    }
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
    // Quest dual-hand tracking often drops primary for a frame while the other hand
    // moves between snap nodes. Debounce only when the sibling is still actively holding
    // (release_arm_s_ == 0). If both release together, skip debounce so parks stay snappy.
    const auto& other = hands_[hand ^ 1u];
    const bool sibling_still_holding =
        other.session.open && other.session.phase == VoodooSessionPhase::Dragging &&
        other.release_arm_s_ <= 0.f;
    if (sibling_still_holding) {
      hr.release_arm_s_ += std::max(0.f, dt_seconds);
      if (hr.release_arm_s_ < kReleaseDebounceS) {
        update_far_while_dragging(hand, hand_pose, head, head_valid, dt_seconds, centroids,
                                  workspace);
        return;
      }
    } else {
      hr.release_arm_s_ = kReleaseDebounceS;  // armed — park/cancel this frame
    }
    // Accidental taps: hold < 0.25 s cancels far+near for this gesture.
    if (hr.drag_hold_s_ < kMinHoldToParkS) {
      voodoo_trace_log("release_cancel hand=%u hold=%.3f deb=%.3f sib=%d", hand, hr.drag_hold_s_,
                       hr.release_arm_s_, sibling_still_holding ? 1 : 0);
      cancel_drag(hand);
    } else {
      voodoo_trace_log("release_park hand=%u snap=%llu hold=%.3f deb=%.3f", hand,
                       static_cast<unsigned long long>(hr.session.snapped_centroid_id),
                       hr.drag_hold_s_, hr.release_arm_s_);
      // Focus-anchor snap → agent Multi-Choice (no portal). Task 7.3.
      const EntityId snap_id = hr.session.snapped_centroid_id;
      if (snap_id != 0) {
        if (const CentroidState* c = centroids.find(snap_id);
            c != nullptr && c->kind == CentroidKind::FocusAnchor) {
          pending_agent_.pending = true;
          pending_agent_.hand = hand;
          pending_agent_.focus_id = snap_id;
          cancel_drag(hand);
          return;
        }
      }
      park(hand, hand_pose, hand_valid, head, head_valid, centroids, workspace);
    }
    return;
  }

  hr.release_arm_s_ = 0.f;
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
  const VoodooPortal* portal = nullptr;
  const VoodooGhost* ghost = nullptr;
  if (!find_ghost(entity_id, &portal, &ghost)) {
    if (out_portal != nullptr) {
      *out_portal = nullptr;
    }
    if (out_ghost != nullptr) {
      *out_ghost = nullptr;
    }
    return false;
  }
  if (out_portal != nullptr) {
    *out_portal = const_cast<VoodooPortal*>(portal);
  }
  if (out_ghost != nullptr) {
    *out_ghost = const_cast<VoodooGhost*>(ghost);
  }
  return true;
}

Pose VoodooVolumeEngine::map_far_to_near_pose(const VoodooPortal& portal, const Pose& far_pose,
                                              float scale) const {
  return PortalSpace::map_far_to_near(portal, far_pose, scale);
}

Pose VoodooVolumeEngine::map_near_to_far_pose(const VoodooPortal& portal, const Pose& near_pose,
                                              float scale) const {
  return PortalSpace::map_near_to_far(portal, near_pose, scale);
}

Vec3 VoodooVolumeEngine::clamp_point_to_near_obb(const VoodooPortal& portal, const Vec3& p) const {
  return PortalSpace::clamp_to_near_obb(portal, p);
}

void VoodooVolumeEngine::remove_ghost_from_other_portals(uint64_t keep_portal_id,
                                                        EntityId entity_id) {
  for (auto& portal : portals_) {
    if (portal.id == keep_portal_id) {
      continue;
    }
    erase_content_ghost(portal, entity_id);
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
  if (portal.lock_scale < 1e-4f) {
    portal.lock_scale = content_scale_for_portal(portal);
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

void VoodooVolumeEngine::refit_after_box_expand(EntityId box_id, Workspace& workspace) {
  const WorkspaceEntry* box = workspace.find(box_id);
  if (box == nullptr) {
    return;
  }

  VoodooPortal* portal = nullptr;
  const uint64_t pid = portal_for_ghost_entity(box_id);
  if (pid != 0) {
    portal = portal_mut(pid);
  }
  if (portal == nullptr) {
    for (auto& p : portals_) {
      if (!p.far_bb.open) {
        continue;
      }
      if (point_in_obb(box->pose.position, p.far_bb.pose, p.far_bb.extents)) {
        portal = &p;
        break;
      }
    }
  }
  if (portal == nullptr || !portal->far_bb.open) {
    return;
  }

  bool any = false;
  Vec3 mn{};
  Vec3 mx{};
  auto include_pt = [&](const Vec3& p) {
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
  };

  include_pt(box->pose.position);
  for (const auto& g : portal->ghosts) {
    if (!g.is_parent_box && g.entity_id != 0) {
      include_pt(g.far_pose.position);
    }
  }
  for (const auto& e : workspace.entries()) {
    if (e.parent_box == box_id && e.in_field) {
      include_pt(e.pose.position);
    }
  }
  if (!any) {
    return;
  }

  const float lock = portal->lock_scale > 1e-4f ? portal->lock_scale : 0.5f;
  const Vec3 fitted = fit_extents_for_aabb(mn, mx);
  portal->far_bb.pose.position =
      Vec3{0.5f * (mn.x + mx.x), 0.5f * (mn.y + mx.y), 0.5f * (mn.z + mx.z)};
  portal->far_bb.extents = Vec3{std::max(fitted.x, portal->far_bb.extents.x),
                                std::max(fitted.y, portal->far_bb.extents.y),
                                std::max(fitted.z, portal->far_bb.extents.z)};
  portal->far_bb.aspect_x = params_.near_bb_voxels_x;
  portal->far_bb.aspect_y = params_.near_bb_voxels_y;
  portal->far_bb.aspect_z = params_.near_bb_voxels_z;
  portal->near_bb.extents = Vec3{portal->far_bb.extents.x * lock, portal->far_bb.extents.y * lock,
                                 portal->far_bb.extents.z * lock};
  portal->near_bb.aspect_x = params_.near_bb_voxels_x;
  portal->near_bb.aspect_y = params_.near_bb_voxels_y;
  portal->near_bb.aspect_z = params_.near_bb_voxels_z;

  for (const auto& e : workspace.entries()) {
    if (e.parent_box == box_id && e.in_field) {
      (void)ensure_ghost_in_portal(*portal, e.id, workspace);
    }
  }
  for (auto& g : portal->ghosts) {
    if (!g.is_parent_box) {
      g.scale = lock;
    }
  }
  refresh_ghost_near_poses(*portal);

  size_t pinned = portals_.size();
  for (size_t i = 0; i < portals_.size(); ++i) {
    if (portals_[i].id == portal->id) {
      pinned = i;
      break;
    }
  }
  if (pinned < portals_.size()) {
    bump_siblings_min_clear(pinned);
    refresh_ghost_near_poses(portals_[pinned]);
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

bool VoodooVolumeEngine::pose_lerp_active(EntityId entity_id) const {
  return pose_lerps_.active(entity_id);
}

void VoodooVolumeEngine::begin_pose_lerp(EntityId entity_id, const Pose& to, Workspace& workspace) {
  pose_lerps_.begin(entity_id, to, workspace);
}

void VoodooVolumeEngine::tick_pose_lerps(float dt_seconds, Workspace& workspace) {
  pose_lerps_.tick(dt_seconds, workspace, [this](EntityId id, const Pose& p) {
    VoodooPortal* portal = nullptr;
    VoodooGhost* ghost = nullptr;
    if (find_ghost_mut(id, &portal, &ghost) && ghost != nullptr && portal != nullptr) {
      ghost->far_pose = p;
      refresh_ghost_near_poses(*portal);
    }
  });
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
  auto& lerps = pose_lerps_.states();
  lerps.erase(std::remove_if(lerps.begin(), lerps.end(),
                             [entity_id](const PoseLerpState& l) {
                               return l.active && l.entity_id == entity_id;
                             }),
              lerps.end());
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

  // Multi-select: summon the whole selection set when the tapped id is a member.
  // Multi open-air uses the box-contents cubic matrix + near-field bump.
  std::vector<EntityId> ids;
  if (workspace.is_selected(entity_id) && workspace.selection_ids().size() > 1) {
    ids = workspace.selection_ids();
  } else {
    ids.push_back(entity_id);
  }

  VoodooPortal* home = nullptr;
  VoodooGhost* primary_ghost = nullptr;
  const bool primary_has_ghost =
      find_ghost_mut(entity_id, &home, &primary_ghost) && home != nullptr &&
      primary_ghost != nullptr;
  const uint64_t pinch_portal = portal_at_near_point(pinch_world);

  // Insert: selected near-field real(s) + pinch inside a near BB → lerp to mapped far.
  if (!primary_has_ghost) {
    if (pinch_portal == 0) {
      std::vector<PlaceItem> items;
      items.reserve(ids.size());
      for (EntityId id : ids) {
        const WorkspaceEntry* e = workspace.find(id);
        if (e == nullptr || !e->in_field) {
          continue;
        }
        items.push_back(PlaceItem{id, e->pose, e->pose});
      }
      if (items.empty()) {
        return false;
      }
      const bool multi = items.size() > 1;
      return place_items_at_pinch(nullptr, items, pinch_world, workspace,
                                  /*cluster_open_air=*/false, /*cube_matrix=*/multi);
    }
    VoodooPortal* target = portal_mut(pinch_portal);
    if (target == nullptr) {
      return false;
    }
    bool any = false;
    for (EntityId id : ids) {
      if (!ensure_ghost_in_portal(*target, id, workspace)) {
        continue;
      }
      Pose near_pose{};
      near_pose.position = pinch_world;
      near_pose.orientation = target->near_bb.pose.orientation;
      any = move_ghost_to_portal(id, *target, near_pose, &workspace) || any;
    }
    return any;
  }

  std::vector<PlaceItem> items;
  items.reserve(ids.size());
  for (EntityId id : ids) {
    VoodooPortal* p = nullptr;
    VoodooGhost* g = nullptr;
    if (!find_ghost_mut(id, &p, &g) || g == nullptr) {
      continue;
    }
    // Same home portal only — cross-portal multi-summon is 0.8.2.
    if (home != nullptr && p != home) {
      continue;
    }
    PlaceItem item{id, g->far_pose, g->near_pose};
    if (const WorkspaceEntry* e = workspace.find(id)) {
      item.far_pose.orientation = e->pose.orientation;
    }
    items.push_back(item);
  }
  if (items.empty()) {
    return false;
  }
  const bool multi = items.size() > 1;
  return place_items_at_pinch(home, items, pinch_world, workspace, /*cluster_open_air=*/false,
                              /*cube_matrix=*/multi);
}


// --- component forwards (orbit / ghost) ---
Vec3 VoodooVolumeEngine::effective_portal_pos(const VoodooPortal& portal) {
  return VoodooOrbitPacker::effective_portal_pos(portal);
}
void VoodooVolumeEngine::set_portal_orbit_target(VoodooPortal& portal, const Vec3& to,
                                                 const Pose& head, bool head_valid) {
  orbit_.set_portal_orbit_target(portal, to, head, head_valid);
}
bool VoodooVolumeEngine::walk_orbit(const Vec3& body, const Vec3& fwd, const Vec3& right, float az0,
                                    float y, const std::function<bool(const Vec3&)>& pred,
                                    Vec3* out) const {
  return orbit_.walk_orbit(body, fwd, right, az0, y, pred, out);
}
float VoodooVolumeEngine::near_footprint_radius_m() const { return orbit_.near_footprint_radius_m(); }
float VoodooVolumeEngine::portal_near_footprint_radius(const VoodooBoundingBox& near_bb) {
  return VoodooOrbitPacker::portal_near_footprint_radius(near_bb);
}
uint32_t VoodooVolumeEngine::max_orbit_slots() const { return orbit_.max_orbit_slots(); }
float VoodooVolumeEngine::orbit_radius_at_az(float az) const { return orbit_.orbit_radius_at_az(az); }
Vec3 VoodooVolumeEngine::pos_on_orbit_ellipse(const Vec3& body, const Vec3& fwd, const Vec3& right,
                                              float az, float y) const {
  return orbit_.pos_on_orbit_ellipse(body, fwd, right, az, y);
}
float VoodooVolumeEngine::azimuth_on_orbit(const Vec3& body, const Vec3& fwd, const Vec3& right,
                                           const Vec3& p) const {
  return orbit_.azimuth_on_orbit(body, fwd, right, p);
}
bool VoodooVolumeEngine::near_targets_clear() const { return orbit_.near_targets_clear(); }
bool VoodooVolumeEngine::near_targets_clear_indices(const std::vector<size_t>& idxs) const {
  return orbit_.near_targets_clear_indices(idxs);
}
void VoodooVolumeEngine::merge_groups(uint64_t from_id, uint64_t into_id) {
  orbit_.merge_groups(from_id, into_id);
}
size_t VoodooVolumeEngine::find_overlap_portal(const VoodooBoundingBox& candidate,
                                               size_t skip_index) const {
  return orbit_.find_overlap_portal(candidate, skip_index);
}
bool VoodooVolumeEngine::group_is_full_circle(uint64_t group_id, const Pose& head,
                                              bool head_valid) const {
  return orbit_.group_is_full_circle(group_id, head, head_valid);
}
bool VoodooVolumeEngine::near_bbs_overlap_xz(const VoodooBoundingBox& a, const VoodooBoundingBox& b,
                                             float gap_m) {
  return VoodooOrbitPacker::near_bbs_overlap_xz(a, b, gap_m);
}
bool VoodooVolumeEngine::orbit_near_bbs_clear_of_paste(const Vec3& paste_c, float paste_r) {
  return orbit_.orbit_near_bbs_clear_of_paste(paste_c, paste_r);
}
Vec3 VoodooVolumeEngine::resolve_cut_paste_origin(const Vec3& pinch_world, float cluster_radius_m) {
  return orbit_.resolve_cut_paste_origin(pinch_world, cluster_radius_m);
}
Pose VoodooVolumeEngine::orbit_near_to_hand(const Pose& seed, const Pose& head, bool head_valid,
                                            const Pose& hand_pose, bool hand_valid,
                                            uint32_t hand) const {
  return orbit_.orbit_near_to_hand(seed, head, head_valid, hand_pose, hand_valid, hand);
}
bool VoodooVolumeEngine::bump_resolve(const Pose& head, bool head_valid, size_t pinned_index) {
  return orbit_.bump_resolve(head, head_valid, pinned_index);
}
void VoodooVolumeEngine::tick_portals(float dt_seconds, const Pose& head, bool head_valid) {
  orbit_.tick_portals(dt_seconds, head, head_valid);
}
void VoodooVolumeEngine::bump_near_field_clutter(const VoodooPortal& portal, Workspace& workspace,
                                                 const Pose& head, bool head_valid, bool animate) {
  orbit_.bump_near_field_clutter(portal, workspace, head, head_valid, animate);
}
void VoodooVolumeEngine::bump_siblings_min_clear(size_t pinned_index) {
  orbit_.bump_siblings_min_clear(pinned_index);
}

void VoodooVolumeEngine::update_ghost_drag_arrow(uint32_t hand, const Pose& hand_pose,
                                                 const Pose& head, bool head_valid,
                                                 const Vec3& stick) {
  ghost_drag_.update_arrow(hand, hand_pose, head, head_valid, stick);
}
void VoodooVolumeEngine::refresh_ghost_drag_arrows(uint32_t hand, const Pose& hand_pose,
                                                   const Pose& head, bool head_valid) {
  ghost_drag_.refresh_arrows(hand, hand_pose, head, head_valid);
}
void VoodooVolumeEngine::audition_enter_portal(GhostDragState& gd, VoodooPortal& portal,
                                               const Pose& hand_pose, Workspace& workspace) {
  ghost_drag_.audition_enter_portal(gd, portal, hand_pose, workspace);
}
void VoodooVolumeEngine::audition_exit_portal(GhostDragState& gd, const Pose& hand_pose,
                                              Workspace& workspace) {
  ghost_drag_.audition_exit_portal(gd, hand_pose, workspace);
}
void VoodooVolumeEngine::audition_apply_matrix_snap(GhostDragState& gd, const Pose& hand_pose,
                                                    Workspace& workspace) {
  ghost_drag_.audition_apply_matrix_snap(gd, hand_pose, workspace);
}
bool VoodooVolumeEngine::ghost_drag_begin(uint32_t hand, EntityId entity_id, const Pose& hand_pose,
                                          Workspace& workspace) {
  return ghost_drag_.begin(hand, entity_id, hand_pose, workspace);
}
void VoodooVolumeEngine::ghost_drag_tick(uint32_t hand, const Pose& hand_pose, bool hand_valid,
                                         const Pose& head, bool head_valid, float dt_seconds,
                                         Workspace& workspace) {
  ghost_drag_.tick(hand, hand_pose, hand_valid, head, head_valid, dt_seconds, workspace);
}
void VoodooVolumeEngine::ghost_drag_end(uint32_t hand, bool release_inside_near_bb,
                                        Workspace& workspace) {
  ghost_drag_.end(hand, release_inside_near_bb, workspace);
}
bool VoodooVolumeEngine::ghost_drag_active(uint32_t hand) const { return ghost_drag_.active(hand); }
EntityId VoodooVolumeEngine::ghost_drag_entity(uint32_t hand) const {
  return ghost_drag_.entity(hand);
}
GhostDragKind VoodooVolumeEngine::ghost_drag_kind(uint32_t hand) const {
  return ghost_drag_.kind(hand);
}
const VoodooDragArrow& VoodooVolumeEngine::ghost_drag_arrow(uint32_t hand) const {
  return ghost_drag_.arrow(hand);
}
const VoodooDragArrow& VoodooVolumeEngine::ghost_drag_far_arrow(uint32_t hand) const {
  return ghost_drag_.far_arrow(hand);
}
bool VoodooVolumeEngine::apply_near_ghost_snap_hold(EntityId object_id, EntityId box_id,
                                                    float hold_above_m, Workspace& workspace) {
  return ghost_drag_.apply_near_ghost_snap_hold(object_id, box_id, hold_above_m, workspace);
}
void VoodooVolumeEngine::update_ghost_matrix_snap(uint32_t hand, Workspace& workspace) {
  ghost_drag_.update_matrix_snap(hand, workspace);
}

}  // namespace threeos

