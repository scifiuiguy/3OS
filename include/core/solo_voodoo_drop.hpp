#pragma once

#include "core/centroid_registry.hpp"
#include "core/types.hpp"
#include "core/voodoo_volume.hpp"
#include "storage/workspace.hpp"

#include <cstdint>
#include <vector>

namespace threeos {

/// Solo Voodoo Drop (Task 8.1.2): gaze + palm ("Use The Force") on a far object.
/// Each hand is independent: Gaze+Left OR Gaze+Right may arm; both may be ready at once.
/// The pinching hand must itself have palm-on-target. First successful drop wins; a short
/// lockout blocks the other hand. Pinch held longer than kPinchTapMaxS is a drag → never drop.
class SoloVoodooDrop {
 public:
  /// cos(~7 deg): tight gaze pin on the object.
  static constexpr float kGazeDotMin = 0.9925f;
  /// cos(~45 deg): palm vaguely toward the object (gaze stays the tight pin).
  static constexpr float kPalmDotMin = 0.7071f;
  /// Match host TriggerHoldDragSeconds — longer press is a drag, not a tap-drop.
  /// Drag blocks Drop on that release only; it does not dismiss the armed far BB.
  static constexpr float kPinchTapMaxS = 0.22f;
  /// After a drop, ignore further solo pinches briefly (dual-hand double-fire).
  static constexpr float kPostDropLockoutS = 0.50f;
  static constexpr size_t kMaxCandidates = 8;

  enum class Event : uint32_t {
    None = 0,
    Armed = 1,
    Dismissed = 2,
    Drop = 3,
  };

  struct State {
    bool armed = false;
    uint32_t hand = 0;  // last hand that met palm criteria (or that dropped)
    EntityId target = 0;  // resolved drop id (may be cluster centroid)
    Vec3 far_position{};
    Vec3 far_extents{};  // full W×H×D; zeros ⇒ host default solo box
  };

  std::vector<EntityId> candidates(const Pose& head, bool head_valid, const Workspace& workspace,
                                   const CentroidRegistry& centroids,
                                   const VoodooVolumeEngine& voodoo) const;

  /// palm_normal: world palmar face (host: palm joint local −Y).
  /// hand_pos: palm node. Kernel dots palm_normal vs normalize(target − hand_pos).
  Event tick(uint32_t hand, EntityId clear_target, const Vec3& palm_normal, bool pinch_pressed,
             const Pose& head, bool head_valid, const Vec3& hand_pos, float now_s,
             const Workspace& workspace, const CentroidRegistry& centroids,
             const VoodooVolumeEngine& voodoo);

  const State& state() const { return state_; }
  EntityId dropped_target() const { return dropped_target_; }
  void reset() {
    state_ = State{};
    dropped_target_ = 0;
    for (int i = 0; i < 2; ++i) {
      palm_ok_[i] = false;
      pinch_down_[i] = false;
      pinch_t0_[i] = 0.f;
      pinch_drag_[i] = false;
      pinch_eligible_[i] = false;
    }
    drop_frame_latch_ = false;
    lockout_until_ = 0.f;
  }

  void begin_frame() { drop_frame_latch_ = false; }

  /// True while Use The Force, armed solo, or post-drop lockout — host/kernel must not open snap drag.
  bool blocks_voodoo_snap(float now_s) const;

  static float gaze_dot(const Pose& head, const Vec3& target);
  static bool eligible(const WorkspaceEntry& e, const Workspace& workspace,
                       const CentroidRegistry& centroids, const VoodooVolumeEngine& voodoo);

 private:
  State state_{};
  EntityId dropped_target_ = 0;
  bool palm_ok_[2]{};
  bool pinch_down_[2]{};
  float pinch_t0_[2]{};
  bool pinch_drag_[2]{};      // held past tap window → release must not drop
  bool pinch_eligible_[2]{};  // palm was on target at press edge (pinch may break palm)
  bool drop_frame_latch_ = false;
  float lockout_until_ = 0.f;
};

}  // namespace threeos
