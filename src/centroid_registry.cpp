#include "core/centroid_registry.hpp"

#include <algorithm>

namespace threeos {

void CentroidRegistry::clear() {
  gods_.clear();
  centroids_.clear();
  next_god_id_ = 100;
}

void CentroidRegistry::clear_gods() {
  gods_.clear();
  next_god_id_ = 100;
}

EntityId CentroidRegistry::register_god(const std::string& name, const Pose& anchor) {
  if (name.empty()) {
    return 0;
  }
  GodRoot g;
  g.id = next_god_id_++;
  g.name = name;
  g.anchor = anchor;
  gods_.push_back(std::move(g));
  return gods_.back().id;
}

bool CentroidRegistry::set_god_anchor(EntityId god_id, const Pose& anchor) {
  for (auto& g : gods_) {
    if (g.id == god_id) {
      g.anchor = anchor;
      return true;
    }
  }
  return false;
}

const GodRoot* CentroidRegistry::find_god(EntityId god_id) const {
  for (const auto& g : gods_) {
    if (g.id == god_id) {
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
    const bool match = (c->kind == CentroidKind::God)
                           ? (e.parent_box == 0 && e.god_root_id == centroid_id)
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

void CentroidRegistry::rebuild(const Workspace& workspace) {
  centroids_.clear();
  const auto& entries = workspace.entries();

  // God centroids: average of in-field root children tagged with this god.
  for (const auto& god : gods_) {
    Vec3 sum{};
    uint32_t n = 0;
    for (const auto& e : entries) {
      if (!e.in_field || e.parent_box != 0 || e.god_root_id != god.id) {
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
    c.id = god.id;
    c.kind = CentroidKind::God;
    c.linked_entity_id = god.id;
    c.member_count = n;
    c.position = Vec3{sum.x / static_cast<float>(n), sum.y / static_cast<float>(n),
                      sum.z / static_cast<float>(n)};
    c.name = god.name;
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
