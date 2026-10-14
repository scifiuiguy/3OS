#pragma once

#include "core/types.hpp"

#include <cstdint>

namespace threeos {

class VoodooVolumeEngine;
class Workspace;
struct GhostDragState;
struct VoodooPortal;
struct VoodooDragArrow;
struct Pose;
enum class GhostDragKind : uint32_t;

/// Blue-channel ghost-scale / audition drag (independent of yellow voodoo session).
class GhostDragController {
 public:
  explicit GhostDragController(VoodooVolumeEngine& engine) : engine_(engine) {}

  bool begin(uint32_t hand, EntityId entity_id, const Pose& hand_pose, Workspace& workspace);
  void tick(uint32_t hand, const Pose& hand_pose, bool hand_valid, const Pose& head,
            bool head_valid, float dt_seconds, Workspace& workspace);
  void end(uint32_t hand, bool release_inside_near_bb, Workspace& workspace);
  void refresh_arrows(uint32_t hand, const Pose& hand_pose, const Pose& head, bool head_valid);

  void audition_enter_portal(GhostDragState& gd, VoodooPortal& portal, const Pose& hand_pose,
                             Workspace& workspace);
  void audition_exit_portal(GhostDragState& gd, const Pose& hand_pose, Workspace& workspace);
  void audition_apply_matrix_snap(GhostDragState& gd, const Pose& hand_pose,
                                  Workspace& workspace);
  void update_arrow(uint32_t hand, const Pose& hand_pose, const Pose& head, bool head_valid,
                    const Vec3& stick);

  bool active(uint32_t hand) const;
  EntityId entity(uint32_t hand) const;
  GhostDragKind kind(uint32_t hand) const;
  const VoodooDragArrow& arrow(uint32_t hand) const;
  const VoodooDragArrow& far_arrow(uint32_t hand) const;

  bool apply_near_ghost_snap_hold(EntityId object_id, EntityId box_id, float hold_above_m,
                                  Workspace& workspace);

  /// Portal/near-real Box snap while a ghost-scale drag is active on `hand`.
  void update_matrix_snap(uint32_t hand, Workspace& workspace);

 private:
  static constexpr float kGhostSnapM = 0.07f;
  // Midway between the clipped 0.04 m seat and the gappy 0.14 m far hold.
  static constexpr float kGhostHoldAboveM = 0.09f;
  // After hold, centers sit ~holdAbove apart — sticky must allow that gap.
  static constexpr float kGhostStickyM = kGhostHoldAboveM + 0.03f;
  // Near-field real Boxes use the wider workspace snap radius.
  static constexpr float kNearRealSnapM = 0.2f;

  VoodooVolumeEngine& engine_;
};

}  // namespace threeos
