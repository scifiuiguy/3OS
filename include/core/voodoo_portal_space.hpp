#pragma once

#include "core/types.hpp"
#include "core/voodoo_geom.hpp"

#include <algorithm>
#include <cmath>

namespace threeos {

struct VoodooBoundingBox;
struct VoodooPortal;
struct VoodooGhost;

/// Far↔near portal transforms, footprint, and ghost near-pose refresh.
class PortalSpace {
 public:
  static float content_scale(const VoodooPortal& portal);

  static float near_footprint_radius(const VoodooBoundingBox& near_bb);

  static Vec3 near_bb_bottom_center(const VoodooBoundingBox& bb);

  static Pose map_far_to_near(const VoodooPortal& portal, const Pose& far_pose, float scale);

  static Pose map_near_to_far(const VoodooPortal& portal, const Pose& near_pose, float scale);

  static Vec3 clamp_to_near_obb(const VoodooPortal& portal, const Vec3& p);

  static void refresh_ghost_near_poses(VoodooPortal& portal);
};

}  // namespace threeos
