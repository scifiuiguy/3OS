#pragma once

#include "core/types.hpp"
#include "storage/workspace.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace threeos {

enum class CentroidKind : uint32_t {
  None = 0,
  MindPalace = 1,
  Box = 2,
  Temporary = 3,  // near-portal temp centroid (Task 6.4)
  FocusAnchor = 4,  // world target (e.g. NPC head); snap ≠ portal content
};

struct MindPalaceRoot {
  EntityId id = 0;
  std::string name;
  Pose anchor{};
};

struct CentroidState {
  EntityId id = 0;               // stable: mind palace id or box entity id
  CentroidKind kind = CentroidKind::None;
  EntityId linked_entity_id = 0; // same as id for 6.2
  uint32_t member_count = 0;
  Vec3 position{};
  std::string name;
};

/// Live centroid registry: average of in-field sibling poses; omit empty sets.
class CentroidRegistry {
 public:
  void clear();
  void clear_mind_palaces();

  /// Register a mind palace root (imaginary parent). Returns id (0 on failure).
  EntityId register_mind_palace(const std::string& name, const Pose& anchor);
  bool set_mind_palace_anchor(EntityId mind_palace_id, const Pose& anchor);
  const MindPalaceRoot* find_mind_palace(EntityId mind_palace_id) const;
  const std::vector<MindPalaceRoot>& mind_palaces() const { return mind_palaces_; }

  /// Temporary centroid for a near portal (survives rebuild until cleared).
  EntityId register_temporary(const std::string& name, const Vec3& position);
  bool set_temporary_position(EntityId id, const Vec3& position);
  bool clear_temporary(EntityId id);

  /// World focus anchor (NPC head, etc.). Survives rebuild; host updates pose each frame.
  EntityId register_focus_anchor(const std::string& name, const Pose& pose);
  bool set_focus_anchor_pose(EntityId id, const Pose& pose);
  bool clear_focus_anchor(EntityId id);

  /// Rebuild from workspace: mind palace roots + expanded boxes with in-field children.
  /// Temporary + FocusAnchor centroids are preserved across rebuild.
  void rebuild(const Workspace& workspace);

  const std::vector<CentroidState>& centroids() const { return centroids_; }
  const CentroidState* find(EntityId id) const;

  /// Axis-aligned bounds of in-field members for a centroid (false if empty/unknown).
  bool members_aabb(const Workspace& workspace, EntityId centroid_id, Vec3* out_min,
                    Vec3* out_max) const;

  /// True if `entity_id` belongs to a live centroid the user can still snap to: an expanded,
  /// in-field parent Box, a Box that is itself expanded, or a mind palace with members.
  /// A file carried out of its folder (parent Box no longer in field) has none.
  bool has_accessible_centroid(const Workspace& workspace, EntityId entity_id) const;

  /// Accessible centroid id for `entity_id` (0 if none). Same rules as has_accessible_centroid.
  EntityId accessible_centroid_id(const Workspace& workspace, EntityId entity_id) const;

 private:
  EntityId next_mind_palace_id_ = 100;       // below workspace ids (2000+)
  EntityId next_temp_id_ = 50;       // below mind palaces
  EntityId next_focus_id_ = 20;      // below temps
  std::vector<MindPalaceRoot> mind_palaces_;
  std::vector<CentroidState> temps_;
  std::vector<CentroidState> focus_anchors_;
  std::vector<CentroidState> centroids_;
};

}  // namespace threeos
