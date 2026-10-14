#include "core/voodoo_cut_paste.hpp"

#include "core/voodoo_volume.hpp"

#include <string>

namespace threeos {

bool VoodooCutPaste::arm(uint64_t portal_id) {
  const VoodooPortal* p = engine_.portal_const(portal_id);
  if (p == nullptr || !p->near_bb.open) {
    return false;
  }
  cut_portal_id_ = portal_id;
  return true;
}

void VoodooCutPaste::cancel() { cut_portal_id_ = 0; }

std::vector<EntityId> VoodooCutPaste::entity_ids() const {
  std::vector<EntityId> ids;
  const VoodooPortal* p = engine_.portal_const(cut_portal_id_);
  if (p == nullptr) {
    return ids;
  }
  for (const auto& g : p->ghosts) {
    if (!g.is_parent_box && g.entity_id != 0) {
      ids.push_back(g.entity_id);
    }
  }
  return ids;
}

bool VoodooCutPaste::paste_at(const Vec3& pinch_world, Workspace& workspace) {
  VoodooPortal* home = engine_.portal_mut(cut_portal_id_);
  if (home == nullptr) {
    cut_portal_id_ = 0;
    return false;
  }

  std::vector<PlaceItem> items;
  items.reserve(home->ghosts.size());
  for (const auto& g : home->ghosts) {
    if (!g.is_parent_box && g.entity_id != 0) {
      items.push_back(PlaceItem{g.entity_id, g.far_pose, g.near_pose});
    }
  }
  if (items.empty()) {
    cut_portal_id_ = 0;
    return false;
  }

  cut_portal_id_ = 0;
  return place_items_at_pinch(home, items, pinch_world, workspace, /*cluster_open_air=*/true);
}

bool VoodooCutPaste::paste_into(EntityId box_id, Workspace& workspace) {
  if (box_id == 0) {
    return false;
  }
  VoodooPortal* home = engine_.portal_mut(cut_portal_id_);
  if (home == nullptr) {
    cut_portal_id_ = 0;
    return false;
  }
  for (const auto& g : home->ghosts) {
    if (g.entity_id == box_id) {
      return false;
    }
  }

  const auto ids = entity_ids();
  if (ids.empty()) {
    cut_portal_id_ = 0;
    return false;
  }

  cut_portal_id_ = 0;
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
  engine_.erase_all_content_ghosts(*home);
  return any;
}

bool VoodooCutPaste::place_items_at_pinch(VoodooPortal* home, const std::vector<PlaceItem>& items,
                                          const Vec3& pinch_world, Workspace& workspace,
                                          bool cluster_open_air) {
  // Reuse engine placement (shared with summon) until summon moves here too.
  std::vector<VoodooVolumeEngine::PlaceItem> eng_items;
  eng_items.reserve(items.size());
  for (const auto& it : items) {
    eng_items.push_back(VoodooVolumeEngine::PlaceItem{it.id, it.far_pose, it.near_pose});
  }
  return engine_.place_items_at_pinch(home, eng_items, pinch_world, workspace, cluster_open_air);
}

}  // namespace threeos
