#include "interop/threeos_marshaller.hpp"

#include "core/centroid_registry.hpp"
#include "core/mind_palace_layout.hpp"
#include "core/hand_interaction.hpp"
#include "core/interaction_facade.hpp"
#include "core/kinematics.hpp"
#include "core/multi_choice.hpp"
#include "core/topology.hpp"
#include "core/voodoo_volume.hpp"
#include "storage/glyph_registry.hpp"
#include "storage/workspace.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kMajor = 0;
constexpr uint32_t kMinor = 6;
constexpr uint32_t kPatch = 0;
constexpr uint32_t kAbiVersion = 6;

std::mutex g_mu;
bool g_initialized = false;
std::string g_last_error;
threeos::InteractionFacade g_facade;
threeos::TopologyWorkspace g_topology;
threeos::KinematicsEngine g_kinematics;
threeos::GlyphRegistry g_glyphs;
threeos::Workspace g_workspace;
threeos::HandInteractionEngine g_hands;
threeos::VoodooVolumeEngine g_voodoo;
threeos::CentroidRegistry g_centroids;
threeos::MultiChoiceEngine g_multichoice;
bool g_hands_pushed_this_frame = false;
uint64_t g_tick_count = 0;
std::optional<threeos::Pose> g_pending_portal_pose;
uint64_t g_pending_portal_entity = 0;

// Unsnap catch-up: visual lerps from hold → free kinematic pose (no pop).
constexpr float kUnsnapCatchupSeconds = 0.12f;
float g_unsnap_blend = 1.f;
threeos::Pose g_unsnap_hold{};
// Latched hand driving kinematics while possessed (0=left, 1=right, -1=none).
int g_kin_hand_latch = -1;

void set_error(const char* msg) { g_last_error = msg ? msg : ""; }

threeos::Vec3 lerp_vec(const threeos::Vec3& a, const threeos::Vec3& b, float u) {
  return threeos::Vec3{a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u,
                       a.z + (b.z - a.z) * u};
}

threeos::Pose lerp_pose(const threeos::Pose& a, const threeos::Pose& b, float u) {
  // Hold/escape share orientation in practice; blend position only.
  threeos::Pose p = b;
  p.position = lerp_vec(a.position, b.position, u);
  return p;
}

float smoothstep01(float t) {
  t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
  return t * t * (3.f - 2.f * t);
}

void clear_unsnap_catchup() {
  g_unsnap_blend = 1.f;
  g_unsnap_hold = {};
}

threeos::Vec3 from_c(const ThreeOS_Vec3& v) { return threeos::Vec3{v.x, v.y, v.z}; }
ThreeOS_Vec3 to_c(const threeos::Vec3& v) { return ThreeOS_Vec3{v.x, v.y, v.z}; }

threeos::Quat from_c(const ThreeOS_Quat& q) { return threeos::Quat{q.x, q.y, q.z, q.w}; }
ThreeOS_Quat to_c(const threeos::Quat& q) { return ThreeOS_Quat{q.x, q.y, q.z, q.w}; }

threeos::Pose from_c(const ThreeOS_Pose& p) {
  return threeos::Pose{from_c(p.position), from_c(p.orientation)};
}
ThreeOS_Pose to_c(const threeos::Pose& p) {
  return ThreeOS_Pose{to_c(p.position), to_c(p.orientation)};
}

threeos::Aabb from_c(const ThreeOS_Aabb& a) {
  return threeos::Aabb{from_c(a.min), from_c(a.max)};
}

void fill_dome(ThreeOS_DomeState* out) {
  const auto& d = g_topology.dome();
  std::memset(out, 0, sizeof(*out));
  out->active = d.active ? 1u : 0u;
  out->scale_ratio = d.scale_ratio;
  out->pose = to_c(d.pose);
  out->radius_m = d.radius_m;
}

void fill_proxy(const threeos::DoubleProxyState& p, ThreeOS_DoubleProxyState* out) {
  std::memset(out, 0, sizeof(*out));
  out->id = p.id;
  out->active = p.active ? 1u : 0u;
  out->scale_ratio = p.scale_ratio;
  out->far_aabb.min = to_c(p.far_aabb.min);
  out->far_aabb.max = to_c(p.far_aabb.max);
  out->near_aabb.min = to_c(p.near_aabb.min);
  out->near_aabb.max = to_c(p.near_aabb.max);
}

struct DriveHand {
  threeos::Pose pose{};
  bool valid = false;
  int latch = -1;
};

void sync_facade_interaction_state(const ThreeOS_InteropInputFrame* frame) {
  if (g_kinematics.state().phase == threeos::KineticPhase::Possessed) {
    g_facade.set_state(threeos::InteractionState::Possessed);
  } else if ((frame->button_flags & THREEOS_BTN_RIGHT_SELECT) != 0 ||
             (frame->button_flags & THREEOS_BTN_LEFT_SELECT) != 0) {
    g_facade.set_state(threeos::InteractionState::Hovering);
  } else {
    g_facade.set_state(threeos::InteractionState::Idle);
  }
}

void ensure_hand_samples(const ThreeOS_InteropInputFrame* frame) {
  if (g_hands_pushed_this_frame) {
    return;
  }
  const bool right = (frame->tracking_flags & THREEOS_TRACK_RIGHT_AIM) != 0;
  const bool left = (frame->tracking_flags & THREEOS_TRACK_LEFT_AIM) != 0;
  auto synth = [&](bool tracked, const ThreeOS_Pose& aim, float trigger, float grip,
                   uint32_t select_bit, uint32_t grab_bit, uint32_t ternary_bit) {
    threeos::HandSample s{};
    s.tracked = tracked;
    s.tip_flags = 0x10u;  // index-proximal bit — controller aim as probe anchor
    s.index_tip = from_c(aim.position);
    s.thumb_tip = s.index_tip;
    s.middle_tip = s.index_tip;
    s.ring_tip = s.index_tip;
    s.index_proximal = s.index_tip;
    s.primary_value = trigger;
    s.secondary_value = grip;
    s.ternary_value = (frame->button_flags & ternary_bit) != 0 ? 1.f : 0.f;
    s.primary_pressed = (frame->button_flags & select_bit) != 0;
    s.secondary_pressed = (frame->button_flags & grab_bit) != 0;
    s.ternary_pressed = (frame->button_flags & ternary_bit) != 0;
    // Palm from controller aim "up" vector (local +Y).
    const auto q = from_c(aim.orientation);
    s.palm_normal = threeos::Vec3{2.f * (q.x * q.y - q.w * q.z),
                                  1.f - 2.f * (q.x * q.x + q.z * q.z),
                                  2.f * (q.y * q.z + q.w * q.x)};
    s.palm_mode_valid = false;
    return s;
  };
  g_hands.set_hands(
      synth(left, frame->left_aim, frame->left_trigger, frame->left_grip_value,
            THREEOS_BTN_LEFT_SELECT, THREEOS_BTN_LEFT_GRAB, THREEOS_BTN_LEFT_TERNARY),
      synth(right, frame->right_aim, frame->right_trigger, frame->right_grip_value,
            THREEOS_BTN_RIGHT_SELECT, THREEOS_BTN_RIGHT_GRAB, THREEOS_BTN_RIGHT_TERNARY));
}

int pick_drive_hand(const ThreeOS_InteropInputFrame* frame) {
  constexpr float kTrig = 0.55f;
  const bool l = frame->left_trigger >= kTrig ||
                 (frame->button_flags & THREEOS_BTN_LEFT_SELECT) != 0;
  const bool r = frame->right_trigger >= kTrig ||
                 (frame->button_flags & THREEOS_BTN_RIGHT_SELECT) != 0;
  if (l && !r) {
    return 0;
  }
  if (r && !l) {
    return 1;
  }
  if (l && r) {
    return frame->left_trigger >= frame->right_trigger ? 0 : 1;
  }
  const uint32_t h = g_hands.intents().handedness;
  return h <= 1u ? static_cast<int>(h) : -1;
}

DriveHand resolve_drive_hand(const ThreeOS_InteropInputFrame* frame) {
  const bool right = (frame->tracking_flags & THREEOS_TRACK_RIGHT_AIM) != 0;
  const bool left = (frame->tracking_flags & THREEOS_TRACK_LEFT_AIM) != 0;

  if (g_kinematics.state().phase == threeos::KineticPhase::Possessed) {
    if (g_kin_hand_latch < 0) {
      g_kin_hand_latch = pick_drive_hand(frame);
    }
  } else {
    g_kin_hand_latch = -1;
  }

  DriveHand drive{};
  drive.latch = g_kin_hand_latch;

  auto try_cursor_hand = [&](int h) -> bool {
    if (h != 0 && h != 1) {
      return false;
    }
    if (!g_hands.cursor(static_cast<uint32_t>(h)).active) {
      return false;
    }
    drive.pose.position = g_hands.cursor(static_cast<uint32_t>(h)).position;
    drive.pose.orientation =
        from_c(h == 0 ? frame->left_aim.orientation : frame->right_aim.orientation);
    return true;
  };
  auto try_aim_hand = [&](int h) -> bool {
    if (h == 0 && left) {
      drive.pose = from_c(frame->left_aim);
      return true;
    }
    if (h == 1 && right) {
      drive.pose = from_c(frame->right_aim);
      return true;
    }
    return false;
  };

  const int prefer = g_kin_hand_latch >= 0 ? g_kin_hand_latch : pick_drive_hand(frame);
  if (prefer == 0 || prefer == 1) {
    // Latched / selected hand: NEVER fall back to the opposite hand (that was the
    // left-drag → right-stick bug when the left cursor was briefly inactive).
    if (try_cursor_hand(prefer) || try_aim_hand(prefer)) {
      drive.valid = true;
    }
  } else if (try_cursor_hand(0) || try_aim_hand(0)) {
    drive.valid = true;
  } else if (try_cursor_hand(1) || try_aim_hand(1)) {
    drive.valid = true;
  }
  return drive;
}

void tick_possess_snap_pipeline(const ThreeOS_InteropInputFrame* frame, const DriveHand& drive,
                                ThreeOS_InteropTransformDelta* out_delta) {
  const bool head_valid = (frame->tracking_flags & THREEOS_TRACK_HEAD) != 0;
  const bool was_snapped = g_workspace.snap().pending;
  const threeos::Pose prior_hold = g_workspace.snap().hold_pose;
  bool kin_moved = g_kinematics.tick(drive.pose, from_c(frame->head), head_valid,
                                     frame->time_seconds, drive.valid);
  const float floor_contact_y =
      g_kinematics.params().floor_y + g_kinematics.params().object_radius;

  // Box snap while possessing a workspace Object or Box (box-into-box).
  // While snapped, kinematics keeps an escape pose (stick accumulates). Display
  // stays on hold until escape leaves the snap radius, then we lerp catch-up.
  // During catch-up, snap is suppressed so a Box just above the hold cannot
  // re-suck the object while the user is trying to leave.
  threeos::Pose display_pose = g_kinematics.state().pose;
  bool force_display = false;
  const bool catchup_active = g_unsnap_blend < 1.f;
  if (g_kinematics.state().phase == threeos::KineticPhase::Possessed &&
      g_kinematics.state().entity_id != 0) {
    if (catchup_active) {
      if (g_workspace.snap().pending) {
        g_workspace.clear_snap();
      }
    } else {
      const bool snap_edge =
          g_workspace.update_snap(g_kinematics.state().entity_id, g_kinematics.state().pose);
      const bool now_pending = g_workspace.snap().pending;

      if (snap_edge && now_pending) {
        clear_unsnap_catchup();
        g_kinematics.set_pose(g_workspace.snap().hold_pose);
        g_kinematics.zero_velocity();
        if (drive.valid) {
          g_kinematics.reanchor_hand(drive.pose);
        }
        display_pose = g_workspace.snap().hold_pose;
        force_display = true;
        kin_moved = true;
      } else if (was_snapped && !now_pending) {
        g_unsnap_hold = prior_hold;
        g_unsnap_blend = 0.f;
        kin_moved = true;
      } else if (now_pending) {
        display_pose = g_workspace.snap().hold_pose;
        force_display = true;
        kin_moved = true;
      }
    }
  } else if (g_kinematics.state().phase != threeos::KineticPhase::Possessed) {
    clear_unsnap_catchup();
  }

  if (g_unsnap_blend < 1.f &&
      g_kinematics.state().phase == threeos::KineticPhase::Possessed) {
    constexpr float kDt = 1.f / 72.f;
    g_unsnap_blend += kDt / kUnsnapCatchupSeconds;
    if (g_unsnap_blend >= 1.f) {
      g_unsnap_blend = 1.f;
    }
    const float u = smoothstep01(g_unsnap_blend);
    display_pose = lerp_pose(g_unsnap_hold, g_kinematics.state().pose, u);
    force_display = true;
    kin_moved = true;
  }

  if (g_kinematics.state().entity_id != 0 &&
      (g_kinematics.state().phase == threeos::KineticPhase::Possessed ||
       g_kinematics.state().phase == threeos::KineticPhase::Coasting || kin_moved)) {
    if (auto* we = g_workspace.find_mut(g_kinematics.state().entity_id)) {
      we->pose = force_display ? display_pose : g_kinematics.state().pose;
    }
  }

  if (out_delta != nullptr) {
    std::memset(out_delta, 0, sizeof(*out_delta));
    if (g_pending_portal_pose.has_value()) {
      out_delta->entity_id = g_pending_portal_entity;
      out_delta->pose = to_c(*g_pending_portal_pose);
      out_delta->flags = THREEOS_XFORM_PORTAL;
      g_pending_portal_pose.reset();
      g_pending_portal_entity = 0;
    } else if (kin_moved && g_kinematics.state().entity_id != 0) {
      out_delta->entity_id = g_kinematics.state().entity_id;
      out_delta->pose = to_c(force_display ? display_pose : g_kinematics.state().pose);
      out_delta->flags = THREEOS_XFORM_KINEMATIC;
      const float py = force_display ? display_pose.position.y
                                    : g_kinematics.state().pose.position.y;
      if (py <= floor_contact_y + 1e-4f) {
        out_delta->flags |= THREEOS_XFORM_FLOOR_CLAMP;
      }
    } else {
      out_delta->entity_id = 0;
      out_delta->pose = frame->head;
      out_delta->flags = frame->tracking_flags;
    }
  }
}

float voodoo_frame_dt(double time_seconds) {
  static double s_prev_voodoo_t = -1.0;
  float dt = 1.f / 72.f;
  if (s_prev_voodoo_t >= 0.0) {
    const double raw = time_seconds - s_prev_voodoo_t;
    if (raw > 1e-4 && raw < 0.25) {
      dt = static_cast<float>(raw);
    }
  }
  s_prev_voodoo_t = time_seconds;
  return dt;
}

threeos::Pose resolve_voodoo_hand_pose(const ThreeOS_InteropInputFrame* frame, uint32_t hand,
                                       bool left, bool right, bool* out_valid) {
  const bool cursor_ok = g_hands.cursor(hand).active;
  threeos::Pose vhand{};
  if (cursor_ok) {
    vhand.position = g_hands.cursor(hand).position;
  } else if (hand == 0u && left) {
    vhand = from_c(frame->left_aim);
  } else if (hand == 1u && right) {
    vhand = from_c(frame->right_aim);
  }
  *out_valid = cursor_ok || (hand == 0u ? left : right);
  return vhand;
}

float tick_voodoo_frame(const ThreeOS_InteropInputFrame* frame) {
  const float dt = voodoo_frame_dt(frame->time_seconds);
  const bool right = (frame->tracking_flags & THREEOS_TRACK_RIGHT_AIM) != 0;
  const bool left = (frame->tracking_flags & THREEOS_TRACK_LEFT_AIM) != 0;
  const bool head_valid = (frame->tracking_flags & THREEOS_TRACK_HEAD) != 0;
  const threeos::Pose head_pose = from_c(frame->head);
  const bool selection_blocks_voodoo = g_workspace.selection() != 0;
  const bool kin_possessed = g_kinematics.state().phase == threeos::KineticPhase::Possessed;

  for (uint32_t vh = 0; vh < 2u; ++vh) {
    bool hand_ok = false;
    const threeos::Pose vhand = resolve_voodoo_hand_pose(frame, vh, left, right, &hand_ok);
    threeos::InteractionIntents vintents = g_hands.pending_intents(vh);
    const bool hand_busy = selection_blocks_voodoo || g_voodoo.ghost_drag_active(vh) ||
                           (kin_possessed && g_kin_hand_latch == static_cast<int>(vh));
    if (hand_busy) {
      using threeos::InteractionIntentFlags;
      const uint32_t clear = static_cast<uint32_t>(InteractionIntentFlags::VoodooSnap) |
                             static_cast<uint32_t>(InteractionIntentFlags::VoodooFree);
      vintents.flags = static_cast<InteractionIntentFlags>(static_cast<uint32_t>(vintents.flags) &
                                                           ~clear);
      g_voodoo.cancel_open_session(vh);
    }
    g_voodoo.tick_hand(vh, vintents, vhand, hand_ok, head_pose, head_valid, dt, g_centroids,
                       g_workspace);
  }
  // Focus-anchor snap release → agent Multi-Choice at near-BB navel rest (Task 7.3).
  {
    const auto pa = g_voodoo.consume_pending_agent_open();
    if (pa.pending) {
      const threeos::Pose center = g_voodoo.near_rest_pose(head_pose, head_valid);
      g_multichoice.open_agent(pa.hand, center);
    }
  }
  g_voodoo.tick_portals(dt, head_pose, head_valid);
  g_voodoo.tick_pose_lerps(dt, g_workspace);
  g_voodoo.sync_ghosts_from_workspace(g_workspace);
  return dt;
}

void tick_ghost_drags_frame(const ThreeOS_InteropInputFrame* frame, float dt,
                            ThreeOS_InteropTransformDelta* out_delta) {
  const bool right = (frame->tracking_flags & THREEOS_TRACK_RIGHT_AIM) != 0;
  const bool left = (frame->tracking_flags & THREEOS_TRACK_LEFT_AIM) != 0;
  const bool head_valid = (frame->tracking_flags & THREEOS_TRACK_HEAD) != 0;
  const threeos::Pose head_pose = from_c(frame->head);

  for (uint32_t gh = 0; gh < 2u; ++gh) {
    if (!g_voodoo.ghost_drag_active(gh)) {
      continue;
    }
    bool hand_ok = false;
    const threeos::Pose ghand = resolve_voodoo_hand_pose(frame, gh, left, right, &hand_ok);
    g_voodoo.ghost_drag_tick(gh, ghand, hand_ok, head_pose, head_valid, dt, g_workspace);
    if (g_voodoo.ghost_drag_active(gh)) {
      g_voodoo.refresh_ghost_drag_arrows(gh, ghand, head_pose, head_valid);
    }
    g_voodoo.update_ghost_matrix_snap(gh, g_workspace);
    const threeos::EntityId gid = g_voodoo.ghost_drag_entity(gh);
    if (gid != 0 && out_delta != nullptr && out_delta->entity_id == 0) {
      if (const auto* we = g_workspace.find(gid)) {
        out_delta->entity_id = gid;
        out_delta->pose = to_c(we->pose);
        out_delta->flags = THREEOS_XFORM_KINEMATIC;
      }
    }
  }
}

void sync_centroids_and_lerp_delta(ThreeOS_InteropTransformDelta* out_delta) {
  for (const auto& portal : g_voodoo.portals()) {
    if (portal.temporary_centroid_id != 0) {
      const auto& bb = portal.near_bb;
      g_centroids.set_temporary_position(
          portal.temporary_centroid_id,
          threeos::Vec3{bb.pose.position.x, bb.pose.position.y - bb.extents.y * 0.5f,
                        bb.pose.position.z});
    }
  }
  g_centroids.rebuild(g_workspace);

  if (out_delta != nullptr && out_delta->entity_id == 0) {
    for (uint32_t i = 0; i < g_workspace.entries().size(); ++i) {
      const auto& e = g_workspace.entries()[i];
      if (g_voodoo.pose_lerp_active(e.id)) {
        out_delta->entity_id = e.id;
        out_delta->pose = to_c(e.pose);
        out_delta->flags = THREEOS_XFORM_KINEMATIC;
        break;
      }
    }
  }
}

}  // namespace

static_assert(sizeof(ThreeOS_Vec3) == 12, "ThreeOS_Vec3");
static_assert(sizeof(ThreeOS_Quat) == 16, "ThreeOS_Quat");
static_assert(sizeof(ThreeOS_Pose) == 28, "ThreeOS_Pose");
static_assert(sizeof(ThreeOS_InteropInputFrame) == 192, "InteropInputFrame frozen size");
static_assert(sizeof(ThreeOS_InteropTransformDelta) == 48, "InteropTransformDelta frozen size");
static_assert(sizeof(ThreeOS_DomeState) == 48, "DomeState size");
static_assert(sizeof(ThreeOS_DoubleProxyState) == 64, "DoubleProxyState size");
static_assert(sizeof(ThreeOS_KineticState) == 48, "KineticState size");
static_assert(sizeof(ThreeOS_StickDebug) == 64, "StickDebug size");
static_assert(sizeof(ThreeOS_WorkspaceItem) == 200, "WorkspaceItem size");
static_assert(sizeof(ThreeOS_SnapState) == 64, "SnapState size");
static_assert(sizeof(ThreeOS_StorageEvent) == 344, "StorageEvent size");
static_assert(sizeof(ThreeOS_HandSample) == 128, "HandSample size");
static_assert(sizeof(ThreeOS_PointCursor) == 32, "PointCursor size");
static_assert(sizeof(ThreeOS_InteractionIntents) == 16, "InteractionIntents size");
static_assert(sizeof(ThreeOS_VoodooFieldParams) == 32, "VoodooFieldParams size");
static_assert(sizeof(ThreeOS_VoodooBoundingBox) == 80, "VoodooBoundingBox size");
static_assert(sizeof(ThreeOS_VoodooSession) == 176, "VoodooSession size");
static_assert(sizeof(ThreeOS_VoodooDragArrow) == 64, "VoodooDragArrow size");
static_assert(sizeof(ThreeOS_Centroid) == 64, "Centroid size");

extern "C" THREEOS_API uint32_t threeos_version(void) {
  return (kMajor << 16) | (kMinor << 8) | kPatch;
}

extern "C" THREEOS_API uint32_t threeos_abi_version(void) { return kAbiVersion; }

extern "C" THREEOS_API uint32_t threeos_sizeof_input_frame(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_InteropInputFrame));
}

extern "C" THREEOS_API uint32_t threeos_sizeof_transform_delta(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_InteropTransformDelta));
}

extern "C" THREEOS_API uint32_t threeos_sizeof_dome_state(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_DomeState));
}

extern "C" THREEOS_API uint32_t threeos_sizeof_double_proxy_state(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_DoubleProxyState));
}

extern "C" THREEOS_API uint32_t threeos_sizeof_stick_debug(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_StickDebug));
}

extern "C" THREEOS_API uint32_t threeos_sizeof_kinetic_state(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_KineticState));
}

extern "C" THREEOS_API uint32_t threeos_sizeof_workspace_item(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_WorkspaceItem));
}

extern "C" THREEOS_API uint32_t threeos_sizeof_snap_state(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_SnapState));
}

extern "C" THREEOS_API uint32_t threeos_sizeof_storage_event(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_StorageEvent));
}

extern "C" THREEOS_API int32_t threeos_init(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  g_initialized = true;
  g_tick_count = 0;
  g_facade.set_state(threeos::InteractionState::Idle);
  g_topology = threeos::TopologyWorkspace{};
  g_kinematics = threeos::KinematicsEngine{};
  g_glyphs.clear();
  g_workspace.clear();
  g_workspace.set_glyph_registry(&g_glyphs);
  g_pending_portal_pose.reset();
  g_pending_portal_entity = 0;
  g_hands.reset();
  g_voodoo.reset();
  g_centroids.clear();
  g_multichoice.reset();
  g_hands_pushed_this_frame = false;
  set_error("");
  return 0;
}

extern "C" THREEOS_API void threeos_shutdown(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  g_initialized = false;
  g_kinematics.clear();
  g_workspace.clear();
  g_glyphs.clear();
  g_voodoo.reset();
  g_centroids.clear();
  g_multichoice.reset();
  g_pending_portal_pose.reset();
  set_error("");
}

extern "C" THREEOS_API int32_t threeos_tick(const ThreeOS_InteropInputFrame* frame,
                                            ThreeOS_InteropTransformDelta* out_delta) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("threeos_tick called before threeos_init");
    return -1;
  }
  if (frame == nullptr) {
    set_error("threeos_tick: null frame");
    return -2;
  }

  ++g_tick_count;
  sync_facade_interaction_state(frame);
  ensure_hand_samples(frame);
  const bool head_valid = (frame->tracking_flags & THREEOS_TRACK_HEAD) != 0;
  g_hands.tick(from_c(frame->head), head_valid, frame->time_seconds);
  g_hands_pushed_this_frame = false;

  const DriveHand drive = resolve_drive_hand(frame);
  tick_possess_snap_pipeline(frame, drive, out_delta);
  g_centroids.rebuild(g_workspace);
  const float voodoo_dt = tick_voodoo_frame(frame);
  tick_ghost_drags_frame(frame, voodoo_dt, out_delta);
  sync_centroids_and_lerp_delta(out_delta);

  set_error("");
  return 0;
}

extern "C" THREEOS_API const char* threeos_last_error(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  return g_last_error.c_str();
}

extern "C" THREEOS_API int32_t threeos_topology_set_scale(float world_to_proxy_ratio) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (!(world_to_proxy_ratio > 0.f)) {
    set_error("scale must be > 0");
    return -2;
  }
  g_topology.set_world_to_proxy_scale(world_to_proxy_ratio);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_open_dome(const ThreeOS_Pose* head_pose) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (head_pose == nullptr) {
    set_error("null head_pose");
    return -2;
  }
  g_topology.open_dome(from_c(*head_pose));
  set_error("");
  return 0;
}

extern "C" THREEOS_API void threeos_topology_close_dome(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (g_initialized) {
    g_topology.close_dome();
  }
}

extern "C" THREEOS_API int32_t threeos_topology_get_dome(ThreeOS_DomeState* out_dome) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (out_dome == nullptr) {
    set_error("null out_dome");
    return -2;
  }
  fill_dome(out_dome);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_open_double_proxy(uint64_t id,
                                                                 const ThreeOS_Aabb* far_aabb,
                                                                 const ThreeOS_Pose* head_pose) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (far_aabb == nullptr || head_pose == nullptr) {
    set_error("null far_aabb or head_pose");
    return -2;
  }
  if (!g_topology.open_double_proxy(id, from_c(*far_aabb), from_c(*head_pose))) {
    set_error("open_double_proxy failed");
    return -3;
  }
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_close_double_proxy(uint64_t id) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (!g_topology.close_double_proxy(id)) {
    set_error("proxy not found");
    return -2;
  }
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_get_double_proxy(
    uint64_t id, ThreeOS_DoubleProxyState* out_proxy) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (out_proxy == nullptr) {
    set_error("null out_proxy");
    return -2;
  }
  const auto* p = g_topology.find_double_proxy(id);
  if (p == nullptr) {
    set_error("proxy not found");
    return -3;
  }
  fill_proxy(*p, out_proxy);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_select_dome_region(
    uint64_t proxy_id, ThreeOS_VoxelCoord far_min, ThreeOS_VoxelCoord far_max,
    const ThreeOS_Pose* head_pose) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (head_pose == nullptr) {
    set_error("null head_pose");
    return -2;
  }
  threeos::VoxelCoord vmin{far_min.x, far_min.y, far_min.z};
  threeos::VoxelCoord vmax{far_max.x, far_max.y, far_max.z};
  if (!g_topology.select_dome_region(proxy_id, vmin, vmax, from_c(*head_pose))) {
    set_error("select_dome_region failed (dome closed?)");
    return -3;
  }
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_portal_near_to_far(uint64_t proxy_id,
                                                                  const ThreeOS_Pose* near_pose,
                                                                  ThreeOS_Pose* out_far) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (near_pose == nullptr || out_far == nullptr) {
    set_error("null pose");
    return -2;
  }
  const auto result = g_topology.portal_near_to_far(proxy_id, from_c(*near_pose));
  if (!result) {
    set_error("portal_near_to_far: not in near volume or missing proxy");
    return -3;
  }
  *out_far = to_c(*result);
  g_pending_portal_pose = *result;
  g_pending_portal_entity = proxy_id;
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_portal_far_to_near(uint64_t proxy_id,
                                                                  const ThreeOS_Pose* far_pose,
                                                                  ThreeOS_Pose* out_near) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (far_pose == nullptr || out_near == nullptr) {
    set_error("null pose");
    return -2;
  }
  const auto result = g_topology.portal_far_to_near(proxy_id, from_c(*far_pose));
  if (!result) {
    set_error("portal_far_to_near: not in far volume or missing proxy");
    return -3;
  }
  *out_near = to_c(*result);
  g_pending_portal_pose = *result;
  g_pending_portal_entity = proxy_id;
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_portal_cross(uint64_t from_proxy_id,
                                                            uint64_t to_proxy_id,
                                                            const ThreeOS_Pose* near_in_to,
                                                            ThreeOS_Pose* out_far) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (near_in_to == nullptr || out_far == nullptr) {
    set_error("null pose");
    return -2;
  }
  const auto result = g_topology.portal_cross(from_proxy_id, to_proxy_id, from_c(*near_in_to));
  if (!result) {
    set_error("portal_cross failed");
    return -3;
  }
  *out_far = to_c(*result);
  g_pending_portal_pose = *result;
  g_pending_portal_entity = to_proxy_id;
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_dome_drop(const ThreeOS_Pose* dome_local_pose,
                                                         ThreeOS_Pose* out_far) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (dome_local_pose == nullptr || out_far == nullptr) {
    set_error("null pose");
    return -2;
  }
  if (!g_topology.dome().active) {
    set_error("dome not open");
    return -3;
  }
  const auto result = g_topology.dome_drop(from_c(*dome_local_pose));
  *out_far = to_c(result);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_far_to_near(uint64_t proxy_id,
                                                           const ThreeOS_Vec3* far_world,
                                                           ThreeOS_Vec3* out_near) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized || far_world == nullptr || out_near == nullptr) {
    set_error("bad args");
    return -1;
  }
  *out_near = to_c(g_topology.far_to_near(proxy_id, from_c(*far_world)));
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_near_to_far(uint64_t proxy_id,
                                                           const ThreeOS_Vec3* near_world,
                                                           ThreeOS_Vec3* out_far) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized || near_world == nullptr || out_far == nullptr) {
    set_error("bad args");
    return -1;
  }
  *out_far = to_c(g_topology.near_to_far(proxy_id, from_c(*near_world)));
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_world_to_voxel(const ThreeOS_Vec3* world,
                                                              ThreeOS_VoxelCoord* out_voxel) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized || world == nullptr || out_voxel == nullptr) {
    set_error("bad args");
    return -1;
  }
  const auto vc = g_topology.world_to_voxel(from_c(*world));
  out_voxel->x = vc.x;
  out_voxel->y = vc.y;
  out_voxel->z = vc.z;
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_voxel_to_world(ThreeOS_VoxelCoord voxel,
                                                              ThreeOS_Vec3* out_world) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized || out_world == nullptr) {
    set_error("bad args");
    return -1;
  }
  *out_world =
      to_c(g_topology.voxel_center_to_world(threeos::VoxelCoord{voxel.x, voxel.y, voxel.z}));
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_kinematics_set_params(float gain, float deadzone_m,
                                                            float friction, float floor_y) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (!(gain > 0.f) || !(deadzone_m >= 0.f) || !(friction >= 0.f)) {
    set_error("invalid kinematics params");
    return -2;
  }
  auto p = g_kinematics.params();
  p.gain = gain;
  p.deadzone_m = deadzone_m;
  p.friction = friction;
  p.floor_y = floor_y;
  g_kinematics.set_params(p);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_kinematics_possess(uint64_t entity_id,
                                                         const ThreeOS_Pose* object_pose) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (entity_id == 0 || object_pose == nullptr) {
    set_error("bad possess args");
    return -2;
  }
  g_kinematics.possess(entity_id, from_c(*object_pose));
  g_facade.set_state(threeos::InteractionState::Possessed);
  g_kin_hand_latch = -1;
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_kinematics_possess_with_hand(uint64_t entity_id,
                                                                   const ThreeOS_Pose* object_pose,
                                                                   uint32_t hand) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (entity_id == 0 || object_pose == nullptr) {
    set_error("bad possess args");
    return -2;
  }
  if (hand > 1u) {
    set_error("drive hand must be 0 or 1");
    return -2;
  }
  g_kinematics.possess(entity_id, from_c(*object_pose));
  g_facade.set_state(threeos::InteractionState::Possessed);
  g_kin_hand_latch = static_cast<int>(hand);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_kinematics_set_drive_hand(uint32_t hand) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (hand > 1u) {
    set_error("drive hand must be 0 or 1");
    return -2;
  }
  const int prev = g_kin_hand_latch;
  g_kin_hand_latch = static_cast<int>(hand);
  // Re-anchor only when the drive hand *changes*. Calling this every frame while
  // possessed used to freeze start_hand to the current cursor → zero stick / no motion.
  if (prev != g_kin_hand_latch &&
      g_kinematics.state().phase == threeos::KineticPhase::Possessed &&
      g_hands.cursor(hand).active) {
    threeos::Pose p{};
    p.position = g_hands.cursor(hand).position;
    g_kinematics.reanchor_hand(p);
  }
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_kinematics_release(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  g_kin_hand_latch = -1;
  // Drop-into-box: releasing while snapped commits insert (no coast).
  if (g_workspace.snap().pending) {
    const threeos::EntityId inserted = g_workspace.snap().object_id;
    std::string err;
    if (!g_workspace.commit_insert(&err)) {
      set_error(err.c_str());
      // Keep pose parked on the hold; do not coast away from the snap.
      // Host should poll/ack any prior FS event, then retry commit.
      g_kinematics.zero_velocity();
      g_kinematics.clear();
      return -3;
    }
    const auto temps = g_voodoo.on_entity_inserted(inserted);
    for (const auto temp : temps) {
      g_centroids.clear_temporary(temp);
    }
    g_centroids.rebuild(g_workspace);
    g_kinematics.clear();
    set_error("");
    return 0;
  }
  g_kinematics.release();
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_kinematics_get_state(ThreeOS_KineticState* out_state) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (out_state == nullptr) {
    set_error("null out_state");
    return -2;
  }
  std::memset(out_state, 0, sizeof(*out_state));
  const auto& s = g_kinematics.state();
  out_state->entity_id = s.entity_id;
  out_state->phase = static_cast<uint32_t>(s.phase);
  out_state->vel_x = s.velocity.x;
  out_state->vel_y = s.velocity.y;
  out_state->vel_z = s.velocity.z;
  out_state->floor_y = g_kinematics.params().floor_y;
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_kinematics_get_stick_debug(ThreeOS_StickDebug* out_debug) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (out_debug == nullptr) {
    set_error("null out_debug");
    return -2;
  }
  std::memset(out_debug, 0, sizeof(*out_debug));
  const auto& d = g_kinematics.stick_debug();
  out_debug->active = d.active ? 1u : 0u;
  out_debug->in_deadzone = d.in_deadzone ? 1u : 0u;
  out_debug->magnitude = d.magnitude;
  out_debug->stick = to_c(d.stick);
  out_debug->ray_origin = to_c(d.ray_origin);
  out_debug->ray_tip = to_c(d.ray_tip);
  out_debug->deadzone_armed = d.deadzone_armed ? 1u : 0u;
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_kinematics_set_object_pose(const ThreeOS_Pose* object_pose) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (object_pose == nullptr) {
    set_error("null pose");
    return -2;
  }
  g_kinematics.set_pose(from_c(*object_pose));
  set_error("");
  return 0;
}

namespace {

void copy_cstr(char* dst, size_t dst_len, const std::string& src) {
  if (dst == nullptr || dst_len == 0) {
    return;
  }
  std::snprintf(dst, dst_len, "%s", src.c_str());
}

void fill_workspace_item(const threeos::WorkspaceEntry& e, ThreeOS_WorkspaceItem* out) {
  std::memset(out, 0, sizeof(*out));
  out->entity_id = e.id;
  out->kind = static_cast<uint32_t>(e.kind);
  out->flags = 0;
  if (e.in_field) {
    out->flags |= 1u;
  }
  if (g_workspace.snap().pending && g_workspace.snap().object_id == e.id) {
    out->flags |= 2u;
  }
  if (e.kind == threeos::StorageKind::Box && e.expanded) {
    out->flags |= 4u;
  }
  out->pose = to_c(e.pose);
  out->child_count = e.child_count;
  out->box_visual = 0;
  if (e.kind == threeos::StorageKind::Box) {
    out->box_visual = (e.expanded || e.glyph_id == "box_open") ? 2u : 1u;
  }
  copy_cstr(out->name, sizeof(out->name), e.name);
  if (e.kind == threeos::StorageKind::Box) {
    char sec[32];
    if (e.child_count == 1) {
      std::snprintf(sec, sizeof(sec), "1 object");
    } else {
      std::snprintf(sec, sizeof(sec), "%u objects", e.child_count);
    }
    copy_cstr(out->secondary_label, sizeof(out->secondary_label), sec);
  }
  copy_cstr(out->glyph_id, sizeof(out->glyph_id), e.glyph_id);
  out->tint_r = 1.f;
  out->tint_g = 1.f;
  out->tint_b = 1.f;
  out->tint_a = 0.55f;
  if (const auto* g = g_glyphs.find_by_id(e.glyph_id)) {
    out->tint_r = g->tint_rgba[0];
    out->tint_g = g->tint_rgba[1];
    out->tint_b = g->tint_rgba[2];
    out->tint_a = g->tint_rgba[3];
  }
}

}  // namespace

extern "C" THREEOS_API void threeos_storage_clear(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    return;
  }
  g_workspace.clear();
  g_workspace.set_glyph_registry(&g_glyphs);
  g_voodoo.reset();  // portals + temp centroids tied to workspace
  g_centroids.clear();
  g_centroids.rebuild(g_workspace);
}

extern "C" THREEOS_API int32_t threeos_storage_add_object(const char* name, const char* relative_path,
                                                         uint64_t* out_entity_id) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (name == nullptr || relative_path == nullptr) {
    set_error("null name/path");
    return -2;
  }
  const auto id = g_workspace.add_object(name, relative_path);
  g_workspace.resolve_glyphs();
  if (out_entity_id) {
    *out_entity_id = id;
  }
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_storage_add_box(const char* name, const char* relative_path,
                                                      uint32_t child_count, uint64_t* out_entity_id) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (name == nullptr || relative_path == nullptr) {
    set_error("null name/path");
    return -2;
  }
  const auto id = g_workspace.add_box(name, relative_path, child_count);
  g_workspace.resolve_glyphs();
  if (out_entity_id) {
    *out_entity_id = id;
  }
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_storage_layout_demo(const ThreeOS_Pose* head_pose) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  threeos::Pose head{};
  head.position = threeos::Vec3{0.f, 1.5f, 0.f};
  head.orientation = threeos::Quat{0.f, 0.f, 0.f, 1.f};
  if (head_pose != nullptr) {
    head = from_c(*head_pose);
  }
  g_workspace.layout_demo_field(head);
  g_workspace.resolve_glyphs();
  set_error("");
  return 0;
}

extern "C" THREEOS_API uint32_t threeos_storage_item_count(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    return 0;
  }
  return static_cast<uint32_t>(g_workspace.entries().size());
}

extern "C" THREEOS_API int32_t threeos_storage_get_item(uint32_t index,
                                                       ThreeOS_WorkspaceItem* out_item) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (out_item == nullptr) {
    set_error("null out_item");
    return -2;
  }
  if (index >= g_workspace.entries().size()) {
    set_error("index out of range");
    return -3;
  }
  fill_workspace_item(g_workspace.entries()[index], out_item);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_storage_get_snap(ThreeOS_SnapState* out_snap) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (out_snap == nullptr) {
    set_error("null out_snap");
    return -2;
  }
  std::memset(out_snap, 0, sizeof(*out_snap));
  const auto& s = g_workspace.snap();
  out_snap->pending = s.pending ? 1u : 0u;
  out_snap->object_id = s.object_id;
  out_snap->box_id = s.box_id;
  out_snap->hold_pose = to_c(s.hold_pose);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_storage_try_commit_insert(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  const threeos::EntityId inserted = g_workspace.snap().object_id;
  std::string err;
  if (!g_workspace.commit_insert(&err)) {
    set_error(err.c_str());
    return -2;
  }
  const auto temps = g_voodoo.on_entity_inserted(inserted);
  for (const auto temp : temps) {
    g_centroids.clear_temporary(temp);
  }
  g_centroids.rebuild(g_workspace);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_storage_insert_into(uint64_t object_id, uint64_t box_id) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  std::string err;
  if (!g_workspace.insert_into(object_id, box_id, &err)) {
    set_error(err.c_str());
    return -2;
  }
  const auto temps = g_voodoo.on_entity_inserted(object_id);
  for (const auto temp : temps) {
    g_centroids.clear_temporary(temp);
  }
  g_centroids.rebuild(g_workspace);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_storage_poll_event(ThreeOS_StorageEvent* out_event) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (out_event == nullptr) {
    set_error("null out_event");
    return -2;
  }
  std::memset(out_event, 0, sizeof(*out_event));
  const auto& e = g_workspace.pending_event();
  out_event->type = static_cast<uint32_t>(e.type);
  out_event->awaiting_ack = e.awaiting_ack ? 1u : 0u;
  out_event->object_id = e.object_id;
  out_event->box_id = e.box_id;
  copy_cstr(out_event->source_rel, sizeof(out_event->source_rel), e.source_rel);
  copy_cstr(out_event->dest_rel, sizeof(out_event->dest_rel), e.dest_rel);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_storage_ack_event(int32_t success) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  g_workspace.ack_event(success != 0);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_storage_set_box_open(uint64_t box_id, int32_t open) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  g_workspace.set_box_visual_open(box_id, open != 0);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_storage_set_parent_box(uint64_t entity_id,
                                                              uint64_t parent_box_id) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (!g_workspace.set_parent_box(entity_id, parent_box_id)) {
    set_error("set_parent_box failed");
    return -2;
  }
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_storage_set_in_field(uint64_t entity_id, int32_t in_field) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (!g_workspace.set_in_field(entity_id, in_field != 0)) {
    set_error("set_in_field failed");
    return -2;
  }
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_storage_get_parent_box(uint64_t entity_id,
                                                             uint64_t* out_parent_box_id) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (out_parent_box_id == nullptr) {
    set_error("null out_parent_box_id");
    return -2;
  }
  const auto* e = g_workspace.find(entity_id);
  if (e == nullptr) {
    set_error("unknown entity");
    return -3;
  }
  *out_parent_box_id = e->parent_box;
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_storage_expand_box(uint64_t box_id) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  std::string err;
  if (!g_workspace.layout_box_expand(box_id, &err)) {
    set_error(err.c_str());
    return -2;
  }
  g_workspace.resolve_glyphs();
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_storage_collapse_box(uint64_t box_id) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  std::string err;
  if (!g_workspace.collapse_box(box_id, &err)) {
    set_error(err.c_str());
    return -2;
  }
  g_workspace.resolve_glyphs();
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_glyph_install_pack(const uint8_t* bytes, uint32_t byte_count) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (bytes == nullptr || byte_count == 0) {
    set_error("empty pack");
    return -2;
  }
  std::string err;
  if (!g_glyphs.install_pack(bytes, byte_count, &err)) {
    set_error(err.c_str());
    return -3;
  }
  g_workspace.resolve_glyphs();
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_selection_get(uint64_t* out_entity_id) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (out_entity_id == nullptr) {
    set_error("null out_entity_id");
    return -2;
  }
  *out_entity_id = g_workspace.selection();
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_selection_set(uint64_t entity_id) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (!g_workspace.set_selection(entity_id)) {
    set_error("unknown entity");
    return -2;
  }
  set_error("");
  return 0;
}

extern "C" THREEOS_API void threeos_selection_clear(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    return;
  }
  g_workspace.clear_selection();
  set_error("");
}

namespace {

threeos::HandSample from_c_hand(const ThreeOS_HandSample* in) {
  threeos::HandSample s{};
  if (in == nullptr || in->tracked == 0) {
    return s;
  }
  s.tracked = true;
  s.tip_flags = in->tip_flags;
  s.thumb_tip = from_c(in->thumb_tip);
  s.index_tip = from_c(in->index_tip);
  s.middle_tip = from_c(in->middle_tip);
  s.ring_tip = from_c(in->ring_tip);
  s.palm_normal = from_c(in->palm_normal);
  s.palm_mode_valid = in->palm_mode_valid != 0;
  s.palm_mode = static_cast<threeos::PalmMode>(in->palm_mode);
  s.primary_value = in->primary_value;
  s.secondary_value = in->secondary_value;
  s.ternary_value = in->ternary_value;
  s.primary_pressed = in->primary_pressed != 0;
  s.secondary_pressed = in->secondary_pressed != 0;
  s.ternary_pressed = in->ternary_pressed != 0;
  s.index_proximal = from_c(in->index_proximal);
  return s;
}

}  // namespace

extern "C" THREEOS_API uint32_t threeos_sizeof_hand_sample(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_HandSample));
}
extern "C" THREEOS_API uint32_t threeos_sizeof_point_cursor(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_PointCursor));
}
extern "C" THREEOS_API uint32_t threeos_sizeof_interaction_intents(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_InteractionIntents));
}

extern "C" THREEOS_API int32_t threeos_interaction_set_hands(const ThreeOS_HandSample* left,
                                                             const ThreeOS_HandSample* right) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  g_hands.set_hands(from_c_hand(left), from_c_hand(right));
  g_hands_pushed_this_frame = true;
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_interaction_get_cursor(uint32_t hand,
                                                              ThreeOS_PointCursor* out_cursor) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (out_cursor == nullptr || hand > 1u) {
    set_error("bad cursor args");
    return -2;
  }
  const auto& c = g_hands.cursor(hand);
  std::memset(out_cursor, 0, sizeof(*out_cursor));
  out_cursor->active = c.active ? 1u : 0u;
  out_cursor->driving_channel = static_cast<uint32_t>(c.driving_channel);
  out_cursor->scale = c.scale;
  out_cursor->driving_distance_m = c.driving_distance_m;
  out_cursor->position = to_c(c.position);
  out_cursor->palm_mode = static_cast<uint32_t>(c.palm_mode);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_interaction_get_intents(
    ThreeOS_InteractionIntents* out_intents) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (out_intents == nullptr) {
    set_error("null intents");
    return -2;
  }
  const auto& i = g_hands.intents();
  out_intents->flags = static_cast<uint32_t>(i.flags);
  out_intents->palm_mode = static_cast<uint32_t>(i.active_palm);
  out_intents->active_channel = static_cast<uint32_t>(i.active_channel);
  out_intents->handedness = i.handedness;
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_interaction_set_pinch_thresh_m(float meters) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  g_hands.set_pinch_thresh_m(meters);
  set_error("");
  return 0;
}

extern "C" THREEOS_API uint32_t threeos_sizeof_voodoo_field_params(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_VoodooFieldParams));
}
extern "C" THREEOS_API uint32_t threeos_sizeof_voodoo_bounding_box(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_VoodooBoundingBox));
}
extern "C" THREEOS_API uint32_t threeos_sizeof_voodoo_session(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_VoodooSession));
}
extern "C" THREEOS_API uint32_t threeos_sizeof_voodoo_drag_arrow(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_VoodooDragArrow));
}

extern "C" THREEOS_API int32_t threeos_voodoo_get_field_params(ThreeOS_VoodooFieldParams* out_params) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (out_params == nullptr) {
    set_error("null params");
    return -2;
  }
  const auto& p = g_voodoo.params();
  std::memset(out_params, 0, sizeof(*out_params));
  out_params->voxel_size_m = p.voxel_size_m;
  out_params->near_bb_voxels = {p.near_bb_voxels_x, p.near_bb_voxels_y, p.near_bb_voxels_z};
  out_params->field_radius_m = p.field_radius_m;
  out_params->field_height_m = p.field_height_m;
  out_params->arm_reach_m = p.arm_reach_m;
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_voodoo_set_field_params(const ThreeOS_VoodooFieldParams* params) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (params == nullptr) {
    set_error("null params");
    return -2;
  }
  threeos::VoodooFieldParams p{};
  p.voxel_size_m = params->voxel_size_m;
  p.near_bb_voxels_x = params->near_bb_voxels.x;
  p.near_bb_voxels_y = params->near_bb_voxels.y;
  p.near_bb_voxels_z = params->near_bb_voxels.z;
  p.field_radius_m = params->field_radius_m;
  p.field_height_m = params->field_height_m;
  p.arm_reach_m = params->arm_reach_m > 0.f ? params->arm_reach_m : 0.3048f;
  if (!g_voodoo.set_params(p)) {
    set_error("voodoo field params out of range");
    return -3;
  }
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_voodoo_set_enclosure_boxes(
    const ThreeOS_VoodooEnclosureBox* boxes, uint32_t count) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (count > 0 && boxes == nullptr) {
    set_error("null enclosure boxes");
    return -2;
  }
  if (count == 0) {
    g_voodoo.clear_enclosure_boxes();
    set_error("");
    return 0;
  }
  threeos::VoodooVolumeEngine::EnclosureBox tmp[threeos::VoodooVolumeEngine::kMaxEnclosureBoxes];
  const uint32_t n = std::min(count, threeos::VoodooVolumeEngine::kMaxEnclosureBoxes);
  for (uint32_t i = 0; i < n; ++i) {
    tmp[i].center = from_c(boxes[i].center);
    tmp[i].half_extents = from_c(boxes[i].half_extents);
    tmp[i].orientation = from_c(boxes[i].orientation);
    tmp[i].kind = boxes[i].kind;
  }
  g_voodoo.set_enclosure_boxes(tmp, n);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_voodoo_set_room_bounds(const ThreeOS_Vec3* min_xyz,
                                                              const ThreeOS_Vec3* max_xyz,
                                                              int32_t enabled) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (!enabled) {
    g_voodoo.clear_room_bounds();
    set_error("");
    return 0;
  }
  if (min_xyz == nullptr || max_xyz == nullptr) {
    set_error("null room bounds");
    return -2;
  }
  g_voodoo.set_room_bounds(from_c(*min_xyz), from_c(*max_xyz), true);
  set_error("");
  return 0;
}

static void fill_voodoo_bb(ThreeOS_VoodooBoundingBox* out, const threeos::VoodooBoundingBox& b) {
  std::memset(out, 0, sizeof(*out));
  out->open = b.open ? 1u : 0u;
  out->pose = to_c(b.pose);
  out->extents = to_c(b.extents);
  out->aspect_voxels = {b.aspect_x, b.aspect_y, b.aspect_z};
  out->linked_centroid_id = b.linked_centroid_id;
}

extern "C" THREEOS_API int32_t threeos_voodoo_get_session(uint32_t hand,
                                                          ThreeOS_VoodooSession* out_session) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (hand > 1u) {
    set_error("hand out of range");
    return -3;
  }
  if (out_session == nullptr) {
    set_error("null session");
    return -2;
  }
  const auto& s = g_voodoo.session(hand);
  std::memset(out_session, 0, sizeof(*out_session));
  out_session->mode = static_cast<uint32_t>(s.mode);
  out_session->open = s.open ? 1u : 0u;
  out_session->hand = s.hand;
  fill_voodoo_bb(&out_session->far_bb, s.far_bb);
  fill_voodoo_bb(&out_session->near_bb, s.near_bb);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_voodoo_get_drag_arrow(uint32_t hand,
                                                             ThreeOS_VoodooDragArrow* out_arrow) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (hand > 1u) {
    set_error("hand out of range");
    return -3;
  }
  if (out_arrow == nullptr) {
    set_error("null arrow");
    return -2;
  }
  // Ghost-drag velocity: yellow far twin lives on the ghost-drag state.
  const auto& a = g_voodoo.ghost_drag_active(hand) ? g_voodoo.ghost_drag_far_arrow(hand)
                                                   : g_voodoo.session(hand).arrow;
  std::memset(out_arrow, 0, sizeof(*out_arrow));
  out_arrow->active = a.active ? 1u : 0u;
  out_arrow->in_deadzone = a.in_deadzone ? 1u : 0u;
  out_arrow->magnitude = a.magnitude;
  out_arrow->stick = to_c(a.stick);
  out_arrow->ray_origin = to_c(a.ray_origin);
  out_arrow->ray_tip = to_c(a.ray_tip);
  out_arrow->deadzone_armed = a.deadzone_armed ? 1u : 0u;
  set_error("");
  return 0;
}

extern "C" THREEOS_API uint32_t threeos_sizeof_centroid(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_Centroid));
}

extern "C" THREEOS_API void threeos_mind_palace_clear(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    return;
  }
  g_centroids.clear_mind_palaces();
  g_centroids.rebuild(g_workspace);
}

extern "C" THREEOS_API int32_t threeos_mind_palace_register(const char* name, const ThreeOS_Pose* anchor,
                                                    uint64_t* out_mind_palace_id) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (name == nullptr || out_mind_palace_id == nullptr) {
    set_error("null mind palace args");
    return -2;
  }
  threeos::Pose a{};
  if (anchor != nullptr) {
    a = from_c(*anchor);
  }
  const auto id = g_centroids.register_mind_palace(name, a);
  if (id == 0) {
    set_error("mind palace register failed");
    return -3;
  }
  *out_mind_palace_id = id;
  g_centroids.rebuild(g_workspace);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_mind_palace_set_anchor(uint64_t mind_palace_id, const ThreeOS_Pose* anchor) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (anchor == nullptr) {
    set_error("null anchor");
    return -2;
  }
  if (!g_centroids.set_mind_palace_anchor(mind_palace_id, from_c(*anchor))) {
    set_error("unknown mind palace");
    return -3;
  }
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_storage_set_mind_palace(uint64_t entity_id,
                                                            uint64_t mind_palace_id) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (!g_workspace.set_mind_palace(entity_id, mind_palace_id)) {
    set_error("set_mind_palace failed");
    return -2;
  }
  g_centroids.rebuild(g_workspace);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_storage_layout_mind_palace(uint64_t mind_palace_id,
                                                          const ThreeOS_Pose* center) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (center == nullptr) {
    set_error("null center");
    return -2;
  }
  if (!g_workspace.layout_mind_palace_field(mind_palace_id, from_c(*center))) {
    set_error("layout_mind_palace empty or unknown");
    return -3;
  }
  g_workspace.resolve_glyphs();
  g_centroids.rebuild(g_workspace);
  set_error("");
  return 0;
}

extern "C" THREEOS_API uint32_t threeos_centroid_count(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    return 0;
  }
  return static_cast<uint32_t>(g_centroids.centroids().size());
}

extern "C" THREEOS_API int32_t threeos_centroid_get(uint32_t index, ThreeOS_Centroid* out_centroid) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (out_centroid == nullptr) {
    set_error("null centroid");
    return -2;
  }
  if (index >= g_centroids.centroids().size()) {
    set_error("centroid index oob");
    return -3;
  }
  const auto& c = g_centroids.centroids()[index];
  std::memset(out_centroid, 0, sizeof(*out_centroid));
  out_centroid->id = c.id;
  out_centroid->kind = static_cast<uint32_t>(c.kind);
  out_centroid->member_count = c.member_count;
  out_centroid->linked_entity_id = c.linked_entity_id;
  out_centroid->position = to_c(c.position);
  if (!c.name.empty()) {
    std::strncpy(out_centroid->name, c.name.c_str(), sizeof(out_centroid->name) - 1);
  }
  set_error("");
  return 0;
}

static_assert(sizeof(ThreeOS_VoodooSnapNode) == 48, "VoodooSnapNode size");
static_assert(sizeof(ThreeOS_VoodooPortal) == 200, "VoodooPortal size");
static_assert(sizeof(ThreeOS_VoodooGhost) == 88, "VoodooGhost size");

extern "C" THREEOS_API uint32_t threeos_sizeof_voodoo_snap_node(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_VoodooSnapNode));
}

extern "C" THREEOS_API uint32_t threeos_voodoo_snap_node_count(uint32_t hand) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized || hand > 1u) {
    return 0;
  }
  return static_cast<uint32_t>(g_voodoo.session(hand).snap_nodes.size());
}

extern "C" THREEOS_API int32_t threeos_voodoo_get_snap_node(uint32_t hand, uint32_t index,
                                                            ThreeOS_VoodooSnapNode* out_node) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (hand > 1u) {
    set_error("hand out of range");
    return -3;
  }
  if (out_node == nullptr) {
    set_error("null snap node");
    return -2;
  }
  const auto& nodes = g_voodoo.session(hand).snap_nodes;
  if (index >= nodes.size()) {
    set_error("snap node index oob");
    return -4;
  }
  const auto& n = nodes[index];
  std::memset(out_node, 0, sizeof(*out_node));
  out_node->centroid_id = n.centroid_id;
  out_node->kind = static_cast<uint32_t>(n.kind);
  out_node->snapped = n.snapped ? 1u : 0u;
  out_node->world_position = to_c(n.world_position);
  out_node->radius_m = n.radius_m;
  out_node->field_position = to_c(n.field_position);
  set_error("");
  return 0;
}

extern "C" THREEOS_API uint32_t threeos_sizeof_voodoo_portal(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_VoodooPortal));
}

extern "C" THREEOS_API uint32_t threeos_sizeof_voodoo_ghost(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_VoodooGhost));
}

extern "C" THREEOS_API uint32_t threeos_voodoo_portal_count(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    return 0;
  }
  return static_cast<uint32_t>(g_voodoo.portals().size());
}

extern "C" THREEOS_API int32_t threeos_voodoo_get_portal(uint32_t index,
                                                         ThreeOS_VoodooPortal* out_portal) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (out_portal == nullptr) {
    set_error("null portal");
    return -2;
  }
  if (index >= g_voodoo.portals().size()) {
    set_error("portal index oob");
    return -3;
  }
  const auto& p = g_voodoo.portals()[index];
  std::memset(out_portal, 0, sizeof(*out_portal));
  out_portal->id = p.id;
  out_portal->mode = static_cast<uint32_t>(p.mode);
  out_portal->creating_hand = p.creating_hand;
  fill_voodoo_bb(&out_portal->far_bb, p.far_bb);
  fill_voodoo_bb(&out_portal->near_bb, p.near_bb);
  out_portal->persistent_centroid_id = p.persistent_centroid_id;
  out_portal->temporary_centroid_id = p.temporary_centroid_id;
  out_portal->ghost_count = static_cast<uint32_t>(p.ghosts.size());
  set_error("");
  return 0;
}

extern "C" THREEOS_API uint32_t threeos_voodoo_portal_ghost_count(uint64_t portal_id) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    return 0;
  }
  for (const auto& p : g_voodoo.portals()) {
    if (p.id == portal_id) {
      return static_cast<uint32_t>(p.ghosts.size());
    }
  }
  return 0;
}

extern "C" THREEOS_API int32_t threeos_voodoo_get_portal_ghost(uint64_t portal_id, uint32_t index,
                                                               ThreeOS_VoodooGhost* out_ghost) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (out_ghost == nullptr) {
    set_error("null ghost");
    return -2;
  }
  const threeos::VoodooPortal* portal = nullptr;
  for (const auto& p : g_voodoo.portals()) {
    if (p.id == portal_id) {
      portal = &p;
      break;
    }
  }
  if (portal == nullptr) {
    set_error("portal not found");
    return -3;
  }
  if (index >= portal->ghosts.size()) {
    set_error("ghost index oob");
    return -4;
  }
  const auto& g = portal->ghosts[index];
  std::memset(out_ghost, 0, sizeof(*out_ghost));
  out_ghost->entity_id = g.entity_id;
  out_ghost->is_parent_box = g.is_parent_box ? 1u : 0u;
  out_ghost->far_pose = to_c(g.far_pose);
  out_ghost->near_pose = to_c(g.near_pose);
  // Cut ducks opacity on the host (unused0 flag); keep geometric scale intact.
  const bool cut = !g.is_parent_box && g_voodoo.cut_portal_id() == portal_id;
  out_ghost->scale = g.scale;
  out_ghost->unused0 = cut ? 1.f : 0.f;
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_voodoo_dismiss_portal(uint64_t portal_id) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  uint64_t temp = 0;
  for (const auto& p : g_voodoo.portals()) {
    if (p.id == portal_id) {
      temp = p.temporary_centroid_id;
      break;
    }
  }
  if (!g_voodoo.dismiss_portal(portal_id)) {
    set_error("portal not found");
    return -2;
  }
  if (temp != 0) {
    g_centroids.clear_temporary(temp);
  }
  set_error("");
  return 0;
}

extern "C" THREEOS_API uint32_t threeos_voodoo_creation_allowed(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    return 0;
  }
  return g_voodoo.creation_allowed() ? 1u : 0u;
}

extern "C" THREEOS_API uint32_t threeos_voodoo_point_in_near_bb(float x, float y, float z) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    return 0;
  }
  return g_voodoo.point_in_any_near_bb(threeos::Vec3{x, y, z}) ? 1u : 0u;
}

extern "C" THREEOS_API uint64_t threeos_voodoo_portal_at_near_point(float x, float y, float z) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    return 0;
  }
  return g_voodoo.portal_at_near_point(threeos::Vec3{x, y, z});
}

extern "C" THREEOS_API uint64_t threeos_voodoo_portal_for_ghost(uint64_t entity_id) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    return 0;
  }
  return g_voodoo.portal_for_ghost_entity(entity_id);
}

extern "C" THREEOS_API int32_t threeos_voodoo_ghost_drag_begin(uint32_t hand, uint64_t entity_id) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (hand > 1u) {
    set_error("hand out of range");
    return -3;
  }
  threeos::Pose hand_pose{};
  bool hand_valid = false;
  if (g_hands.cursor(hand).active) {
    hand_pose.position = g_hands.cursor(hand).position;
    hand_valid = true;
  }
  if (!hand_valid) {
    set_error("no hand cursor");
    return -4;
  }
  if (!g_voodoo.ghost_drag_begin(hand, entity_id, hand_pose, g_workspace)) {
    set_error("ghost drag begin refused");
    return -5;
  }
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_voodoo_ghost_drag_end(uint32_t hand,
                                                            uint32_t release_inside_near_bb) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (hand > 1u) {
    set_error("hand out of range");
    return -3;
  }
  const threeos::EntityId gid = g_voodoo.ghost_drag_entity(hand);
  // Tick clears snap when out of range; if still pending for this drag, commit.
  // Hold seats ~0.09 m above the box, so a raw 0.07 m 3D check always failed.
  const bool commit_snap = g_workspace.snap().pending && g_workspace.snap().object_id == gid &&
                           g_workspace.snap().box_id != 0;
  if (commit_snap) {
    std::string err;
    if (!g_workspace.commit_insert(&err)) {
      set_error(err.c_str());
      g_workspace.clear_snap();
      g_voodoo.ghost_drag_end(hand, release_inside_near_bb != 0u, g_workspace);
      g_centroids.rebuild(g_workspace);
      return -3;
    }
    const auto temps = g_voodoo.on_entity_inserted(gid);
    for (const auto temp : temps) {
      g_centroids.clear_temporary(temp);
    }
    // Always end the drag after commit — leaving it active kept ticking the
    // inserted entity and could fight host hide/RebuildViews (disappear/reappear).
    g_voodoo.ghost_drag_end(hand, release_inside_near_bb != 0u, g_workspace);
    g_centroids.rebuild(g_workspace);
    set_error("");
    return 0;
  }
  g_workspace.clear_snap();
  g_voodoo.ghost_drag_end(hand, release_inside_near_bb != 0u, g_workspace);
  g_centroids.rebuild(g_workspace);
  set_error("");
  return 0;
}

extern "C" THREEOS_API uint32_t threeos_voodoo_ghost_drag_active(uint32_t hand) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized || hand > 1u) {
    return 0;
  }
  return g_voodoo.ghost_drag_active(hand) ? 1u : 0u;
}

extern "C" THREEOS_API uint64_t threeos_voodoo_ghost_drag_entity(uint32_t hand) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized || hand > 1u) {
    return 0;
  }
  return g_voodoo.ghost_drag_entity(hand);
}

extern "C" THREEOS_API int32_t threeos_voodoo_get_ghost_drag_arrow(
    uint32_t hand, ThreeOS_VoodooDragArrow* out_arrow) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (hand > 1u) {
    set_error("hand out of range");
    return -3;
  }
  if (out_arrow == nullptr) {
    set_error("null arrow");
    return -2;
  }
  const auto& gd = g_voodoo.ghost_drag_state(hand);
  const auto& a = g_voodoo.ghost_drag_arrow(hand);
  std::memset(out_arrow, 0, sizeof(*out_arrow));
  out_arrow->active = a.active ? 1u : 0u;
  out_arrow->in_deadzone = a.in_deadzone ? 1u : 0u;
  out_arrow->magnitude = a.magnitude;
  out_arrow->stick = to_c(a.stick);
  out_arrow->ray_origin = to_c(a.ray_origin);
  out_arrow->ray_tip = to_c(a.ray_tip);
  out_arrow->deadzone_armed = a.deadzone_armed ? 1u : 0u;
  out_arrow->reserved1 = gd.direct_follow ? 1u : 0u;
  out_arrow->reserved2 = gd.near_stick_visible ? 1u : 0u;
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_voodoo_summon(uint64_t entity_id, float x, float y,
                                                     float z) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (!g_voodoo.summon_selected(entity_id, threeos::Vec3{x, y, z}, g_workspace)) {
    set_error("summon refused");
    return -5;
  }
  g_centroids.rebuild(g_workspace);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_voodoo_cut_arm(uint64_t portal_id) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (!g_voodoo.cut_arm(portal_id)) {
    set_error("cut arm refused");
    return -2;
  }
  set_error("");
  return 0;
}

extern "C" THREEOS_API void threeos_voodoo_cut_cancel(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    return;
  }
  g_voodoo.cut_cancel();
}

extern "C" THREEOS_API uint64_t threeos_voodoo_cut_portal(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    return 0;
  }
  return g_voodoo.cut_portal_id();
}

extern "C" THREEOS_API int32_t threeos_voodoo_cut_paste_at(float x, float y, float z) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (!g_voodoo.cut_paste_at(threeos::Vec3{x, y, z}, g_workspace)) {
    set_error("cut paste refused");
    return -5;
  }
  g_centroids.rebuild(g_workspace);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_voodoo_cut_paste_into(uint64_t box_id) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (!g_voodoo.cut_paste_into(box_id, g_workspace)) {
    set_error("cut paste-into refused");
    return -5;
  }
  g_centroids.rebuild(g_workspace);
  set_error("");
  return 0;
}

namespace {

void fill_multichoice_state(ThreeOS_MultiChoiceState* out) {
  std::memset(out, 0, sizeof(*out));
  out->open = g_multichoice.open() ? 1u : 0u;
  out->mode = static_cast<uint32_t>(g_multichoice.mode());
  out->section = static_cast<uint32_t>(g_multichoice.section());
  out->hand = g_multichoice.hand();
  out->center = to_c(g_multichoice.center());
  out->min_radius_m = threeos::MultiChoiceEngine::kMainMinM;
  out->max_radius_m = threeos::MultiChoiceEngine::kMainMaxM;
  out->hovered_slot = g_multichoice.hovered_slot();
  out->page_index = g_multichoice.page_index();
  out->page_back_enabled = g_multichoice.page_back_enabled();
  out->page_forward_enabled = g_multichoice.page_forward_enabled();
  out->confirm_open = g_multichoice.confirm_open() ? 1u : 0u;
  out->confirm_hovered = g_multichoice.confirm_hovered();
  out->confirm_center = to_c(g_multichoice.confirm_center());
  out->confirm_min_m = threeos::MultiChoiceEngine::kConfirmMinM;
  out->confirm_max_m = threeos::MultiChoiceEngine::kConfirmMaxM;
  out->context_box_id = g_multichoice.context_box();
  out->pending_target_id = g_multichoice.pending_target();
  out->pending_kind = static_cast<uint32_t>(g_multichoice.pending_kind());
  out->cursor_inside = g_multichoice.cursor_inside_any() ? 1u : 0u;
  copy_cstr(out->title, sizeof(out->title), g_multichoice.title());
  copy_cstr(out->hover_label, sizeof(out->hover_label), g_multichoice.hover_label());
  const auto* slots = g_multichoice.slots();
  for (uint32_t i = 0; i < 8; ++i) {
    out->slots[i].kind = static_cast<uint32_t>(slots[i].kind);
    out->slots[i].enabled = slots[i].enabled;
    out->slots[i].target_id = slots[i].target_id;
    copy_cstr(out->slots[i].label, sizeof(out->slots[i].label), slots[i].label);
  }
}

}  // namespace

extern "C" THREEOS_API uint32_t threeos_sizeof_multi_choice_state(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_MultiChoiceState));
}
extern "C" THREEOS_API uint32_t threeos_sizeof_multi_choice_slot(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_MultiChoiceSlot));
}
extern "C" THREEOS_API uint32_t threeos_sizeof_drive_root(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_DriveRoot));
}

extern "C" THREEOS_API int32_t threeos_drive_roots_set(const ThreeOS_DriveRoot* roots,
                                                       uint32_t count) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  std::vector<threeos::DriveRootDesc> tmp;
  tmp.reserve(count);
  if (roots != nullptr) {
    for (uint32_t i = 0; i < count; ++i) {
      if (roots[i].id == 0) {
        continue;
      }
      threeos::DriveRootDesc d;
      d.id = roots[i].id;
      d.name = roots[i].name;
      tmp.push_back(std::move(d));
    }
  }
  g_multichoice.set_available_roots(tmp.data(), static_cast<uint32_t>(tmp.size()));
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_favorite_set(uint64_t box_id, uint32_t favored) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (!g_multichoice.set_favorite(box_id, favored != 0)) {
    set_error("favorite set failed");
    return -2;
  }
  set_error("");
  return 0;
}

extern "C" THREEOS_API uint32_t threeos_favorite_get(uint64_t box_id) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    return 0;
  }
  return g_multichoice.is_favorite(box_id) ? 1u : 0u;
}

extern "C" THREEOS_API int32_t threeos_multichoice_open_drive(uint32_t hand,
                                                              const ThreeOS_Pose* center) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (center == nullptr) {
    set_error("null center");
    return -2;
  }
  g_multichoice.open_drive_picker(hand, from_c(*center));
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_multichoice_open_folder(uint32_t hand,
                                                               const ThreeOS_Pose* center,
                                                               uint64_t box_id) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (center == nullptr || box_id == 0) {
    set_error("null folder args");
    return -2;
  }
  if (!g_multichoice.open_folder_context(hand, from_c(*center), box_id)) {
    set_error("open folder context failed");
    return -3;
  }
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_multichoice_open_agent(uint32_t hand,
                                                              const ThreeOS_Pose* center) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (center == nullptr) {
    set_error("null center");
    return -2;
  }
  g_multichoice.open_agent(hand, from_c(*center));
  set_error("");
  return 0;
}

extern "C" THREEOS_API uint64_t threeos_focus_anchor_register(const char* name,
                                                              const ThreeOS_Pose* pose) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return 0;
  }
  if (pose == nullptr) {
    set_error("null pose");
    return 0;
  }
  const char* n = (name != nullptr && name[0] != '\0') ? name : "focus";
  const uint64_t id = g_centroids.register_focus_anchor(n, from_c(*pose));
  set_error(id != 0 ? "" : "focus register failed");
  return id;
}

extern "C" THREEOS_API int32_t threeos_focus_anchor_set_pose(uint64_t id,
                                                             const ThreeOS_Pose* pose) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (id == 0 || pose == nullptr) {
    set_error("null focus args");
    return -2;
  }
  if (!g_centroids.set_focus_anchor_pose(id, from_c(*pose))) {
    set_error("unknown focus anchor");
    return -3;
  }
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_focus_anchor_clear(uint64_t id) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (id == 0) {
    set_error("null id");
    return -2;
  }
  if (!g_centroids.clear_focus_anchor(id)) {
    set_error("unknown focus anchor");
    return -3;
  }
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_voodoo_near_rest_pose(const ThreeOS_Pose* head,
                                                             ThreeOS_Pose* out_pose) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (head == nullptr || out_pose == nullptr) {
    set_error("null near-rest args");
    return -2;
  }
  const threeos::Pose p = g_voodoo.near_rest_pose(from_c(*head), /*head_valid=*/true);
  *out_pose = to_c(p);
  set_error("");
  return 0;
}

extern "C" THREEOS_API void threeos_multichoice_close(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    return;
  }
  g_multichoice.close_all();
}

extern "C" THREEOS_API int32_t threeos_multichoice_tick(float cursor_x, float cursor_y,
                                                        float cursor_z) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  g_multichoice.tick_hover(threeos::Vec3{cursor_x, cursor_y, cursor_z});
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_multichoice_confirm(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  g_multichoice.on_primary_confirm();
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_multichoice_get_state(ThreeOS_MultiChoiceState* out_state) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (out_state == nullptr) {
    set_error("null state");
    return -2;
  }
  fill_multichoice_state(out_state);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_multichoice_poll_action(uint32_t* out_action,
                                                               uint64_t* out_target_id,
                                                               uint32_t* out_kind) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  threeos::EntityId tid = 0;
  threeos::MultiChoiceSlotKind kind = threeos::MultiChoiceSlotKind::Empty;
  const auto action = g_multichoice.consume_action(&tid, &kind);
  if (out_action) {
    *out_action = static_cast<uint32_t>(action);
  }
  if (out_target_id) {
    *out_target_id = tid;
  }
  if (out_kind) {
    *out_kind = static_cast<uint32_t>(kind);
  }
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_mind_palace_layout_next(uint64_t mind_palace_id,
                                                       const ThreeOS_Pose* head,
                                                       const char* display_name) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (mind_palace_id == 0 || head == nullptr) {
    set_error("null mind palace layout args");
    return -2;
  }
  threeos::Pose anchor{};
  std::string err;
  const std::string name = display_name ? display_name : "";
  if (!threeos::place_next_mind_palace_anchor(from_c(*head), true, g_centroids,
                                      g_voodoo.params().field_radius_m,
                                      g_voodoo.params().field_height_m, name, &anchor, &err,
                                      mind_palace_id)) {
    set_error(err.c_str());
    return -3;
  }
  g_centroids.set_mind_palace_anchor(mind_palace_id, anchor);
  if (!g_workspace.layout_mind_palace_field(mind_palace_id, anchor)) {
    set_error("layout_mind_palace empty or unknown");
    return -4;
  }
  g_workspace.resolve_glyphs();
  threeos::resolve_mutual_object_overlaps(g_workspace, from_c(*head), true,
                                          g_workspace.field_spacing_m());
  g_centroids.rebuild(g_workspace);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_storage_clear_field(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  // Dismiss portals first (Jump-to-Root must not tear Unity scene — only kernel field).
  std::vector<uint64_t> ids;
  ids.reserve(g_voodoo.portals().size());
  for (const auto& p : g_voodoo.portals()) {
    ids.push_back(p.id);
  }
  for (uint64_t id : ids) {
    g_voodoo.dismiss_portal(id);
  }
  g_voodoo.cut_cancel();
  for (const auto& e : g_workspace.entries()) {
    g_workspace.set_in_field(e.id, false);
  }
  g_workspace.clear_selection();
  g_workspace.clear_snap();
  g_centroids.rebuild(g_workspace);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_field_resolve_overlaps(const ThreeOS_Pose* head) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  threeos::Pose h{};
  bool hv = false;
  if (head != nullptr) {
    h = from_c(*head);
    hv = true;
  }
  threeos::resolve_mutual_object_overlaps(g_workspace, h, hv, g_workspace.field_spacing_m());
  g_centroids.rebuild(g_workspace);
  set_error("");
  return 0;
}
