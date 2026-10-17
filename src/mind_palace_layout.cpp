#include "core/mind_palace_layout.hpp"

#include "storage/workspace.hpp"

#include <cmath>
#include <vector>

namespace threeos {
namespace {

float length_xz(float x, float z) { return std::sqrt(x * x + z * z); }

Vec3 flat_forward(const Pose& head, bool head_valid) {
  if (!head_valid) {
    return Vec3{0.f, 0.f, -1.f};
  }
  const Quat& q = head.orientation;
  const float x = 2.f * (q.x * q.z + q.w * q.y);
  const float z = 1.f - 2.f * (q.x * q.x + q.y * q.y);
  const float len = length_xz(x, z);
  if (len < 1e-5f) {
    return Vec3{0.f, 0.f, -1.f};
  }
  return Vec3{x / len, 0.f, z / len};
}

Quat yaw_facing(const Vec3& from, const Vec3& to) {
  Vec3 dir{to.x - from.x, 0.f, to.z - from.z};
  const float len = length_xz(dir.x, dir.z);
  if (len < 1e-5f) {
    return Quat{0.f, 0.f, 0.f, 1.f};
  }
  dir.x /= len;
  dir.z /= len;
  // Face toward user: look along -dir from placement back to body.
  const float yaw = std::atan2(-dir.x, dir.z);  // face = -dir relative to -Z
  const float half = yaw * 0.5f;
  return Quat{0.f, std::sin(half), 0.f, std::cos(half)};
}

bool slot_clear(const Vec3& cand, const std::vector<Vec3>& occupied, float spacing) {
  for (const auto& o : occupied) {
    const float dx = cand.x - o.x;
    const float dy = cand.y - o.y;
    const float dz = cand.z - o.z;
    if (std::sqrt(dx * dx + dy * dy + dz * dz) < spacing) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool place_next_mind_palace_anchor(const Pose& head, bool head_valid, const CentroidRegistry& centroids,
                           float field_radius_m, float field_height_m, const std::string& mind_palace_name,
                           Pose* out_anchor, std::string* err, EntityId exclude_mind_palace_id) {
  if (out_anchor == nullptr) {
    if (err) {
      *err = "null anchor out";
    }
    return false;
  }
  const Vec3 body{head_valid ? head.position.x : 0.f, head_valid ? head.position.y : 1.5f,
                  head_valid ? head.position.z : 0.f};
  const Vec3 fwd = flat_forward(head, head_valid);
  const Vec3 right{fwd.z, 0.f, -fwd.x};

  std::vector<Vec3> occupied;
  occupied.reserve(centroids.mind_palaces().size());
  for (const auto& g : centroids.mind_palaces()) {
    if (exclude_mind_palace_id != 0 && g.id == exclude_mind_palace_id) {
      continue;
    }
    occupied.push_back(g.anchor.position);
  }

  constexpr float kBaseRangeM = 10.f * 0.3048f;  // 10 ft
  constexpr float kBaseHeightM = 2.5f * 0.3048f;
  constexpr float kMindPalaceSpacingM = 1.2f;
  const float y0 = body.y + kBaseHeightM;
  const float r_max = field_radius_m > 1.f ? field_radius_m : 15.24f;
  const float h_max = field_height_m > 1.f ? field_height_m : 3.f;

  // First mind palace: straight ahead.
  if (occupied.empty()) {
    out_anchor->position =
        Vec3{body.x + fwd.x * kBaseRangeM, y0, body.z + fwd.z * kBaseRangeM};
    out_anchor->orientation = yaw_facing(out_anchor->position, body);
    return true;
  }

  // Lateral orbit at base radius, then concentric rings, then vertical stacks.
  const float ring_step = std::max(kMindPalaceSpacingM, 1.0f);
  for (float ring_r = kBaseRangeM; ring_r <= r_max + 1e-3f; ring_r += ring_step) {
    const int slots = std::max(6, static_cast<int>(2.f * 3.14159265f * ring_r / kMindPalaceSpacingM));
    for (int i = 0; i < slots; ++i) {
      const float az = (static_cast<float>(i) / static_cast<float>(slots)) * 6.2831853f;
      // az=0 → forward
      const float c = std::cos(az);
      const float s = std::sin(az);
      const Vec3 cand{body.x + (fwd.x * c + right.x * s) * ring_r, y0,
                      body.z + (fwd.z * c + right.z * s) * ring_r};
      if (slot_clear(cand, occupied, kMindPalaceSpacingM)) {
        out_anchor->position = cand;
        out_anchor->orientation = yaw_facing(cand, body);
        return true;
      }
    }
  }

  // Stack upward at base ring positions.
  const float y_step = kMindPalaceSpacingM;
  for (float y = y0 + y_step; y <= body.y + h_max + 1e-3f; y += y_step) {
    const int slots =
        std::max(6, static_cast<int>(2.f * 3.14159265f * kBaseRangeM / kMindPalaceSpacingM));
    for (int i = 0; i < slots; ++i) {
      const float az = (static_cast<float>(i) / static_cast<float>(slots)) * 6.2831853f;
      const float c = std::cos(az);
      const float s = std::sin(az);
      const Vec3 cand{body.x + (fwd.x * c + right.x * s) * kBaseRangeM, y,
                      body.z + (fwd.z * c + right.z * s) * kBaseRangeM};
      if (slot_clear(cand, occupied, kMindPalaceSpacingM * 0.85f)) {
        out_anchor->position = cand;
        out_anchor->orientation = yaw_facing(cand, body);
        return true;
      }
    }
  }

  if (err) {
    *err = std::string("mind palace field full: ") + (mind_palace_name.empty() ? "(unnamed)" : mind_palace_name);
  }
  return false;
}

void resolve_mutual_object_overlaps(Workspace& workspace, const Pose& head, bool head_valid,
                                    float min_spacing_m, uint32_t iterations) {
  const float spacing = min_spacing_m > 0.01f ? min_spacing_m : Workspace::kDefaultFieldSpacingM;
  const Vec3 fwd = flat_forward(head, head_valid);
  const Vec3 right{fwd.z, 0.f, -fwd.x};

  std::vector<WorkspaceEntry*> items;
  const auto snap = workspace.entries();
  items.reserve(snap.size());
  for (const auto& e : snap) {
    if (!e.in_field || e.id == 0) {
      continue;
    }
    if (auto* p = workspace.find_mut(e.id)) {
      items.push_back(p);
    }
  }

  for (uint32_t iter = 0; iter < iterations; ++iter) {
    bool moved = false;
    for (size_t i = 0; i < items.size(); ++i) {
      for (size_t j = i + 1; j < items.size(); ++j) {
        Vec3& a = items[i]->pose.position;
        Vec3& b = items[j]->pose.position;
        // Use full 3D distance so cubic matrix stacks (same XZ, different Y) are not
        // treated as overlaps — XZ-only was shoving MindPalace/box grids on every park.
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        float dz = b.z - a.z;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist + 1e-4f >= spacing) {
          continue;
        }
        if (dist < 1e-4f) {
          // Degenerate: push along body-lateral on XZ.
          dx = right.x;
          dy = 0.f;
          dz = right.z;
          dist = length_xz(dx, dz);
          if (dist < 1e-4f) {
            dx = 1.f;
            dz = 0.f;
            dist = 1.f;
          }
        }
        const float push = (spacing - dist) * 0.5f;
        const float inv = 1.f / dist;
        a.x -= dx * inv * push;
        a.y -= dy * inv * push;
        a.z -= dz * inv * push;
        b.x += dx * inv * push;
        b.y += dy * inv * push;
        b.z += dz * inv * push;
        moved = true;
      }
    }
    if (!moved) {
      break;
    }
  }
}

}  // namespace threeos
