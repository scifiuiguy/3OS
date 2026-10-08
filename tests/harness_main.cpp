#include "interop/threeos_abi.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

bool expect_eq(const char* name, uint32_t actual, uint32_t expected) {
  if (actual != expected) {
    std::fprintf(stderr, "FAIL %s: got %u expected %u\n", name, actual, expected);
    return false;
  }
  std::printf("OK   %s = %u\n", name, actual);
  return true;
}

bool expect_near(const char* name, float actual, float expected, float eps) {
  if (std::fabs(actual - expected) > eps) {
    std::fprintf(stderr, "FAIL %s: got %f expected %f (eps %f)\n", name, actual, expected, eps);
    return false;
  }
  std::printf("OK   %s ≈ %f\n", name, actual);
  return true;
}

ThreeOS_Pose identity_pose_at(float x, float y, float z) {
  ThreeOS_Pose p{};
  p.position = {x, y, z};
  p.orientation = {0.f, 0.f, 0.f, 1.f};
  return p;
}

}  // namespace

int main() {
  bool ok = true;

  const uint32_t version = threeos_version();
  const uint32_t major = (version >> 16) & 0xFFu;
  const uint32_t minor = (version >> 8) & 0xFFu;
  const uint32_t patch = version & 0xFFu;
  std::printf("threeos_version => %u.%u.%u (0x%08X)\n", major, minor, patch, version);
  ok = expect_eq("major", major, 0) && ok;
  ok = expect_eq("minor", minor, 2) && ok;
  ok = expect_eq("patch", patch, 0) && ok;
  ok = expect_eq("abi_version", threeos_abi_version(), 2) && ok;
  ok = expect_eq("sizeof_input_frame", threeos_sizeof_input_frame(), 192) && ok;
  ok = expect_eq("sizeof_transform_delta", threeos_sizeof_transform_delta(), 48) && ok;
  ok = expect_eq("sizeof_dome_state", threeos_sizeof_dome_state(), 48) && ok;
  ok = expect_eq("sizeof_double_proxy_state", threeos_sizeof_double_proxy_state(), 64) && ok;

  if (threeos_tick(nullptr, nullptr) != -1) {
    std::fprintf(stderr, "FAIL tick before init should return -1\n");
    ok = false;
  }

  if (threeos_init() != 0) {
    std::fprintf(stderr, "FAIL threeos_init: %s\n", threeos_last_error());
    return EXIT_FAILURE;
  }

  // --- Voxel indexing (~1 inch) ---
  ThreeOS_Vec3 world{0.03f, 0.0f, -0.03f};
  ThreeOS_VoxelCoord vc{};
  if (threeos_topology_world_to_voxel(&world, &vc) != 0) {
    std::fprintf(stderr, "FAIL world_to_voxel: %s\n", threeos_last_error());
    ok = false;
  } else {
    if (vc.x != 1) {
      std::fprintf(stderr, "FAIL voxel.x: got %d expected 1\n", vc.x);
      ok = false;
    } else {
      std::printf("OK   voxel.x = %d\n", vc.x);
    }
    // z = floor(-0.03/0.0254) = floor(-1.18) = -2
    if (vc.z != -2) {
      std::fprintf(stderr, "FAIL voxel.z: got %d expected -2\n", vc.z);
      ok = false;
    } else {
      std::printf("OK   voxel.z = %d\n", vc.z);
    }
  }

  ThreeOS_Vec3 back{};
  if (threeos_topology_voxel_to_world(ThreeOS_VoxelCoord{0, 0, 0}, &back) != 0) {
    std::fprintf(stderr, "FAIL voxel_to_world\n");
    ok = false;
  } else {
    ok = expect_near("voxel0.center.x", back.x, 0.0127f, 1e-4f) && ok;
  }

  // Configurable scale (not hardcoded 1:100 only)
  ok = (threeos_topology_set_scale(50.f) == 0) && ok;
  ok = (threeos_topology_set_scale(100.f) == 0) && ok;

  // --- Dome open / drop ---
  const ThreeOS_Pose head = identity_pose_at(0.f, 1.6f, 0.f);
  if (threeos_topology_open_dome(&head) != 0) {
    std::fprintf(stderr, "FAIL open_dome: %s\n", threeos_last_error());
    ok = false;
  }
  ThreeOS_DomeState dome{};
  if (threeos_topology_get_dome(&dome) != 0 || dome.active == 0) {
    std::fprintf(stderr, "FAIL get_dome active\n");
    ok = false;
  } else {
    std::printf("OK   dome active scale=%.1f radius=%.2f\n", dome.scale_ratio, dome.radius_m);
    ok = expect_near("dome.scale", dome.scale_ratio, 100.f, 1e-3f) && ok;
  }

  ThreeOS_Pose local = identity_pose_at(0.f, 0.f, -0.1f);  // 0.1m proxy → 10m far at 1:100
  ThreeOS_Pose far_drop{};
  if (threeos_topology_dome_drop(&local, &far_drop) != 0) {
    std::fprintf(stderr, "FAIL dome_drop: %s\n", threeos_last_error());
    ok = false;
  } else {
    // Dome is ahead of head (-Z); local -0.1 * 100 = -10 along dome forward
    ok = expect_near("dome_drop.z", far_drop.position.z, dome.pose.position.z - 10.f, 0.05f) &&
         ok;
  }

  // --- Double-proxy remaps + portals ---
  ThreeOS_Aabb far_aabb{};
  far_aabb.min = {-1.f, 0.f, -6.f};
  far_aabb.max = {1.f, 2.f, -4.f};
  const uint64_t proxy_a = 1;
  if (threeos_topology_open_double_proxy(proxy_a, &far_aabb, &head) != 0) {
    std::fprintf(stderr, "FAIL open_double_proxy A: %s\n", threeos_last_error());
    ok = false;
  }

  ThreeOS_DoubleProxyState proxy{};
  if (threeos_topology_get_double_proxy(proxy_a, &proxy) != 0 || proxy.active == 0) {
    std::fprintf(stderr, "FAIL get_double_proxy\n");
    ok = false;
  } else {
    std::printf("OK   proxy A scale=%.3f\n", proxy.scale_ratio);
  }

  // Center of far → center of near
  ThreeOS_Vec3 far_center{0.f, 1.f, -5.f};
  ThreeOS_Vec3 near_pt{};
  if (threeos_topology_far_to_near(proxy_a, &far_center, &near_pt) != 0) {
    std::fprintf(stderr, "FAIL far_to_near\n");
    ok = false;
  } else {
    const float ncx = 0.5f * (proxy.near_aabb.min.x + proxy.near_aabb.max.x);
    const float ncy = 0.5f * (proxy.near_aabb.min.y + proxy.near_aabb.max.y);
    const float ncz = 0.5f * (proxy.near_aabb.min.z + proxy.near_aabb.max.z);
    ok = expect_near("far_to_near.x", near_pt.x, ncx, 1e-3f) && ok;
    ok = expect_near("far_to_near.y", near_pt.y, ncy, 1e-3f) && ok;
    ok = expect_near("far_to_near.z", near_pt.z, ncz, 1e-3f) && ok;
  }

  ThreeOS_Pose near_pose = identity_pose_at(near_pt.x, near_pt.y, near_pt.z);
  ThreeOS_Pose teleported{};
  if (threeos_topology_portal_near_to_far(proxy_a, &near_pose, &teleported) != 0) {
    std::fprintf(stderr, "FAIL portal_near_to_far: %s\n", threeos_last_error());
    ok = false;
  } else {
    ok = expect_near("portal.n2f.x", teleported.position.x, 0.f, 1e-3f) && ok;
    ok = expect_near("portal.n2f.y", teleported.position.y, 1.f, 1e-3f) && ok;
    ok = expect_near("portal.n2f.z", teleported.position.z, -5.f, 1e-3f) && ok;
  }

  ThreeOS_Pose back_near{};
  if (threeos_topology_portal_far_to_near(proxy_a, &teleported, &back_near) != 0) {
    std::fprintf(stderr, "FAIL portal_far_to_near: %s\n", threeos_last_error());
    ok = false;
  } else {
    ok = expect_near("portal.f2n.x", back_near.position.x, near_pt.x, 1e-3f) && ok;
  }

  // Cross-proxy A → B
  ThreeOS_Aabb far_b{};
  far_b.min = {3.f, 0.f, -6.f};
  far_b.max = {5.f, 2.f, -4.f};
  const uint64_t proxy_b = 2;
  if (threeos_topology_open_double_proxy(proxy_b, &far_b, &head) != 0) {
    std::fprintf(stderr, "FAIL open_double_proxy B\n");
    ok = false;
  }
  ThreeOS_DoubleProxyState proxy_b_state{};
  threeos_topology_get_double_proxy(proxy_b, &proxy_b_state);
  ThreeOS_Pose drop_in_b = identity_pose_at(
      0.5f * (proxy_b_state.near_aabb.min.x + proxy_b_state.near_aabb.max.x),
      0.5f * (proxy_b_state.near_aabb.min.y + proxy_b_state.near_aabb.max.y),
      0.5f * (proxy_b_state.near_aabb.min.z + proxy_b_state.near_aabb.max.z));
  ThreeOS_Pose cross_far{};
  if (threeos_topology_portal_cross(proxy_a, proxy_b, &drop_in_b, &cross_far) != 0) {
    std::fprintf(stderr, "FAIL portal_cross: %s\n", threeos_last_error());
    ok = false;
  } else {
    ok = expect_near("cross.x", cross_far.position.x, 4.f, 1e-3f) && ok;
    ok = expect_near("cross.z", cross_far.position.z, -5.f, 1e-3f) && ok;
  }

  // select_dome_region
  if (threeos_topology_select_dome_region(3, ThreeOS_VoxelCoord{-10, 0, -200},
                                          ThreeOS_VoxelCoord{10, 40, -160}, &head) != 0) {
    std::fprintf(stderr, "FAIL select_dome_region: %s\n", threeos_last_error());
    ok = false;
  } else {
    std::printf("OK   select_dome_region opened proxy 3\n");
  }

  // Tick still works; pending portal may surface on next tick
  ThreeOS_InteropInputFrame frame{};
  frame.frame_index = 1;
  frame.time_seconds = 0.016;
  frame.tracking_flags = THREEOS_TRACK_HEAD;
  frame.head = head;
  ThreeOS_InteropTransformDelta delta{};
  if (threeos_tick(&frame, &delta) != 0) {
    std::fprintf(stderr, "FAIL tick: %s\n", threeos_last_error());
    ok = false;
  } else {
    std::printf("OK   tick entity_id=%llu flags=0x%X\n",
                static_cast<unsigned long long>(delta.entity_id), delta.flags);
  }

  threeos_shutdown();
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
