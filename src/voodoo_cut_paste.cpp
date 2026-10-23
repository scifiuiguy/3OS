#include "core/voodoo_cut_paste.hpp"

#include "core/voodoo_volume.hpp"
#include "storage/workspace.hpp"

#include <algorithm>
#include <string>

namespace threeos {

bool VoodooCutPaste::is_armed(uint64_t portal_id) const {
  if (portal_id == 0) {
    return false;
  }
  return std::find(cut_portal_ids_.begin(), cut_portal_ids_.end(), portal_id) !=
         cut_portal_ids_.end();
}

bool VoodooCutPaste::arm(uint64_t portal_id) {
  const VoodooPortal* p = engine_.portal_const(portal_id);
  if (p == nullptr || !p->near_bb.open) {
    return false;
  }
  if (is_armed(portal_id)) {
    return true;
  }
  cut_portal_ids_.push_back(portal_id);
  return true;
}

void VoodooCutPaste::cancel() { cut_portal_ids_.clear(); }

void VoodooCutPaste::disarm_portal(uint64_t portal_id) {
  cut_portal_ids_.erase(
      std::remove(cut_portal_ids_.begin(), cut_portal_ids_.end(), portal_id),
      cut_portal_ids_.end());
}

std::vector<EntityId> VoodooCutPaste::entity_ids() const {
  std::vector<EntityId> ids;
  for (uint64_t pid : cut_portal_ids_) {
    const VoodooPortal* p = engine_.portal_const(pid);
    if (p == nullptr) {
      continue;
    }
    for (const auto& g : p->ghosts) {
      if (!g.is_parent_box && g.entity_id != 0) {
        ids.push_back(g.entity_id);
      }
    }
  }
  return ids;
}

bool VoodooCutPaste::paste_at(const Vec3& pinch_world, Workspace& workspace) {
  if (cut_portal_ids_.empty()) {
    return false;
  }

  // Use the first still-valid portal as the "home" for placement math; gather
  // items from every armed portal.
  VoodooPortal* home = nullptr;
  std::vector<PlaceItem> items;
  for (uint64_t pid : cut_portal_ids_) {
    VoodooPortal* p = engine_.portal_mut(pid);
    if (p == nullptr) {
      continue;
    }
    if (home == nullptr) {
      home = p;
    }
    for (const auto& g : p->ghosts) {
      if (!g.is_parent_box && g.entity_id != 0) {
        items.push_back(PlaceItem{g.entity_id, g.far_pose, g.near_pose});
      }
    }
  }
  if (home == nullptr || items.empty()) {
    cut_portal_ids_.clear();
    return false;
  }

  const bool ok =
      place_items_at_pinch(home, items, pinch_world, workspace, /*cluster_open_air=*/true);
  // Clear ghosts from all armed portals after a successful cluster paste.
  if (ok) {
    for (uint64_t pid : cut_portal_ids_) {
      if (VoodooPortal* p = engine_.portal_mut(pid)) {
        engine_.erase_all_content_ghosts(*p);
      }
    }
  }
  cut_portal_ids_.clear();
  return ok;
}

bool VoodooCutPaste::paste_into(EntityId box_id, Workspace& workspace) {
  if (box_id == 0 || cut_portal_ids_.empty()) {
    return false;
  }

  // Refuse pasting into a box that is itself a cut content ghost.
  for (uint64_t pid : cut_portal_ids_) {
    const VoodooPortal* p = engine_.portal_const(pid);
    if (p == nullptr) {
      continue;
    }
    for (const auto& g : p->ghosts) {
      if (g.entity_id == box_id) {
        return false;
      }
    }
  }

  const auto ids = entity_ids();
  if (ids.empty()) {
    cut_portal_ids_.clear();
    return false;
  }

  bool any = false;
  for (EntityId id : ids) {
    if (id == box_id) {
      continue;
    }
    std::string err;
    if (workspace.insert_into(id, box_id, &err)) {
      any = true;
      engine_.remove_content_ghost(id);
      auto& lerps = engine_.pose_lerps_.states();
      lerps.erase(std::remove_if(lerps.begin(), lerps.end(),
                                 [id](const PoseLerpState& l) { return l.entity_id == id; }),
                  lerps.end());
    }
  }

  for (uint64_t pid : cut_portal_ids_) {
    if (VoodooPortal* p = engine_.portal_mut(pid)) {
      engine_.erase_all_content_ghosts(*p);
    }
  }
  cut_portal_ids_.clear();
  return any;
}

bool VoodooCutPaste::place_items_at_pinch(VoodooPortal* home, const std::vector<PlaceItem>& items,
                                          const Vec3& pinch_world, Workspace& workspace,
                                          bool cluster_open_air) {
  std::vector<VoodooVolumeEngine::PlaceItem> eng_items;
  eng_items.reserve(items.size());
  for (const auto& it : items) {
    eng_items.push_back(VoodooVolumeEngine::PlaceItem{it.id, it.far_pose, it.near_pose});
  }
  return engine_.place_items_at_pinch(home, eng_items, pinch_world, workspace, cluster_open_air);
}

}  // namespace threeos
