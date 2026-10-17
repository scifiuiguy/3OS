#pragma once

#include "core/centroid_registry.hpp"
#include "core/hand_interaction.hpp"
#include "core/types.hpp"
#include "core/voodoo_cut_paste.hpp"
#include "core/voodoo_geom.hpp"
#include "core/voodoo_ghost_drag.hpp"
#include "core/voodoo_orbit_packer.hpp"
#include "core/voodoo_portal_space.hpp"
#include "core/voodoo_pose_lerp.hpp"
#include "core/voodoo_stick_viz.hpp"
#include "storage/workspace.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

namespace threeos {

enum class VoodooMode : uint32_t {
  None = 0,
  Snap = 1,
  FreeRange = 2,
};

struct VoodooFieldParams {
  float voxel_size_m = 0.0762f;           // 3"
  int32_t near_bb_voxels_x = 4;           // W
  int32_t near_bb_voxels_y = 6;           // H (18")
  int32_t near_bb_voxels_z = 4;           // D
  float field_radius_m = 15.24f;          // 50 ft
  float field_height_m = 9.144f;          // 30 ft
  float arm_reach_m = 0.3048f;            // 12" → full field travel
};

struct VoodooBoundingBox {
  bool open = false;
  Pose pose{};
  Vec3 extents{};  // full W×H×D meters (x,y,z)
  int32_t aspect_x = 4;
  int32_t aspect_y = 6;
  int32_t aspect_z = 4;
  EntityId linked_centroid_id = 0;
};

struct VoodooDragArrow {
  bool active = false;
  bool in_deadzone = false;
  bool deadzone_armed = false;
  float magnitude = 0.f;
  Vec3 stick{};
  Vec3 ray_origin{};
  Vec3 ray_tip{};
};

/// Snap-mode node in hand/arrow space (field-scaled inverse of far BB gain).
struct VoodooSnapNode {
  EntityId centroid_id = 0;
  CentroidKind kind = CentroidKind::None;
  bool snapped = false;
  Vec3 world_position{};
  Vec3 hand_offset{};
  Vec3 field_position{};
  float radius_m = 0.035f;
};

enum class VoodooSessionPhase : uint32_t {
  Idle = 0,
  Dragging = 1,
  Parked = 2,
};

/// Near-portal ghost of a far entity (or parent Box base marker).
struct VoodooGhost {
  EntityId entity_id = 0;
  bool is_parent_box = false;
  Pose far_pose{};
  Pose near_pose{};
  float scale = 1.f;
};

/// Parked far+near portal (survives subsequent hand sessions).
struct VoodooPortal {
  uint64_t id = 0;
  VoodooMode mode = VoodooMode::None;
  uint32_t creating_hand = 0;
  VoodooBoundingBox far_bb{};
  VoodooBoundingBox near_bb{};
  EntityId persistent_centroid_id = 0;
  EntityId temporary_centroid_id = 0;
  /// Collision cohort: non-overlapping parks start alone; bumps merge groups.
  uint64_t group_id = 0;
  /// Park-time near_w/far_w; kept constant so near grows with far (no live ghost shrink).
  float lock_scale = 1.f;
  std::vector<VoodooGhost> ghosts{};
  Vec3 bump_from{};
  Vec3 bump_to{};
  float bump_t = 1.f;  // 0..1; 1 = settled
};

/// Task 6.5: live ghost-scale drag (blue stick channel, independent of yellow voodoo session).
enum class GhostDragKind : uint32_t {
  /// Already a portal ghost — clamp to near BB + expand far BB.
  Committed = 0,
  /// Near-field real auditioning: ghost only while inside; exit destroys ghost; release commits.
  Audition = 1,
};

struct GhostDragState {
  bool active = false;
  GhostDragKind kind = GhostDragKind::Committed;
  /// Audition only: currently showing a ghost inside a portal (false = outside, no ghost).
  bool audition_inside = false;
  /// Cursor was inside the glyph (near real or near ghost) at grab → 1:1 translation sync.
  bool direct_follow = false;
  /// Object position − cursor at grab (preserved so the glyph does not snap to the tip).
  Vec3 grab_offset{};
  /// Saved across matrix-snap seat so direct-follow can return to the cursor on unsnap.
  Vec3 grab_offset_pre_snap{};
  bool have_grab_offset_pre_snap = false;
  /// Free/escape position while matrix-snapped (audition outside a portal). Hold is
  /// display-only; escape must accumulate or sticky snap can never clear.
  Vec3 snap_escape{};
  bool have_snap_escape = false;
  /// After sticky unsnap, suppress re-enter on this Box until free pose leaves enter radius
  /// (lateral escape clears the 3″ hold buffer while still inside the 0.2 m arm radius).
  EntityId snap_cooldown_box = 0;
  uint32_t hand = 0;
  EntityId entity_id = 0;
  uint64_t portal_id = 0;
  float scale = 1.f;
  Pose start_hand{};
  Pose start_far{};
  Vec3 velocity{};
  bool have_start_hand = false;
  bool deadzone_armed = false;
  bool deadzone_left_once = false;
  bool clamped = false;
  /// Near-BB (blue) stick viz — hand-space stick at the near grab origin.
  VoodooDragArrow arrow{};
  /// Far-BB (yellow) twin — same stick mapped through portal scale into far space.
  VoodooDragArrow far_arrow{};
  /// Host policy: blue velocity stick only while auditioning inside a near BB or
  /// committed with a live portal ghost (not after pull-out / far-only drag).
  bool near_stick_visible = false;
};

struct VoodooSessionState {
  VoodooMode mode = VoodooMode::None;
  bool open = false;
  uint32_t hand = 0;
  VoodooSessionPhase phase = VoodooSessionPhase::Idle;
  VoodooBoundingBox far_bb{};
  VoodooBoundingBox near_bb{};
  VoodooDragArrow arrow{};
  std::vector<VoodooSnapNode> snap_nodes{};
  EntityId snapped_centroid_id = 0;
  /// Set when open_drag refused due to near-BB capacity.
  bool creation_refused = false;
};

class VoodooVolumeEngine {
 public:
  static constexpr float kDefaultDeadzoneM = 0.015f;
  static constexpr float kFarRestForwardM = 1.0f;
  /// Navel-forward near rest (was ~16″; +2″ → ~18″).
  static constexpr float kNearRestForwardM = 0.33f + 5.f * 0.0254f;
  /// Behind-user orbit node for arc bump paths (2 ft).
  static constexpr float kOrbitBehindM = 2.f * 0.3048f;
  static constexpr float kNearRestYBelowNavelM = 4.f * 0.0254f;
  static constexpr float kNavelBelowHeadM = 0.35f;
  static constexpr float kFarStartScale = 2.f;
  static constexpr float kFarDampTauS = 0.045f;
  static constexpr float kStickVizForwardOffsetM = StickViz::kForwardOffsetM;
  static constexpr float kStickVizUpOffsetM = StickViz::kUpOffsetM;
  static constexpr float kSnapNodeRadiusM = 0.035f;
  static constexpr float kSnapPullMul = 2.2f;
  static constexpr float kExtentsLerpPerSec = 4.f;
  static constexpr uint32_t kMaxNearPortals = 12u;
  static constexpr float kMinHoldToParkS = 0.25f;
  static constexpr float kBumpDurationS = 0.22f;
  static constexpr float kNearClearGapM = 0.04f;
  static constexpr float kGlyphHalfM = 0.05f;  // encapsulation pad (~GlyphSizeM/2)
  /// Cursor-in-glyph radius for direct grab (near reals; ghosts multiply by max(scale, 0.15)).
  static constexpr float kDirectGrabRadiusM = 0.09f;
  static constexpr float kSummonLerpS = PoseLerpQueue::kSummonLerpS;

  const VoodooFieldParams& params() const { return params_; }
  bool set_params(const VoodooFieldParams& p);

  const VoodooSessionState& session(uint32_t hand) const {
    return hand < 2u ? hands_[hand].session : hands_[0].session;
  }

  const std::vector<VoodooPortal>& portals() const { return portals_; }
  /// Ellipse packing estimate (front rest × behind node); hard-capped at kMaxNearPortals.
  uint32_t max_orbit_slots() const;
  /// Hard portal cap only. Collide-with-full-group is refused at park (pose-dependent).
  bool creation_allowed() const { return portals_.size() < kMaxNearPortals; }
  bool dismiss_portal(uint64_t portal_id);

  /// Task 6.6: voodoo-level cut (content ghosts only; parent Box excluded).
  uint64_t cut_portal_id() const;
  bool cut_arm(uint64_t portal_id);
  void cut_cancel();
  bool cut_active() const;
  /// Move all cut content ghosts to pinch as a billboarded centroid cluster
  /// (open-air or other near BB). Leaves source portal open, no refit.
  bool cut_paste_at(const Vec3& pinch_world, Workspace& workspace);
  /// Move/insert all cut content into `box_id` (must not belong to the cut portal).
  bool cut_paste_into(EntityId box_id, Workspace& workspace);
  /// Content entity ids currently armed for cut (excludes parent Box ghost).
  std::vector<EntityId> cut_entity_ids() const;

  /// Point-in-any-open-near-BB (for cursor tint).
  bool point_in_any_near_bb(const Vec3& p) const;
  /// Portal whose open near BB contains `p`, or 0 if none.
  uint64_t portal_at_near_point(const Vec3& p) const;
  /// Portal that currently owns a non-parent ghost for `entity_id`, or 0.
  uint64_t portal_for_ghost_entity(EntityId entity_id) const;

  /// Task 6.5 ghost-scale / audition drag (palm ignored; host begins on selected hold).
  bool ghost_drag_begin(uint32_t hand, EntityId entity_id, const Pose& hand_pose, Workspace& workspace);
  void ghost_drag_tick(uint32_t hand, const Pose& hand_pose, bool hand_valid, const Pose& head,
                       bool head_valid, float dt_seconds, Workspace& workspace);
  /// After ghost_drag_tick: velocity stick arrows (direct / near / far policy).
  void refresh_ghost_drag_arrows(uint32_t hand, const Pose& hand_pose, const Pose& head,
                                 bool head_valid);
  /// Commit audition when release is inside a near BB; cancel audition if outside.
  void ghost_drag_end(uint32_t hand, bool release_inside_near_bb, Workspace& workspace);
  bool ghost_drag_active(uint32_t hand) const;
  const GhostDragState& ghost_drag_state(uint32_t hand) const {
    static const GhostDragState kEmpty{};
    return hand < 2u ? ghost_drags_[hand] : kEmpty;
  }
  const VoodooDragArrow& ghost_drag_arrow(uint32_t hand) const;
  /// Far-space twin of the blue ghost-drag arrow (yellow host viz).
  const VoodooDragArrow& ghost_drag_far_arrow(uint32_t hand) const;
  EntityId ghost_drag_entity(uint32_t hand) const;
  GhostDragKind ghost_drag_kind(uint32_t hand) const;

  /// Selected ghost → open-air / cross-portal; selected near-real → insert into near BB.
  /// Destination real pose fast-lerps; does not grow far BB via temp on open-air summon.
  bool summon_selected(EntityId entity_id, const Vec3& pinch_world, Workspace& workspace);

  /// Remove non-parent content ghosts for `entity_id` from all portals (e.g. after box insert).
  void remove_content_ghost(EntityId entity_id);

  /// Seat a content ghost just above another box ghost in the same portal (near snap hold).
  bool apply_near_ghost_snap_hold(EntityId object_id, EntityId box_id, float hold_above_m,
                                  Workspace& workspace);

  /// Portal/near-real Box snap while ghost-dragging `hand` (after ghost_drag_tick).
  void update_ghost_matrix_snap(uint32_t hand, Workspace& workspace);

  /// Keep ghost far poses aligned with workspace; prune/alloc by far-BB membership.
  void sync_ghosts_from_workspace(const Workspace& workspace);

  void tick_pose_lerps(float dt_seconds, Workspace& workspace);
  bool pose_lerp_active(EntityId entity_id) const;

  Vec3 near_extents_m() const {
    return Vec3{params_.near_bb_voxels_x * params_.voxel_size_m,
                params_.near_bb_voxels_y * params_.voxel_size_m,
                params_.near_bb_voxels_z * params_.voxel_size_m};
  }

  Vec3 far_start_extents_m() const {
    const Vec3 n = near_extents_m();
    return Vec3{n.x * kFarStartScale, n.y * kFarStartScale, n.z * kFarStartScale};
  }

  void reset();

  /// Drive one hand. Pass live centroids + workspace for snap / park ghosts.
  void tick_hand(uint32_t hand, const InteractionIntents& intents, const Pose& hand_pose,
                 bool hand_valid, const Pose& head, bool head_valid, float dt_seconds,
                 CentroidRegistry& centroids, Workspace& workspace);

  /// Advance bump-lerps for all portals (call once per tick after hands).
  void tick_portals(float dt_seconds, const Pose& head, bool head_valid);

  /// Abort an in-flight far/near create without parking (e.g. object drag took the hand).
  void cancel_open_session(uint32_t hand);

  /// After a workspace insert/put-away: drop ghosts/lerps for `entity_id` and dismiss
  /// any portal whose snap root was that entity. Returns temporary centroid ids to clear.
  std::vector<EntityId> on_entity_inserted(EntityId entity_id);

 private:
  struct HandRuntime {
    VoodooSessionState session{};
    Pose start_hand_{};
    Pose rest_far_{};
    Vec3 far_smoothed_{};
    Vec3 extents_smoothed_{};
    bool have_start_hand_ = false;
    bool have_far_smoothed_ = false;
    bool deadzone_armed_ = false;
    bool deadzone_left_once_ = false;
    float drag_hold_s_ = 0.f;
    /// Portal created by the last park on this hand (0 = none). Session near/far
    /// snapshots sync from this portal so they do not go stale after bump-lerp.
    uint64_t parked_portal_id_ = 0;
  };

  void open_drag(uint32_t hand, VoodooMode mode, const Pose& hand_pose, const Pose& head,
                 bool head_valid);
  void park(uint32_t hand, const Pose& hand_pose, bool hand_valid, const Pose& head,
            bool head_valid, CentroidRegistry& centroids, Workspace& workspace);
  void cancel_drag(uint32_t hand);
  void clear_hand_session(uint32_t hand);
  void update_far_while_dragging(uint32_t hand, const Pose& hand_pose, const Pose& head,
                                 bool head_valid, float dt_seconds,
                                 const CentroidRegistry& centroids, const Workspace& workspace);
  void update_arrow(uint32_t hand, const Pose& hand_pose, const Pose& head, bool head_valid,
                    const Vec3* stick_override);
  void rebuild_snap_nodes(uint32_t hand, const CentroidRegistry& centroids);
  void rebuild_ghosts(VoodooPortal& portal, const CentroidRegistry& centroids,
                      const Workspace& workspace);
  /// Recompute ghost near poses from current far/near BB (after seat / bump).
  void refresh_ghost_near_poses(VoodooPortal& portal) const;
  static Vec3 near_bb_bottom_center(const VoodooBoundingBox& bb);
  VoodooPortal* portal_mut(uint64_t portal_id);
  const VoodooPortal* portal_const(uint64_t portal_id) const;
  bool find_ghost_mut(EntityId entity_id, VoodooPortal** out_portal, VoodooGhost** out_ghost);
  bool find_ghost(EntityId entity_id, const VoodooPortal** out_portal,
                  const VoodooGhost** out_ghost) const;
  Pose map_far_to_near_pose(const VoodooPortal& portal, const Pose& far_pose, float scale) const;
  Pose map_near_to_far_pose(const VoodooPortal& portal, const Pose& near_pose, float scale) const;
  Vec3 clamp_point_to_near_obb(const VoodooPortal& portal, const Vec3& p) const;
  /// near_w/far_w content scale (matches park-time lock_scale formula).
  static float content_scale_for_portal(const VoodooPortal& portal);
  void erase_content_ghost(VoodooPortal& portal, EntityId entity_id);
  void erase_all_content_ghosts(VoodooPortal& portal);
  /// Seat `entity_id` as a content ghost in `target` at `near_pose`; strip other portals;
  /// optional far lerp. Returns false if entity_id is 0.
  bool move_ghost_to_portal(EntityId entity_id, VoodooPortal& target, const Pose& near_pose,
                            Workspace* workspace_for_lerp);
  /// Strip content ghosts for `entity_id` from every portal (keep parent markers).
  void strip_content_ghost_everywhere(EntityId entity_id);
  struct PlaceItem {
    EntityId id = 0;
    Pose far_pose{};
    Pose near_pose{};
  };
  /// Shared cut-paste / summon placement: cross-portal cluster or open-air release.
  /// `home` may be null for summon of a near-field real with no ghost.
  /// `cluster_open_air`: cut paste orbits/bumps a multi-glyph footprint; summon uses pinch as-is.
  bool place_items_at_pinch(VoodooPortal* home, const std::vector<PlaceItem>& items,
                            const Vec3& pinch_world, Workspace& workspace, bool cluster_open_air);
  void update_ghost_drag_arrow(uint32_t hand, const Pose& hand_pose, const Pose& head,
                               bool head_valid, const Vec3& stick);
  bool ensure_ghost_in_portal(VoodooPortal& portal, EntityId entity_id, const Workspace& workspace);
  void remove_ghost_from_other_portals(uint64_t keep_portal_id, EntityId entity_id);
  static void arm_deadzone(bool& armed, bool& left_once, float mag, float deadzone_m);
  static Vec3 stick_viz_origin(const Vec3& start_hand, const Pose& head, bool head_valid);
  static void fill_drag_arrow(VoodooDragArrow& arrow, const Vec3& origin, const Vec3& stick,
                              float mag, bool deadzone_armed, float deadzone_m);
  static Vec3 effective_portal_pos(const VoodooPortal& portal);
  void set_portal_orbit_target(VoodooPortal& portal, const Vec3& to, const Pose& head,
                               bool head_valid);
  /// Walk ±orbit from `az0` until `pred(cand)`; writes `*out`. Shared by paste clear + nudge.
  bool walk_orbit(const Vec3& body, const Vec3& fwd, const Vec3& right, float az0, float y,
                  const std::function<bool(const Vec3&)>& pred, Vec3* out) const;
  /// Grow/recenter far BB around content; grow near by lock_scale.
  /// Always min-clears sibling near BBs (pinned bumper stays put). `settle_bump`
  /// recenters the far BB; live drag expands about a fixed far center.
  void refit_far_bb_to_content_ghosts(VoodooPortal& portal, Workspace& workspace,
                                     bool settle_bump);
  /// Push in-field objects out of an open near BB footprint (XZ).
  /// When `animate` is true, targets travel via pose-lerp (~under 0.25 s); else snap.
  void bump_near_field_clutter(const VoodooPortal& portal, Workspace& workspace,
                               const Pose& head, bool head_valid, bool animate);
  /// Open-air cut paste: orbit overlapping near BBs clear of a paste footprint; if the
  /// ring cannot absorb them, nudge the paste origin on the body orbit instead.
  /// Near BBs never leave the near field. Returns the (possibly nudged) paste centroid.
  Vec3 resolve_cut_paste_origin(const Vec3& pinch_world, float cluster_radius_m);
  /// Orbit near BBs clear of a paste cylinder, then cascade sibling overlaps on the
  /// body orbit (bumped A into B → B yields). Returns false if the ring cannot clear.
  bool orbit_near_bbs_clear_of_paste(const Vec3& paste_c, float paste_r);
  static float portal_near_footprint_radius(const VoodooBoundingBox& near_bb);
  void begin_pose_lerp(EntityId entity_id, const Pose& to, Workspace& workspace);
  void audition_enter_portal(GhostDragState& gd, VoodooPortal& portal, const Pose& hand_pose,
                             Workspace& workspace);
  void audition_exit_portal(GhostDragState& gd, const Pose& hand_pose, Workspace& workspace);
  /// Near-field matrix Box snap while auditioning outside a portal: seat on hold and
  /// re-anchor grab/stick so the object does not keep tunneling through the target.
  void audition_apply_matrix_snap(GhostDragState& gd, const Pose& hand_pose,
                                  Workspace& workspace);
  /// Pin `pinned_index` as bumper (pose/yaw fixed); other near BBs in its group orbit clear.
  /// Merge groups on contact. Returns false if the group cannot clear (full ring) — caller rolls back.
  bool bump_resolve(const Pose& head, bool head_valid, size_t pinned_index);
  /// Resize hygiene: push overlapping siblings just far enough in XZ to clear the
  /// pinned bumper's footprint. No ellipse snap / orbit walk (avoids mid-drag runaway).
  void bump_siblings_min_clear(size_t pinned_index);
  Pose orbit_near_to_hand(const Pose& seed, const Pose& head, bool head_valid,
                          const Pose& hand_pose, bool hand_valid, uint32_t hand) const;
  float near_footprint_radius_m() const;
  bool near_targets_clear() const;
  bool near_targets_clear_indices(const std::vector<size_t>& idxs) const;
  bool group_is_full_circle(uint64_t group_id, const Pose& head, bool head_valid) const;
  void merge_groups(uint64_t from_id, uint64_t into_id);
  size_t find_overlap_portal(const VoodooBoundingBox& candidate, size_t skip_index) const;
  Vec3 pos_on_orbit_ellipse(const Vec3& body, const Vec3& fwd, const Vec3& right, float az,
                            float y) const;
  float azimuth_on_orbit(const Vec3& body, const Vec3& fwd, const Vec3& right,
                         const Vec3& p) const;
  float orbit_radius_at_az(float az) const;
  static float wrap_pi(float a) { return voodoo_geom::wrap_pi(a); }
  static float shortest_daz(float from_az, float to_az) {
    return voodoo_geom::shortest_daz(from_az, to_az);
  }
  static bool near_bbs_overlap_xz(const VoodooBoundingBox& a, const VoodooBoundingBox& b,
                                  float gap_m);
  Vec3 fit_extents_for_aabb(const Vec3& aabb_min, const Vec3& aabb_max) const;
  void apply_near_extents(VoodooBoundingBox& bb) const;
  void apply_far_start_extents(VoodooBoundingBox& bb) const;
  Pose far_rest_pose(const Pose& head, bool head_valid) const;
  Pose near_rest_pose(const Pose& head, bool head_valid) const;
  static Quat yaw_billboard_toward(const Vec3& from, const Pose& head, bool head_valid) {
    return voodoo_geom::yaw_billboard_toward(from, head, head_valid);
  }
  static Vec3 flat_forward(const Pose& head, bool head_valid) {
    return voodoo_geom::flat_forward(head, head_valid);
  }
  static bool point_in_obb(const Vec3& p, const Pose& box_pose, const Vec3& full_extents) {
    return voodoo_geom::point_in_obb(p, box_pose, full_extents);
  }
  static Vec3 quat_rotate_yaw(const Quat& q, const Vec3& v) {
    return voodoo_geom::quat_rotate_yaw(q, v);
  }
  static Quat quat_inverse_yaw(const Quat& q) { return voodoo_geom::quat_inverse_yaw(q); }

  static constexpr float kGhostDragGain = 10.f;  // match kinematics gain (1/s)

  friend class VoodooOrbitPacker;
  friend class GhostDragController;
  friend class VoodooCutPaste;

  VoodooFieldParams params_{};
  HandRuntime hands_[2]{};
  GhostDragState ghost_drags_[2]{};
  PoseLerpQueue pose_lerps_{};
  float deadzone_m_ = kDefaultDeadzoneM;
  std::vector<VoodooPortal> portals_{};
  uint64_t next_portal_id_ = 1;
  uint64_t next_group_id_ = 1;
  Pose last_head_{};
  bool last_head_valid_ = false;
  VoodooOrbitPacker orbit_{*this};
  GhostDragController ghost_drag_{*this};
  VoodooCutPaste cut_{*this};
};

}  // namespace threeos
