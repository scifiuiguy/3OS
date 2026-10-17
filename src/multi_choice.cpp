#include "core/multi_choice.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace threeos {
namespace {

float length(const Vec3& v) {
  return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vec3 normalize_or(const Vec3& v, const Vec3& fallback) {
  const float len = length(v);
  if (len < 1e-6f) {
    return fallback;
  }
  return Vec3{v.x / len, v.y / len, v.z / len};
}

Vec3 quat_rotate(const Quat& q, const Vec3& v) {
  const Vec3 u{q.x, q.y, q.z};
  const float s = q.w;
  const Vec3 uxv{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
  const float udotv = u.x * v.x + u.y * v.y + u.z * v.z;
  return Vec3{2.f * udotv * u.x + (s * s - (u.x * u.x + u.y * u.y + u.z * u.z)) * v.x +
                  2.f * s * uxv.x,
              2.f * udotv * u.y + (s * s - (u.x * u.x + u.y * u.y + u.z * u.z)) * v.y +
                  2.f * s * uxv.y,
              2.f * udotv * u.z + (s * s - (u.x * u.x + u.y * u.y + u.z * u.z)) * v.z +
                  2.f * s * uxv.z};
}

void widget_axes(const Pose& widget, Vec3* right, Vec3* up, Vec3* forward) {
  *right = normalize_or(quat_rotate(widget.orientation, Vec3{1.f, 0.f, 0.f}), Vec3{1.f, 0.f, 0.f});
  *up = normalize_or(quat_rotate(widget.orientation, Vec3{0.f, 1.f, 0.f}), Vec3{0.f, 1.f, 0.f});
  *forward = normalize_or(quat_rotate(widget.orientation, Vec3{0.f, 0.f, -1.f}),
                          Vec3{0.f, 0.f, -1.f});
}

float dot3(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

}  // namespace

void MultiChoiceEngine::copy_label(char* dst, size_t n, const std::string& s) {
  if (dst == nullptr || n == 0) {
    return;
  }
  if (s.empty()) {
    dst[0] = '\0';
    return;
  }
  const size_t copy = std::min(n - 1, s.size());
  std::memcpy(dst, s.data(), copy);
  dst[copy] = '\0';
}

void MultiChoiceEngine::reset() {
  mode_ = MultiChoiceMode::Closed;
  hand_ = 0;
  center_ = {};
  section_ = MultiChoiceSection::RootDrive;
  page_index_ = 0;
  for (auto& s : slots_) {
    s = {};
  }
  page_back_enabled_ = 0;
  page_forward_enabled_ = 0;
  title_[0] = '\0';
  hover_label_[0] = '\0';
  hovered_slot_ = -1;
  cursor_inside_any_ = false;
  confirm_open_ = false;
  confirm_center_ = {};
  confirm_hovered_ = -1;
  pending_target_ = 0;
  pending_kind_ = MultiChoiceSlotKind::Empty;
  context_box_ = 0;
  context_box_favored_ = false;
  roots_.clear();
  favorites_.clear();
  pending_action_ = MultiChoiceAction::None;
  action_target_ = 0;
  action_kind_ = MultiChoiceSlotKind::Empty;
  confirm_armed_ = false;
}

void MultiChoiceEngine::set_available_roots(const DriveRootDesc* roots, uint32_t count) {
  roots_.clear();
  if (roots == nullptr || count == 0) {
    if (mode_ == MultiChoiceMode::DrivePicker && section_ == MultiChoiceSection::RootDrive) {
      rebuild_slots();
    }
    return;
  }
  roots_.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    if (roots[i].id == 0 || roots[i].name.empty()) {
      continue;
    }
    roots_.push_back(roots[i]);
  }
  if (mode_ == MultiChoiceMode::DrivePicker && section_ == MultiChoiceSection::RootDrive) {
    rebuild_slots();
  }
}

bool MultiChoiceEngine::set_favorite(EntityId box_id, bool favored) {
  if (box_id == 0) {
    return false;
  }
  if (favored) {
    favorites_.insert(box_id);
  } else {
    favorites_.erase(box_id);
  }
  if (mode_ == MultiChoiceMode::FolderContext && context_box_ == box_id) {
    context_box_favored_ = favored;
    rebuild_slots();
  } else if (mode_ == MultiChoiceMode::DrivePicker && section_ == MultiChoiceSection::Favorites) {
    rebuild_slots();
  }
  return true;
}

bool MultiChoiceEngine::is_favorite(EntityId box_id) const {
  return box_id != 0 && favorites_.count(box_id) != 0;
}

void MultiChoiceEngine::close_all() {
  mode_ = MultiChoiceMode::Closed;
  confirm_open_ = false;
  confirm_hovered_ = -1;
  hovered_slot_ = -1;
  cursor_inside_any_ = false;
  pending_target_ = 0;
  pending_kind_ = MultiChoiceSlotKind::Empty;
  context_box_ = 0;
  confirm_armed_ = false;
  for (auto& s : slots_) {
    s = {};
  }
  title_[0] = '\0';
  hover_label_[0] = '\0';
}

bool MultiChoiceEngine::open_drive_picker(uint32_t hand, const Pose& center) {
  close_all();
  mode_ = MultiChoiceMode::DrivePicker;
  hand_ = hand;
  center_ = center;
  section_ = MultiChoiceSection::RootDrive;
  page_index_ = 0;
  confirm_armed_ = false;
  copy_label(title_, sizeof(title_), "Root Drive:");
  rebuild_slots();
  return true;
}

bool MultiChoiceEngine::open_folder_context(uint32_t hand, const Pose& center, EntityId box_id) {
  if (box_id == 0) {
    return false;
  }
  close_all();
  mode_ = MultiChoiceMode::FolderContext;
  hand_ = hand;
  center_ = center;
  context_box_ = box_id;
  context_box_favored_ = is_favorite(box_id);
  section_ = MultiChoiceSection::RootDrive;
  page_index_ = 0;
  confirm_armed_ = false;
  copy_label(title_, sizeof(title_), "Folder:");
  rebuild_slots();
  return true;
}

uint32_t MultiChoiceEngine::option_count_for_section() const {
  if (mode_ == MultiChoiceMode::FolderContext) {
    return 1;  // favorite heart only for now
  }
  if (section_ == MultiChoiceSection::RootDrive) {
    return static_cast<uint32_t>(roots_.size());
  }
  return static_cast<uint32_t>(favorites_.size());
}

void MultiChoiceEngine::rebuild_pagination_flags(uint32_t option_count) {
  page_back_enabled_ = 0;
  page_forward_enabled_ = 0;
  const uint32_t pages =
      option_count == 0 ? 1u : (option_count + kPageSize - 1) / kPageSize;
  // Within-section pages.
  if (page_index_ > 0) {
    page_back_enabled_ = 1;
  }
  if (page_index_ + 1 < pages) {
    page_forward_enabled_ = 1;
  }
  // Drive picker: section bridge Root Drive ↔ Favorites (keeps arrows visible / intentional).
  if (mode_ == MultiChoiceMode::DrivePicker) {
    if (section_ == MultiChoiceSection::RootDrive && page_index_ + 1 >= pages) {
      page_forward_enabled_ = 1;  // last root page → Favorites
    }
    if (section_ == MultiChoiceSection::Favorites && page_index_ == 0) {
      page_back_enabled_ = 1;  // Favorites → Root Drive
    }
  }
}

void MultiChoiceEngine::fill_drive_page() {
  for (auto& s : slots_) {
    s = {};
  }
  const uint32_t total = static_cast<uint32_t>(roots_.size());
  rebuild_pagination_flags(total);
  const uint32_t start = page_index_ * kPageSize;
  for (uint32_t i = 0; i < kPageSize; ++i) {
    const uint32_t idx = start + i;
    if (idx >= total) {
      break;
    }
    slots_[i].kind = MultiChoiceSlotKind::DriveRoot;
    slots_[i].target_id = roots_[idx].id;
    slots_[i].enabled = 1;
    // Path-style label for in-slot + hover (e.g. "/3OS_Demo").
    copy_label(slots_[i].label, sizeof(slots_[i].label),
               std::string("/") + roots_[idx].name);
  }
}

void MultiChoiceEngine::fill_favorites_page() {
  for (auto& s : slots_) {
    s = {};
  }
  std::vector<EntityId> ids(favorites_.begin(), favorites_.end());
  std::sort(ids.begin(), ids.end());
  const uint32_t total = static_cast<uint32_t>(ids.size());
  rebuild_pagination_flags(total);
  const uint32_t start = page_index_ * kPageSize;
  for (uint32_t i = 0; i < kPageSize; ++i) {
    const uint32_t idx = start + i;
    if (idx >= total) {
      break;
    }
    slots_[i].kind = MultiChoiceSlotKind::FavoriteBox;
    slots_[i].target_id = ids[idx];
    slots_[i].enabled = 1;
    copy_label(slots_[i].label, sizeof(slots_[i].label),
               std::string("Box ") + std::to_string(ids[idx]));
  }
}

void MultiChoiceEngine::fill_folder_context() {
  for (auto& s : slots_) {
    s = {};
  }
  rebuild_pagination_flags(1);
  slots_[0].kind = MultiChoiceSlotKind::ActionFavorite;
  slots_[0].target_id = context_box_;
  slots_[0].enabled = 1;
  copy_label(slots_[0].label, sizeof(slots_[0].label),
             context_box_favored_ ? "Unfavorite" : "Favorite");
}

void MultiChoiceEngine::rebuild_slots() {
  if (mode_ == MultiChoiceMode::Closed) {
    return;
  }
  if (mode_ == MultiChoiceMode::FolderContext) {
    fill_folder_context();
    return;
  }
  if (section_ == MultiChoiceSection::RootDrive) {
    copy_label(title_, sizeof(title_), "Root Drive:");
    fill_drive_page();
  } else {
    copy_label(title_, sizeof(title_), "Favorites:");
    fill_favorites_page();
  }
}

uint32_t MultiChoiceEngine::octant_from_slot_index(uint32_t slot) {
  // Fill 0..7 = TLF,TRF,BLF,BRF,TLB,TRB,BLB,BRB → geometric bits (R,U,Fwd).
  static constexpr uint32_t kMap[8] = {2u, 3u, 0u, 1u, 6u, 7u, 4u, 5u};
  return kMap[slot & 7u];
}

uint32_t MultiChoiceEngine::slot_index_from_octant(uint32_t octant) {
  // Involution of octant_from_slot_index.
  static constexpr uint32_t kMap[8] = {2u, 3u, 0u, 1u, 6u, 7u, 4u, 5u};
  return kMap[octant & 7u];
}

Vec3 MultiChoiceEngine::octant_local_offset(uint32_t octant, float radius_m) {
  const float sx = (octant & 1u) ? 1.f : -1.f;
  const float sy = (octant & 2u) ? 1.f : -1.f;
  const float sz = (octant & 4u) ? 1.f : -1.f;
  const float inv = 1.f / std::sqrt(3.f);
  return Vec3{sx * radius_m * inv, sy * radius_m * inv, -sz * radius_m * inv};
}

Vec3 MultiChoiceEngine::slot_local_offset(uint32_t slot, float radius_m) {
  return octant_local_offset(octant_from_slot_index(slot), radius_m);
}

MultiChoiceHoverResult MultiChoiceEngine::compute_hover(const Pose& widget, const Vec3& cursor,
                                                        float min_m, float max_m,
                                                        bool pagination_pair) {
  MultiChoiceHoverResult r{};
  Vec3 right{};
  Vec3 up{};
  Vec3 forward{};
  widget_axes(widget, &right, &up, &forward);
  const Vec3 delta{cursor.x - widget.position.x, cursor.y - widget.position.y,
                   cursor.z - widget.position.z};
  r.magnitude = length(delta);
  if (r.magnitude < 1e-6f) {
    r.slot = pagination_pair ? 8 : -1;
    return r;
  }
  const Vec3 dir = normalize_or(delta, Vec3{0.f, 0.f, -1.f});
  r.dot_right = dot3(dir, right);
  r.dot_up = dot3(dir, up);
  r.dot_forward = dot3(dir, forward);

  if (r.magnitude < min_m) {
    if (!pagination_pair) {
      r.slot = -1;
      return r;
    }
    // Center pagination pair: left of center = back (8), right = forward (9).
    r.slot = (r.dot_right < 0.f) ? 8 : 9;
    return r;
  }
  if (r.magnitude > max_m) {
    r.slot = -1;
    return r;
  }
  uint32_t oct = 0;
  if (r.dot_right > 0.f) {
    oct |= 1u;
  }
  if (r.dot_up > 0.f) {
    oct |= 2u;
  }
  if (r.dot_forward > 0.f) {
    oct |= 4u;
  }
  r.slot = static_cast<int32_t>(slot_index_from_octant(oct));
  return r;
}

void MultiChoiceEngine::tick_hover(const Vec3& cursor) {
  hovered_slot_ = -1;
  confirm_hovered_ = -1;
  cursor_inside_any_ = false;
  hover_label_[0] = '\0';
  if (mode_ == MultiChoiceMode::Closed) {
    return;
  }

  if (confirm_open_) {
    const auto ch =
        compute_hover(confirm_center_, cursor, kConfirmMinM, kConfirmMaxM, /*pagination=*/false);
    if (ch.magnitude <= kConfirmMaxM) {
      cursor_inside_any_ = true;
    }
    if (ch.slot == 0 || ch.slot == 1) {
      // Map octants with −X / +X bias to Add (left) / Jump (right) for 2-slot strip.
      confirm_hovered_ = (ch.dot_right < 0.f) ? 0 : 1;
      copy_label(hover_label_, sizeof(hover_label_),
                 confirm_hovered_ == 0 ? "Add" : "Jump to Root");
    } else if (ch.slot >= 0 && ch.slot <= 7) {
      confirm_hovered_ = (ch.dot_right < 0.f) ? 0 : 1;
      copy_label(hover_label_, sizeof(hover_label_),
                 confirm_hovered_ == 0 ? "Add" : "Jump to Root");
    }
  }

  const bool use_pagination = mode_ == MultiChoiceMode::DrivePicker;
  const auto h = compute_hover(center_, cursor, kMainMinM, kMainMaxM, use_pagination);
  if (h.magnitude <= kMainMaxM) {
    cursor_inside_any_ = true;
  }
  if (!confirm_armed_) {
    if (h.magnitude > kMainMaxM) {
      confirm_armed_ = true;
    } else if (h.slot >= 0 && h.slot <= 7 &&
               slots_[static_cast<size_t>(h.slot)].enabled != 0) {
      confirm_armed_ = true;
    }
  }
  if (confirm_open_ && confirm_hovered_ >= 0) {
    // Prefer confirm strip when both hovered.
    return;
  }
  if (h.slot >= 0 && h.slot <= 7) {
    hovered_slot_ = h.slot;
    if (slots_[static_cast<size_t>(h.slot)].enabled) {
      copy_label(hover_label_, sizeof(hover_label_), slots_[static_cast<size_t>(h.slot)].label);
    }
  } else if (h.slot == 8 && page_back_enabled_) {
    hovered_slot_ = 8;
    copy_label(hover_label_, sizeof(hover_label_), "Back");
  } else if (h.slot == 9 && page_forward_enabled_) {
    hovered_slot_ = 9;
    copy_label(hover_label_, sizeof(hover_label_), "Forward");
  } else if (h.slot == 8 || h.slot == 9) {
    hovered_slot_ = h.slot;  // disabled pagination still reports hover for viz
  }
}

void MultiChoiceEngine::open_confirm_strip(EntityId target, MultiChoiceSlotKind kind) {
  pending_target_ = target;
  pending_kind_ = kind;
  confirm_open_ = true;
  confirm_center_ = center_;
  // Place strip toward the user (local +Z) in front of the main widget.
  const Vec3 toward_user =
      quat_rotate(center_.orientation, Vec3{0.f, 0.f, 1.f});
  confirm_center_.position.x += toward_user.x * kConfirmForwardM;
  confirm_center_.position.y += toward_user.y * kConfirmForwardM;
  confirm_center_.position.z += toward_user.z * kConfirmForwardM;
  confirm_hovered_ = -1;
}

bool MultiChoiceEngine::on_primary_confirm() {
  if (mode_ == MultiChoiceMode::Closed) {
    return false;
  }

  // Outside both thresholds → dismiss all.
  if (!cursor_inside_any_) {
    close_all();
    return true;
  }

  // Still in spawn center: ignore primary (blocks accidental Root→Favorites).
  if (!confirm_armed_) {
    return false;
  }

  if (confirm_open_) {
    if (confirm_hovered_ == 0) {
      pending_action_ = MultiChoiceAction::Add;
      action_target_ = pending_target_;
      action_kind_ = pending_kind_;
      close_all();
      return true;
    }
    if (confirm_hovered_ == 1) {
      pending_action_ = MultiChoiceAction::JumpToRoot;
      action_target_ = pending_target_;
      action_kind_ = pending_kind_;
      close_all();
      return true;
    }
    // Inside strip but no option → dismiss.
    close_all();
    return true;
  }

  if (hovered_slot_ == 8 && page_back_enabled_) {
    if (page_index_ > 0) {
      --page_index_;
      rebuild_slots();
      return true;
    }
    if (mode_ == MultiChoiceMode::DrivePicker &&
        section_ == MultiChoiceSection::Favorites) {
      section_ = MultiChoiceSection::RootDrive;
      const uint32_t total = static_cast<uint32_t>(roots_.size());
      page_index_ = total <= kPageSize ? 0u : (total - 1) / kPageSize;
      rebuild_slots();
      return true;
    }
  }
  if (hovered_slot_ == 9 && page_forward_enabled_) {
    const uint32_t total = option_count_for_section();
    const uint32_t pages = total == 0 ? 1u : (total + kPageSize - 1) / kPageSize;
    if (page_index_ + 1 < pages) {
      ++page_index_;
      rebuild_slots();
      return true;
    }
    if (mode_ == MultiChoiceMode::DrivePicker &&
        section_ == MultiChoiceSection::RootDrive) {
      section_ = MultiChoiceSection::Favorites;
      page_index_ = 0;
      rebuild_slots();
      return true;
    }
  }

  if (hovered_slot_ >= 0 && hovered_slot_ <= 7) {
    const auto& slot = slots_[static_cast<size_t>(hovered_slot_)];
    if (!slot.enabled) {
      return false;
    }
    if (slot.kind == MultiChoiceSlotKind::ActionFavorite) {
      const bool now = !is_favorite(slot.target_id);
      set_favorite(slot.target_id, now);
      pending_action_ = MultiChoiceAction::ToggleFavorite;
      action_target_ = slot.target_id;
      action_kind_ = slot.kind;
      return true;
    }
    if (slot.kind == MultiChoiceSlotKind::DriveRoot ||
        slot.kind == MultiChoiceSlotKind::FavoriteBox) {
      open_confirm_strip(slot.target_id, slot.kind);
      return true;
    }
  }
  return false;
}

MultiChoiceAction MultiChoiceEngine::consume_action(EntityId* out_target_id,
                                                    MultiChoiceSlotKind* out_kind) {
  const MultiChoiceAction a = pending_action_;
  if (out_target_id) {
    *out_target_id = action_target_;
  }
  if (out_kind) {
    *out_kind = action_kind_;
  }
  pending_action_ = MultiChoiceAction::None;
  action_target_ = 0;
  action_kind_ = MultiChoiceSlotKind::Empty;
  return a;
}

}  // namespace threeos
