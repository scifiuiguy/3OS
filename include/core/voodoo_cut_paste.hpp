#pragma once

#include "core/types.hpp"

#include <cstdint>
#include <vector>

namespace threeos {

class VoodooVolumeEngine;
class Workspace;
struct VoodooPortal;

/// Voodoo-level scissors cut/paste (content ghosts only).
class VoodooCutPaste {
 public:
  explicit VoodooCutPaste(VoodooVolumeEngine& engine) : engine_(engine) {}

  uint64_t portal_id() const { return cut_portal_id_; }
  bool active() const { return cut_portal_id_ != 0; }

  bool arm(uint64_t portal_id);
  void cancel();
  std::vector<EntityId> entity_ids() const;
  bool paste_at(const Vec3& pinch_world, Workspace& workspace);
  bool paste_into(EntityId box_id, Workspace& workspace);

  struct PlaceItem {
    EntityId id = 0;
    Pose far_pose{};
    Pose near_pose{};
  };
  bool place_items_at_pinch(VoodooPortal* home, const std::vector<PlaceItem>& items,
                            const Vec3& pinch_world, Workspace& workspace, bool cluster_open_air);

 private:
  VoodooVolumeEngine& engine_;
  uint64_t cut_portal_id_ = 0;
};

}  // namespace threeos
