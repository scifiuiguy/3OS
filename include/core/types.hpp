#pragma once

#include <cstdint>

namespace threeos {

/// Discrete ~1-inch voxel index in the allocentric field grid.
struct VoxelCoord {
  int32_t x = 0;
  int32_t y = 0;
  int32_t z = 0;
};

/// Position in meters (OpenXR: right-handed, Y-up).
struct Vec3 {
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
};

/// Unit quaternion (x, y, z, w), OpenXR right-handed Y-up.
struct Quat {
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
  float w = 1.f;
};

struct Pose {
  Vec3 position{};
  Quat orientation{};
};

enum class InteractionState : uint32_t {
  Idle = 0,
  Hovering = 1,
  Possessed = 2,
  Transforming = 3,
};

enum class AnchorMode : uint32_t {
  Egocentric = 0,
  Allocentric = 1,
  ObjectLocked = 2,
};

enum class LayoutMode : uint32_t {
  RealAnchor = 0,
  Matrix = 1,
  Cluster = 2,
};

using EntityId = uint64_t;

/// Folder analogue — volumetric box container.
struct Box {
  EntityId id = 0;
  Pose pose{};
  VoxelCoord extent{1, 1, 1};
  uint32_t flags = 0;
  uint32_t _pad0 = 0;
};

/// File analogue — application-defined object glyph.
struct Object {
  EntityId id = 0;
  Pose pose{};
  EntityId parent_box = 0;
  uint32_t flags = 0;
  uint32_t _pad0 = 0;
};

static_assert(sizeof(VoxelCoord) == 12, "VoxelCoord layout");
static_assert(sizeof(Vec3) == 12, "Vec3 layout");
static_assert(sizeof(Quat) == 16, "Quat layout");
static_assert(sizeof(Pose) == 28, "Pose layout");
static_assert(sizeof(Box) == 56, "Box layout");
static_assert(sizeof(Object) == 56, "Object layout");

}  // namespace threeos
