#include "core/voodoo_portal_space.hpp"

#include "core/voodoo_volume.hpp"

namespace threeos {

float PortalSpace::content_scale(const VoodooPortal& portal) {
  const float near_w = std::max(portal.near_bb.extents.x, 1e-4f);
  const float far_w = std::max(portal.far_bb.extents.x, 1e-4f);
  return std::max(0.05f, std::min(1.f, near_w / far_w));
}

float PortalSpace::near_footprint_radius(const VoodooBoundingBox& near_bb) {
  return 0.5f * std::sqrt(near_bb.extents.x * near_bb.extents.x +
                          near_bb.extents.z * near_bb.extents.z);
}

Vec3 PortalSpace::near_bb_bottom_center(const VoodooBoundingBox& bb) {
  return Vec3{bb.pose.position.x, bb.pose.position.y - bb.extents.y * 0.5f, bb.pose.position.z};
}

Pose PortalSpace::map_far_to_near(const VoodooPortal& portal, const Pose& far_pose, float scale) {
  Pose out = far_pose;
  const Vec3 d{far_pose.position.x - portal.far_bb.pose.position.x,
               far_pose.position.y - portal.far_bb.pose.position.y,
               far_pose.position.z - portal.far_bb.pose.position.z};
  const Vec3 local =
      voodoo_geom::quat_rotate_yaw(voodoo_geom::quat_inverse_yaw(portal.far_bb.pose.orientation), d);
  const Vec3 scaled{local.x * scale, local.y * scale, local.z * scale};
  const Vec3 world = voodoo_geom::quat_rotate_yaw(portal.near_bb.pose.orientation, scaled);
  out.position = Vec3{portal.near_bb.pose.position.x + world.x,
                      portal.near_bb.pose.position.y + world.y,
                      portal.near_bb.pose.position.z + world.z};
  out.orientation = portal.near_bb.pose.orientation;
  return out;
}

Pose PortalSpace::map_near_to_far(const VoodooPortal& portal, const Pose& near_pose, float scale) {
  Pose out = near_pose;
  const float inv = 1.f / std::max(scale, 1e-4f);
  const Vec3 d{near_pose.position.x - portal.near_bb.pose.position.x,
               near_pose.position.y - portal.near_bb.pose.position.y,
               near_pose.position.z - portal.near_bb.pose.position.z};
  const Vec3 local = voodoo_geom::quat_rotate_yaw(
      voodoo_geom::quat_inverse_yaw(portal.near_bb.pose.orientation), d);
  const Vec3 unscaled{local.x * inv, local.y * inv, local.z * inv};
  const Vec3 world = voodoo_geom::quat_rotate_yaw(portal.far_bb.pose.orientation, unscaled);
  out.position = Vec3{portal.far_bb.pose.position.x + world.x,
                      portal.far_bb.pose.position.y + world.y,
                      portal.far_bb.pose.position.z + world.z};
  out.orientation = portal.far_bb.pose.orientation;
  return out;
}

Vec3 PortalSpace::clamp_to_near_obb(const VoodooPortal& portal, const Vec3& p) {
  const Vec3 d{p.x - portal.near_bb.pose.position.x, p.y - portal.near_bb.pose.position.y,
               p.z - portal.near_bb.pose.position.z};
  Vec3 local = voodoo_geom::quat_rotate_yaw(
      voodoo_geom::quat_inverse_yaw(portal.near_bb.pose.orientation), d);
  const Vec3 half{portal.near_bb.extents.x * 0.5f, portal.near_bb.extents.y * 0.5f,
                  portal.near_bb.extents.z * 0.5f};
  local.x = std::max(-half.x, std::min(half.x, local.x));
  local.y = std::max(-half.y, std::min(half.y, local.y));
  local.z = std::max(-half.z, std::min(half.z, local.z));
  const Vec3 world = voodoo_geom::quat_rotate_yaw(portal.near_bb.pose.orientation, local);
  return Vec3{portal.near_bb.pose.position.x + world.x, portal.near_bb.pose.position.y + world.y,
              portal.near_bb.pose.position.z + world.z};
}

void PortalSpace::refresh_ghost_near_poses(VoodooPortal& portal) {
  for (auto& g : portal.ghosts) {
    if (g.is_parent_box) {
      g.near_pose.position = near_bb_bottom_center(portal.near_bb);
      g.near_pose.orientation = portal.near_bb.pose.orientation;
      continue;
    }
    g.near_pose = map_far_to_near(portal, g.far_pose, g.scale);
  }
}

}  // namespace threeos
