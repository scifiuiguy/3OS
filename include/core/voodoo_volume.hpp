#pragma once

#include "core/hand_interaction.hpp"
#include "core/types.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

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

enum class VoodooSessionPhase : uint32_t {
  Idle = 0,
  Dragging = 1,
  Parked = 2,
};

struct VoodooSessionState {
  VoodooMode mode = VoodooMode::None;
  bool open = false;
  uint32_t hand = 0;
  VoodooSessionPhase phase = VoodooSessionPhase::Idle;
  VoodooBoundingBox far_bb{};
  VoodooBoundingBox near_bb{};
  VoodooDragArrow arrow{};
};

class VoodooVolumeEngine {
 public:
  static constexpr float kDefaultDeadzoneM = 0.015f;
  /// Far BB spawn: farther out for field placement.
  static constexpr float kFarRestForwardM = 1.0f;
  /// Near BB bottom-center: ~neutral elbow/cursor reach + 3″ (Quest-tuned).
  static constexpr float kNearRestForwardM = 0.33f + 3.f * 0.0254f;  // ≈0.4062 m
  /// Drop bottom anchor 4″ below navel height (Quest-tuned).
  static constexpr float kNearRestYBelowNavelM = 4.f * 0.0254f;
  static constexpr float kNavelBelowHeadM = 0.35f;
  /// Far BB start size = near extents × this (near stays 1×).
  static constexpr float kFarStartScale = 2.f;
  /// Subtle far-position follow (seconds); low latency, cuts high-gain jitter.
  static constexpr float kFarDampTauS = 0.045f;
  /// Stick viz: push origin away from body along floor-forward.
  static constexpr float kStickVizForwardOffsetM = 4.f * 0.0254f;
  static constexpr float kStickVizUpOffsetM = 4.f * 0.0254f;

  const VoodooFieldParams& params() const { return params_; }
  bool set_params(const VoodooFieldParams& p);

  const VoodooSessionState& session(uint32_t hand) const {
    return hand < 2u ? hands_[hand].session : hands_[0].session;
  }

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

  /// Drive one hand from its intents. Release parks far + opens near; tracking loss cancels.
  void tick_hand(uint32_t hand, const InteractionIntents& intents, const Pose& hand_pose,
                 bool hand_valid, const Pose& head, bool head_valid, float dt_seconds);

 private:
  struct HandRuntime {
    VoodooSessionState session{};
    Pose start_hand_{};
    Pose rest_far_{};
    Vec3 far_smoothed_{};
    bool have_start_hand_ = false;
    bool have_far_smoothed_ = false;
    bool deadzone_armed_ = false;
    bool deadzone_left_once_ = false;
  };

  void open_drag(uint32_t hand, VoodooMode mode, const Pose& hand_pose, const Pose& head,
                 bool head_valid);
  void park(uint32_t hand, const Pose& head, bool head_valid);
  void cancel_drag(uint32_t hand);
  void dismiss(uint32_t hand);
  void update_far_while_dragging(uint32_t hand, const Pose& hand_pose, const Pose& head,
                                 bool head_valid, float dt_seconds);
  void update_arrow(uint32_t hand, const Pose& hand_pose, const Pose& head, bool head_valid);
  void apply_near_extents(VoodooBoundingBox& bb) const;
  void apply_far_start_extents(VoodooBoundingBox& bb) const;
  Pose far_rest_pose(const Pose& head, bool head_valid) const;
  /// Pose is box center; bottom-center sits at near rest anchor for neutral cursor land.
  Pose near_rest_pose(const Pose& head, bool head_valid) const;
  /// Yaw-only billboard: box +Z faces toward head on the floor plane.
  static Quat yaw_billboard_toward(const Vec3& from, const Pose& head, bool head_valid);
  static Vec3 flat_forward(const Pose& head, bool head_valid);

  VoodooFieldParams params_{};
  HandRuntime hands_[2]{};
  float deadzone_m_ = kDefaultDeadzoneM;
};

}  // namespace threeos
