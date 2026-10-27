#include "core/hand_interaction.hpp"

#include <algorithm>
#include <cmath>

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

float dist(const Vec3& a, const Vec3& b) {
  return length(Vec3{a.x - b.x, a.y - b.y, a.z - b.z});
}

float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

}  // namespace

void HandInteractionEngine::reset() {
  hands_[0] = {};
  hands_[1] = {};
  cursors_[0] = {};
  cursors_[1] = {};
  intents_ = {};
  pinch_thresh_m_ = kDefaultPinchThreshM;
  mode_toggle_armed_sticky_ = false;
  mode_toggle_thumb_latched_ = false;
  mode_toggle_left_thumb_closed_ = false;
  mode_toggle_right_thumb_closed_ = false;
}

void HandInteractionEngine::clear_voodoo_intents(uint32_t hand) {
  if (hand >= 2u) {
    return;
  }
  using F = InteractionIntentFlags;
  const uint32_t mask = static_cast<uint32_t>(F::VoodooSnap) | static_cast<uint32_t>(F::VoodooFree);
  auto& p = hands_[hand].pending_intents;
  p.flags = static_cast<F>(static_cast<uint32_t>(p.flags) & ~mask);
}

void HandInteractionEngine::set_hands(const HandSample& left, const HandSample& right) {
  hands_[0].sample = left;
  hands_[1].sample = right;
}

PalmMode HandInteractionEngine::quantize_palm(const Vec3& palm_normal_world, PalmMode previous,
                                              bool has_previous) {
  const Vec3 n = normalize_or(palm_normal_world, Vec3{0.f, 1.f, 0.f});
  const float y = n.y;
  // Palm-down while pinching is rarely a flat -Y; -0.40 was a false-positive magnet
  // for ordinary sideways reaches. Enter Down only when clearly floor-facing; keep
  // mild hysteresis so intentional Down does not flicker off mid-hold.
  constexpr float kEnterUp = 0.55f;
  constexpr float kLeaveUp = 0.30f;
  constexpr float kEnterDown = -0.55f;
  constexpr float kLeaveDown = -0.35f;

  if (y >= kEnterUp) {
    return PalmMode::Up;
  }
  if (y <= kEnterDown) {
    return PalmMode::Down;
  }
  if (has_previous) {
    if (previous == PalmMode::Up && y >= kLeaveUp) {
      return PalmMode::Up;
    }
    if (previous == PalmMode::Down && y <= kLeaveDown) {
      return PalmMode::Down;
    }
  }
  return PalmMode::Sideways;
}

Vec3 HandInteractionEngine::floor_forward(const Pose& head) {
  const Quat& q = head.orientation;
  const float x = 2.f * (q.x * q.z + q.w * q.y);
  const float z = 1.f - 2.f * (q.x * q.x + q.y * q.y);
  return normalize_or(Vec3{x, 0.f, z}, Vec3{0.f, 0.f, 1.f});
}

Vec3 HandInteractionEngine::cursor_probe(const Vec3& midpoint, const Pose& head, bool head_valid) {
  const Vec3 fwd = head_valid ? floor_forward(head) : Vec3{0.f, 0.f, 1.f};
  return Vec3{midpoint.x + fwd.x * kCursorForwardOffsetM, midpoint.y + kCursorUpOffsetM,
              midpoint.z + fwd.z * kCursorForwardOffsetM};
}

Vec3 HandInteractionEngine::cursor_probe_hand(uint32_t hand, const Vec3& midpoint, const Pose& head,
                                              bool head_valid) {
  Vec3 p = cursor_probe(midpoint, head, head_valid);
  const Vec3 fwd = head_valid ? floor_forward(head) : Vec3{0.f, 0.f, 1.f};
  // OpenXR Y-up: right = cross(up, floor_forward) = (fwd.z, 0, -fwd.x)
  const Vec3 right = normalize_or(Vec3{fwd.z, 0.f, -fwd.x}, Vec3{1.f, 0.f, 0.f});
  // OpenXR +X is opposite Unity after host X-flip: bias +OX_right on right hand so
  // Unity shows medial of the knuckle (−OX_right on left → Unity lateral/right-of-hand).
  const float sign = hand == 0 ? -1.f : 1.f;
  p.x += right.x * kCursorLateralOffsetM * sign;
  p.z += right.z * kCursorLateralOffsetM * sign;
  return p;
}

float HandInteractionEngine::approach_scale(float distance_m, float thresh_m) {
  if (distance_m >= thresh_m) {
    return 1.f;
  }
  if (distance_m <= 0.f) {
    return kCursorScaleMin;
  }
  const float t = distance_m / thresh_m;
  return kCursorScaleMin + (1.f - kCursorScaleMin) * t;
}

namespace {

constexpr float kModeTogglePalmTowardHead = 0.50f;
constexpr float kModeToggleFingerClusterM = 0.060f;
constexpr float kModeToggleThumbOutM = 0.015f;
constexpr float kModeToggleThumbCloseEnterM = 0.038f;
constexpr float kModeToggleThumbCloseExitM = 0.058f;
constexpr float kModeToggleBookSepMinM = 0.06f;
constexpr float kModeToggleBookSepMaxM = 0.50f;
constexpr float kModeToggleHeightDeltaMaxM = 0.20f;

Vec3 palm_proxy(const HandSample& s) {
  if (s.has_tips(0x10u)) {
    return s.index_proximal;
  }
  // Average available tips.
  Vec3 acc{};
  int n = 0;
  auto add = [&](uint32_t bit, const Vec3& p) {
    if (s.has_tips(bit)) {
      acc.x += p.x;
      acc.y += p.y;
      acc.z += p.z;
      ++n;
    }
  };
  add(0x1u, s.thumb_tip);
  add(0x2u, s.index_tip);
  add(0x4u, s.middle_tip);
  add(0x8u, s.ring_tip);
  if (n == 0) {
    return {};
  }
  const float inv = 1.f / static_cast<float>(n);
  return Vec3{acc.x * inv, acc.y * inv, acc.z * inv};
}

bool fingers_unsplayed(const HandSample& s) {
  // Need at least two finger tips (index/middle/ring) to judge cluster.
  Vec3 tips[3];
  int n = 0;
  if (s.has_tips(0x2u)) {
    tips[n++] = s.index_tip;
  }
  if (s.has_tips(0x4u)) {
    tips[n++] = s.middle_tip;
  }
  if (s.has_tips(0x8u)) {
    tips[n++] = s.ring_tip;
  }
  if (n < 2) {
    return false;
  }
  float max_d = 0.f;
  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      max_d = std::max(max_d, dist(tips[i], tips[j]));
    }
  }
  return max_d <= kModeToggleFingerClusterM;
}

bool palm_toward_head(const HandSample& s, const Pose& head) {
  if (!s.has_tips(0x1u) && !s.has_tips(0x10u)) {
    return false;
  }
  const Vec3 palm = palm_proxy(s);
  const Vec3 to_head = normalize_or(
      Vec3{head.position.x - palm.x, head.position.y - palm.y, head.position.z - palm.z},
      Vec3{0.f, 0.f, -1.f});
  const Vec3 n = normalize_or(s.palm_normal, to_head);
  return n.x * to_head.x + n.y * to_head.y + n.z * to_head.z >= kModeTogglePalmTowardHead;
}

bool thumb_outward(const HandSample& s, uint32_t hand /*0 L, 1 R*/, const Pose& head,
                   bool head_valid) {
  if (!s.has_tips(0x1u)) {
    return false;
  }
  const Vec3 palm = palm_proxy(s);
  const Vec3 fwd = head_valid ? HandInteractionEngine::floor_forward(head) : Vec3{0.f, 0.f, 1.f};
  const Vec3 right = normalize_or(Vec3{fwd.z, 0.f, -fwd.x}, Vec3{1.f, 0.f, 0.f});
  const Vec3 thumb_off{s.thumb_tip.x - palm.x, s.thumb_tip.y - palm.y, s.thumb_tip.z - palm.z};
  // Left thumb out → −right; right thumb out → +right.
  const float lateral = thumb_off.x * right.x + thumb_off.z * right.z;
  return hand == 0 ? (lateral <= -kModeToggleThumbOutM) : (lateral >= kModeToggleThumbOutM);
}

}  // namespace

bool HandInteractionEngine::thumb_closed(const HandSample& hand, bool was_closed) {
  if (!hand.tracked || !hand.has_tips(0x1u) || !hand.has_tips(0x10u)) {
    return false;
  }
  const float d = dist(hand.thumb_tip, hand.index_proximal);
  if (was_closed) {
    return d <= kModeToggleThumbCloseExitM;
  }
  return d <= kModeToggleThumbCloseEnterM;
}

bool HandInteractionEngine::open_book_base(const HandSample& left, const HandSample& right,
                                           const Pose& head, bool head_valid) {
  if (!head_valid || !left.tracked || !right.tracked) {
    return false;
  }
  if (!palm_toward_head(left, head) || !palm_toward_head(right, head)) {
    return false;
  }
  if (!fingers_unsplayed(left) || !fingers_unsplayed(right)) {
    return false;
  }
  const Vec3 lp = palm_proxy(left);
  const Vec3 rp = palm_proxy(right);
  const float sep = dist(lp, rp);
  if (sep < kModeToggleBookSepMinM || sep > kModeToggleBookSepMaxM) {
    return false;
  }
  if (std::fabs(lp.y - rp.y) > kModeToggleHeightDeltaMaxM) {
    return false;
  }
  return true;
}

bool HandInteractionEngine::open_book_enter(const HandSample& left, const HandSample& right,
                                            const Pose& head, bool head_valid) {
  if (!open_book_base(left, right, head, head_valid)) {
    return false;
  }
  return thumb_outward(left, 0, head, head_valid) && thumb_outward(right, 1, head, head_valid);
}

void HandInteractionEngine::tick_mode_toggle(const Pose& head, bool head_valid) {
  const HandSample& L = hands_[0].sample;
  const HandSample& R = hands_[1].sample;

  // Enter requires thumbs out; stay armed through thumb-close while base pose holds.
  if (open_book_enter(L, R, head, head_valid)) {
    mode_toggle_armed_sticky_ = true;
  } else if (!open_book_base(L, R, head, head_valid)) {
    mode_toggle_armed_sticky_ = false;
  }
  const bool armed = mode_toggle_armed_sticky_;

  mode_toggle_left_thumb_closed_ = thumb_closed(L, mode_toggle_left_thumb_closed_);
  mode_toggle_right_thumb_closed_ = thumb_closed(R, mode_toggle_right_thumb_closed_);
  const bool both_closed = mode_toggle_left_thumb_closed_ && mode_toggle_right_thumb_closed_;

  bool pulse = false;
  if (!armed) {
    // Cancel arming: drop latch so a later arm needs a fresh close edge.
    mode_toggle_thumb_latched_ = false;
  } else if (mode_toggle_thumb_latched_) {
    if (!both_closed) {
      mode_toggle_thumb_latched_ = false;
    }
  } else if (both_closed) {
    mode_toggle_thumb_latched_ = true;
    pulse = true;
  }

  if (armed) {
    intents_.flags |= InteractionIntentFlags::ModeToggleArmed;
  }
  if (pulse) {
    intents_.flags |= InteractionIntentFlags::ModeTogglePulse;
  }
}

PinchChannel HandInteractionEngine::pick_driving_channel(const HandSample& s, float thresh_m,
                                                         float* out_d) {
  struct Cand {
    PinchChannel ch;
    float d;
  };
  Cand cands[3];
  int n = 0;

  const bool tip_mode = s.has_tips(0x3u);  // thumb + index
  if (tip_mode) {
    const float d0 = dist(s.index_tip, s.thumb_tip);
    if (d0 <= thresh_m) {
      cands[n++] = {PinchChannel::Primary, d0};
    }
    if (s.has_tips(0x5u)) {  // thumb + middle
      const float d1 = dist(s.middle_tip, s.thumb_tip);
      if (d1 <= thresh_m) {
        cands[n++] = {PinchChannel::Secondary, d1};
      }
    }
    if (s.has_tips(0x9u)) {  // thumb + ring
      const float d2 = dist(s.ring_tip, s.thumb_tip);
      if (d2 <= thresh_m) {
        cands[n++] = {PinchChannel::Ternary, d2};
      }
    }
  } else {
    auto virt = [thresh_m](float value) {
      return (1.f - clampf(value, 0.f, 1.f)) * thresh_m;
    };
    if (s.primary_value > 0.02f) {
      cands[n++] = {PinchChannel::Primary, virt(s.primary_value)};
    }
    if (s.secondary_value > 0.02f) {
      cands[n++] = {PinchChannel::Secondary, virt(s.secondary_value)};
    }
    if (s.ternary_value > 0.02f || s.ternary_pressed) {
      cands[n++] = {PinchChannel::Ternary, s.ternary_pressed ? 0.f : virt(s.ternary_value)};
    }
  }

  if (n == 0) {
    if (out_d) {
      *out_d = tip_mode ? dist(s.index_tip, s.thumb_tip) : thresh_m;
    }
    return PinchChannel::None;
  }

  int best = 0;
  for (int i = 1; i < n; ++i) {
    if (cands[i].d < cands[best].d) {
      best = i;
    }
  }
  if (out_d) {
    *out_d = cands[best].d;
  }
  return cands[best].ch;
}

Vec3 HandInteractionEngine::pair_midpoint(const HandSample& s, PinchChannel ch) {
  Vec3 a = s.index_tip;
  if (ch == PinchChannel::Secondary) {
    a = s.middle_tip;
  } else if (ch == PinchChannel::Ternary) {
    a = s.ring_tip;
  }
  return Vec3{(a.x + s.thumb_tip.x) * 0.5f, (a.y + s.thumb_tip.y) * 0.5f,
              (a.z + s.thumb_tip.z) * 0.5f};
}

void HandInteractionEngine::tick_hand(uint32_t hand, const Pose& head, bool head_valid,
                                      double time_seconds) {
  HandRuntime& rt = hands_[hand];
  PointCursor& cur = cursors_[hand];
  const HandSample& s = rt.sample;

  if (!s.tracked) {
    cur = {};
    rt.prev_primary = false;
    rt.prev_secondary = false;
    rt.prev_ternary = false;
    rt.recovering_scale = false;
    rt.scale = 1.f;
    rt.last_time = time_seconds;
    rt.have_intent_palm_latch = false;
    rt.pending_intents = {};
    return;
  }

  PalmMode palm = s.palm_mode_valid
                      ? s.palm_mode
                      : quantize_palm(s.palm_normal, rt.last_palm, rt.have_palm_history);
  rt.last_palm = palm;
  rt.have_palm_history = true;

  float d = pinch_thresh_m_;
  const PinchChannel drive = pick_driving_channel(s, pinch_thresh_m_, &d);
  // Index proximal (MCP) splits tip vs wrist; fall back thumb → index tip.
  Vec3 tip = s.index_proximal;
  if (!s.has_tips(0x10u)) {
    tip = s.has_tips(0x1u) ? s.thumb_tip : (s.has_tips(0x2u) ? s.index_tip : s.thumb_tip);
  }

  cur.active = true;
  cur.position = cursor_probe_hand(hand, tip, head, head_valid);
  cur.driving_channel = drive;
  cur.driving_distance_m = d;
  cur.palm_mode = palm;

  const bool primary_edge = s.primary_pressed && !rt.prev_primary;
  const bool secondary_edge = s.secondary_pressed && !rt.prev_secondary;
  const bool ternary_edge = s.ternary_pressed && !rt.prev_ternary;
  const bool any_pressed = s.primary_pressed || s.secondary_pressed || s.ternary_pressed;
  const bool press_edge = primary_edge || secondary_edge || ternary_edge;

  // Freeze palm for intent routing at first press edge so mid-hold tilt cannot
  // flip a telekinesis drag into VoodooSnap (or the reverse).
  if (!any_pressed) {
    rt.have_intent_palm_latch = false;
  } else if (!rt.have_intent_palm_latch) {
    rt.intent_palm_latched = palm;
    rt.have_intent_palm_latch = true;
  }
  const PalmMode intent_palm =
      rt.have_intent_palm_latch ? rt.intent_palm_latched : palm;

  double dt = 0.0;
  if (rt.last_time > 0.0) {
    dt = time_seconds - rt.last_time;
  }
  rt.last_time = time_seconds;
  dt = dt < 0.0 ? 0.0 : (dt > 0.1 ? 0.1 : dt);

  if (press_edge) {
    rt.recovering_scale = true;
  }

  if (rt.recovering_scale) {
    rt.scale = std::min(1.f, rt.scale + static_cast<float>(dt) * kCursorScaleRecoverPerSec);
    if (rt.scale >= 0.999f) {
      rt.scale = 1.f;
      rt.recovering_scale = false;
    }
  } else if (drive != PinchChannel::None && !any_pressed) {
    rt.scale = approach_scale(d, pinch_thresh_m_);
  } else if (!any_pressed) {
    rt.scale = 1.f;
  }
  cur.scale = rt.scale;

  // Intents: accumulate for this hand; tick() picks the preferred hand afterward.
  InteractionIntents local{};
  local.handedness = hand;
  local.active_palm = intent_palm;
  if (s.primary_pressed) {
    local.flags |= InteractionIntentFlags::PrimaryHeld;
    local.active_channel = PinchChannel::Primary;
    if (primary_edge) {
      local.flags |= InteractionIntentFlags::SelectPulse;
    }
    // Quest OS steals palm-up index pinch — leave Up unmapped for app intents.
    // Sideways hold → telekinesis drag; palm-down + primary → snap voodoo.
    if (intent_palm == PalmMode::Sideways) {
      local.flags |= InteractionIntentFlags::DragActive;
    } else if (intent_palm == PalmMode::Down) {
      local.flags |= InteractionIntentFlags::VoodooSnap;
    }
  }
  if (s.secondary_pressed) {
    local.flags |= InteractionIntentFlags::SecondaryHeld;
    if (local.active_channel == PinchChannel::None) {
      local.active_channel = PinchChannel::Secondary;
    }
    if (intent_palm == PalmMode::Down) {
      local.flags |= InteractionIntentFlags::VoodooFree;
    } else if (intent_palm == PalmMode::Up) {
      local.flags |= InteractionIntentFlags::MindPalaceMenu;
      // README: palm-up + secondary pinch dismisses that hand's voodoo volume.
      if (secondary_edge) {
        local.flags |= InteractionIntentFlags::VoodooDismissPulse;
      }
    }
  }
  if (s.ternary_pressed) {
    local.flags |= InteractionIntentFlags::TernaryHeld;
    if (local.active_channel == PinchChannel::None) {
      local.active_channel = PinchChannel::Ternary;
    }
  }

  // Stash per-hand intents in cursor scale unused — use a side array instead.
  // Store on HandRuntime via extending struct.
  rt.pending_intents = local;

  rt.prev_primary = s.primary_pressed;
  rt.prev_secondary = s.secondary_pressed;
  rt.prev_ternary = s.ternary_pressed;
}

void HandInteractionEngine::tick(const Pose& head, bool head_valid, double time_seconds) {
  tick_hand(0, head, head_valid, time_seconds);
  tick_hand(1, head, head_valid, time_seconds);

  const uint32_t prev_hand = intents_.handedness;
  const bool prev_primary =
      has_flag(intents_.flags, InteractionIntentFlags::PrimaryHeld);
  intents_ = {};

  const bool left_primary =
      hands_[0].sample.tracked &&
      has_flag(hands_[0].pending_intents.flags, InteractionIntentFlags::PrimaryHeld);
  const bool right_primary =
      hands_[1].sample.tracked &&
      has_flag(hands_[1].pending_intents.flags, InteractionIntentFlags::PrimaryHeld);
  const bool right_busy = hands_[1].sample.tracked &&
                          (has_flag(hands_[1].pending_intents.flags, InteractionIntentFlags::PrimaryHeld) ||
                           has_flag(hands_[1].pending_intents.flags, InteractionIntentFlags::SecondaryHeld) ||
                           has_flag(hands_[1].pending_intents.flags, InteractionIntentFlags::TernaryHeld));
  const bool left_busy = hands_[0].sample.tracked &&
                         (has_flag(hands_[0].pending_intents.flags, InteractionIntentFlags::PrimaryHeld) ||
                          has_flag(hands_[0].pending_intents.flags, InteractionIntentFlags::SecondaryHeld) ||
                          has_flag(hands_[0].pending_intents.flags, InteractionIntentFlags::TernaryHeld));

  int pick = -1;
  // Prefer the hand actually holding primary pinch; sticky when both are held.
  if (left_primary != right_primary) {
    pick = left_primary ? 0 : 1;
  } else if (left_primary && right_primary) {
    if (prev_primary && prev_hand <= 1 &&
        has_flag(hands_[prev_hand].pending_intents.flags, InteractionIntentFlags::PrimaryHeld)) {
      pick = static_cast<int>(prev_hand);
    } else {
      pick = 1;
    }
  } else if (right_busy) {
    pick = 1;
  } else if (left_busy) {
    pick = 0;
  } else if (hands_[1].sample.tracked) {
    pick = 1;
  } else if (hands_[0].sample.tracked) {
    pick = 0;
  }
  if (pick >= 0) {
    intents_ = hands_[static_cast<size_t>(pick)].pending_intents;
  }

  // Dual-hand dismiss-all: both palms-up secondary edges in the same frame.
  // OR single-hand dismiss pulse onto the aggregated intents so hosts can see it
  // even when the preferred hand is the other side.
  const bool left_dismiss = hands_[0].sample.tracked && has_flag(hands_[0].pending_intents.flags,
                                                                 InteractionIntentFlags::VoodooDismissPulse);
  const bool right_dismiss = hands_[1].sample.tracked && has_flag(hands_[1].pending_intents.flags,
                                                                  InteractionIntentFlags::VoodooDismissPulse);
  if (left_dismiss && right_dismiss) {
    intents_.flags |= InteractionIntentFlags::VoodooDismissAllPulse;
    intents_.flags |= InteractionIntentFlags::VoodooDismissPulse;
  } else if (left_dismiss || right_dismiss) {
    intents_.flags |= InteractionIntentFlags::VoodooDismissPulse;
    if (pick < 0) {
      intents_.handedness = left_dismiss ? 0u : 1u;
      intents_.active_palm = PalmMode::Up;
      intents_.active_channel = PinchChannel::Secondary;
      intents_.flags |= InteractionIntentFlags::MindPalaceMenu;
      intents_.flags |= InteractionIntentFlags::SecondaryHeld;
    }
  }

  // Dual-hand MR↔VR open-book is OR'd after preferred-hand aggregation.
  tick_mode_toggle(head, head_valid);
}

}  // namespace threeos
