#pragma once

#include "core/types.hpp"
#include "storage/workspace.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace threeos {

enum class CentroidKind : uint32_t {
  None = 0,
  God = 1,
  Box = 2,
  Temporary = 3,  // near-portal temp centroid (Task 6.4)
};

struct GodRoot {
  EntityId id = 0;
  std::string name;
  Pose anchor{};
};

struct CentroidState {
  EntityId id = 0;               // stable: god id or box entity id
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
  void clear_gods();

  /// Register a god root (imaginary parent). Returns id (0 on failure).
  EntityId register_god(const std::string& name, const Pose& anchor);
  bool set_god_anchor(EntityId god_id, const Pose& anchor);
  const GodRoot* find_god(EntityId god_id) const;
  const std::vector<GodRoot>& gods() const { return gods_; }

  /// Temporary centroid for a near portal (survives rebuild until cleared).
  EntityId register_temporary(const std::string& name, const Vec3& position);
  bool set_temporary_position(EntityId id, const Vec3& position);
  bool clear_temporary(EntityId id);

  /// Rebuild from workspace: god roots + expanded boxes with in-field children.
  /// Temporary centroids are preserved across rebuild.
  void rebuild(const Workspace& workspace);

  const std::vector<CentroidState>& centroids() const { return centroids_; }
  const CentroidState* find(EntityId id) const;

  /// Axis-aligned bounds of in-field members for a centroid (false if empty/unknown).
  bool members_aabb(const Workspace& workspace, EntityId centroid_id, Vec3* out_min,
                    Vec3* out_max) const;

 private:
  EntityId next_god_id_ = 100;       // below workspace ids (2000+)
  EntityId next_temp_id_ = 50;       // below gods
  std::vector<GodRoot> gods_;
  std::vector<CentroidState> temps_;
  std::vector<CentroidState> centroids_;
};

}  // namespace threeos
