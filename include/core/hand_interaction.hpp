#pragma once

#include "core/types.hpp"

#include <cstdint>

namespace threeos {

enum class PalmMode : uint32_t {
  Up = 0,
  Sideways = 1,
  Down = 2,
};

enum class PinchChannel : uint32_t {
  None = 0,
  Primary = 1,
  Secondary = 2,
  Ternary = 3,
};

enum class InteractionIntentFlags : uint32_t {
  None = 0,
  SelectPulse = 1u << 0,
  DragActive = 1u << 1,
  VoodooSnap = 1u << 2,  // primary + palm down
  PrimaryHeld = 1u << 3,
  SecondaryHeld = 1u << 4,
  TernaryHeld = 1u << 5,
  VoodooFree = 1u << 6,  // secondary + palm down
  MindPalaceMenu = 1u << 7,     // secondary + palm up (Multi-Choice when no dismiss)
  ModeToggleArmed = 1u << 8,    // open-book pose held (MR↔VR glyph)
  ModeTogglePulse = 1u << 9,    // latched dual-thumb-close edge → confirm toggle
  VoodooDismissPulse = 1u << 10,     // secondary press edge + palm up (one hand)
  VoodooDismissAllPulse = 1u << 11,  // both hands VoodooDismissPulse same frame
};

inline InteractionIntentFlags operator|(InteractionIntentFlags a, InteractionIntentFlags b) {
  return static_cast<InteractionIntentFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline InteractionIntentFlags& operator|=(InteractionIntentFlags& a, InteractionIntentFlags b) {
  a = a | b;
  return a;
}
inline bool has_flag(InteractionIntentFlags set, InteractionIntentFlags bit) {
  return (static_cast<uint32_t>(set) & static_cast<uint32_t>(bit)) != 0;
}

/// Default pinch-approach threshold (2.5 inches).
constexpr float kDefaultPinchThreshM = 0.0635f;
constexpr float kCursorForwardOffsetM = 0.1778f;  // 7"
constexpr float kCursorUpOffsetM = 0.0762f;       // 3"
constexpr float kCursorLateralOffsetM = 0.1016f;  // 4" (OpenXR: left→−right, right→+right; Unity X-flip)
constexpr float kCursorScaleMin = 0.1f;  // shrink 90% at full pinch before pop
constexpr float kCursorScaleRecoverPerSec = 12.f;  // fast-lerp toward 1.0

struct HandSample {
  bool tracked = false;
  uint32_t tip_flags = 0;  // bit0 thumb,1 index,2 middle,3 ring,4 index proximal
  Vec3 thumb_tip{};
  Vec3 index_tip{};
  Vec3 middle_tip{};
  Vec3 ring_tip{};
  Vec3 palm_normal{};  // OpenXR world; used if palm_mode_valid == false
  bool palm_mode_valid = false;
  /// Neutral default — Down as default + hysteresis caused false VoodooSnap.
  PalmMode palm_mode = PalmMode::Sideways;
  float primary_value = 0.f;    // 0..1 approach / trigger
  float secondary_value = 0.f;
  float ternary_value = 0.f;
  bool primary_pressed = false;
  bool secondary_pressed = false;
  bool ternary_pressed = false;
  Vec3 index_proximal{};  // cursor probe anchor when tip_flags bit4 set

  bool has_tips(uint32_t mask) const { return (tip_flags & mask) == mask; }
};

struct PointCursor {
  bool active = false;
  Vec3 position{};
  float scale = 1.f;
  PinchChannel driving_channel = PinchChannel::None;
  float driving_distance_m = 0.f;
  PalmMode palm_mode = PalmMode::Sideways;
};

struct InteractionIntents {
  InteractionIntentFlags flags = InteractionIntentFlags::None;
  PalmMode active_palm = PalmMode::Sideways;
  PinchChannel active_channel = PinchChannel::None;
  uint32_t handedness = 0;  // 0 left, 1 right, 2 none / mixed
};

class HandInteractionEngine {
 public:
  void reset();

  void set_hands(const HandSample& left, const HandSample& right);
  void tick(const Pose& head, bool head_valid, double time_seconds);

  const PointCursor& cursor(uint32_t hand /*0 left, 1 right*/) const {
    return hand < 2 ? cursors_[hand] : cursors_[0];
  }
  const InteractionIntents& intents() const { return intents_; }
  /// Per-hand intents before preferred-hand aggregation (for dual voodoo sessions).
  const InteractionIntents& pending_intents(uint32_t hand) const {
    return hand < 2 ? hands_[hand].pending_intents : hands_[0].pending_intents;
  }

  float pinch_thresh_m() const { return pinch_thresh_m_; }
  void set_pinch_thresh_m(float m) { pinch_thresh_m_ = m > 1e-4f ? m : kDefaultPinchThreshM; }

  /// Pure helpers (also used by harness).
  static PalmMode quantize_palm(const Vec3& palm_normal_world, PalmMode previous, bool has_previous);
  static Vec3 floor_forward(const Pose& head);
  static Vec3 cursor_probe(const Vec3& midpoint, const Pose& head, bool head_valid);
  /// Cursor probe plus 1" lateral offset (hand 0 left → +right, hand 1 right → −right).
  static Vec3 cursor_probe_hand(uint32_t hand, const Vec3& midpoint, const Pose& head,
                                bool head_valid);
  static float approach_scale(float distance_m, float thresh_m);
  /// Open-book base (palms/fingers/sep) vs enter (base + thumbs out). Used by 0.7 Task 7.7.
  static bool open_book_base(const HandSample& left, const HandSample& right, const Pose& head,
                             bool head_valid);
  static bool open_book_enter(const HandSample& left, const HandSample& right, const Pose& head,
                              bool head_valid);
  static bool thumb_closed(const HandSample& hand, bool was_closed);

 private:
  struct HandRuntime {
    HandSample sample{};
    bool prev_primary = false;
    bool prev_secondary = false;
    bool prev_ternary = false;
    bool recovering_scale = false;
    float scale = 1.f;
    double last_time = 0.0;
    InteractionIntents pending_intents{};
    PalmMode last_palm = PalmMode::Sideways;
    bool have_palm_history = false;
    /// Palm mode frozen at primary/secondary press edge until release (no mid-hold flips).
    bool have_intent_palm_latch = false;
    PalmMode intent_palm_latched = PalmMode::Sideways;
  };

  void tick_hand(uint32_t hand, const Pose& head, bool head_valid, double time_seconds);
  static PinchChannel pick_driving_channel(const HandSample& s, float thresh_m, float* out_d);
  static Vec3 pair_midpoint(const HandSample& s, PinchChannel ch);

  void tick_mode_toggle(const Pose& head, bool head_valid);

  HandRuntime hands_[2]{};
  PointCursor cursors_[2]{};
  InteractionIntents intents_{};
  float pinch_thresh_m_ = kDefaultPinchThreshM;
  bool mode_toggle_armed_sticky_ = false;   // entered with thumbs out; holds through thumb close
  bool mode_toggle_thumb_latched_ = false;  // held closed after pulse until dual open
  bool mode_toggle_left_thumb_closed_ = false;
  bool mode_toggle_right_thumb_closed_ = false;
};

}  // namespace threeos
