#pragma once

#include "core/types.hpp"
#include "storage/workspace.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace threeos {

struct PoseLerpState {
  bool active = false;
  EntityId entity_id = 0;
  Pose from{};
  Pose to{};
  float t = 0.f;
};

/// Fast visual travel for summon / insert / cross-portal.
class PoseLerpQueue {
 public:
  static constexpr float kSummonLerpS = 0.20f;

  void clear() { lerps_.clear(); }

  bool active(EntityId entity_id) const;

  void begin(EntityId entity_id, const Pose& to, Workspace& workspace);

  /// `on_pose` syncs portal ghosts (far pose + near refresh) each frame.
  void tick(float dt_seconds, Workspace& workspace,
            const std::function<void(EntityId, const Pose&)>& on_pose);

  std::vector<PoseLerpState>& states() { return lerps_; }
  const std::vector<PoseLerpState>& states() const { return lerps_; }

 private:
  std::vector<PoseLerpState> lerps_{};
};

}  // namespace threeos
