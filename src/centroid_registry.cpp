#include "core/centroid_registry.hpp"

#include <algorithm>
#include <string>

namespace threeos {

void CentroidRegistry::clear() {
  mind_palaces_.clear();
  temps_.clear();
  focus_anchors_.clear();
  centroids_.clear();
  next_mind_palace_id_ = 100;
  next_temp_id_ = 50;
  next_focus_id_ = 20;
}

void CentroidRegistry::clear_mind_palaces() {
  mind_palaces_.clear();
  next_mind_palace_id_ = 100;
}

EntityId CentroidRegistry::register_temporary(const std::string& name, const Vec3& position) {
  CentroidState c;
  c.id = next_temp_id_++;
  c.kind = CentroidKind::Temporary;
  c.linked_entity_id = c.id;
  c.member_count = 0;
  c.position = position;
  c.name = name.empty() ? "temp" : name;
  if (c.name.size() > 15) {
    c.name.resize(15);
  }
  temps_.push_back(c);
  centroids_.push_back(c);
  return c.id;
}

bool CentroidRegistry::set_temporary_position(EntityId id, const Vec3& position) {
  for (auto& t : temps_) {
    if (t.id == id) {
      t.position = position;
      for (auto& c : centroids_) {
        if (c.id == id) {
          c.position = position;
        }
      }
      return true;
    }
  }
  return false;
}

bool CentroidRegistry::clear_temporary(EntityId id) {
  const auto it = std::remove_if(temps_.begin(), temps_.end(),
                                 [id](const CentroidState& t) { return t.id == id; });
  if (it == temps_.end()) {
    return false;
  }
  temps_.erase(it, temps_.end());
  centroids_.erase(std::remove_if(centroids_.begin(), centroids_.end(),
                                  [id](const CentroidState& c) { return c.id == id; }),
                   centroids_.end());
  return true;
}

EntityId CentroidRegistry::register_focus_anchor(const std::string& name, const Pose& pose) {
  CentroidState c;
  c.id = next_focus_id_++;
  c.kind = CentroidKind::FocusAnchor;
  c.linked_entity_id = c.id;
  c.member_count = 1;
  c.position = pose.position;
  c.name = name.empty() ? "focus" : name;
  if (c.name.size() > 15) {
    c.name.resize(15);
  }
  focus_anchors_.push_back(c);
  centroids_.push_back(c);
  return c.id;
}

bool CentroidRegistry::set_focus_anchor_pose(EntityId id, const Pose& pose) {
  for (auto& f : focus_anchors_) {
    if (f.id == id) {
      f.position = pose.position;
      for (auto& c : centroids_) {
        if (c.id == id) {
          c.position = pose.position;
        }
      }
      return true;
    }
  }
  return false;
}

bool CentroidRegistry::clear_focus_anchor(EntityId id) {
  const auto it = std::remove_if(focus_anchors_.begin(), focus_anchors_.end(),
                                 [id](const CentroidState& f) { return f.id == id; });
  if (it == focus_anchors_.end()) {
    return false;
  }
  focus_anchors_.erase(it, focus_anchors_.end());
  centroids_.erase(std::remove_if(centroids_.begin(), centroids_.end(),
                                  [id](const CentroidState& c) { return c.id == id; }),
                   centroids_.end());
  return true;
}

EntityId CentroidRegistry::register_mind_palace(const std::string& name, const Pose& anchor) {
  if (name.empty()) {
    return 0;
  }
  MindPalaceRoot g;
  g.id = next_mind_palace_id_++;
  g.name = name;
  g.anchor = anchor;
  mind_palaces_.push_back(std::move(g));
  return mind_palaces_.back().id;
}

bool CentroidRegistry::set_mind_palace_anchor(EntityId mind_palace_id, const Pose& anchor) {
  for (auto& g : mind_palaces_) {
    if (g.id == mind_palace_id) {
      g.anchor = anchor;
      return true;
    }
  }
  return false;
}

const MindPalaceRoot* CentroidRegistry::find_mind_palace(EntityId mind_palace_id) const {
  for (const auto& g : mind_palaces_) {
    if (g.id == mind_palace_id) {
      return &g;
    }
  }
  return nullptr;
}

const CentroidState* CentroidRegistry::find(EntityId id) const {
  for (const auto& c : centroids_) {
    if (c.id == id) {
      return &c;
    }
  }
  return nullptr;
}

bool CentroidRegistry::members_aabb(const Workspace& workspace, EntityId centroid_id, Vec3* out_min,
                                    Vec3* out_max) const {
  const CentroidState* c = find(centroid_id);
  if (c == nullptr || out_min == nullptr || out_max == nullptr) {
    return false;
  }
  bool any = false;
  Vec3 mn{};
  Vec3 mx{};
  for (const auto& e : workspace.entries()) {
    if (!e.in_field) {
      continue;
    }
    const bool match = (c->kind == CentroidKind::MindPalace)
                           ? (e.parent_box == 0 && e.mind_palace_id == centroid_id)
                           : (e.parent_box == centroid_id);
    if (!match) {
      continue;
    }
    const Vec3& p = e.pose.position;
    if (!any) {
      mn = mx = p;
      any = true;
    } else {
      mn.x = std::min(mn.x, p.x);
      mn.y = std::min(mn.y, p.y);
      mn.z = std::min(mn.z, p.z);
      mx.x = std::max(mx.x, p.x);
      mx.y = std::max(mx.y, p.y);
      mx.z = std::max(mx.z, p.z);
    }
  }
  if (!any) {
    return false;
  }
  // Pad by half start-extent so a single point still has volume.
  constexpr float kPad = 0.05f;
  *out_min = Vec3{mn.x - kPad, mn.y - kPad, mn.z - kPad};
  *out_max = Vec3{mx.x + kPad, mx.y + kPad, mx.z + kPad};
  return true;
}

EntityId CentroidRegistry::accessible_centroid_id(const Workspace& workspace,
                                                  EntityId entity_id) const {
  const WorkspaceEntry* e = workspace.find(entity_id);
  if (e == nullptr) {
    return 0;
  }
  if (e->kind == StorageKind::Box && e->expanded && find(e->id) != nullptr) {
    return e->id;
  }
  if (e->parent_box != 0) {
    const WorkspaceEntry* parent = workspace.find(e->parent_box);
    if (parent != nullptr && parent->in_field && parent->expanded && find(parent->id) != nullptr) {
      return parent->id;
    }
  }
  if (e->mind_palace_id != 0) {
    const CentroidState* c = find(e->mind_palace_id);
    if (c != nullptr && c->kind == CentroidKind::MindPalace) {
      return e->mind_palace_id;
    }
  }
  return 0;
}

bool CentroidRegistry::has_accessible_centroid(const Workspace& workspace,
                                               EntityId entity_id) const {
  return accessible_centroid_id(workspace, entity_id) != 0;
}

void CentroidRegistry::rebuild(const Workspace& workspace) {
  centroids_.clear();
  // Preserve temporary near-portal centroids + world focus anchors.
  for (const auto& t : temps_) {
    centroids_.push_back(t);
  }
  for (const auto& f : focus_anchors_) {
    centroids_.push_back(f);
  }
  const auto& entries = workspace.entries();

  // MindPalace centroids: average of in-field root children tagged with this mind palace.
  for (const auto& mp : mind_palaces_) {
    Vec3 sum{};
    uint32_t n = 0;
    for (const auto& e : entries) {
      if (!e.in_field || e.parent_box != 0 || e.mind_palace_id != mp.id) {
        continue;
      }
      sum.x += e.pose.position.x;
      sum.y += e.pose.position.y;
      sum.z += e.pose.position.z;
      ++n;
    }
    if (n == 0) {
      continue;  // omit empty
    }
    CentroidState c;
    c.id = mp.id;
    c.kind = CentroidKind::MindPalace;
    c.linked_entity_id = mp.id;
    c.member_count = n;
    c.position = Vec3{sum.x / static_cast<float>(n), sum.y / static_cast<float>(n),
                      sum.z / static_cast<float>(n)};
    c.name = mp.name;
    centroids_.push_back(std::move(c));
  }

  // Open (expanded) Box centroids: average of in-field children.
  for (const auto& box : entries) {
    if (box.kind != StorageKind::Box || !box.expanded) {
      continue;
    }
    Vec3 sum{};
    uint32_t n = 0;
    for (const auto& e : entries) {
      if (!e.in_field || e.parent_box != box.id) {
        continue;
      }
      sum.x += e.pose.position.x;
      sum.y += e.pose.position.y;
      sum.z += e.pose.position.z;
      ++n;
    }
    if (n == 0) {
      continue;
    }
    CentroidState c;
    c.id = box.id;
    c.kind = CentroidKind::Box;
    c.linked_entity_id = box.id;
    c.member_count = n;
    c.position = Vec3{sum.x / static_cast<float>(n), sum.y / static_cast<float>(n),
                      sum.z / static_cast<float>(n)};
    c.name = box.name;
    centroids_.push_back(std::move(c));
  }
}

}  // namespace threeos
