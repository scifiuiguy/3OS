#pragma once

#include "core/types.hpp"

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace threeos {

enum class MultiChoiceMode : uint32_t {
  Closed = 0,
  DrivePicker = 1,
  FolderContext = 2,
};

enum class MultiChoiceSection : uint32_t {
  RootDrive = 0,
  Favorites = 1,
};

enum class MultiChoiceSlotKind : uint32_t {
  Empty = 0,
  DriveRoot = 1,
  FavoriteBox = 2,
  ActionFavorite = 3,  // folder-context slot 0
  ConfirmAdd = 4,
  ConfirmJump = 5,
};

enum class MultiChoiceAction : uint32_t {
  None = 0,
  Add = 1,
  JumpToRoot = 2,
  ToggleFavorite = 3,
};

/// Host-fed available drive root (Documents folder stand-in).
struct DriveRootDesc {
  EntityId id = 0;
  std::string name;
};

struct MultiChoiceSlot {
  MultiChoiceSlotKind kind = MultiChoiceSlotKind::Empty;
  EntityId target_id = 0;  // drive root id or box entity id
  char label[32]{};
  uint32_t enabled = 0;
};

/// Pure hover math (CubeDotSensor-style dots + magnitude bands). Shared with harness.
struct MultiChoiceHoverResult {
  int32_t slot = -1;  // 0..7 octant, 8=page back, 9=page forward, -1=none
  float magnitude = 0.f;
  float dot_right = 0.f;
  float dot_up = 0.f;
  float dot_forward = 0.f;
};

class MultiChoiceEngine {
 public:
  // ~6″ cube (2×2×2 @ 3″ voxels): center pagination inside ~1.4″; octants out to ~4.3″.
  static constexpr float kMainMinM = 0.035f;
  static constexpr float kMainMaxM = 0.11f;
  static constexpr float kConfirmMinM = 0.015f;
  static constexpr float kConfirmMaxM = 0.07f;
  /// Confirm strip sits toward the user (+Z) from the main widget, not above.
  static constexpr float kConfirmForwardM = 0.10f;
  static constexpr uint32_t kPageSize = 8;

  void reset();

  void set_available_roots(const DriveRootDesc* roots, uint32_t count);
  const std::vector<DriveRootDesc>& available_roots() const { return roots_; }

  bool set_favorite(EntityId box_id, bool favored);
  bool is_favorite(EntityId box_id) const;
  const std::unordered_set<EntityId>& favorites() const { return favorites_; }

  /// Fresh widget; closes any prior groups. Returns false if already closed args invalid.
  bool open_drive_picker(uint32_t hand, const Pose& center);
  bool open_folder_context(uint32_t hand, const Pose& center, EntityId box_id);
  void close_all();

  bool open() const { return mode_ != MultiChoiceMode::Closed; }
  MultiChoiceMode mode() const { return mode_; }
  uint32_t hand() const { return hand_; }
  const Pose& center() const { return center_; }
  MultiChoiceSection section() const { return section_; }
  uint32_t page_index() const { return page_index_; }
  bool confirm_open() const { return confirm_open_; }
  const Pose& confirm_center() const { return confirm_center_; }
  EntityId context_box() const { return context_box_; }
  EntityId pending_target() const { return pending_target_; }
  MultiChoiceSlotKind pending_kind() const { return pending_kind_; }

  /// Update hover from cursor (OpenXR). Outside both thresholds → hover none.
  void tick_hover(const Vec3& cursor);

  int32_t hovered_slot() const { return hovered_slot_; }
  int32_t confirm_hovered() const { return confirm_hovered_; }  // 0=Add, 1=Jump, -1=none
  bool cursor_inside_any() const { return cursor_inside_any_; }

  const MultiChoiceSlot* slots() const { return slots_; }
  uint32_t page_back_enabled() const { return page_back_enabled_; }
  uint32_t page_forward_enabled() const { return page_forward_enabled_; }
  const char* title() const { return title_; }
  const char* hover_label() const { return hover_label_; }

  /// Primary-pinch rising edge. Returns true if state changed.
  bool on_primary_confirm();

  /// Last committed action after confirm strip / favorite toggle (host polls then clears).
  MultiChoiceAction consume_action(EntityId* out_target_id, MultiChoiceSlotKind* out_kind);

  /// Octant / center hover from direction + magnitude. Widget axes from pose.
  /// Geometric octant bits: bit0=+right, bit1=+up, bit2=+forward(−Z). Result.slot for
  /// octants is remapped to fill index (see slot_index_from_octant).
  static MultiChoiceHoverResult compute_hover(const Pose& widget, const Vec3& cursor, float min_m,
                                              float max_m, bool pagination_pair);

  /// Fill order 0..7: TLF, TRF, BLF, BRF, TLB, TRB, BLB, BRB (front toward user / +Z).
  static uint32_t octant_from_slot_index(uint32_t slot /*0..7*/);
  static uint32_t slot_index_from_octant(uint32_t octant /*0..7*/);

  /// Local offset for geometric octant bits (±right ±up ±forward) * radius.
  static Vec3 octant_local_offset(uint32_t octant /*0..7*/, float radius_m);
  /// Local offset for fill-order slot index.
  static Vec3 slot_local_offset(uint32_t slot /*0..7*/, float radius_m);

 private:
  void rebuild_slots();
  void rebuild_pagination_flags(uint32_t option_count);
  uint32_t option_count_for_section() const;
  void fill_drive_page();
  void fill_favorites_page();
  void fill_folder_context();
  void open_confirm_strip(EntityId target, MultiChoiceSlotKind kind);
  static void copy_label(char* dst, size_t n, const std::string& s);

  MultiChoiceMode mode_ = MultiChoiceMode::Closed;
  uint32_t hand_ = 0;
  Pose center_{};
  MultiChoiceSection section_ = MultiChoiceSection::RootDrive;
  uint32_t page_index_ = 0;
  MultiChoiceSlot slots_[8]{};
  uint32_t page_back_enabled_ = 0;
  uint32_t page_forward_enabled_ = 0;
  char title_[32]{};
  char hover_label_[32]{};
  int32_t hovered_slot_ = -1;
  bool cursor_inside_any_ = false;

  bool confirm_open_ = false;
  Pose confirm_center_{};
  int32_t confirm_hovered_ = -1;
  EntityId pending_target_ = 0;
  MultiChoiceSlotKind pending_kind_ = MultiChoiceSlotKind::Empty;

  EntityId context_box_ = 0;
  bool context_box_favored_ = false;

  std::vector<DriveRootDesc> roots_;
  std::unordered_set<EntityId> favorites_;

  MultiChoiceAction pending_action_ = MultiChoiceAction::None;
  EntityId action_target_ = 0;
  MultiChoiceSlotKind action_kind_ = MultiChoiceSlotKind::Empty;

  /// After open, block primary until cursor leaves center spawn or hovers an enabled slot
  /// (prevents center pagination from jumping Root→Favorites on the open frame).
  bool confirm_armed_ = false;
};

}  // namespace threeos
