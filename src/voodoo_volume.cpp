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
  if (cut_.portal_id() == portal_id) { cut_.cancel(); }
  portals_.erase(it, portals_.end());
  return true;
}

uint64_t VoodooVolumeEngine::cut_portal_id() const { return cut_.portal_id(); }
bool VoodooVolumeEngine::cut_arm(uint64_t portal_id) { return cut_.arm(portal_id); }
void VoodooVolumeEngine::cut_cancel() { cut_.cancel(); }
bool VoodooVolumeEngine::cut_active() const { return cut_.active(); }
std::vector<EntityId> VoodooVolumeEngine::cut_entity_ids() const { return cut_.entity_ids(); }

bool VoodooVolumeEngine::place_items_at_pinch(VoodooPortal* home, const std::vector<PlaceItem>& items,
                                              const Vec3& pinch_world, Workspace& workspace,
                                              bool cluster_open_air) {
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
      if (move_ghost_to_portal(it.id, *target, near_pose, &workspace)) {
        any = true;
      }
    }
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
  portal.lock_scale = content_scale_for_portal(portal);
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
  const Vec3 origin = stick_viz_origin(hr.start_hand_.position, head, head_valid);
  arm_deadzone(hr.deadzone_armed_, hr.deadzone_left_once_, mag, deadzone_m_);
  fill_drag_arrow(hr.session.arrow, origin, stick, mag, hr.deadzone_armed_, deadzone_m_);

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
      PlaceItem item{entity_id, e->pose, e->pose};
      return place_items_at_pinch(nullptr, {item}, pinch_world, workspace,
                                  /*cluster_open_air=*/false);
    }
    VoodooPortal* target = portal_mut(pinch_portal);
    if (target == nullptr || !ensure_ghost_in_portal(*target, entity_id, workspace)) {
      return false;
    }
    Pose near_pose{};
    near_pose.position = pinch_world;
    near_pose.orientation = target->near_bb.pose.orientation;
    return move_ghost_to_portal(entity_id, *target, near_pose, &workspace);
  }

  PlaceItem item{entity_id, ghost->far_pose, ghost->near_pose};
  if (const WorkspaceEntry* e = workspace.find(entity_id)) {
    item.far_pose.orientation = e->pose.orientation;
  }
  return place_items_at_pinch(home, {item}, pinch_world, workspace, /*cluster_open_air=*/false);
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

