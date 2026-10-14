#pragma once

#include "core/types.hpp"
#include "core/voodoo_geom.hpp"

namespace threeos {

struct VoodooDragArrow;

/// Deadzone arming + stick-ray fill shared by yellow session and blue ghost-drag arrows.
class StickViz {
 public:
  static constexpr float kForwardOffsetM = 4.f * 0.0254f;
  static constexpr float kUpOffsetM = 4.f * 0.0254f;

  static void arm_deadzone(bool& armed, bool& left_once, float mag, float deadzone_m) {
    if (!armed) {
      if (mag >= deadzone_m) {
        left_once = true;
      }
      if (left_once && mag < deadzone_m) {
        armed = true;
      }
    }
  }

  static Vec3 origin(const Vec3& start_hand, const Pose& head, bool head_valid) {
    const Vec3 flat = voodoo_geom::flat_forward(head, head_valid);
    return Vec3{start_hand.x - flat.x * kForwardOffsetM,
                start_hand.y - flat.y * kForwardOffsetM + kUpOffsetM,
                start_hand.z - flat.z * kForwardOffsetM};
  }

  static void fill(VoodooDragArrow& arrow, const Vec3& o, const Vec3& stick, float mag,
                   bool deadzone_armed, float deadzone_m);
};

}  // namespace threeos
