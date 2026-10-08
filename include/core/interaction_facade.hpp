#pragma once

#include "core/types.hpp"

#include <functional>

namespace threeos {

/// Stub orchestrator for Phase 0.1 — state + callback slots only.
class InteractionFacade {
 public:
  using SelectFn = std::function<void(EntityId)>;
  using HoverFn = std::function<void(EntityId)>;
  using MultiSelectFn = std::function<void()>;

  InteractionState state() const { return state_; }
  void set_state(InteractionState s) { state_ = s; }

  void set_on_object_select(SelectFn fn) { on_select_ = std::move(fn); }
  void set_on_object_hover_enter(HoverFn fn) { on_hover_enter_ = std::move(fn); }
  void set_on_multi_select_triggered(MultiSelectFn fn) {
    on_multi_select_ = std::move(fn);
  }

  void emit_select(EntityId id) {
    if (on_select_) {
      on_select_(id);
    }
  }

  void emit_hover_enter(EntityId id) {
    if (on_hover_enter_) {
      on_hover_enter_(id);
    }
  }

  void emit_multi_select() {
    if (on_multi_select_) {
      on_multi_select_();
    }
  }

 private:
  InteractionState state_ = InteractionState::Idle;
  SelectFn on_select_;
  HoverFn on_hover_enter_;
  MultiSelectFn on_multi_select_;
};

}  // namespace threeos
