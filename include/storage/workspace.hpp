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
  bool in_field = true;  // false once inserted / collapsed out of the field
  EntityId parent_box = 0;
  /// MindPalace root this entry belongs to (0 = unassigned / legacy single field).
  EntityId mind_palace_id = 0;
  std::string glyph_id;
  /// Box-only: contents currently expanded into the field.
  bool expanded = false;
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

/// Near-field demo workspace: host-fed listing, glyph resolve, snap-insert, box expand.
class Workspace {
 public:
  static constexpr float kSnapDistanceM = 0.2f;
  static constexpr float kHoldAboveBoxM = 0.14f;
  /// Once seated, free/escape pose may leave this radius around the hold to unsnap
  /// (3″). Enter still uses `kSnapDistanceM` from the Box center.
  static constexpr float kSnapEscapeBufferM = 3.f * 0.0254f;
  static constexpr float kDefaultFieldSpacingM = 0.22f;
  static constexpr int kMaxExpandLiftCells = 32;

  void clear();
  void set_glyph_registry(GlyphRegistry* registry) { registry_ = registry; }

  float field_spacing_m() const { return field_spacing_m_; }
  void set_field_spacing_m(float m) {
    field_spacing_m_ = m > 0.01f ? m : kDefaultFieldSpacingM;
  }

  EntityId add_object(const std::string& name, const std::string& relative_path);
  EntityId add_box(const std::string& name, const std::string& relative_path,
                   uint32_t child_count);

  /// Parent under a box (0 = root). Does not change in_field.
  bool set_parent_box(EntityId id, EntityId parent_box_id);
  bool set_in_field(EntityId id, bool in_field);
  bool set_mind_palace(EntityId id, EntityId mind_palace_id);

  /// Place root-field items in a cubic matrix 1 m in front of `head` (OpenXR).
  /// Side length = ceil(cbrt(n)) so 1–8 → 2³, 9–27 → 3³, etc. (n==1 → 1³).
  /// Uses yaw-only heading (world up) so head roll/pitch does not tilt the field.
  /// If `mind_palace_id != 0`, only that mind palace's root children are laid out (still 1 m ahead).
  void layout_demo_field(const Pose& head);
  void layout_demo_field(const Pose& head, EntityId mind_palace_id);

  /// Place one mind palace's root in-field children in a cubic matrix centered on `center`.
  /// Yaw from `center.orientation` (floor-parallel). Returns false if mind palace has no items.
  bool layout_mind_palace_field(EntityId mind_palace_id, const Pose& center);

  /// Show children of `box_id` in a cubic matrix above the box (collision-aware).
  /// Children are entries with parent_box == box_id; they are set in_field=true.
  bool layout_box_expand(EntityId box_id, std::string* err);

  /// Hide expanded children (in_field=false); close box visual.
  bool collapse_box(EntityId box_id, std::string* err);

  /// Resolve glyph_ids from registry (extensions / box state).
  void resolve_glyphs();

  const std::vector<WorkspaceEntry>& entries() const { return entries_; }
  WorkspaceEntry* find_mut(EntityId id);
  const WorkspaceEntry* find(EntityId id) const;

  /// While dragging an Object or Box at `pose`, update snap vs other Boxes.
  /// `pose` should be the free/escape kinematic pose (not the display hold).
  /// Once snapped, stays on that Box until `pose` leaves `kSnapEscapeBufferM`
  /// around the hold seat (ignores nearer neighbors — prevents hold-pose cascade).
  /// When `apply_pose` is false, only snap state is updated (ghost-scale drag targets
  /// with a near pose must not teleport the far workspace entry onto the ghost).
  /// Returns true if snap enter/exit edge occurred.
  bool update_snap(EntityId object_id, const Pose& pose, bool apply_pose = true);

  /// Arm snap to a concrete box without pose search (ghost near-field targeting).
  /// Does not move the object pose. Returns false if the pair is invalid.
  bool arm_snap_to_box(EntityId object_id, EntityId box_id);

  const SnapState& snap() const { return snap_; }
  void clear_snap();

  /// Commit insert if snap pending. Queues FS event; hides object from field.
  bool commit_insert(std::string* err);

  /// Insert `object_id` into `box_id` without requiring a prior snap (cross-proxy select-drop).
  bool insert_into(EntityId object_id, EntityId box_id, std::string* err);

  /// Roll back a failed FS ack (restore object to field).
  bool rollback_insert(EntityId object_id, EntityId box_id);

  const StorageEvent& pending_event() const { return event_; }
  void ack_event(bool success);

  /// Temporarily show box open glyph during host anim (cosmetic).
  void set_box_visual_open(EntityId box_id, bool open);

  /// Global single selection (`0` = none).
  EntityId selection() const { return selected_id_; }
  void clear_selection() { selected_id_ = 0; }
  /// Returns false if `id` is nonzero and not a known workspace entry.
  bool set_selection(EntityId id);

 private:
  EntityId next_id_ = 2000;
  EntityId selected_id_ = 0;
  float field_spacing_m_ = kDefaultFieldSpacingM;
  std::vector<WorkspaceEntry> entries_;
  GlyphRegistry* registry_ = nullptr;
  SnapState snap_{};
  StorageEvent event_{};
  // For rollback
  Pose last_insert_object_pose_{};
  bool have_rollback_ = false;
  EntityId rollback_object_ = 0;
  EntityId rollback_box_ = 0;
  EntityId rollback_prev_parent_ = 0;
};

}  // namespace threeos
