#include "core/kinematics.hpp"

#include <algorithm>
#include <cmath>

namespace threeos {
namespace {

Vec3 rotate_by_quat(const Quat& q, const Vec3& v) {
  const float ix = q.w * v.x + q.y * v.z - q.z * v.y;
  const float iy = q.w * v.y + q.z * v.x - q.x * v.z;
  const float iz = q.w * v.z + q.x * v.y - q.y * v.x;
  const float iw = -q.x * v.x - q.y * v.y - q.z * v.z;
  return Vec3{
      ix * q.w + iw * -q.x + iy * -q.z - iz * -q.y,
      iy * q.w + iw * -q.y + iz * -q.x - ix * -q.z,
      iz * q.w + iw * -q.z + ix * -q.y - iy * -q.x,
  };
}

Vec3 flat_forward_xz(const Quat& head_orientation, bool head_valid) {
  Vec3 fwd{0.f, 0.f, -1.f};  // OpenXR -Z forward
  if (head_valid) {
    fwd = rotate_by_quat(head_orientation, Vec3{0.f, 0.f, -1.f});
  }
  fwd.y = 0.f;
  const float len = std::sqrt(fwd.x * fwd.x + fwd.z * fwd.z);
  if (len < 1e-6f) {
    return Vec3{0.f, 0.f, -1.f};
  }
  return Vec3{fwd.x / len, 0.f, fwd.z / len};
}

}  // namespace

void KinematicsEngine::possess(EntityId id, const Pose& object_pose) {
  state_.phase = KineticPhase::Possessed;
  state_.entity_id = id;
  state_.pose = object_pose;
  state_.velocity = {};
  have_start_hand_ = false;
  have_prev_time_ = false;
  stick_debug_ = {};
}

void KinematicsEngine::release() {
  if (state_.phase == KineticPhase::Possessed) {
    state_.phase = KineticPhase::Coasting;
  }
  have_start_hand_ = false;
  stick_debug_ = {};
}

void KinematicsEngine::clear() {
  state_ = KineticState{};
  stick_debug_ = {};
  have_start_hand_ = false;
  have_prev_time_ = false;
}

void KinematicsEngine::reanchor_hand(const Pose& hand_pose) {
  start_hand_ = hand_pose;
  have_start_hand_ = true;
  state_.velocity = {};
}

void KinematicsEngine::apply_floor_clamp() {
  const float min_y = params_.floor_y + params_.object_radius;
  if (state_.pose.position.y < min_y) {
    state_.pose.position.y = min_y;
    if (state_.velocity.y < 0.f) {
      state_.velocity.y = 0.f;
    }
  }
}

void KinematicsEngine::apply_friction(float dt) {
  if (dt <= 0.f) {
    return;
  }
  const float decay = std::exp(-params_.friction * dt);
  state_.velocity.x *= decay;
  state_.velocity.y *= decay;
  state_.velocity.z *= decay;
  if (vec3_length(state_.velocity) < params_.settle_epsilon) {
    state_.velocity = {};
  }
}

void KinematicsEngine::integrate(float dt) {
  if (dt <= 0.f) {
    return;
  }
  state_.pose.position.x += state_.velocity.x * dt;
  state_.pose.position.y += state_.velocity.y * dt;
  state_.pose.position.z += state_.velocity.z * dt;
  apply_floor_clamp();
}

void KinematicsEngine::update_stick_debug(const Pose& hand_pose, const Pose& head_pose,
                                         bool head_valid) {
  stick_debug_ = {};
  if (state_.phase != KineticPhase::Possessed || !have_start_hand_) {
    return;
  }

  const Vec3 stick{
      hand_pose.position.x - start_hand_.position.x,
      hand_pose.position.y - start_hand_.position.y,
      hand_pose.position.z - start_hand_.position.z,
  };
  const float mag = vec3_length(stick);
  const Vec3 flat = flat_forward_xz(head_pose.orientation, head_valid);
  // Host maps Unity↔kernel with an X flip only (Unity +Z ≈ kernel +Z). Kernel
  // "OpenXR forward" is -Z, which points toward the body in that frame — so the
  // viz offset must subtract flat_forward to push the ray away from the stomach.
  const Vec3 origin{
      start_hand_.position.x - flat.x * kStickVizForwardOffsetM,
      start_hand_.position.y - flat.y * kStickVizForwardOffsetM + kStickVizUpOffsetM,
      start_hand_.position.z - flat.z * kStickVizForwardOffsetM,
  };

  stick_debug_.active = true;
  stick_debug_.in_deadzone = mag < params_.deadzone_m;
  stick_debug_.magnitude = mag;
  stick_debug_.stick = stick;
  stick_debug_.ray_origin = origin;
  stick_debug_.ray_tip = Vec3{origin.x + stick.x, origin.y + stick.y, origin.z + stick.z};
}

bool KinematicsEngine::tick(const Pose& hand_pose, const Pose& head_pose, bool head_valid,
                            double time_seconds, bool hand_valid) {
  if (state_.phase == KineticPhase::Idle || state_.entity_id == 0) {
    have_start_hand_ = false;
    have_prev_time_ = false;
    stick_debug_ = {};
    return false;
  }

  float dt = 1.f / 72.f;
  if (have_prev_time_) {
    dt = static_cast<float>(time_seconds - prev_time_);
  }
  prev_time_ = time_seconds;
  have_prev_time_ = true;
  dt = std::clamp(dt, 1.f / 240.f, 0.1f);

  if (state_.phase == KineticPhase::Possessed) {
    if (!hand_valid) {
      apply_friction(dt);
      integrate(dt);
      stick_debug_ = {};
      return true;
    }

    if (!have_start_hand_) {
      start_hand_ = hand_pose;
      have_start_hand_ = true;
      state_.velocity = {};
      update_stick_debug(hand_pose, head_pose, head_valid);
      return true;
    }

    const Vec3 stick{
        hand_pose.position.x - start_hand_.position.x,
        hand_pose.position.y - start_hand_.position.y,
        hand_pose.position.z - start_hand_.position.z,
    };
    const float mag = vec3_length(stick);

    if (mag < params_.deadzone_m) {
      // Inside deadzone: hard-stop. No creep, no residual coast.
      state_.velocity = {};
      update_stick_debug(hand_pose, head_pose, head_valid);
      return true;
    }

    // Radial remap: drive from (|stick| - deadzone) so velocity rises from 0
    // at the deadzone boundary instead of jumping to gain * deadzone.
    const float eff = (mag - params_.deadzone_m) / mag;
    state_.velocity = Vec3{
        params_.gain * stick.x * eff,
        params_.gain * stick.y * eff,
        params_.gain * stick.z * eff,
    };
    integrate(dt);
    update_stick_debug(hand_pose, head_pose, head_valid);
    return true;
  }

  // Coasting after release
  stick_debug_ = {};
  apply_friction(dt);
  if (vec3_length(state_.velocity) < params_.settle_epsilon) {
    state_.velocity = {};
    state_.phase = KineticPhase::Idle;
    apply_floor_clamp();
    return true;
  }
  integrate(dt);
  return true;
}

}  // namespace threeos
