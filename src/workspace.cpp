#include "storage/workspace.hpp"

#include <cmath>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace threeos {
namespace {

float dist(const Vec3& a, const Vec3& b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  const float dz = a.z - b.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

struct CellKey {
  int x = 0;
  int y = 0;
  int z = 0;
  bool operator==(const CellKey& o) const {
    return x == o.x && y == o.y && z == o.z;
  }
};

struct CellKeyHash {
  size_t operator()(const CellKey& k) const {
    // 3D spatial hash (simple large primes).
    const auto ux = static_cast<uint32_t>(k.x) * 73856093u;
    const auto uy = static_cast<uint32_t>(k.y) * 19349663u;
    const auto uz = static_cast<uint32_t>(k.z) * 83492791u;
    return static_cast<size_t>(ux ^ uy ^ uz);
  }
};

CellKey cell_of(const Vec3& p, float spacing) {
  const float s = spacing > 1e-6f ? spacing : Workspace::kDefaultFieldSpacingM;
  return CellKey{
      static_cast<int>(std::floor(p.x / s)),
      static_cast<int>(std::floor(p.y / s)),
      static_cast<int>(std::floor(p.z / s)),
  };
}

std::string extension_of(const std::string& path) {
  const auto slash = path.find_last_of("/\\");
  const std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
  const auto dot = base.find_last_of('.');
  if (dot == std::string::npos) {
    return {};
  }
  return base.substr(dot);
}

}  // namespace

void Workspace::clear() {
  entries_.clear();
  next_id_ = 2000;
  selected_id_ = 0;
  field_spacing_m_ = kDefaultFieldSpacingM;
  snap_ = {};
  event_ = {};
  have_rollback_ = false;
  rollback_prev_parent_ = 0;
}

bool Workspace::set_selection(EntityId id) {
  if (id == 0) {
    selected_id_ = 0;
    return true;
  }
  if (find(id) == nullptr) {
    return false;
  }
  selected_id_ = id;
  return true;
}

EntityId Workspace::add_object(const std::string& name, const std::string& relative_path) {
  WorkspaceEntry e;
  e.id = next_id_++;
  e.kind = StorageKind::Object;
  e.name = name;
  e.relative_path = relative_path;
  e.child_count = 0;
  e.in_field = true;
  e.parent_box = 0;
  e.expanded = false;
  entries_.push_back(std::move(e));
  return entries_.back().id;
}

EntityId Workspace::add_box(const std::string& name, const std::string& relative_path,
                            uint32_t child_count) {
  WorkspaceEntry e;
  e.id = next_id_++;
  e.kind = StorageKind::Box;
  e.name = name;
  e.relative_path = relative_path;
  e.child_count = child_count;
  e.in_field = true;
  e.parent_box = 0;
  e.expanded = false;
  entries_.push_back(std::move(e));
  return entries_.back().id;
}

bool Workspace::set_parent_box(EntityId id, EntityId parent_box_id) {
  WorkspaceEntry* e = find_mut(id);
  if (e == nullptr) {
    return false;
  }
  if (parent_box_id != 0) {
    const WorkspaceEntry* p = find(parent_box_id);
    if (p == nullptr || p->kind != StorageKind::Box) {
      return false;
    }
    if (parent_box_id == id) {
      return false;
    }
  }
  e->parent_box = parent_box_id;
  return true;
}

bool Workspace::set_in_field(EntityId id, bool in_field) {
  WorkspaceEntry* e = find_mut(id);
  if (e == nullptr) {
    return false;
  }
  e->in_field = in_field;
  return true;
}

namespace {

Vec3 quat_rotate(const Quat& q, const Vec3& v) {
  // q * (0,v) * q̅
  const float qx = q.x, qy = q.y, qz = q.z, qw = q.w;
  const float tx = 2.f * (qy * v.z - qz * v.y);
  const float ty = 2.f * (qz * v.x - qx * v.z);
  const float tz = 2.f * (qx * v.y - qy * v.x);
  return Vec3{
      v.x + qw * tx + (qy * tz - qz * ty),
      v.y + qw * ty + (qz * tx - qx * tz),
      v.z + qw * tz + (qx * ty - qy * tx),
  };
}

Vec3 vec_add(const Vec3& a, const Vec3& b) {
  return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 vec_scale(const Vec3& v, float s) {
  return Vec3{v.x * s, v.y * s, v.z * s};
}

int cube_side_for_count(int n) {
  // 1–8 → 2³, 9–27 → 3³, … (matrix sized to fit the count).
  if (n <= 0) {
    return 1;
  }
  if (n <= 8) {
    return 2;
  }
  int side = 3;
  while (side * side * side < n) {
    ++side;
  }
  return side;
}

}  // namespace

void Workspace::layout_demo_field(const Pose& head) {
  std::vector<WorkspaceEntry*> items;
  items.reserve(entries_.size());
  for (auto& e : entries_) {
    if (!e.in_field || e.parent_box != 0) {
      continue;
    }
    items.push_back(&e);
  }

  const int n = static_cast<int>(items.size());
  if (n == 0) {
    return;
  }

  const int side = cube_side_for_count(n);
  // Local OpenXR axes from head: +X right, +Y up, -Z forward.
  const Vec3 right = quat_rotate(head.orientation, Vec3{1.f, 0.f, 0.f});
  const Vec3 up = quat_rotate(head.orientation, Vec3{0.f, 1.f, 0.f});
  const Vec3 forward = quat_rotate(head.orientation, Vec3{0.f, 0.f, -1.f});
  const Vec3 center = vec_add(head.position, vec_scale(forward, 1.f));

  const float spacing = field_spacing_m_;
  const float extent = 0.5f * spacing * static_cast<float>(side - 1);

  for (int i = 0; i < n; ++i) {
    const int ix = i % side;
    const int iy = (i / side) % side;
    const int iz = i / (side * side);
    const float lx = static_cast<float>(ix) * spacing - extent;
    const float ly = static_cast<float>(iy) * spacing - extent;
    const float lz = static_cast<float>(iz) * spacing - extent;
    items[i]->pose.position = vec_add(
        center, vec_add(vec_add(vec_scale(right, lx), vec_scale(up, ly)), vec_scale(forward, lz)));
    items[i]->pose.orientation = head.orientation;
  }
}

bool Workspace::layout_box_expand(EntityId box_id, std::string* err) {
  WorkspaceEntry* box = find_mut(box_id);
  if (box == nullptr || box->kind != StorageKind::Box) {
    if (err) {
      *err = "expand: not a box";
    }
    return false;
  }

  std::vector<WorkspaceEntry*> children;
  children.reserve(entries_.size());
  for (auto& e : entries_) {
    if (e.parent_box == box_id) {
      children.push_back(&e);
    }
  }

  for (auto* c : children) {
    c->in_field = true;
  }
  box->expanded = true;
  set_box_visual_open(box_id, true);

  const int n = static_cast<int>(children.size());
  if (n == 0) {
    return true;
  }

  const float spacing = field_spacing_m_;
  const int side = cube_side_for_count(n);
  const float extent = 0.5f * spacing * static_cast<float>(side - 1);
  const Vec3 right = quat_rotate(box->pose.orientation, Vec3{1.f, 0.f, 0.f});
  const Vec3 up = quat_rotate(box->pose.orientation, Vec3{0.f, 1.f, 0.f});
  const Vec3 forward = quat_rotate(box->pose.orientation, Vec3{0.f, 0.f, -1.f});

  std::unordered_set<CellKey, CellKeyHash> occupied;
  occupied.reserve(entries_.size() * 2);
  for (const auto& e : entries_) {
    if (!e.in_field || e.id == box_id || e.parent_box == box_id) {
      continue;
    }
    occupied.insert(cell_of(e.pose.position, spacing));
  }

  auto pose_at = [&](int i, int base_y_cells) -> Vec3 {
    const int ix = i % side;
    const int iy = (i / side) % side;
    const int iz = i / (side * side);
    const float lx = static_cast<float>(ix) * spacing - extent;
    const float ly = static_cast<float>(iy + base_y_cells) * spacing;
    const float lz = static_cast<float>(iz) * spacing - extent;
    return vec_add(box->pose.position,
                   vec_add(vec_add(vec_scale(right, lx), vec_scale(up, ly)), vec_scale(forward, lz)));
  };

  int chosen_base = 1;
  bool found = false;
  for (int base_y = 1; base_y <= kMaxExpandLiftCells; ++base_y) {
    bool clear = true;
    for (int i = 0; i < n; ++i) {
      if (occupied.count(cell_of(pose_at(i, base_y), spacing)) != 0) {
        clear = false;
        break;
      }
    }
    if (clear) {
      chosen_base = base_y;
      found = true;
      break;
    }
  }
  if (!found) {
    chosen_base = kMaxExpandLiftCells;
  }

  for (int i = 0; i < n; ++i) {
    children[i]->pose.position = pose_at(i, chosen_base);
    children[i]->pose.orientation = box->pose.orientation;
  }
  box->child_count = static_cast<uint32_t>(n);
  return true;
}

bool Workspace::collapse_box(EntityId box_id, std::string* err) {
  WorkspaceEntry* box = find_mut(box_id);
  if (box == nullptr || box->kind != StorageKind::Box) {
    if (err) {
      *err = "collapse: not a box";
    }
    return false;
  }

  for (auto& e : entries_) {
    if (e.parent_box == box_id) {
      e.in_field = false;
      if (selected_id_ == e.id) {
        selected_id_ = 0;
      }
    }
  }
  box->expanded = false;
  set_box_visual_open(box_id, false);
  return true;
}

void Workspace::resolve_glyphs() {
  for (auto& e : entries_) {
    if (e.kind == StorageKind::Box) {
      const bool want_open = e.expanded || e.glyph_id == "box_open";
      const GlyphMeta* g = nullptr;
      if (registry_) {
        g = registry_->find_box(want_open ? "open" : "closed");
      }
      if (g) {
        e.glyph_id = g->glyph_id;
      } else if (e.glyph_id.empty()) {
        e.glyph_id = want_open ? "box_open" : "procedural_box";
      }
      continue;
    }

    const std::string ext = extension_of(e.relative_path);
    const GlyphMeta* g = registry_ ? registry_->find_for_extension(ext) : nullptr;
    if (g) {
      e.glyph_id = g->glyph_id;
    } else {
      e.glyph_id = "procedural_object";
    }
  }
}

WorkspaceEntry* Workspace::find_mut(EntityId id) {
  for (auto& e : entries_) {
    if (e.id == id) {
      return &e;
    }
  }
  return nullptr;
}

const WorkspaceEntry* Workspace::find(EntityId id) const {
  for (const auto& e : entries_) {
    if (e.id == id) {
      return &e;
    }
  }
  return nullptr;
}

void Workspace::clear_snap() { snap_ = {}; }

bool Workspace::update_snap(EntityId object_id, const Pose& pose) {
  WorkspaceEntry* obj = find_mut(object_id);
  // Objects and Boxes may snap-insert into a different in-field Box.
  if (obj == nullptr || !obj->in_field ||
      (obj->kind != StorageKind::Object && obj->kind != StorageKind::Box)) {
    const bool was = snap_.pending;
    clear_snap();
    return was;
  }

  EntityId best_box = 0;
  float best_d = kSnapDistanceM;
  Pose hold{};
  for (const auto& e : entries_) {
    if (e.kind != StorageKind::Box || !e.in_field || e.id == object_id) {
      continue;
    }
    // Do not nest a Box into one of its descendants (parent_box chain).
    if (obj->kind == StorageKind::Box) {
      bool is_desc = false;
      EntityId walk = e.parent_box;
      while (walk != 0) {
        if (walk == object_id) {
          is_desc = true;
          break;
        }
        const WorkspaceEntry* p = find(walk);
        walk = p != nullptr ? p->parent_box : 0;
      }
      if (is_desc) {
        continue;
      }
    }
    const float d = dist(pose.position, e.pose.position);
    if (d < best_d) {
      best_d = d;
      best_box = e.id;
      hold = e.pose;
      hold.position.y += kHoldAboveBoxM;
    }
  }

  const bool now = best_box != 0;
  const bool edge = now != snap_.pending || (now && best_box != snap_.box_id);
  if (now) {
    snap_.pending = true;
    snap_.object_id = object_id;
    snap_.box_id = best_box;
    snap_.hold_pose = hold;
    obj->pose = hold;
  } else {
    snap_ = {};
    obj->pose = pose;
  }
  return edge;
}

bool Workspace::commit_insert(std::string* err) {
  if (!snap_.pending) {
    if (err) {
      *err = "no snap pending";
    }
    return false;
  }
  if (event_.awaiting_ack) {
    if (err) {
      *err = "prior FS event awaiting ack";
    }
    return false;
  }

  WorkspaceEntry* obj = find_mut(snap_.object_id);
  WorkspaceEntry* box = find_mut(snap_.box_id);
  if (obj == nullptr || box == nullptr) {
    if (err) {
      *err = "missing snap entities";
    }
    return false;
  }
  if (box->kind != StorageKind::Box || obj->id == box->id) {
    if (err) {
      *err = "invalid insert target";
    }
    return false;
  }
  if (obj->kind != StorageKind::Object && obj->kind != StorageKind::Box) {
    if (err) {
      *err = "invalid insert source";
    }
    return false;
  }

  last_insert_object_pose_ = obj->pose;
  have_rollback_ = true;
  rollback_object_ = obj->id;
  rollback_box_ = box->id;
  rollback_prev_parent_ = obj->parent_box;

  event_.type = StorageEventType::ObjectMovedIntoBox;
  event_.object_id = obj->id;
  event_.box_id = box->id;
  event_.source_rel = obj->relative_path;
  event_.dest_rel = box->relative_path;
  if (!event_.dest_rel.empty() && event_.dest_rel.back() != '/' && event_.dest_rel.back() != '\\') {
    event_.dest_rel.push_back('/');
  }
  event_.dest_rel += obj->name;
  event_.awaiting_ack = true;

  // Detach from prior parent bookkeeping before attaching to the snap target.
  if (obj->parent_box != 0 && obj->parent_box != box->id) {
    if (WorkspaceEntry* prev = find_mut(obj->parent_box)) {
      if (prev->child_count > 0) {
        prev->child_count -= 1;
      }
    }
  }

  obj->in_field = false;
  obj->parent_box = box->id;
  box->child_count += 1;
  if (selected_id_ == obj->id) {
    selected_id_ = 0;
  }
  clear_snap();
  return true;
}

bool Workspace::rollback_insert(EntityId object_id, EntityId box_id) {
  WorkspaceEntry* obj = find_mut(object_id);
  WorkspaceEntry* box = find_mut(box_id);
  if (obj == nullptr || box == nullptr) {
    return false;
  }
  const EntityId restore_parent =
      (have_rollback_ && rollback_object_ == object_id) ? rollback_prev_parent_ : 0;
  obj->in_field = true;
  obj->parent_box = restore_parent;
  if (have_rollback_ && rollback_object_ == object_id) {
    obj->pose = last_insert_object_pose_;
  }
  if (box->child_count > 0) {
    box->child_count -= 1;
  }
  if (restore_parent != 0 && restore_parent != box_id) {
    if (WorkspaceEntry* prev = find_mut(restore_parent)) {
      prev->child_count += 1;
    }
  }
  have_rollback_ = false;
  rollback_prev_parent_ = 0;
  return true;
}

void Workspace::ack_event(bool success) {
  if (!event_.awaiting_ack) {
    return;
  }
  if (!success) {
    rollback_insert(event_.object_id, event_.box_id);
  } else {
    // Persist path rewrite in model
    if (auto* obj = find_mut(event_.object_id)) {
      obj->relative_path = event_.dest_rel;
    }
    have_rollback_ = false;
  }
  event_ = {};
}

void Workspace::set_box_visual_open(EntityId box_id, bool open) {
  WorkspaceEntry* box = find_mut(box_id);
  if (box == nullptr || box->kind != StorageKind::Box || registry_ == nullptr) {
    return;
  }
  const GlyphMeta* g = registry_->find_box(open ? "open" : "closed");
  if (g) {
    box->glyph_id = g->glyph_id;
  }
}

}  // namespace threeos
