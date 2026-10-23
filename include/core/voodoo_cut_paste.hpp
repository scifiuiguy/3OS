#pragma once

#include "core/types.hpp"

#include <cstdint>
#include <vector>

namespace threeos {

class VoodooVolumeEngine;
class Workspace;
struct VoodooPortal;

/// Voodoo-level scissors cut/paste (content ghosts only).
/// Cut is pointer-only: armed portal ids + cosmetic duck; disk I/O happens on paste.
/// Multiple portals may be armed together (dual-hand cut for the Mind Palace demo).
class VoodooCutPaste {
 public:
  explicit VoodooCutPaste(VoodooVolumeEngine& engine) : engine_(engine) {}

  /// First armed portal (compat), or 0.
  uint64_t portal_id() const {
    return cut_portal_ids_.empty() ? 0 : cut_portal_ids_.front();
  }
  bool active() const { return !cut_portal_ids_.empty(); }
  bool is_armed(uint64_t portal_id) const;
  const std::vector<uint64_t>& portal_ids() const { return cut_portal_ids_; }

  /// Add portal to the cut set (does not replace other armed portals).
  bool arm(uint64_t portal_id);
  void cancel();
  /// Drop a portal from the cut set (e.g. dismissed).
  void disarm_portal(uint64_t portal_id);

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
  std::vector<uint64_t> cut_portal_ids_{};
};

}  // namespace threeos
