#include "core/solo_voodoo_drop.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace threeos {

namespace {

Vec3 normalized(const Vec3& v) {
  const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
  if (len < 1e-6f) {
    return Vec3{};
  }
  return Vec3{v.x / len, v.y / len, v.z / len};
}

float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vec3 gaze_forward(const Pose& head) {
  const Quat& q = head.orientation;
  return Vec3{2.f * (q.x * q.z + q.w * q.y), 2.f * (q.y * q.z - q.w * q.x),
              1.f - 2.f * (q.x * q.x + q.y * q.y)};
}

/// Gaze/palm pin stays on the looked-at object (or centroid). Cluster lock remaps drop/far BB.
bool resolve_gaze_pin(EntityId id, const Workspace& workspace, const CentroidRegistry& centroids,
                      const VoodooVolumeEngine& voodoo, Vec3* out_pos) {
  if (id == 0 || out_pos == nullptr) {
    return false;
  }
  if (const WorkspaceEntry* entry = workspace.find(id);
      entry != nullptr && SoloVoodooDrop::eligible(*entry, workspace, centroids, voodoo)) {
    *out_pos = entry->pose.position;
    return true;
  }
  if (const CentroidState* c = centroids.find(id); c != nullptr && c->id != 0) {
    if (c->kind == CentroidKind::FocusAnchor || c->kind == CentroidKind::MindPalace) {
      *out_pos = c->position;
      return true;
    }
    if (c->kind == CentroidKind::Box) {
      if (const WorkspaceEntry* box = workspace.find(c->linked_entity_id != 0 ? c->linked_entity_id
                                                                             : c->id);
          box != nullptr && SoloVoodooDrop::eligible(*box, workspace, centroids, voodoo)) {
        *out_pos = box->pose.position;
        return true;
      }
      *out_pos = c->position;
      return true;
    }
  }
  return false;
}

}  // namespace

bool SoloVoodooDrop::blocks_voodoo_snap(float now_s) const {
  if (now_s < lockout_until_) {
    return true;
  }
  if (state_.armed) {
    return true;
  }
  return palm_ok_[0] || palm_ok_[1];
}

float SoloVoodooDrop::gaze_dot(const Pose& head, const Vec3& target) {
  const Vec3 to = normalized(
      Vec3{target.x - head.position.x, target.y - head.position.y, target.z - head.position.z});
  return dot(normalized(gaze_forward(head)), to);
}

bool SoloVoodooDrop::eligible(const WorkspaceEntry& e, const Workspace& /*workspace*/,
                              const CentroidRegistry& /*centroids*/,
                              const VoodooVolumeEngine& voodoo) {
  if (e.id == 0 || !e.in_field) {
    return false;
  }
  // Selected far objects remain solo-droppable (telekinesis and Use The Force coexist).
  if (voodoo.portal_for_ghost_entity(e.id) != 0) {
    return false;
  }
  return true;
}

std::vector<EntityId> SoloVoodooDrop::candidates(const Pose& head, bool head_valid,
                                                 const Workspace& workspace,
                                                 const CentroidRegistry& centroids,
                                                 const VoodooVolumeEngine& voodoo) const {
  std::vector<std::pair<float, EntityId>> scored;
  if (!head_valid) {
    return {};
  }
  for (const auto& e : workspace.entries()) {
    if (!eligible(e, workspace, centroids, voodoo)) {
      continue;
    }
    const float d = gaze_dot(head, e.pose.position);
    if (d >= kGazeDotMin) {
      scored.emplace_back(d, e.id);
    }
  }
  // NPC FocusAnchor + MindPalace Demo cluster centers (Box centroids share workspace ids).
  for (const auto& c : centroids.centroids()) {
    if (c.id == 0) {
      continue;
    }
    if (c.kind != CentroidKind::FocusAnchor && c.kind != CentroidKind::MindPalace) {
      continue;
    }
    const float d = gaze_dot(head, c.position);
    if (d >= kGazeDotMin) {
      scored.emplace_back(d, c.id);
    }
  }
  std::sort(scored.begin(), scored.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });
  std::vector<EntityId> out;
  for (size_t i = 0; i < scored.size() && i < kMaxCandidates; ++i) {
    out.push_back(scored[i].second);
  }
  return out;
}

namespace {

EntityId solo_select_entity(EntityId clear_target, EntityId drop_id, const Workspace& workspace) {
  if (clear_target != 0 && workspace.find(clear_target) != nullptr) {
    return clear_target;
  }
  if (drop_id != 0 && workspace.find(drop_id) != nullptr) {
    return drop_id;
  }
  return clear_target != 0 ? clear_target : drop_id;
}

}  // namespace

SoloVoodooDrop::Event SoloVoodooDrop::tick(uint32_t hand, EntityId clear_target,
                                           const Vec3& palm_normal, bool primary_pinch_pressed,
                                           bool secondary_pinch_pressed, const Pose& head,
                                           bool head_valid, const Vec3& hand_pos, float now_s,
                                           const Workspace& workspace,
                                           const CentroidRegistry& centroids,
                                           const VoodooVolumeEngine& voodoo) {
  if (hand > 1u) {
    return Event::None;
  }

  if (now_s < lockout_until_) {
    if (state_.armed) {
      state_ = State{};
      palm_ok_[0] = palm_ok_[1] = false;
      return Event::Dismissed;
    }
    return Event::None;
  }

  Vec3 pin_pos{};
  const bool pin_ok =
      resolve_gaze_pin(clear_target, workspace, centroids, voodoo, &pin_pos);
  const bool gaze_ok =
      head_valid && pin_ok && gaze_dot(head, pin_pos) >= kGazeDotMin;

  EntityId drop_id = 0;
  Vec3 far_pos{};
  Vec3 far_ext{};
  const bool resolved =
      gaze_ok && voodoo.solo_resolve(clear_target, workspace, centroids, &drop_id, &far_pos,
                                     &far_ext);

  bool this_palm = false;
  if (gaze_ok) {
    // Palm aims at the gazed pin (front object), not the remapped cluster center.
    const Vec3 to_from_palm =
        normalized(Vec3{pin_pos.x - hand_pos.x, pin_pos.y - hand_pos.y, pin_pos.z - hand_pos.z});
    this_palm = dot(normalized(palm_normal), to_from_palm) >= kPalmDotMin;
  }

  // Rising edge of palm-on-target: require a fresh pinch after this hand joins Use The Force.
  // Fixes second-hand join while a stale/partial pinch was already held.
  const bool palm_rose = this_palm && !palm_ok_[hand];
  palm_ok_[hand] = this_palm;
  if (!gaze_ok) {
    palm_ok_[hand] = false;
  }
  if (palm_rose) {
    if (primary_pinch_pressed) {
      pinch_down_[hand] = true;
      pinch_t0_[hand] = now_s;
      pinch_drag_[hand] = true;
      pinch_eligible_[hand] = false;
    } else {
      pinch_down_[hand] = false;
      pinch_drag_[hand] = false;
      pinch_eligible_[hand] = false;
    }
  }

  const bool any_palm = palm_ok_[0] || palm_ok_[1];
  const bool was_armed = state_.armed;

  if (!any_palm) {
    pinch_down_[0] = pinch_down_[1] = false;
    pinch_drag_[0] = pinch_drag_[1] = false;
    pinch_eligible_[0] = pinch_eligible_[1] = false;
    sec_pinch_down_[0] = sec_pinch_down_[1] = false;
    sec_pinch_drag_[0] = sec_pinch_drag_[1] = false;
    sec_pinch_eligible_[0] = sec_pinch_eligible_[1] = false;
    state_ = State{};
    return was_armed ? Event::Dismissed : Event::None;
  }

  if (!gaze_ok) {
    // Other hand may still hold palm this frame; don't pinch-commit without gaze.
    if (!primary_pinch_pressed && !pinch_down_[hand] && !secondary_pinch_pressed &&
        !sec_pinch_down_[hand]) {
      return Event::None;
    }
  }

  if (gaze_ok && any_palm && resolved) {
    if (!state_.armed) {
      state_.armed = true;
      state_.hand = this_palm ? hand : (palm_ok_[0] ? 0u : 1u);
      state_.target = drop_id;
      state_.far_position = far_pos;
      state_.far_extents = far_ext;
      if (primary_pinch_pressed) {
        pinch_down_[hand] = true;
        pinch_t0_[hand] = now_s;
        pinch_drag_[hand] = true;
        pinch_eligible_[hand] = false;
      }
      return Event::Armed;
    }
    state_.target = drop_id;
    state_.far_position = far_pos;
    state_.far_extents = far_ext;
    if (this_palm) {
      state_.hand = hand;
    }
  }

  // Primary pinch: eligibility latched at press while THIS hand's palm is on target.
  if (primary_pinch_pressed) {
    if (!pinch_down_[hand]) {
      pinch_down_[hand] = true;
      pinch_t0_[hand] = now_s;
      pinch_drag_[hand] = false;
      pinch_eligible_[hand] = this_palm && state_.armed && gaze_ok;
    }
    if ((now_s - pinch_t0_[hand]) > kPinchTapMaxS) {
      pinch_drag_[hand] = true;
    }
  } else if (pinch_down_[hand]) {
    const bool was_drag = pinch_drag_[hand] || ((now_s - pinch_t0_[hand]) > kPinchTapMaxS);
    const bool eligible = pinch_eligible_[hand];
    pinch_down_[hand] = false;
    pinch_drag_[hand] = false;
    pinch_eligible_[hand] = false;

    if (!was_drag && eligible && state_.armed && gaze_ok && !drop_frame_latch_) {
      dropped_target_ = state_.target;
      drop_frame_latch_ = true;
      lockout_until_ = now_s + kPostDropLockoutS;
      const uint32_t commit_hand = hand;
      state_ = State{};
      state_.hand = commit_hand;
      palm_ok_[0] = palm_ok_[1] = false;
      pinch_down_[0] = pinch_down_[1] = false;
      pinch_drag_[0] = pinch_drag_[1] = false;
      pinch_eligible_[0] = pinch_eligible_[1] = false;
      sec_pinch_down_[0] = sec_pinch_down_[1] = false;
      sec_pinch_drag_[0] = sec_pinch_drag_[1] = false;
      sec_pinch_eligible_[0] = sec_pinch_eligible_[1] = false;
      return Event::Drop;
    }
  }

  // Secondary pinch while armed: select gaze object without opening the portal.
  if (secondary_pinch_pressed) {
    if (!sec_pinch_down_[hand]) {
      sec_pinch_down_[hand] = true;
      sec_pinch_t0_[hand] = now_s;
      sec_pinch_drag_[hand] = false;
      sec_pinch_eligible_[hand] = this_palm && state_.armed && gaze_ok;
    }
    if ((now_s - sec_pinch_t0_[hand]) > kPinchTapMaxS) {
      sec_pinch_drag_[hand] = true;
    }
  } else if (sec_pinch_down_[hand]) {
    const bool was_drag = sec_pinch_drag_[hand] || ((now_s - sec_pinch_t0_[hand]) > kPinchTapMaxS);
    const bool eligible = sec_pinch_eligible_[hand];
    sec_pinch_down_[hand] = false;
    sec_pinch_drag_[hand] = false;
    sec_pinch_eligible_[hand] = false;

    if (!was_drag && eligible && state_.armed && gaze_ok) {
      last_select_target_ = solo_select_entity(clear_target, state_.target, workspace);
      return Event::Select;
    }
  }

  return Event::None;
}

}  // namespace threeos
