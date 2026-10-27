#pragma once

#include "core/types.hpp"
#include "core/voodoo_volume.hpp"
#include "storage/workspace.hpp"

#include <cstdint>
#include <vector>

namespace threeos {

/// Live drag-selection box (Task 8.1.1). Fully 3D, but its yaw is locked at begin to the
/// floor-parallel gaze heading; only the extents change as the hand moves.
struct MultiSelectBox {
  bool active = false;
  /// 0 = far field; otherwise the portal whose near BB the drag started in.
  uint64_t portal_id = 0;
  /// Center + yaw-only orientation (OpenXR space of the scope: far world or near world).
  Pose pose{};
  /// Full W×H×D meters.
  Vec3 extents{};
  bool cumulative = false;
};

/// Selection state is always written to the far entity ids. Near-scope drags test ghost
/// near poses but toggle the ghost's far entity, so ghosts only re-render the shared state.
class MultiSelectDrag {
 public:
  /// Starts a drag at `start`. A start inside a near BB scopes the drag to that portal's
  /// ghosts; any other start scopes it to far-field objects. Returns false if already active.
  bool begin(const Vec3& start, const Pose& head, bool head_valid, bool cumulative,
             const VoodooVolumeEngine& voodoo, Workspace& workspace);
  /// Recompute the box and live membership. Replace by default; cumulative keeps the
  /// selection that existed at begin and adds the box contents.
  void update(const Vec3& hand, bool cumulative, const VoodooVolumeEngine& voodoo,
              Workspace& workspace);
  /// Commit: selection stays as last computed.
  void end();
  /// Abort: restore the selection that existed at begin.
  void cancel(Workspace& workspace);

  const MultiSelectBox& box() const { return box_; }

 private:
  void apply_membership(const VoodooVolumeEngine& voodoo, Workspace& workspace) const;

  MultiSelectBox box_{};
  Vec3 start_{};
  float yaw_ = 0.f;
  std::vector<EntityId> baseline_{};
};

}  // namespace threeos
