#pragma once

/**
 * 3OS host ABI (Phase 0.2).
 *
 * Units: meters.
 * Coordinates: OpenXR convention — right-handed, Y-up.
 * Unity hosts MUST convert from Unity left-handed Y-up before filling frames
 * (negate X on position; quaternion (x,y,z,w) -> (-x, y, z, -w)).
 *
 * Ownership: hosts own input frame memory for the duration of threeos_tick.
 * The kernel does not retain pointers after tick returns.
 * Strings from threeos_last_error() are valid until the next API call.
 *
 * Layout freeze (abi_version layout family):
 *   InteropInputFrame = 192 bytes, InteropTransformDelta = 48 bytes (Phase 0.1).
 * Topology structs are additive (Phase 0.2); abi_version bumped to 2.
 */

#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
#  if defined(THREEOS_BUILDING_DLL)
#    define THREEOS_API __declspec(dllexport)
#  else
#    define THREEOS_API __declspec(dllimport)
#  endif
#else
#  define THREEOS_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ThreeOS_Vec3 {
  float x;
  float y;
  float z;
} ThreeOS_Vec3;

typedef struct ThreeOS_Quat {
  float x;
  float y;
  float z;
  float w;
} ThreeOS_Quat;

typedef struct ThreeOS_Pose {
  ThreeOS_Vec3 position;
  ThreeOS_Quat orientation;
} ThreeOS_Pose;

typedef struct ThreeOS_VoxelCoord {
  int32_t x;
  int32_t y;
  int32_t z;
} ThreeOS_VoxelCoord;

typedef struct ThreeOS_Aabb {
  ThreeOS_Vec3 min;
  ThreeOS_Vec3 max;
} ThreeOS_Aabb;

enum ThreeOS_TrackingFlags : uint32_t {
  THREEOS_TRACK_NONE = 0,
  THREEOS_TRACK_HEAD = 1u << 0,
  THREEOS_TRACK_LEFT_AIM = 1u << 1,
  THREEOS_TRACK_RIGHT_AIM = 1u << 2,
  THREEOS_TRACK_LEFT_GRIP = 1u << 3,
  THREEOS_TRACK_RIGHT_GRIP = 1u << 4,
};

enum ThreeOS_ButtonFlags : uint32_t {
  THREEOS_BTN_NONE = 0,
  THREEOS_BTN_LEFT_SELECT = 1u << 0,
  THREEOS_BTN_RIGHT_SELECT = 1u << 1,
  THREEOS_BTN_LEFT_GRAB = 1u << 2,
  THREEOS_BTN_RIGHT_GRAB = 1u << 3,
  THREEOS_BTN_MENU = 1u << 4,
};

/**
 * One host frame of telemetry. Size frozen at 192 bytes for Phase 0.1+.
 */
typedef struct ThreeOS_InteropInputFrame {
  uint64_t frame_index;
  double time_seconds;
  uint32_t tracking_flags;
  uint32_t button_flags;
  ThreeOS_Pose head;
  ThreeOS_Pose left_aim;
  ThreeOS_Pose right_aim;
  ThreeOS_Pose left_grip;
  ThreeOS_Pose right_grip;
  float left_trigger;
  float right_trigger;
  float left_grip_value;
  float right_grip_value;
  uint32_t reserved0;
  uint32_t reserved1;
  uint32_t reserved2; /* pad to 192 bytes */
} ThreeOS_InteropInputFrame;

/**
 * Host-visible transform update. Size frozen at 48 bytes.
 * Phase 0.2: entity_id != 0 means a portal/teleport result for that entity.
 */
typedef struct ThreeOS_InteropTransformDelta {
  uint64_t entity_id;
  ThreeOS_Pose pose;
  uint32_t flags;
  uint32_t reserved0;
  uint32_t reserved1; /* pad to 48 bytes */
} ThreeOS_InteropTransformDelta;

/** World Proxy Half-Dome snapshot (48 bytes). */
typedef struct ThreeOS_DomeState {
  uint32_t active;
  float scale_ratio;
  ThreeOS_Pose pose;
  float radius_m;
  uint32_t reserved0;
  uint32_t reserved1; /* pad to 48 bytes */
} ThreeOS_DomeState;

/** Double-Proxy lattice snapshot (64 bytes). */
typedef struct ThreeOS_DoubleProxyState {
  uint64_t id;
  uint32_t active;
  float scale_ratio;
  ThreeOS_Aabb far_aabb;
  ThreeOS_Aabb near_aabb;
} ThreeOS_DoubleProxyState;

/** Packed semver: (major << 16) | (minor << 8) | patch. Phase 0.2 => 0.2.0 */
THREEOS_API uint32_t threeos_version(void);

/** ABI layout/feature version; 2 = topology APIs present. */
THREEOS_API uint32_t threeos_abi_version(void);

THREEOS_API int32_t threeos_init(void);
THREEOS_API void threeos_shutdown(void);

/**
 * Push one input frame. Returns 0 on success, negative on error.
 * out_delta may be null; when non-null and a portal moved an entity this frame,
 * entity_id is set and pose is the new far/near world pose.
 */
THREEOS_API int32_t threeos_tick(const ThreeOS_InteropInputFrame* frame,
                                 ThreeOS_InteropTransformDelta* out_delta);

THREEOS_API const char* threeos_last_error(void);

THREEOS_API uint32_t threeos_sizeof_input_frame(void);
THREEOS_API uint32_t threeos_sizeof_transform_delta(void);
THREEOS_API uint32_t threeos_sizeof_dome_state(void);
THREEOS_API uint32_t threeos_sizeof_double_proxy_state(void);

/* --- Topology (Phase 0.2) --- */

/** Far-world meters per proxy meter (default 100). */
THREEOS_API int32_t threeos_topology_set_scale(float world_to_proxy_ratio);

THREEOS_API int32_t threeos_topology_open_dome(const ThreeOS_Pose* head_pose);
THREEOS_API void threeos_topology_close_dome(void);
THREEOS_API int32_t threeos_topology_get_dome(ThreeOS_DomeState* out_dome);

THREEOS_API int32_t threeos_topology_open_double_proxy(uint64_t id, const ThreeOS_Aabb* far_aabb,
                                                       const ThreeOS_Pose* head_pose);
THREEOS_API int32_t threeos_topology_close_double_proxy(uint64_t id);
THREEOS_API int32_t threeos_topology_get_double_proxy(uint64_t id,
                                                      ThreeOS_DoubleProxyState* out_proxy);

THREEOS_API int32_t threeos_topology_select_dome_region(uint64_t proxy_id,
                                                        ThreeOS_VoxelCoord far_min,
                                                        ThreeOS_VoxelCoord far_max,
                                                        const ThreeOS_Pose* head_pose);

THREEOS_API int32_t threeos_topology_portal_near_to_far(uint64_t proxy_id,
                                                        const ThreeOS_Pose* near_pose,
                                                        ThreeOS_Pose* out_far);
THREEOS_API int32_t threeos_topology_portal_far_to_near(uint64_t proxy_id,
                                                        const ThreeOS_Pose* far_pose,
                                                        ThreeOS_Pose* out_near);
THREEOS_API int32_t threeos_topology_portal_cross(uint64_t from_proxy_id, uint64_t to_proxy_id,
                                                  const ThreeOS_Pose* near_in_to,
                                                  ThreeOS_Pose* out_far);
THREEOS_API int32_t threeos_topology_dome_drop(const ThreeOS_Pose* dome_local_pose,
                                               ThreeOS_Pose* out_far);

/** Remap helpers (no portal containment check). */
THREEOS_API int32_t threeos_topology_far_to_near(uint64_t proxy_id, const ThreeOS_Vec3* far_world,
                                                 ThreeOS_Vec3* out_near);
THREEOS_API int32_t threeos_topology_near_to_far(uint64_t proxy_id, const ThreeOS_Vec3* near_world,
                                                 ThreeOS_Vec3* out_far);

THREEOS_API int32_t threeos_topology_world_to_voxel(const ThreeOS_Vec3* world,
                                                    ThreeOS_VoxelCoord* out_voxel);
THREEOS_API int32_t threeos_topology_voxel_to_world(ThreeOS_VoxelCoord voxel,
                                                    ThreeOS_Vec3* out_world);

#ifdef __cplusplus
}
#endif
