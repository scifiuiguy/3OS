#pragma once

/**
 * 3OS host ABI (Phase 0.6).
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
 * Topology (0.2), kinematics (0.3), storage/glyphs (0.4), hand interaction (0.5),
 * voodoo volumes (0.6) are additive; abi_version = 6. InteropInputFrame size remains frozen.
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
  /** Ternary channel (controller A/X / primaryButton). Additive; frame size unchanged. */
  THREEOS_BTN_LEFT_TERNARY = 1u << 5,
  THREEOS_BTN_RIGHT_TERNARY = 1u << 6,
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

enum ThreeOS_TransformFlags : uint32_t {
  THREEOS_XFORM_NONE = 0,
  THREEOS_XFORM_PORTAL = 1u << 0,
  THREEOS_XFORM_KINEMATIC = 1u << 1,
  THREEOS_XFORM_FLOOR_CLAMP = 1u << 2,
};

/**
 * Host-visible transform update. Size frozen at 48 bytes.
 * entity_id != 0 means the host should apply pose to that entity.
 */
typedef struct ThreeOS_InteropTransformDelta {
  uint64_t entity_id;
  ThreeOS_Pose pose;
  uint32_t flags;
  uint32_t reserved0;
  uint32_t reserved1; /* pad to 48 bytes */
} ThreeOS_InteropTransformDelta;

/** Kinetic engine snapshot (48 bytes). */
typedef struct ThreeOS_KineticState {
  uint64_t entity_id;
  uint32_t phase; /* 0 Idle, 1 Possessed, 2 Coasting */
  float vel_x;
  float vel_y;
  float vel_z;
  float floor_y;
  uint32_t reserved0;
  uint32_t reserved1;
  uint32_t reserved2;
  uint32_t reserved3;
  uint32_t reserved4; /* pad to 48 bytes */
} ThreeOS_KineticState;

/**
 * Stick velocity debug ray (64 bytes), OpenXR meters.
 * Host draws ray_origin → ray_tip; kernel owns stick / offset math.
 */
typedef struct ThreeOS_StickDebug {
  uint32_t active;      /* 1 while possessed with frozen start_hand */
  uint32_t in_deadzone; /* 1 if armed and |stick| < deadzone_m */
  float magnitude;      /* |stick| meters */
  float unused0;
  ThreeOS_Vec3 stick;      /* current_hand - start_hand */
  ThreeOS_Vec3 ray_origin; /* start_hand - flat_forward*4" + world +Y*2" */
  ThreeOS_Vec3 ray_tip;    /* ray_origin + stick */
  uint32_t deadzone_armed; /* 0 until leave then re-enter deadzone */
  uint32_t reserved1;
  uint32_t reserved2; /* pad to 64 bytes */
} ThreeOS_StickDebug;

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

/** Workspace item for host glyph/label sync (200 bytes). */
typedef struct ThreeOS_WorkspaceItem {
  uint64_t entity_id;
  uint32_t kind;  /* 0 Object, 1 Box */
  uint32_t flags; /* bit0 in_field, bit1 insert_pending, bit2 box_expanded */
  ThreeOS_Pose pose;
  uint32_t child_count;
  uint32_t box_visual; /* 0 n/a, 1 closed, 2 open */
  char name[64];
  char secondary_label[32];
  char glyph_id[32];
  float tint_r;
  float tint_g;
  float tint_b;
  float tint_a;
  uint32_t reserved0; /* pad to 200 (MSVC 8-byte align) */
} ThreeOS_WorkspaceItem;

/** Snap-to-box state (64 bytes). */
typedef struct ThreeOS_SnapState {
  uint32_t pending;
  uint32_t reserved0;
  uint64_t object_id;
  uint64_t box_id;
  ThreeOS_Pose hold_pose;
  uint32_t reserved1;
  uint32_t reserved2;
  uint32_t reserved3;
} ThreeOS_SnapState;

/** Host FS write-through event (344 bytes). */
typedef struct ThreeOS_StorageEvent {
  uint32_t type; /* 0 none, 1 ObjectMovedIntoBox */
  uint32_t awaiting_ack;
  uint64_t object_id;
  uint64_t box_id;
  char source_rel[160];
  char dest_rel[160];
} ThreeOS_StorageEvent;

/** Packed semver: (major << 16) | (minor << 8) | patch. Phase 0.5 => 0.5.0 */
THREEOS_API uint32_t threeos_version(void);

/** ABI layout/feature version; 5 = hand interaction / point-cursor APIs present. */
THREEOS_API uint32_t threeos_abi_version(void);

THREEOS_API int32_t threeos_init(void);
THREEOS_API void threeos_shutdown(void);

/**
 * Push one input frame. Returns 0 on success, negative on error.
 * While an entity is possessed/coasting, samples aim poses for rate control and
 * writes kinematic deltas. Portal pending results take priority for that frame.
 */
THREEOS_API int32_t threeos_tick(const ThreeOS_InteropInputFrame* frame,
                                 ThreeOS_InteropTransformDelta* out_delta);

THREEOS_API const char* threeos_last_error(void);

THREEOS_API uint32_t threeos_sizeof_input_frame(void);
THREEOS_API uint32_t threeos_sizeof_transform_delta(void);
THREEOS_API uint32_t threeos_sizeof_dome_state(void);
THREEOS_API uint32_t threeos_sizeof_double_proxy_state(void);
THREEOS_API uint32_t threeos_sizeof_kinetic_state(void);
THREEOS_API uint32_t threeos_sizeof_stick_debug(void);
THREEOS_API uint32_t threeos_sizeof_workspace_item(void);
THREEOS_API uint32_t threeos_sizeof_snap_state(void);
THREEOS_API uint32_t threeos_sizeof_storage_event(void);

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

/* --- Kinematics (Phase 0.3) --- */

THREEOS_API int32_t threeos_kinematics_set_params(float gain, float deadzone_m, float friction,
                                                  float floor_y);
THREEOS_API int32_t threeos_kinematics_possess(uint64_t entity_id, const ThreeOS_Pose* object_pose);
/** Possess and latch drive hand in one call (0=left, 1=right). Preferred over possess+set_drive_hand. */
THREEOS_API int32_t threeos_kinematics_possess_with_hand(uint64_t entity_id,
                                                         const ThreeOS_Pose* object_pose,
                                                         uint32_t hand);
/** Latch which hand drives possess stick (0=left, 1=right). Call right after possess. */
THREEOS_API int32_t threeos_kinematics_set_drive_hand(uint32_t hand);
THREEOS_API int32_t threeos_kinematics_release(void);
THREEOS_API int32_t threeos_kinematics_get_state(ThreeOS_KineticState* out_state);
THREEOS_API int32_t threeos_kinematics_get_stick_debug(ThreeOS_StickDebug* out_debug);
THREEOS_API int32_t threeos_kinematics_set_object_pose(const ThreeOS_Pose* object_pose);

/* --- Storage / glyphs (Phase 0.4) --- */

THREEOS_API void threeos_storage_clear(void);
THREEOS_API int32_t threeos_storage_add_object(const char* name, const char* relative_path,
                                               uint64_t* out_entity_id);
THREEOS_API int32_t threeos_storage_add_box(const char* name, const char* relative_path,
                                            uint32_t child_count, uint64_t* out_entity_id);
/** Layout field glyphs in a cubic matrix 1 m in front of head. Null head → origin facing -Z. */
THREEOS_API int32_t threeos_storage_layout_demo(const ThreeOS_Pose* head_pose);
THREEOS_API uint32_t threeos_storage_item_count(void);
THREEOS_API int32_t threeos_storage_get_item(uint32_t index, ThreeOS_WorkspaceItem* out_item);
THREEOS_API int32_t threeos_storage_get_snap(ThreeOS_SnapState* out_snap);
THREEOS_API int32_t threeos_storage_try_commit_insert(void);
/** Insert object/box into target box without prior snap (cross-proxy select-drop). */
THREEOS_API int32_t threeos_storage_insert_into(uint64_t object_id, uint64_t box_id);
THREEOS_API int32_t threeos_storage_poll_event(ThreeOS_StorageEvent* out_event);
THREEOS_API int32_t threeos_storage_ack_event(int32_t success);
THREEOS_API int32_t threeos_storage_set_box_open(uint64_t box_id, int32_t open);

/** Parent an entry under a box (parent_box_id=0 → root). */
THREEOS_API int32_t threeos_storage_set_parent_box(uint64_t entity_id, uint64_t parent_box_id);
THREEOS_API int32_t threeos_storage_set_in_field(uint64_t entity_id, int32_t in_field);
/** Read parent_box (0 = root). Returns 0 on success. */
THREEOS_API int32_t threeos_storage_get_parent_box(uint64_t entity_id, uint64_t* out_parent_box_id);

/**
 * Expand box contents into the field: children with parent_box==box_id become
 * in_field and are laid out in a cubic matrix above the box (collision-aware).
 */
THREEOS_API int32_t threeos_storage_expand_box(uint64_t box_id);
/** Collapse expanded children (in_field=false) and close box visual. */
THREEOS_API int32_t threeos_storage_collapse_box(uint64_t box_id);

/** Install a .3glyph / GLB pack containing THREEOS_glyph metadata. */
THREEOS_API int32_t threeos_glyph_install_pack(const uint8_t* bytes, uint32_t byte_count);

/* --- Selection (Phase 0.4 tack-on; EntityId, 0 = none) --- */

THREEOS_API int32_t threeos_selection_get(uint64_t* out_entity_id);
THREEOS_API int32_t threeos_selection_set(uint64_t entity_id);
THREEOS_API void threeos_selection_clear(void);

/* --- Hand interaction / point cursor (Phase 0.5; additive) --- */

enum ThreeOS_PalmMode : uint32_t {
  THREEOS_PALM_UP = 0,
  THREEOS_PALM_SIDEWAYS = 1,
  THREEOS_PALM_DOWN = 2,
};

enum ThreeOS_PinchChannel : uint32_t {
  THREEOS_PINCH_NONE = 0,
  THREEOS_PINCH_PRIMARY = 1,
  THREEOS_PINCH_SECONDARY = 2,
  THREEOS_PINCH_TERNARY = 3,
};

enum ThreeOS_InteractionIntentFlags : uint32_t {
  THREEOS_INTENT_NONE = 0,
  THREEOS_INTENT_SELECT_PULSE = 1u << 0,
  THREEOS_INTENT_DRAG_ACTIVE = 1u << 1,
  THREEOS_INTENT_VOODOO_SNAP = 1u << 2,  /* primary hold + palm down (was VOODOO_HOLD) */
  THREEOS_INTENT_PRIMARY_HELD = 1u << 3,
  THREEOS_INTENT_SECONDARY_HELD = 1u << 4,
  THREEOS_INTENT_TERNARY_HELD = 1u << 5,
  THREEOS_INTENT_VOODOO_FREE = 1u << 6, /* secondary hold + palm down */
};

enum ThreeOS_VoodooMode : uint32_t {
  THREEOS_VOODOO_NONE = 0,
  THREEOS_VOODOO_SNAP = 1,
  THREEOS_VOODOO_FREE = 2,
};

typedef struct ThreeOS_IVec3 {
  int32_t x;
  int32_t y;
  int32_t z;
} ThreeOS_IVec3;

/** Field metrics for voodoo volumes (32 bytes). */
typedef struct ThreeOS_VoodooFieldParams {
  float voxel_size_m;
  ThreeOS_IVec3 near_bb_voxels; /* x,y,z = W,H,D counts; extents = counts * voxel_size */
  float field_radius_m;
  float field_height_m;
  float arm_reach_m; /* hand travel mapping to full field radius/height; default 12" */
  uint32_t reserved0; /* pad to 32 */
} ThreeOS_VoodooFieldParams;

/** Far or near bounding-box descriptor (80 bytes). */
typedef struct ThreeOS_VoodooBoundingBox {
  uint32_t open;
  uint32_t reserved0;
  ThreeOS_Pose pose;       /* floor-yaw; center */
  ThreeOS_Vec3 extents;    /* full W×H×D meters (x,y,z) */
  ThreeOS_IVec3 aspect_voxels;
  uint32_t reserved1; /* pad so linked_centroid_id is 8-byte aligned */
  uint64_t linked_centroid_id; /* 0 until Task 6.2 */
  uint32_t reserved2;
  uint32_t reserved3; /* pad to 80 */
} ThreeOS_VoodooBoundingBox;

/** Active voodoo session snapshot (176 bytes). */
typedef struct ThreeOS_VoodooSession {
  uint32_t mode; /* ThreeOS_VoodooMode */
  uint32_t open;
  uint32_t hand; /* 0 left, 1 right */
  uint32_t reserved0;
  ThreeOS_VoodooBoundingBox far_bb;
  ThreeOS_VoodooBoundingBox near_bb; /* opens on intentional release */
} ThreeOS_VoodooSession;

/** Yellow voodoo drag arrow (64 bytes; same layout as StickDebug). */
typedef struct ThreeOS_VoodooDragArrow {
  uint32_t active;
  uint32_t in_deadzone;
  float magnitude;
  float unused0;
  ThreeOS_Vec3 stick;
  ThreeOS_Vec3 ray_origin;
  ThreeOS_Vec3 ray_tip;
  uint32_t deadzone_armed;
  uint32_t reserved1;
  uint32_t reserved2;
} ThreeOS_VoodooDragArrow;

/**
 * Per-hand sample from host (128 bytes).
 * Hands: fill tips + tip_flags; pressed from pinch detection.
 * Controllers: tip_flags=0; fill aim as index_tip; primary/secondary/ternary values + pressed.
 */
typedef struct ThreeOS_HandSample {
  uint32_t tracked;
  uint32_t tip_flags; /* bit0 thumb,1 index,2 middle,3 ring,4 index proximal */
  ThreeOS_Vec3 thumb_tip;
  ThreeOS_Vec3 index_tip;
  ThreeOS_Vec3 middle_tip;
  ThreeOS_Vec3 ring_tip;
  ThreeOS_Vec3 palm_normal; /* world; ignored if palm_mode_valid */
  uint32_t palm_mode;       /* ThreeOS_PalmMode */
  uint32_t palm_mode_valid;
  float primary_value;
  float secondary_value;
  float ternary_value;
  uint32_t primary_pressed;
  uint32_t secondary_pressed;
  uint32_t ternary_pressed;
  ThreeOS_Vec3 index_proximal; /* cursor anchor when tip_flags bit4 */
  uint32_t reserved3;
  uint32_t reserved4;
  uint32_t reserved5;
  uint32_t reserved6; /* pad to 128 */
} ThreeOS_HandSample;

/** Point cursor probe for one hand (32 bytes). */
typedef struct ThreeOS_PointCursor {
  uint32_t active;
  uint32_t driving_channel; /* ThreeOS_PinchChannel */
  float scale;
  float driving_distance_m;
  ThreeOS_Vec3 position;
  uint32_t palm_mode;
} ThreeOS_PointCursor;

/** Aggregated facade intents for the preferred busy hand (16 bytes). */
typedef struct ThreeOS_InteractionIntents {
  uint32_t flags;           /* ThreeOS_InteractionIntentFlags */
  uint32_t palm_mode;       /* ThreeOS_PalmMode */
  uint32_t active_channel;  /* ThreeOS_PinchChannel */
  uint32_t handedness;      /* 0 left, 1 right, 2 none */
} ThreeOS_InteractionIntents;

THREEOS_API uint32_t threeos_sizeof_hand_sample(void);
THREEOS_API uint32_t threeos_sizeof_point_cursor(void);
THREEOS_API uint32_t threeos_sizeof_interaction_intents(void);

/** Push both hands (null = clear / untracked). Call each frame before threeos_tick. */
THREEOS_API int32_t threeos_interaction_set_hands(const ThreeOS_HandSample* left,
                                                  const ThreeOS_HandSample* right);

THREEOS_API int32_t threeos_interaction_get_cursor(uint32_t hand /*0 left, 1 right*/,
                                                   ThreeOS_PointCursor* out_cursor);
THREEOS_API int32_t threeos_interaction_get_intents(ThreeOS_InteractionIntents* out_intents);
THREEOS_API int32_t threeos_interaction_set_pinch_thresh_m(float meters);

/* --- Voodoo volumes (Phase 0.6; additive) --- */

THREEOS_API uint32_t threeos_sizeof_voodoo_field_params(void);
THREEOS_API uint32_t threeos_sizeof_voodoo_bounding_box(void);
THREEOS_API uint32_t threeos_sizeof_voodoo_session(void);
THREEOS_API uint32_t threeos_sizeof_voodoo_drag_arrow(void);

THREEOS_API int32_t threeos_voodoo_get_field_params(ThreeOS_VoodooFieldParams* out_params);
THREEOS_API int32_t threeos_voodoo_set_field_params(const ThreeOS_VoodooFieldParams* params);
/** hand: 0 left, 1 right — independent concurrent sessions. */
THREEOS_API int32_t threeos_voodoo_get_session(uint32_t hand, ThreeOS_VoodooSession* out_session);
THREEOS_API int32_t threeos_voodoo_get_drag_arrow(uint32_t hand, ThreeOS_VoodooDragArrow* out_arrow);

/* --- Centroids / god roots (Phase 0.6 Task 6.2; additive) --- */

enum ThreeOS_CentroidKind : uint32_t {
  THREEOS_CENTROID_NONE = 0,
  THREEOS_CENTROID_GOD = 1,
  THREEOS_CENTROID_BOX = 2,
  THREEOS_CENTROID_TEMPORARY = 3,
};

/** Live centroid snapshot (64 bytes). Empty sets are omitted from the list. */
typedef struct ThreeOS_Centroid {
  uint64_t id;               /* god id or box entity id */
  uint32_t kind;             /* ThreeOS_CentroidKind */
  uint32_t member_count;
  uint64_t linked_entity_id; /* same as id in 6.2 */
  ThreeOS_Vec3 position;     /* average of member world positions */
  float unused0;
  char name[16];
  uint32_t reserved0;
  uint32_t reserved1;
} ThreeOS_Centroid;

THREEOS_API uint32_t threeos_sizeof_centroid(void);
THREEOS_API void threeos_god_clear(void);
THREEOS_API int32_t threeos_god_register(const char* name, const ThreeOS_Pose* anchor,
                                         uint64_t* out_god_id);
THREEOS_API int32_t threeos_god_set_anchor(uint64_t god_id, const ThreeOS_Pose* anchor);
THREEOS_API int32_t threeos_storage_set_god_root(uint64_t entity_id, uint64_t god_root_id);
THREEOS_API int32_t threeos_storage_layout_god(uint64_t god_root_id, const ThreeOS_Pose* center);
THREEOS_API uint32_t threeos_centroid_count(void);
THREEOS_API int32_t threeos_centroid_get(uint32_t index, ThreeOS_Centroid* out_centroid);

/* --- Voodoo snap nodes (Phase 0.6 Task 6.3; additive) --- */

/** Snap-mode node near the yellow arrow (48 bytes). */
typedef struct ThreeOS_VoodooSnapNode {
  uint64_t centroid_id;
  uint32_t kind;    /* ThreeOS_CentroidKind */
  uint32_t snapped; /* 1 if tip currently locked to this node */
  ThreeOS_Vec3 world_position;
  float radius_m;
  ThreeOS_Vec3 field_position; /* real centroid in field */
  float unused0;
} ThreeOS_VoodooSnapNode;

THREEOS_API uint32_t threeos_sizeof_voodoo_snap_node(void);
THREEOS_API uint32_t threeos_voodoo_snap_node_count(uint32_t hand);
THREEOS_API int32_t threeos_voodoo_get_snap_node(uint32_t hand, uint32_t index,
                                                 ThreeOS_VoodooSnapNode* out_node);

/* --- Voodoo portals / ghosts (Phase 0.6 Task 6.4; additive) --- */

/** Parked far+near portal snapshot (200 bytes). */
typedef struct ThreeOS_VoodooPortal {
  uint64_t id;
  uint32_t mode; /* ThreeOS_VoodooMode */
  uint32_t creating_hand;
  ThreeOS_VoodooBoundingBox far_bb;
  ThreeOS_VoodooBoundingBox near_bb;
  uint64_t persistent_centroid_id;
  uint64_t temporary_centroid_id;
  uint32_t ghost_count;
  uint32_t reserved0;
} ThreeOS_VoodooPortal;

/** Near-portal ghost of a far entity (88 bytes). */
typedef struct ThreeOS_VoodooGhost {
  uint64_t entity_id;
  uint32_t is_parent_box;
  uint32_t reserved0;
  ThreeOS_Pose far_pose;
  ThreeOS_Pose near_pose;
  float scale;
  float unused0;
  float unused1;
  float unused2;
} ThreeOS_VoodooGhost;

THREEOS_API uint32_t threeos_sizeof_voodoo_portal(void);
THREEOS_API uint32_t threeos_sizeof_voodoo_ghost(void);
THREEOS_API uint32_t threeos_voodoo_portal_count(void);
THREEOS_API int32_t threeos_voodoo_get_portal(uint32_t index, ThreeOS_VoodooPortal* out_portal);
THREEOS_API uint32_t threeos_voodoo_portal_ghost_count(uint64_t portal_id);
THREEOS_API int32_t threeos_voodoo_get_portal_ghost(uint64_t portal_id, uint32_t index,
                                                    ThreeOS_VoodooGhost* out_ghost);
THREEOS_API int32_t threeos_voodoo_dismiss_portal(uint64_t portal_id);
THREEOS_API uint32_t threeos_voodoo_creation_allowed(void);
THREEOS_API uint32_t threeos_voodoo_point_in_near_bb(float x, float y, float z);

/* --- Voodoo ghost drag / summon (Phase 0.6 Task 6.5; additive) --- */

/** Portal id whose open near BB contains the point, or 0. */
THREEOS_API uint64_t threeos_voodoo_portal_at_near_point(float x, float y, float z);
/** Portal owning a content ghost for entity_id, or 0. */
THREEOS_API uint64_t threeos_voodoo_portal_for_ghost(uint64_t entity_id);
THREEOS_API int32_t threeos_voodoo_ghost_drag_begin(uint32_t hand, uint64_t entity_id);
THREEOS_API int32_t threeos_voodoo_ghost_drag_end(uint32_t hand, uint32_t release_inside_near_bb);
THREEOS_API uint32_t threeos_voodoo_ghost_drag_active(uint32_t hand);
THREEOS_API uint64_t threeos_voodoo_ghost_drag_entity(uint32_t hand);
THREEOS_API int32_t threeos_voodoo_get_ghost_drag_arrow(uint32_t hand,
                                                       ThreeOS_VoodooDragArrow* out_arrow);
/** Summon selected ghost to pinch (open-air or cross-portal). */
THREEOS_API int32_t threeos_voodoo_summon(uint64_t entity_id, float x, float y, float z);

#ifdef __cplusplus
}
#endif

