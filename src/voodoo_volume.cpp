#include "core/voodoo_volume.hpp"

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

}  // namespace

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
  return true;
}

void VoodooVolumeEngine::reset() {
  params_ = VoodooFieldParams{};
  dismiss(0);
  dismiss(1);
}

void VoodooVolumeEngine::dismiss(uint32_t hand) {
  if (hand >= 2u) {
    return;
  }
  hands_[hand] = HandRuntime{};
  hands_[hand].session.hand = hand;
}

void VoodooVolumeEngine::cancel_drag(uint32_t hand) {
  if (hand >= 2u) {
    return;
  }
  dismiss(hand);
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
  hr.session.mode = mode;
  hr.session.open = true;
  hr.session.hand = hand;
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
  update_arrow(hand, hand_pose, head, head_valid, nullptr);
}

void VoodooVolumeEngine::park(uint32_t hand, const Pose& head, bool head_valid) {
  auto& hr = hands_[hand];
  if (!hr.session.open || hr.session.phase != VoodooSessionPhase::Dragging) {
    return;
  }
  hr.session.phase = VoodooSessionPhase::Parked;
  hr.session.arrow = {};
  hr.session.snap_nodes.clear();
  hr.have_start_hand_ = false;
  hr.deadzone_armed_ = false;
  hr.deadzone_left_once_ = false;
  hr.session.near_bb.open = true;
  hr.session.near_bb.pose = near_rest_pose(head, head_valid);
  hr.session.near_bb.linked_centroid_id = hr.session.snapped_centroid_id;
  apply_near_extents(hr.session.near_bb);
  hr.session.far_bb.linked_centroid_id = hr.session.snapped_centroid_id;
  hr.session.far_bb.pose.orientation =
      yaw_billboard_toward(hr.session.far_bb.pose.position, head, head_valid);
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

  // Arrow origin matches update_arrow (head-aware); rebuild uses last known origin if any.
  Vec3 origin = hr.session.arrow.ray_origin;
  if (!hr.session.arrow.active) {
    origin = hr.start_hand_.position;
  }

  for (const auto& c : centroids.centroids()) {
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

  // Snap-mode: rebuild nodes, apply tip gravity in hand space.
  EntityId snapped = 0;
  if (hr.session.mode == VoodooMode::Snap) {
    // Need arrow origin for node world positions — compute like update_arrow.
    const Vec3 flat = flat_forward(head, head_valid);
    const Vec3 origin{hr.start_hand_.position.x + flat.x * kStickVizForwardOffsetM,
                      hr.start_hand_.position.y + kStickVizUpOffsetM,
                      hr.start_hand_.position.z + flat.z * kStickVizForwardOffsetM};
    hr.session.arrow.ray_origin = origin;

    rebuild_snap_nodes(hand, centroids);
    // Refresh world positions with current origin.
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
        // Soft pull toward node.
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
      // Center on AABB mid (more accurate than mean of points for fit).
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
    // Snapped: snappier lock; free: subtle damp.
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

  // Keep snap node world positions aligned with current origin.
  for (auto& n : hr.session.snap_nodes) {
    n.world_position =
        Vec3{origin.x + n.hand_offset.x, origin.y + n.hand_offset.y, origin.z + n.hand_offset.z};
    n.snapped = (hr.session.snapped_centroid_id == n.centroid_id);
  }
}

void VoodooVolumeEngine::tick_hand(uint32_t hand, const InteractionIntents& intents,
                                   const Pose& hand_pose, bool hand_valid, const Pose& head,
                                   bool head_valid, float dt_seconds,
                                   const CentroidRegistry& centroids, const Workspace& workspace) {
  if (hand >= 2u) {
    return;
  }
  auto& hr = hands_[hand];
  const bool want_snap = has_flag(intents.flags, InteractionIntentFlags::VoodooSnap);
  const bool want_free = has_flag(intents.flags, InteractionIntentFlags::VoodooFree);
  const bool want_any = want_snap || want_free;

  if (hr.session.phase == VoodooSessionPhase::Parked) {
    if (want_any && hand_valid) {
      dismiss(hand);
      const VoodooMode mode = want_snap ? VoodooMode::Snap : VoodooMode::FreeRange;
      open_drag(hand, mode, hand_pose, head, head_valid);
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
    open_drag(hand, mode, hand_pose, head, head_valid);
    // Build snap nodes / far pose on the same frame the session opens.
    update_far_while_dragging(hand, hand_pose, head, head_valid, dt_seconds, centroids, workspace);
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
    park(hand, head, head_valid);
    return;
  }

  update_far_while_dragging(hand, hand_pose, head, head_valid, dt_seconds, centroids, workspace);
}

}  // namespace threeos
