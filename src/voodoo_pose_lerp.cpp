#include "core/voodoo_pose_lerp.hpp"

#include <algorithm>
#include <cmath>

namespace threeos {

bool PoseLerpQueue::active(EntityId entity_id) const {
  for (const auto& l : lerps_) {
    if (l.active && l.entity_id == entity_id) {
      return true;
    }
  }
  return false;
}

void PoseLerpQueue::begin(EntityId entity_id, const Pose& to, Workspace& workspace) {
  if (entity_id == 0) {
    return;
  }
  Pose from{};
  if (const WorkspaceEntry* e = workspace.find(entity_id)) {
    from = e->pose;
  } else {
    from = to;
  }
  for (auto& l : lerps_) {
    if (l.active && l.entity_id == entity_id) {
      l.from = from;
      l.to = to;
      l.t = 0.f;
      return;
    }
  }
  PoseLerpState st;
  st.active = true;
  st.entity_id = entity_id;
  st.from = from;
  st.to = to;
  st.t = 0.f;
  lerps_.push_back(st);
}

void PoseLerpQueue::tick(float dt_seconds, Workspace& workspace,
                         const std::function<void(EntityId, const Pose&)>& on_pose) {
  const float dt = std::max(0.f, dt_seconds);
  for (auto& l : lerps_) {
    if (!l.active) {
      continue;
    }
    l.t += dt / std::max(kSummonLerpS, 1e-3f);
    if (l.t >= 1.f) {
      l.t = 1.f;
      if (auto* we = workspace.find_mut(l.entity_id)) {
        we->pose = l.to;
      }
      if (on_pose) {
        on_pose(l.entity_id, l.to);
      }
      l.active = false;
      continue;
    }
    const float u = l.t * l.t * (3.f - 2.f * l.t);
    Pose p = l.to;
    p.position = Vec3{l.from.position.x + (l.to.position.x - l.from.position.x) * u,
                      l.from.position.y + (l.to.position.y - l.from.position.y) * u,
                      l.from.position.z + (l.to.position.z - l.from.position.z) * u};
    if (auto* we = workspace.find_mut(l.entity_id)) {
      we->pose = p;
    }
    if (on_pose) {
      on_pose(l.entity_id, p);
    }
  }
  lerps_.erase(std::remove_if(lerps_.begin(), lerps_.end(),
                              [](const PoseLerpState& l) { return !l.active; }),
               lerps_.end());
}

}  // namespace threeos
