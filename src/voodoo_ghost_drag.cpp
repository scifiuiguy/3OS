#include "core/voodoo_ghost_drag.hpp"

#include "core/voodoo_volume.hpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>

#if defined(__ANDROID__)
#include <android/log.h>
#endif

namespace threeos {
namespace {

#if defined(__ANDROID__)
void log_kin_clamp(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  __android_log_vprint(ANDROID_LOG_INFO, "3OS_KinClamp", fmt, ap);
  va_end(ap);
}
#else
void log_kin_clamp(const char* /*fmt*/, ...) {}
#endif

float dist3(const Vec3& a, const Vec3& b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  const float dz = a.z - b.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

struct MrEnclosureClampResult {
  bool moved = false;
  bool on_wall = false;
  Vec3 wall_into_room{};
};

MrEnclosureClampResult clamp_to_mr_enclosure(VoodooVolumeEngine& engine, Vec3& pos, const Pose& head,
                                             bool head_valid, const Vec3& ray_origin) {
  MrEnclosureClampResult out;
  if (!engine.room_bounds_active() && engine.enclosure_box_count() == 0) {
    return out;
  }
  const Vec3 before = pos;
  const float h = engine.mr_enclosure_proxy_half_m();
  const Vec3 half{h, h, h};
  Vec3 wall{};
  out.on_wall =
      engine.constrain_object_center(pos, half, head, head_valid, &wall, &ray_origin);
  if (out.on_wall) {
    out.wall_into_room = wall;
  }
  out.moved = std::fabs(before.x - pos.x) > 1e-5f || std::fabs(before.y - pos.y) > 1e-5f ||
              std::fabs(before.z - pos.z) > 1e-5f;
  return out;
}

void store_mr_wall_contact(GhostDragState& gd, const MrEnclosureClampResult& clamp) {
  gd.have_mr_wall_normal = clamp.on_wall;
  if (clamp.on_wall) {
    gd.mr_wall_into_room = clamp.wall_into_room;
  } else {
    gd.mr_wall_into_room = {};
  }
}

/// Remove motion into the wall (normal points into room). Tangential component is kept at full strength.
Vec3 strip_inward_wall_delta(const Vec3& delta, const Vec3& wall_into_room) {
  const float dn = delta.x * wall_into_room.x + delta.y * wall_into_room.y + delta.z * wall_into_room.z;
  if (dn >= 0.f) {
    return delta;
  }
  return Vec3{delta.x - dn * wall_into_room.x, delta.y - dn * wall_into_room.y,
              delta.z - dn * wall_into_room.z};
}

void log_wall_contact_once(GhostDragState& gd, const MrEnclosureClampResult& clamp, uint32_t stick_path) {
  static EntityId s_logged_entity = 0;
  static uint32_t s_logged_hand = 99u;
  if (!clamp.on_wall) {
    return;
  }
  if (gd.entity_id == s_logged_entity && gd.hand == s_logged_hand) {
    return;
  }
  s_logged_entity = gd.entity_id;
  s_logged_hand = gd.hand;
  log_kin_clamp("drag/wall_contact entity=%llu hand=%u stick_path=%u n=(%.2f,%.2f,%.2f)",
                static_cast<unsigned long long>(gd.entity_id), gd.hand, stick_path,
                clamp.wall_into_room.x, clamp.wall_into_room.y, clamp.wall_into_room.z);
}

void log_drag_begin(const VoodooVolumeEngine& engine, const GhostDragState& gd, bool portal_ghost) {
  log_kin_clamp(
      "drag/begin hand=%u entity=%llu kind=%u direct_follow=%d portal_ghost=%d wm_boxes=%u "
      "room_aabb=%d proxy_r=%.3f",
      gd.hand, static_cast<unsigned long long>(gd.entity_id),
      static_cast<unsigned>(gd.kind), gd.direct_follow ? 1 : 0, portal_ghost ? 1 : 0,
      engine.enclosure_box_count(), engine.room_bounds_active() ? 1 : 0,
      engine.mr_enclosure_proxy_half_m());
}

}  // namespace

void GhostDragController::update_arrow(uint32_t hand, const Pose& hand_pose,
                                                 const Pose& head, bool head_valid,
                                                 const Vec3& stick) {
  (void)stick;
  auto& gd = engine_.ghost_drags_[hand];
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
        engine_.find_ghost_mut(gd.entity_id, &portal, &ghost) && portal != nullptr && ghost != nullptr &&
        portal->id == gd.portal_id;
  }

  (void)head;
  (void)head_valid;
  const Vec3 anchor = gd.start_hand.position;
  const Vec3 cursor = hand_pose.position;
  const VoodooPortal* portal = engine_.portal_const(gd.portal_id);
  const float scale =
      std::max(gd.scale > 1e-4f ? gd.scale : (portal != nullptr ? portal->lock_scale : 1.f), 1e-4f);

  // Anchor = cursor at drag begin; tip = current cursor (host applies ~1" tip clearance).
  if (gd.near_stick_visible) {
    VoodooVolumeEngine::fill_drag_arrow_cursor(gd.arrow, anchor, cursor, gd.deadzone_armed,
                                               engine_.deadzone_m_);
    Vec3 far_anchor = anchor;
    Vec3 far_cursor = cursor;
    if (portal != nullptr) {
      Pose near_a{};
      near_a.position = anchor;
      near_a.orientation = portal->near_bb.pose.orientation;
      Pose near_c{};
      near_c.position = cursor;
      near_c.orientation = portal->near_bb.pose.orientation;
      far_anchor = engine_.map_near_to_far_pose(*portal, near_a, scale).position;
      far_cursor = engine_.map_near_to_far_pose(*portal, near_c, scale).position;
    }
    VoodooVolumeEngine::fill_drag_arrow_cursor(gd.far_arrow, far_anchor, far_cursor, gd.deadzone_armed,
                                               engine_.deadzone_m_);
  } else {
    gd.arrow = {};
    VoodooVolumeEngine::fill_drag_arrow_cursor(gd.far_arrow, anchor, cursor, gd.deadzone_armed,
                                               engine_.deadzone_m_);
  }
}


void GhostDragController::refresh_arrows(uint32_t hand, const Pose& hand_pose,
                                                   const Pose& head, bool head_valid) {
  if (hand >= 2u) {
    return;
  }
  const auto& gd = engine_.ghost_drags_[hand];
  if (!gd.active || !gd.have_start_hand) {
    return;
  }
  const Vec3 stick{hand_pose.position.x - gd.start_hand.position.x,
                   hand_pose.position.y - gd.start_hand.position.y,
                   hand_pose.position.z - gd.start_hand.position.z};
  update_arrow(hand, hand_pose, head, head_valid, stick);
}


void GhostDragController::audition_enter_portal(GhostDragState& gd, VoodooPortal& portal,
                                               const Pose& hand_pose, Workspace& workspace) {
  if (!engine_.ensure_ghost_in_portal(portal, gd.entity_id, workspace)) {
    return;
  }
  VoodooGhost* ghost = nullptr;
  VoodooPortal* p = &portal;
  if (!engine_.find_ghost_mut(gd.entity_id, &p, &ghost) || ghost == nullptr) {
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
  ghost->far_pose = engine_.map_near_to_far_pose(portal, ghost->near_pose, gd.scale);
  {
    const Vec3 origin = ghost->far_pose.position;
    const auto clamp = clamp_to_mr_enclosure(engine_, ghost->far_pose.position, Pose{}, false, origin);
    store_mr_wall_contact(gd, clamp);
  }
  if (auto* we = workspace.find_mut(gd.entity_id)) {
    we->pose = ghost->far_pose;
  }
}


void GhostDragController::audition_exit_portal(GhostDragState& gd, const Pose& hand_pose,
                                              Workspace& workspace) {
  if (VoodooPortal* portal = engine_.portal_mut(gd.portal_id)) {
    engine_.erase_content_ghost(*portal, gd.entity_id);
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


void GhostDragController::audition_apply_matrix_snap(GhostDragState& gd, const Pose& hand_pose,
                                                    Workspace& workspace) {
  if (gd.audition_inside || gd.entity_id == 0) {
    return;
  }
  const auto clamp_free = [&](Vec3& pos) {
    const Vec3 origin = pos;
    const auto r = clamp_to_mr_enclosure(engine_, pos, Pose{}, false, origin);
    store_mr_wall_contact(gd, r);
  };
  auto* we = workspace.find_mut(gd.entity_id);
  if (we == nullptr || !we->in_field) {
    return;
  }
  const bool pending_before =
      workspace.snap().pending && workspace.snap().object_id == gd.entity_id;
  const EntityId prior_box = pending_before ? workspace.snap().box_id : 0;

  // Free/escape pose for sticky tests — never use the seated hold as escape, or the
  // snap can never clear (and never re-anchor every frame; that resets escape too).
  Pose free = we->pose;
  if (pending_before && gd.have_snap_escape) {
    if (gd.direct_follow) {
      // grab_offset is hold−hand from snap enter so escape tracks hand delta from the seat.
      gd.snap_escape =
          Vec3{hand_pose.position.x + gd.grab_offset.x, hand_pose.position.y + gd.grab_offset.y,
               hand_pose.position.z + gd.grab_offset.z};
      clamp_free(gd.snap_escape);
    }
    free.position = gd.snap_escape;
  } else if (gd.direct_follow) {
    free.position = Vec3{hand_pose.position.x + gd.grab_offset.x,
                         hand_pose.position.y + gd.grab_offset.y,
                         hand_pose.position.z + gd.grab_offset.z};
    clamp_free(free.position);
  }

  // Lateral unsnap clears the 3″ hold buffer while free is often still inside the 0.2 m
  // enter radius around the Box center — suppress re-arm until the user leaves that radius.
  if (!pending_before && gd.snap_cooldown_box != 0) {
    const WorkspaceEntry* cool = workspace.find(gd.snap_cooldown_box);
    if (cool != nullptr && cool->kind == StorageKind::Box && cool->in_field &&
        dist3(free.position, cool->pose.position) < Workspace::kSnapDistanceM) {
      workspace.clear_snap();
      we->pose.position = free.position;
      clamp_free(we->pose.position);
      gd.have_snap_escape = false;
      return;
    }
    gd.snap_cooldown_box = 0;
  }

  (void)workspace.update_snap(gd.entity_id, free, /*apply_pose=*/true);

  const bool pending_after =
      workspace.snap().pending && workspace.snap().object_id == gd.entity_id;
  if (!pending_after) {
    if (pending_before) {
      gd.snap_cooldown_box = prior_box;
      // Direct-follow: restore pre-snap cursor grab so the glyph returns to the tip.
      if (gd.direct_follow && gd.have_grab_offset_pre_snap) {
        gd.grab_offset = gd.grab_offset_pre_snap;
        gd.have_grab_offset_pre_snap = false;
        we->pose.position = Vec3{hand_pose.position.x + gd.grab_offset.x,
                                 hand_pose.position.y + gd.grab_offset.y,
                                 hand_pose.position.z + gd.grab_offset.z};
        clamp_free(we->pose.position);
      }
    }
    gd.have_snap_escape = false;
    return;
  }

  if (!pending_before) {
    // Snap enter only: seat is on hold; start escape at hold and re-anchor grab/stick.
    gd.have_snap_escape = true;
    gd.snap_escape = we->pose.position;
    gd.snap_cooldown_box = 0;
    if (gd.direct_follow) {
      if (!gd.have_grab_offset_pre_snap) {
        gd.grab_offset_pre_snap = gd.grab_offset;
        gd.have_grab_offset_pre_snap = true;
      }
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


bool GhostDragController::begin(uint32_t hand, EntityId entity_id, const Pose& hand_pose,
                                         Workspace& workspace) {
  if (hand >= 2u || entity_id == 0) {
    return false;
  }
  engine_.ghost_drags_[hand] = {};

  VoodooPortal* portal = nullptr;
  VoodooGhost* ghost = nullptr;
  const bool has_ghost =
      engine_.find_ghost_mut(entity_id, &portal, &ghost) && portal != nullptr && ghost != nullptr;

  auto& gd = engine_.ghost_drags_[hand];
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
  gd.have_mr_wall_normal = false;
  gd.mr_wall_into_room = {};
  gd.have_snap_escape = false;
  gd.snap_escape = {};
  gd.snap_cooldown_box = 0;

  if (has_ghost) {
    // Committed portal ghost: near-BB grab uses ghost-scale drag. Outside the near BB,
    // drag the far/real workspace object (direct or velocity telekinesis) — do not fail
    // begin and fall back to possess stick on the host.
    if (!VoodooVolumeEngine::point_in_obb(hand_pose.position, portal->near_bb.pose, portal->near_bb.extents)) {
      gd.kind = GhostDragKind::Audition;
      gd.audition_inside = false;
      gd.portal_id = 0;
      if (const WorkspaceEntry* e = workspace.find(entity_id)) {
        gd.start_far = e->pose;
        const float dx = e->pose.position.x - hand_pose.position.x;
        const float dy = e->pose.position.y - hand_pose.position.y;
        const float dz = e->pose.position.z - hand_pose.position.z;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist <= VoodooVolumeEngine::kDirectGrabRadiusM) {
          gd.direct_follow = true;
          gd.grab_offset = Vec3{dx, dy, dz};
        }
      }
      log_drag_begin(engine_, gd, true);
      return true;
    }
    gd.kind = GhostDragKind::Committed;
    gd.portal_id = portal->id;
    gd.scale = std::max(ghost->scale, 1e-4f);
    gd.start_far = ghost->far_pose;
    if (const WorkspaceEntry* e = workspace.find(entity_id)) {
      gd.start_far = e->pose;
      ghost->far_pose = e->pose;
    }
    engine_.refresh_ghost_near_poses(*portal);
    // Cursor inside the near ghost glyph → direct translation sync (same as near reals).
    // Unity floors ghost visual scale at 0.15; match that so hover-in-glyph works.
    const float grab_r = VoodooVolumeEngine::kDirectGrabRadiusM * std::max(ghost->scale, 0.15f);
    const float dx = ghost->near_pose.position.x - hand_pose.position.x;
    const float dy = ghost->near_pose.position.y - hand_pose.position.y;
    const float dz = ghost->near_pose.position.z - hand_pose.position.z;
    const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (dist <= grab_r) {
      gd.direct_follow = true;
      gd.grab_offset = Vec3{dx, dy, dz};
    }
    log_drag_begin(engine_, gd, true);
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
    if (dist <= VoodooVolumeEngine::kDirectGrabRadiusM) {
      gd.direct_follow = true;
      gd.grab_offset = Vec3{dx, dy, dz};
    }
  }
  const uint64_t pid = engine_.portal_at_near_point(hand_pose.position);
  if (pid != 0) {
    for (auto& p : engine_.portals_) {
      if (p.id == pid) {
        audition_enter_portal(gd, p, hand_pose, workspace);
        break;
      }
    }
  }
  log_drag_begin(engine_, gd, false);
  return true;
}


void GhostDragController::tick(uint32_t hand, const Pose& hand_pose, bool hand_valid,
                                         const Pose& head, bool head_valid, float dt_seconds,
                                         Workspace& workspace) {
  if (hand >= 2u) {
    return;
  }
  auto& gd = engine_.ghost_drags_[hand];
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

  VoodooVolumeEngine::arm_deadzone(gd.deadzone_armed, gd.deadzone_left_once, mag, engine_.deadzone_m_);

  // --- Audition: enter/exit portals freely; no bound trap until commit ---
  if (gd.kind == GhostDragKind::Audition) {
    const uint64_t pid = engine_.portal_at_near_point(hand_pose.position);
    if (gd.audition_inside) {
      if (pid == 0 || pid != gd.portal_id) {
        audition_exit_portal(gd, hand_pose, workspace);
        if (pid != 0) {
          for (auto& p : engine_.portals_) {
            if (p.id == pid) {
              audition_enter_portal(gd, p, hand_pose, workspace);
              break;
            }
          }
        }
      }
    } else if (pid != 0) {
      for (auto& p : engine_.portals_) {
        if (p.id == pid) {
          audition_enter_portal(gd, p, hand_pose, workspace);
          break;
        }
      }
    }

    // Direct grab: no deadzone / velocity — object stays translation-synced to cursor.
    if (gd.direct_follow) {
      static EntityId s_no_enclosure_drag_entity = 0;
      Vec3 synced{hand_pose.position.x + gd.grab_offset.x,
                  hand_pose.position.y + gd.grab_offset.y,
                  hand_pose.position.z + gd.grab_offset.z};
      const uint32_t nbox = engine_.enclosure_box_count();
      const bool room = engine_.room_bounds_active();
      if (!room && nbox == 0) {
        if (gd.entity_id != s_no_enclosure_drag_entity) {
          s_no_enclosure_drag_entity = gd.entity_id;
          log_kin_clamp("drag/no_enclosure entity=%llu direct_follow=1 (wm_boxes=0 room=0)",
                        static_cast<unsigned long long>(gd.entity_id));
        }
      } else {
        s_no_enclosure_drag_entity = 0;
      }
      gd.velocity = {};
      if (gd.audition_inside) {
        VoodooPortal* portal = nullptr;
        VoodooGhost* ghost = nullptr;
        if (!engine_.find_ghost_mut(gd.entity_id, &portal, &ghost) || portal == nullptr ||
            ghost == nullptr) {
          gd.audition_inside = false;
          return;
        }
        ghost->near_pose.position = synced;
        ghost->near_pose.orientation = portal->near_bb.pose.orientation;
        gd.scale = std::max(ghost->scale, 1e-4f);
        ghost->far_pose = engine_.map_near_to_far_pose(*portal, ghost->near_pose, gd.scale);
        {
          const Vec3 origin = ghost->far_pose.position;
          const auto r =
              clamp_to_mr_enclosure(engine_, ghost->far_pose.position, head, head_valid, origin);
          store_mr_wall_contact(gd, r);
        }
        if (auto* we = workspace.find_mut(gd.entity_id)) {
          we->pose = ghost->far_pose;
        }
        gd.clamped = false;
      } else if (auto* we = workspace.find_mut(gd.entity_id)) {
        const Vec3 origin = we->pose.position;
        Vec3 delta{synced.x - origin.x, synced.y - origin.y, synced.z - origin.z};
        if (gd.have_mr_wall_normal) {
          delta = strip_inward_wall_delta(delta, gd.mr_wall_into_room);
        }
        Vec3 trial{origin.x + delta.x, origin.y + delta.y, origin.z + delta.z};
        if (room || nbox > 0) {
          const auto r = clamp_to_mr_enclosure(engine_, trial, head, head_valid, origin);
          store_mr_wall_contact(gd, r);
          log_wall_contact_once(gd, r, 0u);
        }
        we->pose.position = trial;
        audition_apply_matrix_snap(gd, hand_pose, workspace);
      }
      return;
    }

    if (mag < engine_.deadzone_m_) {
      gd.velocity = {};
      if (!gd.audition_inside) {
        audition_apply_matrix_snap(gd, hand_pose, workspace);
      }
      return;
    }

    if (gd.audition_inside) {
      VoodooPortal* portal = nullptr;
      VoodooGhost* ghost = nullptr;
      if (!engine_.find_ghost_mut(gd.entity_id, &portal, &ghost) || portal == nullptr || ghost == nullptr) {
        gd.audition_inside = false;
        return;
      }
      // Velocity audition inside: ghost tracks cursor; real tracks mapped far.
      ghost->near_pose.position = hand_pose.position;
      ghost->near_pose.orientation = portal->near_bb.pose.orientation;
      gd.scale = std::max(ghost->scale, 1e-4f);
      ghost->far_pose = engine_.map_near_to_far_pose(*portal, ghost->near_pose, gd.scale);
      {
        const Vec3 origin = ghost->far_pose.position;
        const auto r =
            clamp_to_mr_enclosure(engine_, ghost->far_pose.position, head, head_valid, origin);
        store_mr_wall_contact(gd, r);
      }
      if (auto* we = workspace.find_mut(gd.entity_id)) {
        we->pose = ghost->far_pose;
      }
      gd.clamped = false;
    } else {
      // Outside: velocity stick (cursor was not in the glyph at grab).
      const float eff = (mag - engine_.deadzone_m_) / mag;
      gd.velocity =
          Vec3{VoodooVolumeEngine::kGhostDragGain * stick.x * eff, VoodooVolumeEngine::kGhostDragGain * stick.y * eff,
               VoodooVolumeEngine::kGhostDragGain * stick.z * eff};
      const float dt = std::max(0.f, dt_seconds);
      if (auto* we = workspace.find_mut(gd.entity_id)) {
        const bool snapped =
            workspace.snap().pending && workspace.snap().object_id == gd.entity_id;
        if (snapped && gd.have_snap_escape) {
          const Vec3 origin = gd.snap_escape;
          Vec3 delta{gd.velocity.x * dt, gd.velocity.y * dt, gd.velocity.z * dt};
          if (gd.have_mr_wall_normal) {
            delta = strip_inward_wall_delta(delta, gd.mr_wall_into_room);
          }
          gd.snap_escape = Vec3{origin.x + delta.x, origin.y + delta.y, origin.z + delta.z};
          const auto r =
              clamp_to_mr_enclosure(engine_, gd.snap_escape, head, head_valid, origin);
          store_mr_wall_contact(gd, r);
          log_wall_contact_once(gd, r, 1u);
        } else {
          const Vec3 origin = we->pose.position;
          Vec3 delta{gd.velocity.x * dt, gd.velocity.y * dt, gd.velocity.z * dt};
          if (gd.have_mr_wall_normal) {
            delta = strip_inward_wall_delta(delta, gd.mr_wall_into_room);
          }
          we->pose.position = Vec3{origin.x + delta.x, origin.y + delta.y, origin.z + delta.z};
          const auto r =
              clamp_to_mr_enclosure(engine_, we->pose.position, head, head_valid, origin);
          store_mr_wall_contact(gd, r);
          log_wall_contact_once(gd, r, 1u);
        }
      }
      audition_apply_matrix_snap(gd, hand_pose, workspace);
    }
    return;
  }

  // --- Committed ghost drag: clamp near proxy + live far/near BB refit ---
  VoodooPortal* portal = nullptr;
  VoodooGhost* ghost = nullptr;
  if (!engine_.find_ghost_mut(gd.entity_id, &portal, &ghost) || portal == nullptr || ghost == nullptr ||
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
    const Vec3 clamped = engine_.clamp_point_to_near_obb(*portal, synced);
    gd.clamped = dist3(synced, clamped) > 1e-4f;
    gd.scale = std::max(ghost->scale, 1e-4f);
    Pose near_drive{};
    near_drive.position = synced;
    near_drive.orientation = portal->near_bb.pose.orientation;
    ghost->far_pose = engine_.map_near_to_far_pose(*portal, near_drive, gd.scale);
    if (auto* we = workspace.find_mut(gd.entity_id)) {
      const Vec3 origin = we->pose.position;
      const auto r =
          clamp_to_mr_enclosure(engine_, ghost->far_pose.position, head, head_valid, origin);
      store_mr_wall_contact(gd, r);
      we->pose = ghost->far_pose;
    }
    engine_.refit_far_bb_to_content_ghosts(*portal, workspace, /*settle_bump=*/false);
    gd.scale = std::max(ghost->scale, 1e-4f);
    ghost->near_pose = engine_.map_far_to_near_pose(*portal, ghost->far_pose, ghost->scale);
    ghost->near_pose.position = engine_.clamp_point_to_near_obb(*portal, ghost->near_pose.position);
    ghost->near_pose.orientation = portal->near_bb.pose.orientation;
    return;
  }

  if (mag < engine_.deadzone_m_) {
    gd.velocity = {};
    return;
  }

  const float eff = (mag - engine_.deadzone_m_) / mag;
  gd.scale = std::max(ghost->scale, 1e-4f);
  const float gain_far = VoodooVolumeEngine::kGhostDragGain / gd.scale;
  gd.velocity = Vec3{gain_far * stick.x * eff, gain_far * stick.y * eff, gain_far * stick.z * eff};
  const float dt = std::max(0.f, dt_seconds);
  Pose far = ghost->far_pose;
  const Vec3 origin = far.position;
  Vec3 delta{gd.velocity.x * dt, gd.velocity.y * dt, gd.velocity.z * dt};
  if (gd.have_mr_wall_normal) {
    delta = strip_inward_wall_delta(delta, gd.mr_wall_into_room);
  }
  far.position = Vec3{origin.x + delta.x, origin.y + delta.y, origin.z + delta.z};
  const auto r = clamp_to_mr_enclosure(engine_, far.position, head, head_valid, origin);
  store_mr_wall_contact(gd, r);
  log_wall_contact_once(gd, r, 1u);
  ghost->far_pose = far;

  if (auto* we = workspace.find_mut(gd.entity_id)) {
    we->pose = far;
  }

  {
    const Pose near_unc = engine_.map_far_to_near_pose(*portal, far, ghost->scale);
    gd.clamped = !VoodooVolumeEngine::point_in_obb(near_unc.position, portal->near_bb.pose, portal->near_bb.extents);
  }

  // Grow far+near at lock_scale so dragging past a face expands the portal
  // instead of ejecting the ghost. Possess/far velocity still prunes via sync.
  engine_.refit_far_bb_to_content_ghosts(*portal, workspace, /*settle_bump=*/false);
  gd.scale = std::max(ghost->scale, 1e-4f);
  ghost->near_pose.position = engine_.clamp_point_to_near_obb(*portal, ghost->near_pose.position);
  ghost->near_pose.orientation = portal->near_bb.pose.orientation;
}


void GhostDragController::end(uint32_t hand, bool release_inside_near_bb,
                                        Workspace& workspace) {
  if (hand >= 2u) {
    return;
  }
  auto& gd = engine_.ghost_drags_[hand];
  if (!gd.active) {
    return;
  }

  if (gd.kind == GhostDragKind::Audition) {
    if (gd.audition_inside && release_inside_near_bb) {
      // Commit: keep ghost; workspace stays at mapped far; now refit far BB.
      VoodooPortal* portal = nullptr;
      VoodooGhost* ghost = nullptr;
      if (engine_.find_ghost_mut(gd.entity_id, &portal, &ghost) && portal != nullptr && ghost != nullptr) {
        if (auto* we = workspace.find_mut(gd.entity_id)) {
          we->pose = ghost->far_pose;
        }
        engine_.refit_far_bb_to_content_ghosts(*portal, workspace, /*settle_bump=*/true);
      }
    } else if (gd.audition_inside) {
      // Released outside (or cancelled): destroy ghost; real returns to last near pose.
      Pose leave{};
      leave.position = gd.start_hand.position;
      VoodooPortal* portal = nullptr;
      VoodooGhost* ghost = nullptr;
      if (engine_.find_ghost_mut(gd.entity_id, &portal, &ghost) && ghost != nullptr) {
        leave = ghost->near_pose;
      }
      audition_exit_portal(gd, leave, workspace);
      if (auto* we = workspace.find_mut(gd.entity_id)) {
        we->pose = leave;
      }
    }
    // Already outside: workspace is the near-field real — nothing to clean up.
    if (workspace.snap().pending && workspace.snap().object_id == gd.entity_id) {
      workspace.clear_snap();
    }
    gd = {};
    return;
  }

  VoodooPortal* portal = nullptr;
  VoodooGhost* ghost = nullptr;
  engine_.find_ghost_mut(gd.entity_id, &portal, &ghost);
  if (portal != nullptr && ghost != nullptr) {
    if (auto* we = workspace.find_mut(gd.entity_id)) {
      we->pose = ghost->far_pose;
    }
    engine_.refit_far_bb_to_content_ghosts(*portal, workspace, /*settle_bump=*/true);
    // Refit can land exactly on the OBB face; keep the ghost inside the near volume.
    ghost->near_pose.position =
        engine_.clamp_point_to_near_obb(*portal, ghost->near_pose.position);
    ghost->near_pose.orientation = portal->near_bb.pose.orientation;
  }

  gd = {};
}


bool GhostDragController::active(uint32_t hand) const {
  return hand < 2u && engine_.ghost_drags_[hand].active;
}


EntityId GhostDragController::entity(uint32_t hand) const {
  return hand < 2u && engine_.ghost_drags_[hand].active ? engine_.ghost_drags_[hand].entity_id : 0;
}


GhostDragKind GhostDragController::kind(uint32_t hand) const {
  return hand < 2u && engine_.ghost_drags_[hand].active ? engine_.ghost_drags_[hand].kind
                                                : GhostDragKind::Committed;
}


const VoodooDragArrow& GhostDragController::arrow(uint32_t hand) const {
  static const VoodooDragArrow kEmpty{};
  return hand < 2u ? engine_.ghost_drags_[hand].arrow : kEmpty;
}


const VoodooDragArrow& GhostDragController::far_arrow(uint32_t hand) const {
  static const VoodooDragArrow kEmpty{};
  return hand < 2u ? engine_.ghost_drags_[hand].far_arrow : kEmpty;
}


void GhostDragController::update_matrix_snap(uint32_t hand, Workspace& workspace) {
  if (hand >= 2u || !engine_.ghost_drags_[hand].active) {
    return;
  }
  const EntityId gid = engine_.ghost_drags_[hand].entity_id;
  if (gid == 0) {
    return;
  }

  Pose snap_pose{};
  bool have_snap_pose = false;
  if (const uint64_t pid = engine_.portal_for_ghost_entity(gid); pid != 0) {
    for (const auto& portal : engine_.portals_) {
      if (portal.id != pid) {
        continue;
      }
      for (const auto& g : portal.ghosts) {
        if (!g.is_parent_box && g.entity_id == gid) {
          snap_pose = g.near_pose;
          have_snap_pose = true;
          break;
        }
      }
      break;
    }
  }
  if (!have_snap_pose) {
    if (const auto* we = workspace.find(gid)) {
      snap_pose = we->pose;
      have_snap_pose = true;
    }
  }
  if (!have_snap_pose) {
    return;
  }

  // Near BB is ~0.3 m — far-field 0.2 m radius armed almost every pair.
  // Portal ghosts: snap to sibling ghost Boxes (tight near-space radius).
  // Near-field reals (audition / no portal ghost): snap to in-field Boxes via
  // workspace poses. Committed drag with no ghost (pulled to far) must NOT
  // fall back to far update_snap (distant false inserts + FS thrash).
  const uint64_t snap_portal = engine_.portal_for_ghost_entity(gid);
  const GhostDragKind drag_kind = engine_.ghost_drags_[hand].kind;
  if (snap_portal == 0) {
    if (drag_kind != GhostDragKind::Audition) {
      // Matrix snap (seat + re-anchor) runs inside tick for audition outside a
      // portal. Do not clear here — tick already armed/updated snap.
      workspace.clear_snap();
    }
    return;
  }

  EntityId best_box = 0;
  float best_d = kNearRealSnapM;
  bool best_is_ghost = false;
  const VoodooPortal* snap_portal_ptr = nullptr;
  for (const auto& portal : engine_.portals_) {
    if (portal.id == snap_portal) {
      snap_portal_ptr = &portal;
      break;
    }
  }
  if (snap_portal_ptr == nullptr) {
    return;
  }

  auto near_dist_to_ghost = [&](EntityId box_id) -> float {
    for (const auto& g : snap_portal_ptr->ghosts) {
      if (!g.is_parent_box && g.entity_id == box_id) {
        return dist3(snap_pose.position, g.near_pose.position);
      }
    }
    return 1e9f;
  };

  if (workspace.snap().pending && workspace.snap().object_id == gid &&
      workspace.snap().box_id != 0) {
    const float d_ghost = near_dist_to_ghost(workspace.snap().box_id);
    if (d_ghost < kGhostStickyM) {
      best_box = workspace.snap().box_id;
      best_d = d_ghost;
      best_is_ghost = true;
    } else if (engine_.portal_for_ghost_entity(workspace.snap().box_id) == 0) {
      if (const auto* box = workspace.find(workspace.snap().box_id)) {
        const Vec3 hold{box->pose.position.x,
                        box->pose.position.y + Workspace::kHoldAboveBoxM,
                        box->pose.position.z};
        const float d = dist3(snap_pose.position, hold);
        if (d < Workspace::kSnapEscapeBufferM) {
          best_box = workspace.snap().box_id;
          best_d = d;
          best_is_ghost = false;
        }
      }
    }
  }

  if (best_box == 0) {
    for (const auto& g : snap_portal_ptr->ghosts) {
      if (g.is_parent_box || g.entity_id == gid) {
        continue;
      }
      const auto* we = workspace.find(g.entity_id);
      if (we == nullptr || !we->in_field || we->kind != StorageKind::Box) {
        continue;
      }
      const float d = dist3(snap_pose.position, g.near_pose.position);
      if (d < kGhostSnapM && d < best_d) {
        best_d = d;
        best_box = g.entity_id;
        best_is_ghost = true;
      }
    }
    // Near-field real Boxes (not portal ghosts) — workspace pose is near.
    for (const auto& e : workspace.entries()) {
      if (e.kind != StorageKind::Box || !e.in_field || e.id == gid) {
        continue;
      }
      if (engine_.portal_for_ghost_entity(e.id) != 0) {
        continue;
      }
      const float d = dist3(snap_pose.position, e.pose.position);
      if (d < best_d) {
        best_d = d;
        best_box = e.id;
        best_is_ghost = false;
      }
    }
  }

  if (best_box != 0 && workspace.arm_snap_to_box(gid, best_box)) {
    if (best_is_ghost) {
      (void)apply_near_ghost_snap_hold(gid, best_box, kGhostHoldAboveM, workspace);
    }
  } else {
    workspace.clear_snap();
  }
}


bool GhostDragController::apply_near_ghost_snap_hold(EntityId object_id, EntityId box_id,
                                                    float /*hold_above_m*/, Workspace& workspace) {
  if (object_id == 0 || box_id == 0 || object_id == box_id) {
    return false;
  }
  VoodooPortal* portal = nullptr;
  VoodooGhost* drag_g = nullptr;
  VoodooGhost* box_g = nullptr;
  for (auto& p : engine_.portals_) {
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
  drag_g->far_pose = engine_.map_near_to_far_pose(*portal, drag_g->near_pose, drag_g->scale);
  if (auto* we = workspace.find_mut(object_id)) {
    we->pose = drag_g->far_pose;
  }
  return true;
}

}  // namespace threeos
