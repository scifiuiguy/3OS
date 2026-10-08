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
  ok = expect_eq("minor", minor, 3) && ok;
  ok = expect_eq("patch", patch, 0) && ok;
  ok = expect_eq("abi_version", threeos_abi_version(), 3) && ok;
  ok = expect_eq("sizeof_input_frame", threeos_sizeof_input_frame(), 192) && ok;
  ok = expect_eq("sizeof_transform_delta", threeos_sizeof_transform_delta(), 48) && ok;
  ok = expect_eq("sizeof_dome_state", threeos_sizeof_dome_state(), 48) && ok;
  ok = expect_eq("sizeof_double_proxy_state", threeos_sizeof_double_proxy_state(), 64) && ok;
  ok = expect_eq("sizeof_kinetic_state", threeos_sizeof_kinetic_state(), 48) && ok;
  ok = expect_eq("sizeof_stick_debug", threeos_sizeof_stick_debug(), 64) && ok;

  if (threeos_tick(nullptr, nullptr) != -1) {
    std::fprintf(stderr, "FAIL tick before init should return -1\n");
    ok = false;
  }

  if (threeos_init() != 0) {
    std::fprintf(stderr, "FAIL threeos_init: %s\n", threeos_last_error());
    return EXIT_FAILURE;
  }

  // --- Voxel indexing (~3 inch = 0.0762 m) ---
  ThreeOS_Vec3 world{0.10f, 0.0f, -0.10f};
  ThreeOS_VoxelCoord vc{};
  if (threeos_topology_world_to_voxel(&world, &vc) != 0) {
    std::fprintf(stderr, "FAIL world_to_voxel: %s\n", threeos_last_error());
    ok = false;
  } else {
    // floor(0.10/0.0762) = 1; floor(-0.10/0.0762) = -2
    if (vc.x != 1) {
      std::fprintf(stderr, "FAIL voxel.x: got %d expected 1\n", vc.x);
      ok = false;
    } else {
      std::printf("OK   voxel.x = %d\n", vc.x);
    }
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
    ok = expect_near("voxel0.center.x", back.x, 0.0381f, 1e-4f) && ok;
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

  // --- Kinematics: grab-anchored stick / deadzone / coast / floor clamp ---
  // gain=10 (1/s), deadzone=1cm, friction=4
  if (threeos_kinematics_set_params(10.f, 0.01f, 4.f, 0.f) != 0) {
    std::fprintf(stderr, "FAIL kinematics_set_params: %s\n", threeos_last_error());
    ok = false;
  }

  ThreeOS_Pose obj = identity_pose_at(0.f, 1.0f, -1.f);
  const uint64_t entity = 1001;
  if (threeos_kinematics_possess(entity, &obj) != 0) {
    std::fprintf(stderr, "FAIL possess: %s\n", threeos_last_error());
    ok = false;
  }

  ThreeOS_KineticState ks{};
  threeos_kinematics_get_state(&ks);
  ok = expect_eq("kinetic.phase.possessed", ks.phase, 1) && ok;

  ThreeOS_InteropInputFrame kf{};
  kf.tracking_flags = THREEOS_TRACK_RIGHT_AIM;
  // Freeze start_hand
  kf.right_aim = identity_pose_at(0.f, 1.0f, -0.5f);
  kf.time_seconds = 1.0;
  threeos_tick(&kf, &delta);

  // Inside 1cm deadzone: no drive
  kf.frame_index = 2;
  kf.time_seconds += 1.0 / 72.0;
  kf.right_aim = identity_pose_at(0.005f, 1.0f, -0.5f);
  threeos_tick(&kf, &delta);
  threeos_kinematics_get_state(&ks);
  if (std::fabs(ks.vel_x) > 1e-3f) {
    std::fprintf(stderr, "FAIL deadzone leaked vel_x=%f\n", ks.vel_x);
    ok = false;
  } else {
    std::printf("OK   deadzone vel_x≈%f\n", ks.vel_x);
  }
  const float x_after_deadzone = delta.pose.position.x;

  // Stick 5cm to +X → v = 10 * 0.05 = 0.5 m/s; object advances from current pose
  kf.frame_index = 3;
  kf.time_seconds += 1.0 / 72.0;
  kf.right_aim = identity_pose_at(0.05f, 1.0f, -0.5f);
  threeos_tick(&kf, &delta);
  if (delta.entity_id != entity || (delta.flags & THREEOS_XFORM_KINEMATIC) == 0) {
    std::fprintf(stderr, "FAIL kinematic delta missing\n");
    ok = false;
  } else if (delta.pose.position.x <= x_after_deadzone) {
    std::fprintf(stderr, "FAIL expected +X glide from current, x=%f\n", delta.pose.position.x);
    ok = false;
  } else {
    std::printf("OK   stick glide x=%f (from %f)\n", delta.pose.position.x, x_after_deadzone);
  }
  threeos_kinematics_get_state(&ks);
  ok = expect_near("stick.vel_x", ks.vel_x, 0.5f, 1e-3f) && ok;

  // Stick debug ray: origin = start_hand + flat_forward*(-Z)*2", tip = origin + stick
  ThreeOS_StickDebug sd{};
  if (threeos_kinematics_get_stick_debug(&sd) != 0 || sd.active == 0) {
    std::fprintf(stderr, "FAIL stick_debug inactive\n");
    ok = false;
  } else {
    const float off = 2.f * 0.0254f;
    ok = expect_near("stick_debug.mag", sd.magnitude, 0.05f, 1e-4f) && ok;
    ok = expect_near("stick_debug.stick_x", sd.stick.x, 0.05f, 1e-4f) && ok;
    // identity head → flat forward (0,0,-1); start_hand at (0,1,-0.5)
    ok = expect_near("stick_debug.origin_z", sd.ray_origin.z, -0.5f - off, 1e-4f) && ok;
    ok = expect_near("stick_debug.tip_x", sd.ray_tip.x, 0.05f, 1e-4f) && ok;
    ok = expect_eq("stick_debug.in_deadzone", sd.in_deadzone, 0) && ok;
  }

  // Return hand to start → dampen toward halt
  for (int i = 0; i < 40; ++i) {
    kf.frame_index++;
    kf.time_seconds += 1.0 / 72.0;
    kf.right_aim = identity_pose_at(0.f, 1.0f, -0.5f);
    threeos_tick(&kf, &delta);
  }
  threeos_kinematics_get_state(&ks);
  if (std::fabs(ks.vel_x) > 0.05f) {
    std::fprintf(stderr, "FAIL return-to-origin did not dampen vel_x=%f\n", ks.vel_x);
    ok = false;
  } else {
    std::printf("OK   return-to-origin dampened vel_x≈%f\n", ks.vel_x);
  }
  const float x_before_reverse = delta.pose.position.x;

  // Reverse stick to -X from current object pose (must not snap to possess start)
  kf.frame_index++;
  kf.time_seconds += 1.0 / 72.0;
  kf.right_aim = identity_pose_at(-0.05f, 1.0f, -0.5f);
  threeos_tick(&kf, &delta);
  if (delta.pose.position.x >= x_before_reverse) {
    std::fprintf(stderr, "FAIL reverse should continue from current x (%f -> %f)\n",
                 x_before_reverse, delta.pose.position.x);
    ok = false;
  } else if (std::fabs(delta.pose.position.x - obj.position.x) < 1e-4f) {
    std::fprintf(stderr, "FAIL snapped back to possess start\n");
    ok = false;
  } else {
    std::printf("OK   reverse from current x=%f (possess start was %f)\n", delta.pose.position.x,
                obj.position.x);
  }

  // ~1 ft stick → high speed (~3 m/s at gain 10)
  kf.frame_index++;
  kf.time_seconds += 1.0 / 72.0;
  kf.right_aim = identity_pose_at(0.30f, 1.0f, -0.5f);
  threeos_tick(&kf, &delta);
  threeos_kinematics_get_state(&ks);
  ok = expect_near("one_foot.vel_x", ks.vel_x, 3.0f, 1e-3f) && ok;

  // Release → coast with friction decay
  threeos_kinematics_release();
  threeos_kinematics_get_state(&ks);
  ok = expect_eq("kinetic.phase.coasting", ks.phase, 2) && ok;
  float prev_speed = std::sqrt(ks.vel_x * ks.vel_x + ks.vel_y * ks.vel_y + ks.vel_z * ks.vel_z);
  for (int i = 0; i < 30; ++i) {
    kf.frame_index = static_cast<uint64_t>(100 + i);
    kf.time_seconds += 1.0 / 72.0;
    threeos_tick(&kf, &delta);
  }
  threeos_kinematics_get_state(&ks);
  const float speed = std::sqrt(ks.vel_x * ks.vel_x + ks.vel_y * ks.vel_y + ks.vel_z * ks.vel_z);
  if (!(speed < prev_speed) && ks.phase != 0) {
    std::fprintf(stderr, "FAIL friction did not reduce speed (%f -> %f)\n", prev_speed, speed);
    ok = false;
  } else {
    std::printf("OK   friction settle speed %f -> %f phase=%u\n", prev_speed, speed, ks.phase);
  }

  // Floor clamp: stick downward through y=0
  obj = identity_pose_at(0.f, 0.5f, -1.f);
  threeos_kinematics_possess(entity, &obj);
  kf.right_aim = identity_pose_at(0.f, 0.5f, -0.5f);
  kf.time_seconds += 1.0 / 72.0;
  threeos_tick(&kf, &delta);  // freeze start
  for (int i = 0; i < 20; ++i) {
    kf.frame_index++;
    kf.time_seconds += 1.0 / 72.0;
    kf.right_aim = identity_pose_at(0.f, 0.5f - 0.25f, -0.5f);  // 25cm down stick
    threeos_tick(&kf, &delta);
  }
  if (delta.pose.position.y < 0.074f) {
    std::fprintf(stderr, "FAIL floor clamp: y=%f below radius\n", delta.pose.position.y);
    ok = false;
  } else {
    std::printf("OK   floor clamp y=%f flags=0x%X\n", delta.pose.position.y, delta.flags);
  }
  ok = ((delta.flags & THREEOS_XFORM_FLOOR_CLAMP) != 0) && ok;

  threeos_shutdown();
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
