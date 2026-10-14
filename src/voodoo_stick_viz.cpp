#include "core/voodoo_stick_viz.hpp"

#include "core/voodoo_volume.hpp"

namespace threeos {

void StickViz::fill(VoodooDragArrow& arrow, const Vec3& o, const Vec3& stick, float mag,
                    bool deadzone_armed, float deadzone_m) {
  arrow.active = true;
  arrow.deadzone_armed = deadzone_armed;
  arrow.in_deadzone = deadzone_armed && mag < deadzone_m;
  arrow.magnitude = mag;
  arrow.stick = stick;
  arrow.ray_origin = o;
  arrow.ray_tip = Vec3{o.x + stick.x, o.y + stick.y, o.z + stick.z};
}

}  // namespace threeos
