#pragma once

/**
 * Phase 0.3 kinetic control law (grab-anchored stick velocity).
 *
 * On Possess
 * ----------
 * Freeze `start_hand` to the first valid hand/controller pose after possess.
 * Object pose is seeded from the host; subsequent motion always integrates from
 * the object's *current* pose (never from the possess-time object pose).
 *
 * While Possessed
 * ---------------
 * Stick deflection (meters):
 *   stick = current_hand - start_hand
 *
 * Deadzone (radial, default 0.015 m):
 *   if |stick| < deadzone_m: velocity hard-zeroed (object stops).
 *   else: remapped deflection so exit is continuous from 0:
 *     stick_eff = stick * (|stick| - deadzone_m) / |stick|
 *     v_obj = gain * stick_eff
 * with gain in 1/s. Example: gain=10, deadzone=0.015, |stick|=0.30 m →
 * ~2.85 m/s (not a jump from gain*deadzone at the boundary).
 *
 * Each tick integrates from the current object pose:
 *   p_obj += v_obj * dt
 *
 * On Release
 * ----------
 * Retain v_obj (inertia). Coast with:
 *   v_obj *= exp(-friction * dt)
 * until |v_obj| < settle_epsilon.
 *
 * Floor clamp (minimal enclosure)
 * -------------------------------
 * After integration, if p_obj.y < floor_y + object_radius:
 *   p_obj.y = floor_y + object_radius
 *   if v_obj.y < 0: v_obj.y = 0
 */

#include "core/types.hpp"

#include <cmath>

namespace threeos {

struct KinematicsParams {
  /// Maps stick meters → object speed (1/s). ~10 ⇒ 1 ft stick ≈ 3 m/s.
  float gain = 10.f;
  /// Radial |current_hand - start_hand| deadzone before drive engages (meters).
  float deadzone_m = 0.015f;
  /// Exponential decay rate (1/s) while coasting or inside deadzone.
  float friction = 3.5f;
  float floor_y = 0.f;
  float object_radius = 0.075f;
  float settle_epsilon = 0.02f;  // m/s
};

enum class KineticPhase : uint32_t {
  Idle = 0,
  Possessed = 1,
  Coasting = 2,
};

struct KineticState {
  KineticPhase phase = KineticPhase::Idle;
  EntityId entity_id = 0;
  Pose pose{};
  Vec3 velocity{};
};

/// Host-draw stick velocity ray (OpenXR meters). Computed while possessed.
struct StickDebug {
  bool active = false;
  bool in_deadzone = false;
  float magnitude = 0.f;
  Vec3 stick{};
  Vec3 ray_origin{};
  Vec3 ray_tip{};
};

class KinematicsEngine {
 public:
  /// Stick debug ray offset away from the body (4 inches) so low-hand
  /// telekinesis stays inside the HMD frustum. Applied opposite kernel -Z
  /// forward under the Unity host's X-only handedness map.
  static constexpr float kStickVizForwardOffsetM = 4.f * 0.0254f;
  /// Extra world +Y lift on the stick viz (4 inches).
  static constexpr float kStickVizUpOffsetM = 4.f * 0.0254f;

  const KinematicsParams& params() const { return params_; }
  void set_params(const KinematicsParams& p) { params_ = p; }

  const KineticState& state() const { return state_; }
  const StickDebug& stick_debug() const { return stick_debug_; }

  void possess(EntityId id, const Pose& object_pose);
  void release();
  void clear();

  /// Drive with hand pose at absolute time_seconds. Returns true if pose changed.
  /// Head pose (when valid) sets flat-forward for the stick debug ray.
  bool tick(const Pose& hand_pose, const Pose& head_pose, bool head_valid,
            double time_seconds, bool hand_valid);

  void set_pose(const Pose& pose) { state_.pose = pose; }

  /// Snap enter/exit: clear drive and treat current hand as new grab origin.
  void reanchor_hand(const Pose& hand_pose);
  void zero_velocity() { state_.velocity = {}; }
  bool have_start_hand() const { return have_start_hand_; }

 private:
  void integrate(float dt);
  void apply_floor_clamp();
  void apply_friction(float dt);
  void update_stick_debug(const Pose& hand_pose, const Pose& head_pose, bool head_valid);

  KinematicsParams params_{};
  KineticState state_{};
  StickDebug stick_debug_{};
  bool have_start_hand_ = false;
  Pose start_hand_{};
  bool have_prev_time_ = false;
  double prev_time_ = 0.0;
};

inline float vec3_length(const Vec3& v) {
  return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

}  // namespace threeos
