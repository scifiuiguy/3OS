#include "interop/threeos_marshaller.hpp"

#include "core/interaction_facade.hpp"
#include "core/topology.hpp"

#include <cstring>
#include <mutex>
#include <optional>
#include <string>

namespace {

constexpr uint32_t kMajor = 0;
constexpr uint32_t kMinor = 2;
constexpr uint32_t kPatch = 0;
constexpr uint32_t kAbiVersion = 2;

std::mutex g_mu;
bool g_initialized = false;
std::string g_last_error;
threeos::InteractionFacade g_facade;
threeos::TopologyWorkspace g_topology;
uint64_t g_tick_count = 0;
std::optional<threeos::Pose> g_pending_portal_pose;
uint64_t g_pending_portal_entity = 0;

void set_error(const char* msg) { g_last_error = msg ? msg : ""; }

threeos::Vec3 from_c(const ThreeOS_Vec3& v) { return threeos::Vec3{v.x, v.y, v.z}; }
ThreeOS_Vec3 to_c(const threeos::Vec3& v) { return ThreeOS_Vec3{v.x, v.y, v.z}; }

threeos::Quat from_c(const ThreeOS_Quat& q) { return threeos::Quat{q.x, q.y, q.z, q.w}; }
ThreeOS_Quat to_c(const threeos::Quat& q) { return ThreeOS_Quat{q.x, q.y, q.z, q.w}; }

threeos::Pose from_c(const ThreeOS_Pose& p) {
  return threeos::Pose{from_c(p.position), from_c(p.orientation)};
}
ThreeOS_Pose to_c(const threeos::Pose& p) {
  return ThreeOS_Pose{to_c(p.position), to_c(p.orientation)};
}

threeos::Aabb from_c(const ThreeOS_Aabb& a) {
  return threeos::Aabb{from_c(a.min), from_c(a.max)};
}

void fill_dome(ThreeOS_DomeState* out) {
  const auto& d = g_topology.dome();
  std::memset(out, 0, sizeof(*out));
  out->active = d.active ? 1u : 0u;
  out->scale_ratio = d.scale_ratio;
  out->pose = to_c(d.pose);
  out->radius_m = d.radius_m;
}

void fill_proxy(const threeos::DoubleProxyState& p, ThreeOS_DoubleProxyState* out) {
  std::memset(out, 0, sizeof(*out));
  out->id = p.id;
  out->active = p.active ? 1u : 0u;
  out->scale_ratio = p.scale_ratio;
  out->far_aabb.min = to_c(p.far_aabb.min);
  out->far_aabb.max = to_c(p.far_aabb.max);
  out->near_aabb.min = to_c(p.near_aabb.min);
  out->near_aabb.max = to_c(p.near_aabb.max);
}

}  // namespace

static_assert(sizeof(ThreeOS_Vec3) == 12, "ThreeOS_Vec3");
static_assert(sizeof(ThreeOS_Quat) == 16, "ThreeOS_Quat");
static_assert(sizeof(ThreeOS_Pose) == 28, "ThreeOS_Pose");
static_assert(sizeof(ThreeOS_InteropInputFrame) == 192, "InteropInputFrame frozen size");
static_assert(sizeof(ThreeOS_InteropTransformDelta) == 48, "InteropTransformDelta frozen size");
static_assert(sizeof(ThreeOS_DomeState) == 48, "DomeState size");
static_assert(sizeof(ThreeOS_DoubleProxyState) == 64, "DoubleProxyState size");

extern "C" THREEOS_API uint32_t threeos_version(void) {
  return (kMajor << 16) | (kMinor << 8) | kPatch;
}

extern "C" THREEOS_API uint32_t threeos_abi_version(void) { return kAbiVersion; }

extern "C" THREEOS_API uint32_t threeos_sizeof_input_frame(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_InteropInputFrame));
}

extern "C" THREEOS_API uint32_t threeos_sizeof_transform_delta(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_InteropTransformDelta));
}

extern "C" THREEOS_API uint32_t threeos_sizeof_dome_state(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_DomeState));
}

extern "C" THREEOS_API uint32_t threeos_sizeof_double_proxy_state(void) {
  return static_cast<uint32_t>(sizeof(ThreeOS_DoubleProxyState));
}

extern "C" THREEOS_API int32_t threeos_init(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  g_initialized = true;
  g_tick_count = 0;
  g_facade.set_state(threeos::InteractionState::Idle);
  g_topology = threeos::TopologyWorkspace{};
  g_pending_portal_pose.reset();
  g_pending_portal_entity = 0;
  set_error("");
  return 0;
}

extern "C" THREEOS_API void threeos_shutdown(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  g_initialized = false;
  g_pending_portal_pose.reset();
  set_error("");
}

extern "C" THREEOS_API int32_t threeos_tick(const ThreeOS_InteropInputFrame* frame,
                                            ThreeOS_InteropTransformDelta* out_delta) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("threeos_tick called before threeos_init");
    return -1;
  }
  if (frame == nullptr) {
    set_error("threeos_tick: null frame");
    return -2;
  }

  ++g_tick_count;
  if ((frame->button_flags & THREEOS_BTN_RIGHT_SELECT) != 0 ||
      (frame->button_flags & THREEOS_BTN_LEFT_SELECT) != 0) {
    g_facade.set_state(threeos::InteractionState::Hovering);
  } else {
    g_facade.set_state(threeos::InteractionState::Idle);
  }

  if (out_delta != nullptr) {
    std::memset(out_delta, 0, sizeof(*out_delta));
    if (g_pending_portal_pose.has_value()) {
      out_delta->entity_id = g_pending_portal_entity;
      out_delta->pose = to_c(*g_pending_portal_pose);
      out_delta->flags = 1u;  // portal applied
      g_pending_portal_pose.reset();
      g_pending_portal_entity = 0;
    } else {
      out_delta->entity_id = 0;
      out_delta->pose = frame->head;
      out_delta->flags = frame->tracking_flags;
    }
  }

  set_error("");
  return 0;
}

extern "C" THREEOS_API const char* threeos_last_error(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  return g_last_error.c_str();
}

extern "C" THREEOS_API int32_t threeos_topology_set_scale(float world_to_proxy_ratio) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (!(world_to_proxy_ratio > 0.f)) {
    set_error("scale must be > 0");
    return -2;
  }
  g_topology.set_world_to_proxy_scale(world_to_proxy_ratio);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_open_dome(const ThreeOS_Pose* head_pose) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (head_pose == nullptr) {
    set_error("null head_pose");
    return -2;
  }
  g_topology.open_dome(from_c(*head_pose));
  set_error("");
  return 0;
}

extern "C" THREEOS_API void threeos_topology_close_dome(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (g_initialized) {
    g_topology.close_dome();
  }
}

extern "C" THREEOS_API int32_t threeos_topology_get_dome(ThreeOS_DomeState* out_dome) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (out_dome == nullptr) {
    set_error("null out_dome");
    return -2;
  }
  fill_dome(out_dome);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_open_double_proxy(uint64_t id,
                                                                 const ThreeOS_Aabb* far_aabb,
                                                                 const ThreeOS_Pose* head_pose) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (far_aabb == nullptr || head_pose == nullptr) {
    set_error("null far_aabb or head_pose");
    return -2;
  }
  if (!g_topology.open_double_proxy(id, from_c(*far_aabb), from_c(*head_pose))) {
    set_error("open_double_proxy failed");
    return -3;
  }
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_close_double_proxy(uint64_t id) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (!g_topology.close_double_proxy(id)) {
    set_error("proxy not found");
    return -2;
  }
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_get_double_proxy(
    uint64_t id, ThreeOS_DoubleProxyState* out_proxy) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (out_proxy == nullptr) {
    set_error("null out_proxy");
    return -2;
  }
  const auto* p = g_topology.find_double_proxy(id);
  if (p == nullptr) {
    set_error("proxy not found");
    return -3;
  }
  fill_proxy(*p, out_proxy);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_select_dome_region(
    uint64_t proxy_id, ThreeOS_VoxelCoord far_min, ThreeOS_VoxelCoord far_max,
    const ThreeOS_Pose* head_pose) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (head_pose == nullptr) {
    set_error("null head_pose");
    return -2;
  }
  threeos::VoxelCoord vmin{far_min.x, far_min.y, far_min.z};
  threeos::VoxelCoord vmax{far_max.x, far_max.y, far_max.z};
  if (!g_topology.select_dome_region(proxy_id, vmin, vmax, from_c(*head_pose))) {
    set_error("select_dome_region failed (dome closed?)");
    return -3;
  }
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_portal_near_to_far(uint64_t proxy_id,
                                                                  const ThreeOS_Pose* near_pose,
                                                                  ThreeOS_Pose* out_far) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (near_pose == nullptr || out_far == nullptr) {
    set_error("null pose");
    return -2;
  }
  const auto result = g_topology.portal_near_to_far(proxy_id, from_c(*near_pose));
  if (!result) {
    set_error("portal_near_to_far: not in near volume or missing proxy");
    return -3;
  }
  *out_far = to_c(*result);
  g_pending_portal_pose = *result;
  g_pending_portal_entity = proxy_id;
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_portal_far_to_near(uint64_t proxy_id,
                                                                  const ThreeOS_Pose* far_pose,
                                                                  ThreeOS_Pose* out_near) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (far_pose == nullptr || out_near == nullptr) {
    set_error("null pose");
    return -2;
  }
  const auto result = g_topology.portal_far_to_near(proxy_id, from_c(*far_pose));
  if (!result) {
    set_error("portal_far_to_near: not in far volume or missing proxy");
    return -3;
  }
  *out_near = to_c(*result);
  g_pending_portal_pose = *result;
  g_pending_portal_entity = proxy_id;
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_portal_cross(uint64_t from_proxy_id,
                                                            uint64_t to_proxy_id,
                                                            const ThreeOS_Pose* near_in_to,
                                                            ThreeOS_Pose* out_far) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (near_in_to == nullptr || out_far == nullptr) {
    set_error("null pose");
    return -2;
  }
  const auto result = g_topology.portal_cross(from_proxy_id, to_proxy_id, from_c(*near_in_to));
  if (!result) {
    set_error("portal_cross failed");
    return -3;
  }
  *out_far = to_c(*result);
  g_pending_portal_pose = *result;
  g_pending_portal_entity = to_proxy_id;
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_dome_drop(const ThreeOS_Pose* dome_local_pose,
                                                         ThreeOS_Pose* out_far) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized) {
    set_error("not initialized");
    return -1;
  }
  if (dome_local_pose == nullptr || out_far == nullptr) {
    set_error("null pose");
    return -2;
  }
  if (!g_topology.dome().active) {
    set_error("dome not open");
    return -3;
  }
  const auto result = g_topology.dome_drop(from_c(*dome_local_pose));
  *out_far = to_c(result);
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_far_to_near(uint64_t proxy_id,
                                                           const ThreeOS_Vec3* far_world,
                                                           ThreeOS_Vec3* out_near) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized || far_world == nullptr || out_near == nullptr) {
    set_error("bad args");
    return -1;
  }
  *out_near = to_c(g_topology.far_to_near(proxy_id, from_c(*far_world)));
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_near_to_far(uint64_t proxy_id,
                                                           const ThreeOS_Vec3* near_world,
                                                           ThreeOS_Vec3* out_far) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized || near_world == nullptr || out_far == nullptr) {
    set_error("bad args");
    return -1;
  }
  *out_far = to_c(g_topology.near_to_far(proxy_id, from_c(*near_world)));
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_world_to_voxel(const ThreeOS_Vec3* world,
                                                              ThreeOS_VoxelCoord* out_voxel) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized || world == nullptr || out_voxel == nullptr) {
    set_error("bad args");
    return -1;
  }
  const auto vc = g_topology.world_to_voxel(from_c(*world));
  out_voxel->x = vc.x;
  out_voxel->y = vc.y;
  out_voxel->z = vc.z;
  set_error("");
  return 0;
}

extern "C" THREEOS_API int32_t threeos_topology_voxel_to_world(ThreeOS_VoxelCoord voxel,
                                                              ThreeOS_Vec3* out_world) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_initialized || out_world == nullptr) {
    set_error("bad args");
    return -1;
  }
  *out_world =
      to_c(g_topology.voxel_center_to_world(threeos::VoxelCoord{voxel.x, voxel.y, voxel.z}));
  set_error("");
  return 0;
}
