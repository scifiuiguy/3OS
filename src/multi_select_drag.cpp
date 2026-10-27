#include "core/multi_select_drag.hpp"

#include "core/voodoo_geom.hpp"

#include <algorithm>
#include <cmath>

namespace threeos {

namespace {
constexpr float kMinExtentM = 1e-3f;
}

bool MultiSelectDrag::begin(const Vec3& start, const Pose& head, bool head_valid,
                            bool cumulative, const VoodooVolumeEngine& voodoo,
                            Workspace& workspace) {
  if (box_.active) {
    return false;
  }
  const Vec3 fwd = voodoo_geom::flat_forward(head, head_valid);
  yaw_ = std::atan2(fwd.x, fwd.z);
  start_ = start;
  baseline_ = workspace.selection_ids();

  box_ = MultiSelectBox{};
  box_.active = true;
  box_.portal_id = voodoo.portal_at_near_point(start);
  box_.cumulative = cumulative;
  box_.pose.position = start;
  box_.pose.orientation = voodoo_geom::quat_from_yaw(yaw_);
  box_.extents = Vec3{kMinExtentM, kMinExtentM, kMinExtentM};
  apply_membership(voodoo, workspace);
  return true;
}

void MultiSelectDrag::update(const Vec3& hand, bool cumulative, const VoodooVolumeEngine& voodoo,
                             Workspace& workspace) {
  if (!box_.active) {
    return;
  }
  const Quat q = box_.pose.orientation;
  const Vec3 d{hand.x - start_.x, hand.y - start_.y, hand.z - start_.z};
  const Vec3 local = voodoo_geom::quat_rotate_yaw(voodoo_geom::quat_inverse_yaw(q), d);
  const Vec3 center_local{local.x * 0.5f, local.y * 0.5f, local.z * 0.5f};
  const Vec3 center = voodoo_geom::quat_rotate_yaw(q, center_local);
  box_.pose.position = Vec3{start_.x + center.x, start_.y + center.y, start_.z + center.z};
  box_.extents = Vec3{std::max(std::abs(local.x), kMinExtentM),
                      std::max(std::abs(local.y), kMinExtentM),
                      std::max(std::abs(local.z), kMinExtentM)};
  box_.cumulative = cumulative;
  apply_membership(voodoo, workspace);
}

void MultiSelectDrag::end() {
  box_ = MultiSelectBox{};
  baseline_.clear();
}

void MultiSelectDrag::cancel(Workspace& workspace) {
  if (!box_.active) {
    return;
  }
  workspace.set_selection_ids(baseline_);
  end();
}

void MultiSelectDrag::apply_membership(const VoodooVolumeEngine& voodoo,
                                       Workspace& workspace) const {
  std::vector<EntityId> ids;
  if (box_.cumulative) {
    ids = baseline_;
  }
  auto add = [&ids](EntityId id) {
    if (std::find(ids.begin(), ids.end(), id) == ids.end()) {
      ids.push_back(id);
    }
  };

  if (box_.portal_id == 0) {
    for (const auto& e : workspace.entries()) {
      if (e.id == 0 || !e.in_field) {
        continue;
      }
      if (voodoo_geom::point_in_obb(e.pose.position, box_.pose, box_.extents)) {
        add(e.id);
      }
    }
  } else {
    for (const auto& portal : voodoo.portals()) {
      if (portal.id != box_.portal_id) {
        continue;
      }
      for (const auto& g : portal.ghosts) {
        if (g.is_parent_box) {
          continue;
        }
        if (voodoo_geom::point_in_obb(g.near_pose.position, box_.pose, box_.extents)) {
          add(g.entity_id);
        }
      }
    }
  }
  workspace.set_selection_ids(ids);
}

}  // namespace threeos
