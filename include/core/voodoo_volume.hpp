#pragma once

#include "core/centroid_registry.hpp"
#include "core/hand_interaction.hpp"
#include "core/types.hpp"
#include "storage/workspace.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
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
  std::vector<VoodooGhost> ghosts{};
  Vec3 bump_from{};
  Vec3 bump_to{};
  float bump_t = 1.f;  // 0..1; 1 = settled
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
  static constexpr float kStickVizForwardOffsetM = 4.f * 0.0254f;
  static constexpr float kStickVizUpOffsetM = 4.f * 0.0254f;
  static constexpr float kSnapNodeRadiusM = 0.035f;
  static constexpr float kSnapPullMul = 2.2f;
  static constexpr float kExtentsLerpPerSec = 4.f;
  static constexpr uint32_t kMaxNearPortals = 12u;
  static constexpr float kMinHoldToParkS = 0.25f;
  static constexpr float kBumpDurationS = 0.22f;
  static constexpr float kNearClearGapM = 0.04f;
  static constexpr float kGlyphHalfM = 0.05f;  // encapsulation pad (~GlyphSizeM/2)

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
  /// Point-in-any-open-near-BB (for cursor tint).
  bool point_in_any_near_bb(const Vec3& p) const;

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
                 CentroidRegistry& centroids, const Workspace& workspace);

  /// Advance bump-lerps for all portals (call once per tick after hands).
  void tick_portals(float dt_seconds, const Pose& head, bool head_valid);

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
  };

  void open_drag(uint32_t hand, VoodooMode mode, const Pose& hand_pose, const Pose& head,
                 bool head_valid);
  void park(uint32_t hand, const Pose& hand_pose, bool hand_valid, const Pose& head,
            bool head_valid, CentroidRegistry& centroids, const Workspace& workspace);
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
  /// Pin newest; bump its group along the front↔behind ellipse; merge groups on contact.
  /// Returns false if the active group cannot clear (full ring) — caller rolls back.
  bool bump_resolve(const Pose& head, bool head_valid, size_t pinned_index);
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
  static float wrap_pi(float a);
  static float shortest_daz(float from_az, float to_az);
  static bool near_bbs_overlap_xz(const VoodooBoundingBox& a, const VoodooBoundingBox& b,
                                  float gap_m);
  Vec3 fit_extents_for_aabb(const Vec3& aabb_min, const Vec3& aabb_max) const;
  void apply_near_extents(VoodooBoundingBox& bb) const;
  void apply_far_start_extents(VoodooBoundingBox& bb) const;
  Pose far_rest_pose(const Pose& head, bool head_valid) const;
  Pose near_rest_pose(const Pose& head, bool head_valid) const;
  static Quat yaw_billboard_toward(const Vec3& from, const Pose& head, bool head_valid);
  static Vec3 flat_forward(const Pose& head, bool head_valid);
  static bool point_in_obb(const Vec3& p, const Pose& box_pose, const Vec3& full_extents);
  static Vec3 quat_rotate_yaw(const Quat& q, const Vec3& v);
  static Quat quat_inverse_yaw(const Quat& q);

  VoodooFieldParams params_{};
  HandRuntime hands_[2]{};
  float deadzone_m_ = kDefaultDeadzoneM;
  std::vector<VoodooPortal> portals_{};
  uint64_t next_portal_id_ = 1;
  uint64_t next_group_id_ = 1;
};

}  // namespace threeos
