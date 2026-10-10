#pragma once

#include "core/types.hpp"
#include "storage/glyph_registry.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace threeos {

enum class StorageKind : uint32_t {
  Object = 0,
  Box = 1,
};

enum class StorageEventType : uint32_t {
  None = 0,
  ObjectMovedIntoBox = 1,
};

struct WorkspaceEntry {
  EntityId id = 0;
  StorageKind kind = StorageKind::Object;
  std::string name;
  std::string relative_path;
  uint32_t child_count = 0;
  Pose pose{};
  bool in_field = true;  // false once inserted into a box (demo field)
  EntityId parent_box = 0;
  std::string glyph_id;
};

struct SnapState {
  bool pending = false;
  EntityId object_id = 0;
  EntityId box_id = 0;
  Pose hold_pose{};
};

struct StorageEvent {
  StorageEventType type = StorageEventType::None;
  EntityId object_id = 0;
  EntityId box_id = 0;
  std::string source_rel;
  std::string dest_rel;
  bool awaiting_ack = false;
};

/// Near-field demo workspace: host-fed listing, glyph resolve, snap-insert.
class Workspace {
 public:
  static constexpr float kSnapDistanceM = 0.2f;
  static constexpr float kHoldAboveBoxM = 0.14f;

  void clear();
  void set_glyph_registry(GlyphRegistry* registry) { registry_ = registry; }

  EntityId add_object(const std::string& name, const std::string& relative_path);
  EntityId add_box(const std::string& name, const std::string& relative_path,
                   uint32_t child_count);

  /// Place root-field items in a cubic matrix 1 m in front of `head` (OpenXR).
  /// Side length = ceil(cbrt(n)) so 1–8 → 2³, 9–27 → 3³, etc. (n==1 → 1³).
  void layout_demo_field(const Pose& head);

  /// Resolve glyph_ids from registry (extensions / box state).
  void resolve_glyphs();

  const std::vector<WorkspaceEntry>& entries() const { return entries_; }
  WorkspaceEntry* find_mut(EntityId id);
  const WorkspaceEntry* find(EntityId id) const;

  /// While dragging `object_id` at `pose`, update snap vs boxes in field.
  /// Returns true if snap enter/exit edge occurred (caller should re-anchor stick).
  bool update_snap(EntityId object_id, const Pose& pose);

  const SnapState& snap() const { return snap_; }
  void clear_snap();

  /// Commit insert if snap pending. Queues FS event; hides object from field.
  bool commit_insert(std::string* err);

  /// Roll back a failed FS ack (restore object to field).
  bool rollback_insert(EntityId object_id, EntityId box_id);

  const StorageEvent& pending_event() const { return event_; }
  void ack_event(bool success);

  /// Temporarily show box open glyph during host anim (cosmetic).
  void set_box_visual_open(EntityId box_id, bool open);

 private:
  EntityId next_id_ = 2000;
  std::vector<WorkspaceEntry> entries_;
  GlyphRegistry* registry_ = nullptr;
  SnapState snap_{};
  StorageEvent event_{};
  // For rollback
  Pose last_insert_object_pose_{};
  bool have_rollback_ = false;
  EntityId rollback_object_ = 0;
  EntityId rollback_box_ = 0;
};

}  // namespace threeos
