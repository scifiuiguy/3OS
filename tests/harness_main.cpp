#include "interop/threeos_abi.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

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
  ok = expect_eq("minor", minor, 6) && ok;
  ok = expect_eq("patch", patch, 0) && ok;
  ok = expect_eq("abi_version", threeos_abi_version(), 6) && ok;
  ok = expect_eq("sizeof_input_frame", threeos_sizeof_input_frame(), 192) && ok;
  ok = expect_eq("sizeof_transform_delta", threeos_sizeof_transform_delta(), 48) && ok;
  ok = expect_eq("sizeof_dome_state", threeos_sizeof_dome_state(), 48) && ok;
  ok = expect_eq("sizeof_double_proxy_state", threeos_sizeof_double_proxy_state(), 64) && ok;
  ok = expect_eq("sizeof_kinetic_state", threeos_sizeof_kinetic_state(), 48) && ok;
  ok = expect_eq("sizeof_stick_debug", threeos_sizeof_stick_debug(), 64) && ok;
  ok = expect_eq("sizeof_workspace_item", threeos_sizeof_workspace_item(), 200) && ok;
  ok = expect_eq("sizeof_snap_state", threeos_sizeof_snap_state(), 64) && ok;
  ok = expect_eq("sizeof_storage_event", threeos_sizeof_storage_event(), 344) && ok;
  ok = expect_eq("sizeof_hand_sample", threeos_sizeof_hand_sample(), 128) && ok;
  ok = expect_eq("sizeof_point_cursor", threeos_sizeof_point_cursor(), 32) && ok;
  ok = expect_eq("sizeof_interaction_intents", threeos_sizeof_interaction_intents(), 16) && ok;
  ok = expect_eq("sizeof_voodoo_field_params", threeos_sizeof_voodoo_field_params(), 32) && ok;
  ok = expect_eq("sizeof_voodoo_bounding_box", threeos_sizeof_voodoo_bounding_box(), 80) && ok;
  ok = expect_eq("sizeof_voodoo_session", threeos_sizeof_voodoo_session(), 176) && ok;
  ok = expect_eq("sizeof_voodoo_drag_arrow", threeos_sizeof_voodoo_drag_arrow(), 64) && ok;
  ok = expect_eq("sizeof_voodoo_snap_node", threeos_sizeof_voodoo_snap_node(), 48) && ok;
  ok = expect_eq("sizeof_voodoo_portal", threeos_sizeof_voodoo_portal(), 200) && ok;
  ok = expect_eq("sizeof_voodoo_ghost", threeos_sizeof_voodoo_ghost(), 88) && ok;

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
  // gain=10 (1/s), deadzone=0.015 m, friction=4
  constexpr float kDeadzoneM = 0.015f;
  constexpr float kStickOutsideM = 0.10f;  // outside 0.015 m deadzone
  if (threeos_kinematics_set_params(10.f, kDeadzoneM, 4.f, 0.f) != 0) {
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

  // Inside 0.015 m deadzone: no drive
  kf.frame_index = 2;
  kf.time_seconds += 1.0 / 72.0;
  kf.right_aim = identity_pose_at(0.01f, 1.0f, -0.5f);
  threeos_tick(&kf, &delta);
  threeos_kinematics_get_state(&ks);
  if (std::fabs(ks.vel_x) > 1e-3f) {
    std::fprintf(stderr, "FAIL deadzone leaked vel_x=%f\n", ks.vel_x);
    ok = false;
  } else {
    std::printf("OK   deadzone vel_x≈%f\n", ks.vel_x);
  }
  const float x_after_deadzone = delta.pose.position.x;

  // Stick ~4" to +X → remapped v = gain * (mag - deadzone); hold to accumulate.
  const float kStickEffVel = 10.f * (kStickOutsideM - kDeadzoneM);
  for (int i = 0; i < 12; ++i) {
    kf.frame_index++;
    kf.time_seconds += 1.0 / 72.0;
    kf.right_aim = identity_pose_at(kStickOutsideM, 1.0f, -0.5f);
    threeos_tick(&kf, &delta);
  }
  if (delta.entity_id != entity || (delta.flags & THREEOS_XFORM_KINEMATIC) == 0) {
    std::fprintf(stderr, "FAIL kinematic delta missing\n");
    ok = false;
  } else if (delta.pose.position.x <= x_after_deadzone + 0.01f) {
    std::fprintf(stderr, "FAIL expected +X glide from current, x=%f\n", delta.pose.position.x);
    ok = false;
  } else {
    std::printf("OK   stick glide x=%f (from %f)\n", delta.pose.position.x, x_after_deadzone);
  }
  threeos_kinematics_get_state(&ks);
  ok = expect_near("stick.vel_x", ks.vel_x, kStickEffVel, 1e-3f) && ok;

  // Stick debug ray: origin = start_hand - flat_forward*4" (away from body), tip = origin + stick
  ThreeOS_StickDebug sd{};
  if (threeos_kinematics_get_stick_debug(&sd) != 0 || sd.active == 0) {
    std::fprintf(stderr, "FAIL stick_debug inactive\n");
    ok = false;
  } else {
    const float stick_back = 4.f * 0.0254f;
    const float cursor_fwd = 7.f * 0.0254f;
    const float cursor_lat = 4.f * 0.0254f;
    ok = expect_near("stick_debug.mag", sd.magnitude, kStickOutsideM, 1e-4f) && ok;
    ok = expect_near("stick_debug.stick_x", sd.stick.x, kStickOutsideM, 1e-4f) && ok;
    // Phase 0.5: kinematics aim is the point-cursor probe (aim + 7" floor-forward + 2" up
    // when head invalid → +Z). Stick viz: origin = start_hand - flat_forward*4".
    const float cursor_z = -0.5f + cursor_fwd;
    ok = expect_near("stick_debug.origin_z", sd.ray_origin.z, cursor_z + stick_back, 1e-4f) && ok;
    // Right cursor includes +4" OpenXR lateral (Unity shows left); tip.x = start.x + stick.x.
    ok = expect_near("stick_debug.tip_x", sd.ray_tip.x, kStickOutsideM + cursor_lat, 1e-4f) && ok;
    ok = expect_eq("stick_debug.in_deadzone", sd.in_deadzone, 0) && ok;
    ok = expect_eq("stick_debug.armed_outside", sd.deadzone_armed, 0) && ok;
  }

  // Return hand to start → dampen toward halt (longer settle after ~1 m/s drive)
  for (int i = 0; i < 80; ++i) {
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
  kf.right_aim = identity_pose_at(-kStickOutsideM, 1.0f, -0.5f);
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

  // ~1 ft stick → remapped speed gain * (0.30 - deadzone)
  kf.frame_index++;
  kf.time_seconds += 1.0 / 72.0;
  kf.right_aim = identity_pose_at(0.30f, 1.0f, -0.5f);
  threeos_tick(&kf, &delta);
  threeos_kinematics_get_state(&ks);
  ok = expect_near("one_foot.vel_x", ks.vel_x, 10.f * (0.30f - kDeadzoneM), 1e-3f) && ok;

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

  // --- Storage / glyphs 0.4 ---
  auto load_pack = [&](const char* path) -> bool {
    FILE* f = nullptr;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "rb") != 0 || f == nullptr) {
      std::fprintf(stderr, "FAIL open pack %s\n", path);
      return false;
    }
#else
    f = std::fopen(path, "rb");
    if (f == nullptr) {
      std::fprintf(stderr, "FAIL open pack %s\n", path);
      return false;
    }
#endif
    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    if (std::fread(buf.data(), 1, buf.size(), f) != buf.size()) {
      std::fclose(f);
      std::fprintf(stderr, "FAIL read pack %s\n", path);
      return false;
    }
    std::fclose(f);
    if (threeos_glyph_install_pack(buf.data(), static_cast<uint32_t>(buf.size())) != 0) {
      std::fprintf(stderr, "FAIL install pack %s: %s\n", path, threeos_last_error());
      return false;
    }
    std::printf("OK   installed pack %s\n", path);
    return true;
  };

  threeos_storage_clear();
  ok = load_pack("assets/glyphs/gltf_mark.3glyph") && ok;
  ok = load_pack("assets/glyphs/box.3glyph") && ok;
  ok = load_pack("assets/glyphs/box_open.3glyph") && ok;

  uint64_t obj_id = 0;
  uint64_t box_id = 0;
  ThreeOS_Pose layout_head = identity_pose_at(0.f, 1.5f, 0.f);
  if (threeos_storage_add_object("simple.glb", "simple.glb", &obj_id) != 0 ||
      threeos_storage_add_box("Test1", "Test1", 0, &box_id) != 0 ||
      threeos_storage_layout_demo(&layout_head) != 0) {
    std::fprintf(stderr, "FAIL storage seed: %s\n", threeos_last_error());
    ok = false;
  }
  ok = expect_eq("storage.count", threeos_storage_item_count(), 2) && ok;

  ThreeOS_WorkspaceItem item{};
  // 2 items → 2×2×2 matrix centered 1 m ahead of head (OpenXR -Z).
  // Slot (0,0,0) is at local -extent on each axis; forward*(-extent) pushes +Z toward head.
  threeos_storage_get_item(0, &item);
  ok = expect_near("layout.obj.z", item.pose.position.z, -1.f + 0.11f, 0.08f) && ok;

  // Rolled head must not tilt glyphs off world-up (yaw-only layout).
  {
    threeos_storage_clear();
    uint64_t a = 0;
    uint64_t b = 0;
    ThreeOS_Pose rolled = identity_pose_at(0.f, 1.5f, 0.f);
    const float half = 0.4f;  // ~45° roll about forward-ish; nonzero x/z on quat
    rolled.orientation.x = std::sin(half);
    rolled.orientation.z = std::sin(half * 0.5f);
    rolled.orientation.w = std::cos(half);
    // Renormalize roughly
    const float qn = std::sqrt(rolled.orientation.x * rolled.orientation.x +
                               rolled.orientation.y * rolled.orientation.y +
                               rolled.orientation.z * rolled.orientation.z +
                               rolled.orientation.w * rolled.orientation.w);
    rolled.orientation.x /= qn;
    rolled.orientation.y /= qn;
    rolled.orientation.z /= qn;
    rolled.orientation.w /= qn;
    if (threeos_storage_add_object("a.glb", "a.glb", &a) != 0 ||
        threeos_storage_add_object("b.glb", "b.glb", &b) != 0 ||
        threeos_storage_layout_demo(&rolled) != 0) {
      std::fprintf(stderr, "FAIL yaw-layout seed: %s\n", threeos_last_error());
      ok = false;
    } else {
      ThreeOS_WorkspaceItem wa{};
      threeos_storage_get_item(0, &wa);
      if (std::fabs(wa.pose.orientation.x) > 1e-3f || std::fabs(wa.pose.orientation.z) > 1e-3f) {
        std::fprintf(stderr, "FAIL layout kept head roll (qx=%f qz=%f)\n",
                     wa.pose.orientation.x, wa.pose.orientation.z);
        ok = false;
      } else {
        std::printf("OK   layout yaw-only under rolled head (qy=%f)\n", wa.pose.orientation.y);
      }
    }
    // Restore the main storage seed for the rest of the suite.
    threeos_storage_clear();
    if (threeos_storage_add_object("simple.glb", "simple.glb", &obj_id) != 0 ||
        threeos_storage_add_box("Test1", "Test1", 0, &box_id) != 0 ||
        threeos_storage_layout_demo(&layout_head) != 0) {
      std::fprintf(stderr, "FAIL storage re-seed after yaw test: %s\n", threeos_last_error());
      ok = false;
    }
  }
  threeos_storage_get_item(0, &item);
  if (std::strcmp(item.glyph_id, "gltf_mark") != 0) {
    std::fprintf(stderr, "FAIL object glyph_id=%s expected gltf_mark\n", item.glyph_id);
    ok = false;
  } else {
    std::printf("OK   object glyph_id=%s tint_a=%f\n", item.glyph_id, item.tint_a);
  }
  threeos_storage_get_item(1, &item);
  if (std::strcmp(item.name, "Test1") != 0 || std::strcmp(item.secondary_label, "0 objects") != 0) {
    std::fprintf(stderr, "FAIL box labels name=%s sec=%s\n", item.name, item.secondary_label);
    ok = false;
  } else {
    std::printf("OK   box label Test1 / 0 objects glyph=%s\n", item.glyph_id);
  }

  // Selection ABI
  uint64_t sel = 1;
  threeos_selection_get(&sel);
  if (sel != 0) {
    std::fprintf(stderr, "FAIL selection default nonzero %llu\n",
                 static_cast<unsigned long long>(sel));
    ok = false;
  }
  if (threeos_selection_set(obj_id) != 0) {
    std::fprintf(stderr, "FAIL selection_set obj: %s\n", threeos_last_error());
    ok = false;
  }
  threeos_selection_get(&sel);
  if (sel != obj_id) {
    std::fprintf(stderr, "FAIL selection_get after set\n");
    ok = false;
  } else {
    std::printf("OK   selection_set/get obj=%llu\n", static_cast<unsigned long long>(sel));
  }
  if (threeos_selection_set(999999ull) == 0) {
    std::fprintf(stderr, "FAIL selection_set should reject unknown id\n");
    ok = false;
  } else {
    std::printf("OK   selection_set rejects unknown id\n");
  }
  threeos_selection_clear();
  threeos_selection_get(&sel);
  if (sel != 0) {
    std::fprintf(stderr, "FAIL selection_clear\n");
    ok = false;
  } else {
    std::printf("OK   selection_clear\n");
  }
  threeos_selection_set(obj_id);  // re-select; insert should clear

  // Snap: possess object, move near box
  ThreeOS_Pose obj_pose = identity_pose_at(item.pose.position.x, item.pose.position.y,
                                           item.pose.position.z);
  threeos_storage_get_item(0, &item);
  obj_pose = item.pose;
  threeos_kinematics_possess(obj_id, &obj_pose);
  ThreeOS_InteropInputFrame sf{};
  sf.tracking_flags = THREEOS_TRACK_RIGHT_AIM | THREEOS_TRACK_HEAD;
  sf.right_aim = obj_pose;
  sf.head = identity_pose_at(0.f, 1.5f, 0.f);
  sf.time_seconds = 10.0;
  threeos_tick(&sf, &delta);  // freeze start

  threeos_storage_get_item(1, &item);
  const ThreeOS_Pose box_pose = item.pose;
  // Drive toward box over several frames
  for (int i = 0; i < 20; ++i) {
    sf.frame_index++;
    sf.time_seconds += 1.0 / 72.0;
    const float t = (i + 1) / 20.f;
    sf.right_aim.position.x = obj_pose.position.x + (box_pose.position.x - obj_pose.position.x) * t;
    sf.right_aim.position.y = obj_pose.position.y + (box_pose.position.y - obj_pose.position.y) * t;
    sf.right_aim.position.z = obj_pose.position.z + (box_pose.position.z - obj_pose.position.z) * t;
    threeos_tick(&sf, &delta);
  }
  ThreeOS_SnapState snap{};
  threeos_storage_get_snap(&snap);
  if (snap.pending == 0) {
    std::fprintf(stderr, "FAIL expected snap pending near box\n");
    ok = false;
  } else {
    std::printf("OK   snap pending object=%llu box=%llu\n",
                static_cast<unsigned long long>(snap.object_id),
                static_cast<unsigned long long>(snap.box_id));
  }

  // Escape snap: keep possess, drive stick far so escape pose leaves 0.2 m.
  // Hold a steady far aim so stick-built velocity persists after catch-up.
  ThreeOS_Pose escape_aim = box_pose;
  escape_aim.position.x += 0.35f;
  escape_aim.position.y += 0.35f;
  escape_aim.position.z += 0.35f;
  for (int i = 0; i < 60; ++i) {
    sf.frame_index++;
    sf.time_seconds += 1.0 / 72.0;
    sf.right_aim = escape_aim;
    threeos_tick(&sf, &delta);
  }
  threeos_storage_get_snap(&snap);
  if (snap.pending != 0) {
    std::fprintf(stderr, "FAIL expected snap exit after stick escape\n");
    ok = false;
  } else {
    std::printf("OK   snap exit after stick escape\n");
  }

  // Catch-up (~0.12 s) must land on stick velocity — not reanchor/zero it.
  for (int i = 0; i < 20; ++i) {
    sf.frame_index++;
    sf.time_seconds += 1.0 / 72.0;
    sf.right_aim = escape_aim;
    threeos_tick(&sf, &delta);
  }
  {
    ThreeOS_KineticState ks{};
    threeos_kinematics_get_state(&ks);
    const float speed =
        std::sqrt(ks.vel_x * ks.vel_x + ks.vel_y * ks.vel_y + ks.vel_z * ks.vel_z);
    if (speed < 0.05f) {
      std::fprintf(stderr, "FAIL expected non-zero velocity after snap escape catch-up (speed=%f)\n",
                   speed);
      ok = false;
    } else {
      std::printf("OK   snap escape preserves velocity (speed=%f)\n", speed);
    }
  }

  // Re-enter snap for commit: release + re-possess near the box (escape leaves
  // the object too far for a short stick re-drive).
  threeos_kinematics_release();
  for (int i = 0; i < 10; ++i) {
    sf.frame_index++;
    sf.time_seconds += 1.0 / 72.0;
    threeos_tick(&sf, &delta);
  }
  ThreeOS_Pose near_box = box_pose;
  near_box.position.y += 0.10f;
  threeos_kinematics_possess(obj_id, &near_box);
  sf.right_aim = near_box;
  threeos_tick(&sf, &delta);  // freeze start_hand
  for (int i = 0; i < 12; ++i) {
    sf.frame_index++;
    sf.time_seconds += 1.0 / 72.0;
    sf.right_aim = near_box;
    threeos_tick(&sf, &delta);
  }
  threeos_storage_get_snap(&snap);
  if (snap.pending == 0) {
    std::fprintf(stderr, "FAIL expected snap re-enter before commit\n");
    ok = false;
  } else {
    std::printf("OK   snap re-enter before commit\n");
  }

  // Release while snapped → FS event
  threeos_kinematics_release();
  ThreeOS_StorageEvent ev{};
  threeos_storage_poll_event(&ev);
  if (ev.type != 1 || ev.awaiting_ack == 0) {
    std::fprintf(stderr, "FAIL expected ObjectMovedIntoBox event\n");
    ok = false;
  } else {
    std::printf("OK   storage event %s -> %s\n", ev.source_rel, ev.dest_rel);
  }
  threeos_storage_ack_event(1);
  threeos_storage_get_item(1, &item);
  ok = expect_eq("box.child_count", item.child_count, 1) && ok;
  if (std::strcmp(item.secondary_label, "1 object") != 0) {
    std::fprintf(stderr, "FAIL secondary=%s\n", item.secondary_label);
    ok = false;
  } else {
    std::printf("OK   box secondary=%s\n", item.secondary_label);
  }
  threeos_selection_get(&sel);
  if (sel != 0) {
    std::fprintf(stderr, "FAIL selection should clear on insert, got %llu\n",
                 static_cast<unsigned long long>(sel));
    ok = false;
  } else {
    std::printf("OK   selection cleared on insert\n");
  }

  // Sticky snap: hold above a dense vertical stack must not cascade to the top.
  {
    threeos_storage_clear();
    uint64_t bot = 0;
    uint64_t mid = 0;
    uint64_t top = 0;
    uint64_t sticky_obj = 0;
    ThreeOS_Pose head = identity_pose_at(0.f, 1.5f, 0.f);
    const float spacing = 0.22f;
    const float x = 0.f;
    const float z = -1.f;
    const float y0 = 1.0f;
    if (threeos_storage_add_box("Bot", "Bot", 0, &bot) != 0 ||
        threeos_storage_add_box("Mid", "Mid", 0, &mid) != 0 ||
        threeos_storage_add_box("Top", "Top", 0, &top) != 0 ||
        threeos_storage_add_object("sticky.glb", "sticky.glb", &sticky_obj) != 0) {
      std::fprintf(stderr, "FAIL sticky-snap seed: %s\n", threeos_last_error());
      ok = false;
    } else {
      auto park = [&](uint64_t id, float y, double* t) {
        ThreeOS_Pose p = identity_pose_at(x, y, z);
        threeos_kinematics_possess(id, &p);
        ThreeOS_InteropInputFrame f{};
        f.tracking_flags = THREEOS_TRACK_RIGHT_AIM | THREEOS_TRACK_HEAD;
        f.right_aim = p;
        f.head = head;
        f.time_seconds = *t;
        threeos_tick(&f, &delta);
        *t += 1.0 / 72.0;
        threeos_kinematics_release();
        for (int i = 0; i < 4; ++i) {
          f.frame_index++;
          f.time_seconds = *t;
          *t += 1.0 / 72.0;
          threeos_tick(&f, &delta);
        }
      };
      double t = 50.0;
      park(bot, y0, &t);
      park(mid, y0 + spacing, &t);
      park(top, y0 + 2.f * spacing, &t);

      // Approach from just below the bottom box so first snap is Bot.
      ThreeOS_Pose approach = identity_pose_at(x, y0 + 0.05f, z);
      threeos_kinematics_possess(sticky_obj, &approach);
      ThreeOS_InteropInputFrame sf2{};
      sf2.tracking_flags = THREEOS_TRACK_RIGHT_AIM | THREEOS_TRACK_HEAD;
      sf2.right_aim = approach;
      sf2.head = head;
      sf2.time_seconds = t;
      threeos_tick(&sf2, &delta);
      t += 1.0 / 72.0;
      for (int i = 0; i < 8; ++i) {
        sf2.frame_index++;
        sf2.time_seconds = t;
        t += 1.0 / 72.0;
        sf2.right_aim = approach;
        threeos_tick(&sf2, &delta);
      }
      ThreeOS_SnapState sticky_snap{};
      threeos_storage_get_snap(&sticky_snap);
      if (sticky_snap.pending == 0 || sticky_snap.box_id != bot) {
        std::fprintf(stderr,
                     "FAIL sticky snap expected Bot first (pending=%u box=%llu)\n",
                     sticky_snap.pending,
                     static_cast<unsigned long long>(sticky_snap.box_id));
        ok = false;
      } else {
        // Hold pose sits nearer Mid than Bot; without stickiness it would cascade.
        for (int i = 0; i < 24; ++i) {
          sf2.frame_index++;
          sf2.time_seconds = t;
          t += 1.0 / 72.0;
          sf2.right_aim = approach;
          threeos_tick(&sf2, &delta);
        }
        threeos_storage_get_snap(&sticky_snap);
        if (sticky_snap.pending == 0 || sticky_snap.box_id != bot) {
          std::fprintf(stderr,
                       "FAIL sticky snap cascaded away from Bot (pending=%u box=%llu)\n",
                       sticky_snap.pending,
                       static_cast<unsigned long long>(sticky_snap.box_id));
          ok = false;
        } else {
          std::printf("OK   sticky snap stays on Bot in dense vertical stack\n");
        }

        // Escape toward Mid. As soon as Bot releases, the next ~catch-up window
        // must stay clear even though the free pose is inside Mid's radius.
        ThreeOS_Pose escape_aim = identity_pose_at(x, y0 + spacing, z);
        bool escaped = false;
        for (int i = 0; i < 90; ++i) {
          sf2.frame_index++;
          sf2.time_seconds = t;
          t += 1.0 / 72.0;
          sf2.right_aim = escape_aim;
          threeos_tick(&sf2, &delta);
          threeos_storage_get_snap(&sticky_snap);
          if (sticky_snap.pending == 0) {
            escaped = true;
            break;
          }
        }
        if (!escaped) {
          std::fprintf(stderr, "FAIL sticky snap never left Bot during escape drive\n");
          ok = false;
        } else {
          // Catch-up is ~0.12 s ≈ 9 ticks; stay inside that window.
          bool sucked = false;
          for (int i = 0; i < 8; ++i) {
            sf2.frame_index++;
            sf2.time_seconds = t;
            t += 1.0 / 72.0;
            sf2.right_aim = escape_aim;
            threeos_tick(&sf2, &delta);
            threeos_storage_get_snap(&sticky_snap);
            if (sticky_snap.pending != 0) {
              sucked = true;
              break;
            }
          }
          if (sucked) {
            std::fprintf(stderr,
                         "FAIL snap re-engaged during catch-up (box=%llu)\n",
                         static_cast<unsigned long long>(sticky_snap.box_id));
            ok = false;
          } else {
            std::printf("OK   snap suppressed during unsnap catch-up near Mid\n");
          }
        }
      }
      threeos_kinematics_release();
      for (int i = 0; i < 4; ++i) {
        sf2.frame_index++;
        sf2.time_seconds = t;
        t += 1.0 / 72.0;
        threeos_tick(&sf2, &delta);
      }
    }
  }

  // Expand / collapse: children above box; lift when default layer is occupied.
  {
    threeos_storage_clear();
    uint64_t parent = 0;
    uint64_t child_a = 0;
    uint64_t child_b = 0;
    uint64_t blocker = 0;
    ThreeOS_Pose head = identity_pose_at(0.f, 1.5f, 0.f);
    if (threeos_storage_add_box("Parent", "Parent", 0, &parent) != 0 ||
        threeos_storage_add_object("a.txt", "Parent/a.txt", &child_a) != 0 ||
        threeos_storage_add_object("b.txt", "Parent/b.txt", &child_b) != 0 ||
        threeos_storage_add_object("blocker.glb", "blocker.glb", &blocker) != 0 ||
        threeos_storage_set_parent_box(child_a, parent) != 0 ||
        threeos_storage_set_parent_box(child_b, parent) != 0 ||
        threeos_storage_set_in_field(child_a, 0) != 0 ||
        threeos_storage_set_in_field(child_b, 0) != 0 ||
        threeos_storage_layout_demo(&head) != 0) {
      std::fprintf(stderr, "FAIL expand seed: %s\n", threeos_last_error());
      ok = false;
    } else {
      ThreeOS_WorkspaceItem parent_item{};
      for (uint32_t i = 0; i < threeos_storage_item_count(); ++i) {
        ThreeOS_WorkspaceItem it{};
        threeos_storage_get_item(i, &it);
        if (it.entity_id == parent) {
          parent_item = it;
          break;
        }
      }

      // Occupy the default expand cell (one spacing above parent) to force a lift.
      ThreeOS_Pose blocked = parent_item.pose;
      blocked.position.y += 0.22f;
      threeos_kinematics_possess(blocker, &blocked);
      {
        ThreeOS_InteropInputFrame bf{};
        bf.tracking_flags = THREEOS_TRACK_RIGHT_AIM | THREEOS_TRACK_HEAD;
        bf.right_aim = blocked;
        bf.head = head;
        bf.time_seconds = 40.0;
        threeos_tick(&bf, &delta);
        threeos_kinematics_set_object_pose(&blocked);
        bf.frame_index++;
        bf.time_seconds += 1.0 / 72.0;
        bf.right_aim = blocked;
        threeos_tick(&bf, &delta);
      }
      threeos_kinematics_release();

      if (threeos_storage_expand_box(parent) != 0) {
        std::fprintf(stderr, "FAIL expand_box: %s\n", threeos_last_error());
        ok = false;
      } else {
        float child_min_y = 1e9f;
        float closest_blocker = 1e9f;
        bool children_in_field = true;
        bool parent_expanded = false;
        for (uint32_t i = 0; i < threeos_storage_item_count(); ++i) {
          ThreeOS_WorkspaceItem it{};
          threeos_storage_get_item(i, &it);
          if (it.entity_id == parent) {
            parent_expanded = (it.flags & 4u) != 0;
          }
          if (it.entity_id == child_a || it.entity_id == child_b) {
            if ((it.flags & 1u) == 0) {
              children_in_field = false;
            }
            child_min_y = std::min(child_min_y, it.pose.position.y);
            const float dx = it.pose.position.x - blocked.position.x;
            const float dy = it.pose.position.y - blocked.position.y;
            const float dz = it.pose.position.z - blocked.position.z;
            closest_blocker = std::min(closest_blocker, std::sqrt(dx * dx + dy * dy + dz * dz));
          }
        }

        if (!children_in_field || !parent_expanded) {
          std::fprintf(stderr, "FAIL expand flags in_field/expanded\n");
          ok = false;
        } else if (child_min_y < parent_item.pose.position.y + 0.15f) {
          std::fprintf(stderr, "FAIL expand not above parent (ymin=%f parent=%f)\n",
                       child_min_y, parent_item.pose.position.y);
          ok = false;
        } else if (closest_blocker < 0.11f) {
          std::fprintf(stderr, "FAIL expand overlapped blocker (d=%f)\n", closest_blocker);
          ok = false;
        } else {
          std::printf("OK   expand above parent, avoided blocker (d=%f)\n", closest_blocker);
        }

        if (threeos_storage_collapse_box(parent) != 0) {
          std::fprintf(stderr, "FAIL collapse_box: %s\n", threeos_last_error());
          ok = false;
        } else {
          bool any_child_field = false;
          parent_expanded = false;
          for (uint32_t i = 0; i < threeos_storage_item_count(); ++i) {
            ThreeOS_WorkspaceItem it{};
            threeos_storage_get_item(i, &it);
            if ((it.entity_id == child_a || it.entity_id == child_b) && (it.flags & 1u) != 0) {
              any_child_field = true;
            }
            if (it.entity_id == parent && (it.flags & 4u) != 0) {
              parent_expanded = true;
            }
          }
          if (any_child_field || parent_expanded) {
            std::fprintf(stderr, "FAIL collapse left children/parent expanded\n");
            ok = false;
          } else {
            std::printf("OK   collapse clears in_field + expanded\n");
          }
        }
      }
    }
  }

  // --- Hand interaction / point cursor 0.5 ---
  {
    threeos_interaction_set_pinch_thresh_m(0.0635f);

    ThreeOS_HandSample L{};
    ThreeOS_HandSample R{};
    R.tracked = 1;
    R.tip_flags = 0x1Fu;  // thumb+index+middle+ring+index proximal
    R.thumb_tip = {0.f, 1.2f, -0.3f};
    R.index_tip = {0.02f, 1.2f, -0.3f};   // 2cm — inside thresh
    R.middle_tip = {0.05f, 1.2f, -0.3f};  // 5cm — inside
    R.ring_tip = {0.08f, 1.2f, -0.3f};    // 8cm — outside
    R.index_proximal = {0.f, 1.2f, -0.3f};  // cursor probe anchor
    R.palm_mode = THREEOS_PALM_DOWN;
    R.palm_mode_valid = 1;
    R.primary_value = 0.3f;
    if (threeos_interaction_set_hands(&L, &R) != 0) {
      std::fprintf(stderr, "FAIL set_hands: %s\n", threeos_last_error());
      ok = false;
    }

    ThreeOS_InteropInputFrame hf{};
    hf.frame_index = 9001;
    hf.time_seconds = 10.0;
    hf.tracking_flags = THREEOS_TRACK_HEAD | THREEOS_TRACK_RIGHT_AIM;
    hf.head = identity_pose_at(0.f, 1.6f, 0.f);
    // Head looking +Z on floor plane.
    hf.head.orientation = {0.f, 0.f, 0.f, 1.f};
    hf.right_aim = identity_pose_at(0.f, 1.2f, -0.3f);
    ThreeOS_InteropTransformDelta hd{};
    if (threeos_tick(&hf, &hd) != 0) {
      std::fprintf(stderr, "FAIL hand tick: %s\n", threeos_last_error());
      ok = false;
    }

    ThreeOS_PointCursor cur{};
    if (threeos_interaction_get_cursor(1, &cur) != 0 || cur.active == 0) {
      std::fprintf(stderr, "FAIL right cursor inactive\n");
      ok = false;
    } else {
      // Shortest in-threshold pair is index–thumb (0.02m).
      ok = expect_eq("cursor.driving_primary", cur.driving_channel, THREEOS_PINCH_PRIMARY) && ok;
      // Index proximal (0, 1.2, -0.3) + 0.1778 fwd (+Z) + 0.0762 up + 4" lateral (+right OpenXR).
      ok = expect_near("cursor.x", cur.position.x, 0.f + 0.1016f, 1e-3f) && ok;
      ok = expect_near("cursor.y", cur.position.y, 1.2f + 0.0762f, 1e-3f) && ok;
      ok = expect_near("cursor.z", cur.position.z, -0.3f + 0.1778f, 1e-3f) && ok;
      // scale ≈ 0.1 + 0.9*(0.02/0.0635) ≈ 0.383
      ok = expect_near("cursor.scale_approach", cur.scale, 0.1f + 0.9f * (0.02f / 0.0635f), 0.02f) &&
           ok;
      std::printf("OK   cursor probe + approach scale\n");
    }

    // Primary press edge → SelectPulse; palm down → VoodooSnap (not drag).
    R.primary_pressed = 1;
    threeos_interaction_set_hands(&L, &R);
    hf.frame_index++;
    hf.time_seconds += 0.016;
    threeos_tick(&hf, &hd);
    ThreeOS_InteractionIntents intents{};
    if (threeos_interaction_get_intents(&intents) != 0) {
      std::fprintf(stderr, "FAIL get_intents\n");
      ok = false;
    } else {
      const bool select = (intents.flags & THREEOS_INTENT_SELECT_PULSE) != 0;
      const bool drag = (intents.flags & THREEOS_INTENT_DRAG_ACTIVE) != 0;
      const bool snap = (intents.flags & THREEOS_INTENT_VOODOO_SNAP) != 0;
      if (!select || drag || !snap) {
        std::fprintf(stderr, "FAIL intents palm-down: flags=0x%X (want select+snap)\n",
                     intents.flags);
        ok = false;
      } else {
        std::printf("OK   select+snap intents (palm down)\n");
      }
    }

    // Palm sideways + primary hold → DragActive (no voodoo).
    // Release first: intent palm is latched at press edge for the whole hold.
    R.primary_pressed = 0;
    threeos_interaction_set_hands(&L, &R);
    hf.frame_index++;
    hf.time_seconds += 0.016;
    threeos_tick(&hf, &hd);
    R.palm_mode = THREEOS_PALM_SIDEWAYS;
    R.primary_pressed = 1;
    threeos_interaction_set_hands(&L, &R);
    hf.frame_index++;
    hf.time_seconds += 0.016;
    threeos_tick(&hf, &hd);
    threeos_interaction_get_intents(&intents);
    {
      const bool drag = (intents.flags & THREEOS_INTENT_DRAG_ACTIVE) != 0;
      const bool snap = (intents.flags & THREEOS_INTENT_VOODOO_SNAP) != 0;
      if (!drag || snap) {
        std::fprintf(stderr, "FAIL intents palm-sideways: flags=0x%X (want drag, no snap)\n",
                     intents.flags);
        ok = false;
      } else {
        std::printf("OK   drag intent (palm sideways)\n");
      }
    }

    // Mid-hold palm flip must not switch DragActive → VoodooSnap.
    R.palm_mode = THREEOS_PALM_DOWN;
    R.primary_pressed = 1;
    threeos_interaction_set_hands(&L, &R);
    hf.frame_index++;
    hf.time_seconds += 0.016;
    threeos_tick(&hf, &hd);
    threeos_interaction_get_intents(&intents);
    {
      const bool drag = (intents.flags & THREEOS_INTENT_DRAG_ACTIVE) != 0;
      const bool snap = (intents.flags & THREEOS_INTENT_VOODOO_SNAP) != 0;
      if (!drag || snap) {
        std::fprintf(stderr, "FAIL intents palm-latch: flags=0x%X (want drag, no snap)\n",
                     intents.flags);
        ok = false;
      } else {
        std::printf("OK   palm latched for hold (no mid-gesture voodoo)\n");
      }
    }

    // Palm up + primary hold → neither drag nor voodoo (Quest OS reserves the gesture).
    R.primary_pressed = 0;
    threeos_interaction_set_hands(&L, &R);
    hf.frame_index++;
    hf.time_seconds += 0.016;
    threeos_tick(&hf, &hd);
    R.palm_mode = THREEOS_PALM_UP;
    R.primary_pressed = 1;
    threeos_interaction_set_hands(&L, &R);
    hf.frame_index++;
    hf.time_seconds += 0.016;
    threeos_tick(&hf, &hd);
    threeos_interaction_get_intents(&intents);
    {
      const bool drag = (intents.flags & THREEOS_INTENT_DRAG_ACTIVE) != 0;
      const bool snap = (intents.flags & THREEOS_INTENT_VOODOO_SNAP) != 0;
      const bool freev = (intents.flags & THREEOS_INTENT_VOODOO_FREE) != 0;
      if (drag || snap || freev) {
        std::fprintf(stderr, "FAIL intents palm-up: flags=0x%X (want no drag/voodoo)\n",
                     intents.flags);
        ok = false;
      } else {
        std::printf("OK   palm-up unmapped (OS-reserved)\n");
      }
    }

    // Controller-style synth: trigger select + A/X ternary via frozen frame bits.
    ThreeOS_InteropInputFrame cf{};
    cf.frame_index = 9100;
    cf.time_seconds = 20.0;
    cf.tracking_flags = THREEOS_TRACK_HEAD | THREEOS_TRACK_RIGHT_AIM;
    cf.head = identity_pose_at(0.f, 1.6f, 0.f);
    cf.right_aim = identity_pose_at(0.2f, 1.1f, -0.4f);
    cf.right_trigger = 0.8f;
    cf.button_flags = THREEOS_BTN_RIGHT_SELECT | THREEOS_BTN_RIGHT_TERNARY;
    // Clear explicit hands so tick synthesizes from frame.
    ThreeOS_HandSample empty{};
    threeos_interaction_set_hands(&empty, &empty);
    // Next tick without set_hands → synth path. Drop prior push flag by not calling set_hands.
    // Actually set_hands marks pushed; call tick once after clearing via untracked samples that
    // still "pushed". Force synth by ticking without set_hands after a prior tick consumed push.
    threeos_tick(&cf, &hd);  // consumes pushed empty hands
    cf.frame_index++;
    cf.time_seconds += 0.016;
    threeos_tick(&cf, &hd);  // synth from frame
    threeos_interaction_get_cursor(1, &cur);
    threeos_interaction_get_intents(&intents);
    if (cur.active == 0) {
      std::fprintf(stderr, "FAIL controller synth cursor inactive\n");
      ok = false;
    } else if ((intents.flags & THREEOS_INTENT_PRIMARY_HELD) == 0) {
      std::fprintf(stderr, "FAIL controller synth missing primary held flags=0x%X\n", intents.flags);
      ok = false;
    } else {
      ok = expect_eq("controller.ternary_channel_or_held",
                     (intents.flags & THREEOS_INTENT_TERNARY_HELD) != 0 ? 1u : 0u, 1u) &&
           ok;
      std::printf("OK   controller synth primary+ternary\n");
    }

    // Palm hysteresis: borderline normal stays previous mode.
    ThreeOS_HandSample H{};
    H.tracked = 1;
    H.tip_flags = 0x3u;
    H.thumb_tip = {0.f, 1.f, 0.f};
    H.index_tip = {0.1f, 1.f, 0.f};
    H.palm_mode_valid = 0;
    H.palm_normal = {0.f, 0.75f, 0.f};  // enter Up (≥0.70)
    threeos_interaction_set_hands(&empty, &H);
    cf.frame_index++;
    cf.time_seconds += 0.05;
    threeos_tick(&cf, &hd);
    threeos_interaction_get_cursor(1, &cur);
    ok = expect_eq("palm.enter_up", cur.palm_mode, THREEOS_PALM_UP) && ok;
    H.palm_normal = {0.f, 0.55f, 0.f};  // between leave-up (0.50) and enter — stay Up
    threeos_interaction_set_hands(&empty, &H);
    cf.frame_index++;
    cf.time_seconds += 0.05;
    threeos_tick(&cf, &hd);
    threeos_interaction_get_cursor(1, &cur);
    ok = expect_eq("palm.hysteresis_up", cur.palm_mode, THREEOS_PALM_UP) && ok;
  }

  // --- Phase 0.6 Task 6.1: field params + dual sessions + scaled far + park/near ---
  {
    threeos_selection_clear();
    ThreeOS_VoodooFieldParams fp{};
    if (threeos_voodoo_get_field_params(&fp) != 0) {
      std::fprintf(stderr, "FAIL get field params\n");
      ok = false;
    } else {
      ok = expect_near("voodoo.voxel_size", fp.voxel_size_m, 0.0762f, 1e-6f) && ok;
      ok = expect_eq("voodoo.near_bb.x", static_cast<uint32_t>(fp.near_bb_voxels.x), 4u) && ok;
      ok = expect_eq("voodoo.near_bb.y", static_cast<uint32_t>(fp.near_bb_voxels.y), 6u) && ok;
      ok = expect_eq("voodoo.near_bb.z", static_cast<uint32_t>(fp.near_bb_voxels.z), 4u) && ok;
      ok = expect_near("voodoo.field_radius", fp.field_radius_m, 15.24f, 1e-4f) && ok;
      ok = expect_near("voodoo.field_height", fp.field_height_m, 9.144f, 1e-4f) && ok;
      ok = expect_near("voodoo.arm_reach", fp.arm_reach_m, 0.3048f, 1e-4f) && ok;
      const float ex = fp.near_bb_voxels.x * fp.voxel_size_m;
      const float ey = fp.near_bb_voxels.y * fp.voxel_size_m;
      const float ez = fp.near_bb_voxels.z * fp.voxel_size_m;
      ok = expect_near("voodoo.near_extent.x", ex, 0.3048f, 1e-4f) && ok;
      ok = expect_near("voodoo.near_extent.y", ey, 0.4572f, 1e-4f) && ok;
      ok = expect_near("voodoo.near_extent.z", ez, 0.3048f, 1e-4f) && ok;
      std::printf("OK   voodoo field param defaults\n");
    }

    auto fill_hand = [](ThreeOS_HandSample* h, float x, float y, float z) {
      *h = ThreeOS_HandSample{};
      h->tracked = 1;
      h->tip_flags = 0x1Fu;
      h->thumb_tip = {x, y, z};
      h->index_tip = {x + 0.02f, y, z};
      h->index_proximal = {x, y, z};
      h->palm_mode = THREEOS_PALM_DOWN;
      h->palm_mode_valid = 1;
      h->primary_pressed = 1;
      h->primary_value = 1.f;
    };

    ThreeOS_HandSample L{};
    ThreeOS_HandSample R{};
    fill_hand(&L, -0.2f, 1.2f, -0.3f);
    fill_hand(&R, 0.2f, 1.2f, -0.3f);
    threeos_interaction_set_hands(&L, &R);

    ThreeOS_InteropInputFrame vf{};
    vf.frame_index = 9200;
    vf.time_seconds = 30.0;
    vf.tracking_flags =
        THREEOS_TRACK_HEAD | THREEOS_TRACK_LEFT_AIM | THREEOS_TRACK_RIGHT_AIM;
    vf.head = identity_pose_at(0.f, 1.6f, 0.f);
    vf.head.orientation = {0.f, 0.f, 0.f, 1.f};
    vf.left_aim = identity_pose_at(-0.2f, 1.2f, -0.3f);
    vf.right_aim = identity_pose_at(0.2f, 1.2f, -0.3f);
    ThreeOS_InteropTransformDelta vd{};
    threeos_tick(&vf, &vd);

    ThreeOS_VoodooSession sessL{};
    ThreeOS_VoodooSession sessR{};
    ThreeOS_VoodooDragArrow arrowR{};
    threeos_voodoo_get_session(0, &sessL);
    threeos_voodoo_get_session(1, &sessR);
    threeos_voodoo_get_drag_arrow(1, &arrowR);
    if (sessL.open == 0 || sessR.open == 0 || sessR.mode != THREEOS_VOODOO_SNAP ||
        sessR.far_bb.open == 0 || arrowR.active == 0) {
      std::fprintf(stderr,
                   "FAIL dual snap open L=%u R=%u modeR=%u farR=%u arrowR=%u\n", sessL.open,
                   sessR.open, sessR.mode, sessR.far_bb.open, arrowR.active);
      ok = false;
    } else {
      ok = expect_near("voodoo.far_extent.y", sessR.far_bb.extents.y, 0.9144f, 1e-3f) && ok;
      ok = expect_eq("voodoo.arrow.armed_initial", arrowR.deadzone_armed, 0u) && ok;
      std::printf("OK   dual snap voodoo sessions + yellow arrow\n");
    }

    // Scaled far motion: ~12" along +Z → full field_radius from user (clamped).
    // Hold ≥ 0.25 s so release parks (shorter holds cancel).
    const float rest_z = sessR.far_bb.pose.position.z;
    R.index_tip = {0.22f, 1.2f, -0.3f + 0.3048f};
    R.thumb_tip = {0.2f, 1.2f, -0.3f + 0.3048f};
    R.index_proximal = {0.2f, 1.2f, -0.3f + 0.3048f};
    vf.right_aim = identity_pose_at(0.2f, 1.2f, -0.3f + 0.3048f);
    for (int i = 0; i < 22; ++i) {
      threeos_interaction_set_hands(&L, &R);
      vf.frame_index++;
      vf.time_seconds += 1.0 / 72.0;
      threeos_tick(&vf, &vd);
    }
    threeos_voodoo_get_session(1, &sessR);
    {
      const float dz = sessR.far_bb.pose.position.z - rest_z;
      ok = expect_near("voodoo.far_gain_z", dz, 15.24f - 1.0f, 0.75f) && ok;
      ok = expect_near("voodoo.far_abs_z", sessR.far_bb.pose.position.z, 15.24f, 0.75f) && ok;
      std::printf("OK   far BB scaled gain (~12\" → field radius)\n");
    }

    // Release parks far + opens near (does not dismiss).
    R.primary_pressed = 0;
    R.secondary_pressed = 0;
    L.primary_pressed = 0;
    threeos_interaction_set_hands(&L, &R);
    vf.frame_index++;
    vf.time_seconds += 0.016;
    threeos_tick(&vf, &vd);
    threeos_voodoo_get_session(1, &sessR);
    threeos_voodoo_get_session(0, &sessL);
    if (sessR.open == 0 || sessR.far_bb.open == 0 || sessR.near_bb.open == 0) {
      std::fprintf(stderr, "FAIL park R open=%u far=%u near=%u\n", sessR.open, sessR.far_bb.open,
                   sessR.near_bb.open);
      ok = false;
    } else {
      ok = expect_near("voodoo.near_extent.y", sessR.near_bb.extents.y, 0.4572f, 1e-3f) && ok;
      std::printf("OK   release parks far + opens near\n");
    }
    // Left also parked if it was holding; clear left hold already → should be parked too.
    if (sessL.open != 0) {
      ok = expect_eq("voodoo.L.near_open", sessL.near_bb.open, 1u) && ok;
    }

    // Fresh free-range on right after parked: re-activate replaces (start clear of near BB).
    fill_hand(&R, 0.95f, 1.55f, 0.95f);
    R.primary_pressed = 0;
    R.secondary_pressed = 1;
    threeos_interaction_set_hands(&L, &R);
    vf.frame_index++;
    vf.time_seconds += 0.016;
    vf.right_aim = identity_pose_at(0.95f, 1.55f, 0.95f);
    threeos_tick(&vf, &vd);
    threeos_voodoo_get_session(1, &sessR);
    if (sessR.open == 0 || sessR.mode != THREEOS_VOODOO_FREE || sessR.near_bb.open != 0) {
      std::fprintf(stderr, "FAIL free re-open mode=%u open=%u near=%u\n", sessR.mode, sessR.open,
                   sessR.near_bb.open);
      ok = false;
    } else {
      std::printf("OK   free-range voodoo session\n");
    }

    for (int i = 0; i < 22; ++i) {
      threeos_interaction_set_hands(&L, &R);
      vf.frame_index++;
      vf.time_seconds += 1.0 / 72.0;
      threeos_tick(&vf, &vd);
    }
    R.secondary_pressed = 0;
    threeos_interaction_set_hands(&L, &R);
    vf.frame_index++;
    vf.time_seconds += 0.016;
    threeos_tick(&vf, &vd);
    threeos_voodoo_get_session(1, &sessR);
    ok = expect_eq("voodoo.free_parked_far", sessR.far_bb.open, 1u) && ok;
    ok = expect_eq("voodoo.free_parked_near", sessR.near_bb.open, 1u) && ok;

    // Deadzone arming on selection stick: unarmed until leave then re-enter.
    threeos_kinematics_set_params(10.f, 0.015f, 3.5f, 0.f);
    const uint64_t ent = 4242;
    ThreeOS_Pose obj = identity_pose_at(0.f, 1.f, -1.f);
    threeos_kinematics_possess(ent, &obj);
    ThreeOS_InteropInputFrame kf{};
    kf.frame_index = 9300;
    kf.time_seconds = 40.0;
    kf.tracking_flags = THREEOS_TRACK_HEAD | THREEOS_TRACK_RIGHT_AIM;
    kf.head = identity_pose_at(0.f, 1.6f, 0.f);
    kf.right_aim = identity_pose_at(0.f, 1.0f, -0.5f);
    kf.right_trigger = 1.f;
    kf.button_flags = THREEOS_BTN_RIGHT_SELECT;
    threeos_tick(&kf, &vd);  // freeze start_hand
    ThreeOS_StickDebug sd{};
    threeos_kinematics_get_stick_debug(&sd);
    ok = expect_eq("dz.armed_at_start", sd.deadzone_armed, 0u) && ok;
    // Leave deadzone
    kf.frame_index++;
    kf.time_seconds += 1.0 / 72.0;
    kf.right_aim = identity_pose_at(0.05f, 1.0f, -0.5f);
    threeos_tick(&kf, &vd);
    threeos_kinematics_get_stick_debug(&sd);
    ok = expect_eq("dz.armed_while_out", sd.deadzone_armed, 0u) && ok;
    ok = expect_eq("dz.in_while_out", sd.in_deadzone, 0u) && ok;
    // Re-enter → arm
    kf.frame_index++;
    kf.time_seconds += 1.0 / 72.0;
    kf.right_aim = identity_pose_at(0.005f, 1.0f, -0.5f);
    threeos_tick(&kf, &vd);
    threeos_kinematics_get_stick_debug(&sd);
    ok = expect_eq("dz.armed_reenter", sd.deadzone_armed, 1u) && ok;
    ok = expect_eq("dz.in_after_arm", sd.in_deadzone, 1u) && ok;
    std::printf("OK   stick deadzone arming\n");
    threeos_kinematics_release();
  }

  // --- Phase 0.6 Task 6.2: centroids + dual gods ---
  {
    threeos_storage_clear();
    threeos_god_clear();
    ok = expect_eq("centroid.empty", threeos_centroid_count(), 0u) && ok;

    ThreeOS_Pose anchor{};
    anchor.orientation = {0.f, 0.f, 0.f, 1.f};
    uint64_t godA = 0;
    uint64_t godB = 0;
    ok = (threeos_god_register("DemoA", &anchor, &godA) == 0) && ok;
    ok = (threeos_god_register("DemoB", &anchor, &godB) == 0) && ok;
    ok = expect_eq("god.ids_distinct", godA != godB ? 1u : 0u, 1u) && ok;

    // Empty gods → omitted from list.
    ThreeOS_InteropInputFrame cf{};
    cf.frame_index = 9400;
    cf.time_seconds = 50.0;
    cf.tracking_flags = THREEOS_TRACK_HEAD;
    cf.head = identity_pose_at(0.f, 1.6f, 0.f);
    ThreeOS_InteropTransformDelta cd{};
    threeos_tick(&cf, &cd);
    ok = expect_eq("centroid.omit_empty_gods", threeos_centroid_count(), 0u) && ok;

    uint64_t o1 = 0;
    uint64_t o2 = 0;
    uint64_t o3 = 0;
    threeos_storage_add_object("a.glb", "a.glb", &o1);
    threeos_storage_add_object("b.glb", "b.glb", &o2);
    threeos_storage_add_object("c.glb", "c.glb", &o3);
    threeos_storage_set_god_root(o1, godA);
    threeos_storage_set_god_root(o2, godA);
    threeos_storage_set_god_root(o3, godB);

    ThreeOS_Pose layA = identity_pose_at(-1.f, 3.f, -3.f);
    ThreeOS_Pose layB = identity_pose_at(1.f, 3.f, -3.f);
    ok = (threeos_storage_layout_god(godA, &layA) == 0) && ok;
    ok = (threeos_storage_layout_god(godB, &layB) == 0) && ok;
    threeos_tick(&cf, &cd);
    ok = expect_eq("centroid.two_gods", threeos_centroid_count(), 2u) && ok;

    ThreeOS_Centroid c0{};
    ThreeOS_Centroid c1{};
    threeos_centroid_get(0, &c0);
    threeos_centroid_get(1, &c1);
    ok = expect_eq("centroid.kind_god", c0.kind, THREEOS_CENTROID_GOD) && ok;
    // Average of two DemoA objects near layA (2-cube matrix around center).
    const ThreeOS_Centroid* ca = c0.id == godA ? &c0 : &c1;
    const ThreeOS_Centroid* cb = c0.id == godB ? &c0 : &c1;
    ok = expect_eq("centroid.A.members", ca->member_count, 2u) && ok;
    ok = expect_eq("centroid.B.members", cb->member_count, 1u) && ok;
    ok = expect_near("centroid.A.y", ca->position.y, 3.f, 0.5f) && ok;
    ok = expect_near("centroid.B.y", cb->position.y, 3.f, 0.5f) && ok;

    // Expanded box centroid = average of children; empty → omit.
    uint64_t box = 0;
    threeos_storage_add_box("Nest", "Nest", 2, &box);
    threeos_storage_set_god_root(box, godB);
    uint64_t ch1 = 0;
    uint64_t ch2 = 0;
    threeos_storage_add_object("n1.glb", "Nest/n1.glb", &ch1);
    threeos_storage_add_object("n2.glb", "Nest/n2.glb", &ch2);
    threeos_storage_set_parent_box(ch1, box);
    threeos_storage_set_parent_box(ch2, box);
    threeos_storage_set_in_field(ch1, 0);
    threeos_storage_set_in_field(ch2, 0);
    threeos_tick(&cf, &cd);
    const uint32_t before_expand = threeos_centroid_count();
    threeos_storage_expand_box(box);
    // Place children at known poses via expand layout, then override.
    auto* we_unused = reinterpret_cast<void*>(0);
    (void)we_unused;
    // Move via possess-free: set poses by re-layout isn't exposed; use expand then tick.
    threeos_tick(&cf, &cd);
    ok = expect_eq("centroid.has_box", threeos_centroid_count() > before_expand ? 1u : 0u, 1u) &&
         ok;
    bool found_box = false;
    for (uint32_t i = 0; i < threeos_centroid_count(); ++i) {
      ThreeOS_Centroid cx{};
      threeos_centroid_get(i, &cx);
      if (cx.kind == THREEOS_CENTROID_BOX && cx.id == box) {
        found_box = true;
        ok = expect_eq("centroid.box.members", cx.member_count, 2u) && ok;
      }
    }
    ok = expect_eq("centroid.box_present", found_box ? 1u : 0u, 1u) && ok;

    threeos_storage_collapse_box(box);
    threeos_tick(&cf, &cd);
    found_box = false;
    for (uint32_t i = 0; i < threeos_centroid_count(); ++i) {
      ThreeOS_Centroid cx{};
      threeos_centroid_get(i, &cx);
      if (cx.id == box) {
        found_box = true;
      }
    }
    ok = expect_eq("centroid.box_omitted_collapsed", found_box ? 1u : 0u, 0u) && ok;
    std::printf("OK   centroids god average + box omit\n");
  }

  // --- Phase 0.6 Task 6.3: snap nodes + tip gravity + aspect-locked BB fit ---
  {
    threeos_storage_clear();
    threeos_god_clear();
    threeos_selection_clear();
    threeos_kinematics_release();

    ThreeOS_Pose anchor{};
    anchor.orientation = {0.f, 0.f, 0.f, 1.f};
    uint64_t godA = 0;
    uint64_t godB = 0;
    threeos_god_register("SnapA", &anchor, &godA);
    threeos_god_register("SnapB", &anchor, &godB);
    uint64_t o1 = 0;
    uint64_t o2 = 0;
    uint64_t o3 = 0;
    threeos_storage_add_object("sa.glb", "sa.glb", &o1);
    threeos_storage_add_object("sb.glb", "sb.glb", &o2);
    threeos_storage_add_object("sc.glb", "sc.glb", &o3);
    threeos_storage_set_god_root(o1, godA);
    threeos_storage_set_god_root(o2, godA);
    threeos_storage_set_god_root(o3, godB);
    // Place gods near far-rest (+Z) so hand-space offsets stay within ~arm reach.
    ThreeOS_Pose layA = identity_pose_at(-0.5f, 1.4f, 2.0f);
    ThreeOS_Pose layB = identity_pose_at(0.8f, 1.6f, 2.5f);
    threeos_storage_layout_god(godA, &layA);
    threeos_storage_layout_god(godB, &layB);

    auto fill_hand = [](ThreeOS_HandSample* h, float x, float y, float z) {
      *h = ThreeOS_HandSample{};
      h->tracked = 1;
      h->tip_flags = 0x1Fu;
      h->thumb_tip = {x, y, z};
      h->index_tip = {x + 0.02f, y, z};
      h->index_proximal = {x, y, z};
      h->palm_mode = THREEOS_PALM_DOWN;
      h->palm_mode_valid = 1;
      h->primary_pressed = 1;
      h->primary_value = 1.f;
    };

    ThreeOS_HandSample L{};
    ThreeOS_HandSample R{};
    fill_hand(&R, 0.2f, 1.2f, -0.3f);
    R.primary_pressed = 1;
    threeos_interaction_set_hands(&L, &R);

    ThreeOS_InteropInputFrame vf{};
    vf.frame_index = 9500;
    vf.time_seconds = 60.0;
    vf.tracking_flags =
        THREEOS_TRACK_HEAD | THREEOS_TRACK_LEFT_AIM | THREEOS_TRACK_RIGHT_AIM;
    vf.head = identity_pose_at(0.f, 1.6f, 0.f);
    vf.head.orientation = {0.f, 0.f, 0.f, 1.f};
    vf.right_aim = identity_pose_at(0.2f, 1.2f, -0.3f);
    ThreeOS_InteropTransformDelta vd{};
    threeos_tick(&vf, &vd);

    ok = expect_eq("snap.centroid_count", threeos_centroid_count() >= 2u ? 1u : 0u, 1u) && ok;
    {
      uint32_t persistent = 0;
      for (uint32_t i = 0; i < threeos_centroid_count(); ++i) {
        ThreeOS_Centroid cx{};
        threeos_centroid_get(i, &cx);
        if (cx.kind != THREEOS_CENTROID_TEMPORARY) {
          ++persistent;
        }
      }
      ok = expect_eq("snap.node_count", threeos_voodoo_snap_node_count(1), persistent) && ok;
    }
    ok = expect_eq("snap.free_nodes_L", threeos_voodoo_snap_node_count(0), 0u) && ok;

    ThreeOS_VoodooSession sess{};
    threeos_voodoo_get_session(1, &sess);
    ok = expect_eq("snap.session_open", sess.open, 1u) && ok;
    ok = expect_eq("snap.mode", sess.mode, THREEOS_VOODOO_SNAP) && ok;
    const float rest_x = sess.far_bb.pose.position.x;
    const float rest_y = sess.far_bb.pose.position.y;
    const float rest_z = sess.far_bb.pose.position.z;
    const float start_ext_y = sess.far_bb.extents.y;

    ThreeOS_VoodooSnapNode target{};
    bool found_target = false;
    for (uint32_t i = 0; i < threeos_voodoo_snap_node_count(1); ++i) {
      ThreeOS_VoodooSnapNode n{};
      threeos_voodoo_get_snap_node(1, i, &n);
      if (n.centroid_id == godA) {
        target = n;
        found_target = true;
        break;
      }
    }
    ok = expect_eq("snap.target_node", found_target ? 1u : 0u, 1u) && ok;

    ThreeOS_VoodooFieldParams fp{};
    threeos_voodoo_get_field_params(&fp);
    const float gain_h = fp.field_radius_m / fp.arm_reach_m;
    const float gain_v = fp.field_height_m / fp.arm_reach_m;
    const float sx = (target.field_position.x - rest_x) / gain_h;
    const float sy = (target.field_position.y - rest_y) / gain_v;
    const float sz = (target.field_position.z - rest_z) / gain_h;

    // Move tip to hand-space node (stick ≈ hand_offset).
    fill_hand(&R, 0.2f + sx, 1.2f + sy, -0.3f + sz);
    vf.right_aim = identity_pose_at(0.2f + sx, 1.2f + sy, -0.3f + sz);
    for (int i = 0; i < 18; ++i) {
      threeos_interaction_set_hands(&L, &R);
      vf.frame_index++;
      vf.time_seconds += 1.0 / 72.0;
      threeos_tick(&vf, &vd);
    }

    threeos_voodoo_get_session(1, &sess);
    ThreeOS_VoodooSnapNode snapped_node{};
    bool any_snapped = false;
    for (uint32_t i = 0; i < threeos_voodoo_snap_node_count(1); ++i) {
      ThreeOS_VoodooSnapNode n{};
      threeos_voodoo_get_snap_node(1, i, &n);
      if (n.snapped != 0) {
        snapped_node = n;
        any_snapped = true;
      }
    }
    if (!any_snapped || snapped_node.centroid_id != godA) {
      std::fprintf(stderr, "FAIL snap lock any=%d id=%llu want=%llu far=(%.2f,%.2f,%.2f)\n",
                   any_snapped ? 1 : 0, (unsigned long long)snapped_node.centroid_id,
                   (unsigned long long)godA, sess.far_bb.pose.position.x, sess.far_bb.pose.position.y,
                   sess.far_bb.pose.position.z);
      ok = false;
    } else {
      ok = expect_eq("snap.linked", sess.far_bb.linked_centroid_id == godA ? 1u : 0u, 1u) && ok;
      ok = expect_near("snap.far_x", sess.far_bb.pose.position.x, target.field_position.x, 0.35f) &&
           ok;
      ok = expect_near("snap.far_z", sess.far_bb.pose.position.z, target.field_position.z, 0.35f) &&
           ok;
      // Aspect lock: W/D ratio matches near voxels; Y at least start (fit uses max scale).
      ok = expect_near("snap.aspect_wd", sess.far_bb.extents.x / sess.far_bb.extents.z, 1.f, 1e-3f) &&
           ok;
      ok = expect_eq("snap.extents_grown",
                     sess.far_bb.extents.y + 1e-4f >= start_ext_y ? 1u : 0u, 1u) &&
           ok;
      ok = expect_eq("snap.aspect_vox_y", static_cast<uint32_t>(sess.far_bb.aspect_voxels.y), 6u) &&
           ok;
      std::printf("OK   snap nodes + tip lock + aspect-locked fit\n");
    }

    // Release parks, then free-range re-open outside the near BB (hover blocks create).
    R.primary_pressed = 0;
    R.secondary_pressed = 0;
    threeos_interaction_set_hands(&L, &R);
    vf.frame_index++;
    vf.time_seconds += 0.016;
    threeos_tick(&vf, &vd);

    fill_hand(&R, 0.55f, 1.35f, 0.55f);  // clear of near-BB orbit
    R.primary_pressed = 0;
    R.secondary_pressed = 1;
    threeos_interaction_set_hands(&L, &R);
    vf.frame_index++;
    vf.time_seconds += 0.016;
    vf.right_aim = identity_pose_at(0.55f, 1.35f, 0.55f);
    threeos_tick(&vf, &vd);
    threeos_voodoo_get_session(1, &sess);
    ok = expect_eq("snap.free_mode", sess.mode, THREEOS_VOODOO_FREE) && ok;
    ok = expect_eq("snap.free_node_count", threeos_voodoo_snap_node_count(1), 0u) && ok;
    std::printf("OK   free-range has no snap nodes\n");
  }

  // --- Phase 0.6 Task 6.4: portals, ghosts, orbit, bump, capacity ---
  {
    threeos_storage_clear();
    threeos_god_clear();
    ok = expect_eq("portal.empty", threeos_voodoo_portal_count(), 0u) && ok;
    ok = expect_eq("portal.creation_allowed", threeos_voodoo_creation_allowed(), 1u) && ok;

    ThreeOS_Pose anchor{};
    anchor.orientation = {0.f, 0.f, 0.f, 1.f};
    uint64_t godA = 0;
    threeos_god_register("PortalA", &anchor, &godA);
    uint64_t o1 = 0;
    uint64_t o2 = 0;
    threeos_storage_add_object("p1.glb", "p1.glb", &o1);
    threeos_storage_add_object("p2.glb", "p2.glb", &o2);
    threeos_storage_set_god_root(o1, godA);
    threeos_storage_set_god_root(o2, godA);
    ThreeOS_Pose lay = identity_pose_at(0.f, 1.4f, 2.0f);
    threeos_storage_layout_god(godA, &lay);

    auto fill_hand = [](ThreeOS_HandSample* h, float x, float y, float z, int primary) {
      *h = ThreeOS_HandSample{};
      h->tracked = 1;
      h->tip_flags = 0x1Fu;
      h->thumb_tip = {x, y, z};
      h->index_tip = {x + 0.02f, y, z};
      h->index_proximal = {x, y, z};
      h->palm_mode = THREEOS_PALM_DOWN;
      h->palm_mode_valid = 1;
      h->primary_pressed = primary ? 1u : 0u;
      h->primary_value = primary ? 1.f : 0.f;
    };

    ThreeOS_HandSample L{};
    ThreeOS_HandSample R{};
    fill_hand(&R, 0.25f, 1.2f, -0.25f, 1);
    threeos_interaction_set_hands(&L, &R);

    ThreeOS_InteropInputFrame vf{};
    vf.frame_index = 9600;
    vf.time_seconds = 70.0;
    vf.tracking_flags =
        THREEOS_TRACK_HEAD | THREEOS_TRACK_LEFT_AIM | THREEOS_TRACK_RIGHT_AIM;
    vf.head = identity_pose_at(0.f, 1.6f, 0.f);
    vf.head.orientation = {0.f, 0.f, 0.f, 1.f};
    vf.right_aim = identity_pose_at(0.25f, 1.2f, -0.25f);
    ThreeOS_InteropTransformDelta vd{};
    threeos_tick(&vf, &vd);

    // Snap to god then park → portal with ghosts.
    ThreeOS_VoodooFieldParams fp{};
    threeos_voodoo_get_field_params(&fp);
    ThreeOS_VoodooSession sess{};
    threeos_voodoo_get_session(1, &sess);
    const float rest_x = sess.far_bb.pose.position.x;
    const float rest_y = sess.far_bb.pose.position.y;
    const float rest_z = sess.far_bb.pose.position.z;
    ThreeOS_Centroid ca{};
    for (uint32_t i = 0; i < threeos_centroid_count(); ++i) {
      ThreeOS_Centroid cx{};
      threeos_centroid_get(i, &cx);
      if (cx.id == godA) {
        ca = cx;
        break;
      }
    }
    const float gain_h = fp.field_radius_m / fp.arm_reach_m;
    const float gain_v = fp.field_height_m / fp.arm_reach_m;
    const float sx = (ca.position.x - rest_x) / gain_h;
    const float sy = (ca.position.y - rest_y) / gain_v;
    const float sz = (ca.position.z - rest_z) / gain_h;
    fill_hand(&R, 0.25f + sx, 1.2f + sy, -0.25f + sz, 1);
    vf.right_aim = identity_pose_at(0.25f + sx, 1.2f + sy, -0.25f + sz);
    for (int i = 0; i < 22; ++i) {
      threeos_interaction_set_hands(&L, &R);
      vf.frame_index++;
      vf.time_seconds += 1.0 / 72.0;
      threeos_tick(&vf, &vd);
    }
    fill_hand(&R, 0.25f + sx, 1.2f + sy, -0.25f + sz, 0);
    threeos_interaction_set_hands(&L, &R);
    vf.frame_index++;
    vf.time_seconds += 0.016;
    threeos_tick(&vf, &vd);

    ok = expect_eq("portal.count1", threeos_voodoo_portal_count(), 1u) && ok;
    ThreeOS_VoodooPortal portal{};
    threeos_voodoo_get_portal(0, &portal);
    ok = expect_eq("portal.near_open", portal.near_bb.open, 1u) && ok;
    ok = expect_eq("portal.mode_snap", portal.mode, THREEOS_VOODOO_SNAP) && ok;
    ok = expect_eq("portal.hand_R", portal.creating_hand, 1u) && ok;
    ok = expect_eq("portal.has_temp", portal.temporary_centroid_id != 0 ? 1u : 0u, 1u) && ok;
    // Orbit bias: near BB should be on the right side of head (+X for right hand).
    ok = expect_eq("portal.orbit_right", portal.near_bb.pose.position.x > 0.f ? 1u : 0u, 1u) && ok;
    ok = expect_eq("portal.ghosts_ge2", portal.ghost_count >= 2u ? 1u : 0u, 1u) && ok;

    // Free-range park — start clear of the existing near BB (hover refuses create).
    fill_hand(&R, 0.95f, 1.55f, 0.95f, 0);
    R.secondary_pressed = 1;
    threeos_interaction_set_hands(&L, &R);
    vf.frame_index++;
    vf.time_seconds += 0.016;
    vf.right_aim = identity_pose_at(0.95f, 1.55f, 0.95f);
    threeos_tick(&vf, &vd);
    // Move far toward objects then park.
    R.index_tip = {0.97f, 1.58f, 0.95f + 0.04f};
    R.thumb_tip = {0.95f, 1.58f, 0.95f + 0.04f};
    R.index_proximal = {0.95f, 1.58f, 0.95f + 0.04f};
    vf.right_aim = identity_pose_at(0.95f, 1.58f, 0.95f + 0.04f);
    for (int i = 0; i < 22; ++i) {
      threeos_interaction_set_hands(&L, &R);
      vf.frame_index++;
      vf.time_seconds += 1.0 / 72.0;
      threeos_tick(&vf, &vd);
    }
    R.secondary_pressed = 0;
    threeos_interaction_set_hands(&L, &R);
    vf.frame_index++;
    vf.time_seconds += 0.016;
    threeos_tick(&vf, &vd);
    ok = expect_eq("portal.count2", threeos_voodoo_portal_count(), 2u) && ok;

    // Settle bump, then assert orbit radius never dips below near-rest (~0.406 m).
    for (int i = 0; i < 24; ++i) {
      vf.frame_index++;
      vf.time_seconds += 1.0 / 72.0;
      threeos_tick(&vf, &vd);
    }
    {
      constexpr float kNearRest = 0.33f + 5.f * 0.0254f;  // +2" vs prior 16"
      const float hx = vf.head.position.x;
      const float hz = vf.head.position.z;
      for (uint32_t pi = 0; pi < threeos_voodoo_portal_count(); ++pi) {
        ThreeOS_VoodooPortal pp{};
        threeos_voodoo_get_portal(pi, &pp);
        const float dx = pp.near_bb.pose.position.x - hx;
        const float dz = pp.near_bb.pose.position.z - hz;
        const float r = std::sqrt(dx * dx + dz * dz);
        ok = expect_eq("portal.orbit_radius_ok", r + 1e-3f >= kNearRest ? 1u : 0u, 1u) && ok;
      }
    }

    // Cursor tint query: near BB interior vs exterior.
    threeos_voodoo_get_portal(0, &portal);
    const float nx = portal.near_bb.pose.position.x;
    const float ny = portal.near_bb.pose.position.y;
    const float nz = portal.near_bb.pose.position.z;
    ok = expect_eq("portal.point_inside", threeos_voodoo_point_in_near_bb(nx, ny, nz), 1u) && ok;
    ok = expect_eq("portal.point_outside", threeos_voodoo_point_in_near_bb(nx + 5.f, ny, nz), 0u) &&
         ok;

    // Hover inside near BB refuses a new snap session.
    // Tip is offset so the point-cursor probe lands at the near BB center.
    const float tip_x = nx - 0.1016f;  // −lateral (right hand)
    const float tip_y = ny - 0.0762f;  // −up
    const float tip_z = nz - 0.1778f;  // −forward
    fill_hand(&R, tip_x, tip_y, tip_z, 1);
    threeos_interaction_set_hands(&L, &R);
    vf.frame_index++;
    vf.time_seconds += 0.016;
    vf.right_aim = identity_pose_at(tip_x, tip_y, tip_z);
    threeos_tick(&vf, &vd);
    threeos_voodoo_get_session(1, &sess);
    ok = expect_eq("portal.hover_blocks_create", sess.open == 0 ? 1u : 0u, 1u) && ok;
    ok = expect_eq("portal.count_still2", threeos_voodoo_portal_count(), 2u) && ok;

    ok = (threeos_voodoo_dismiss_portal(portal.id) == 0) && ok;
    ok = expect_eq("portal.after_dismiss", threeos_voodoo_portal_count(), 1u) && ok;

    // Short hold (< 0.25 s) cancels both BBs — no new portal.
    const uint32_t before_short = threeos_voodoo_portal_count();
    fill_hand(&R, 0.6f, 1.35f, 0.6f, 0);
    R.secondary_pressed = 1;
    threeos_interaction_set_hands(&L, &R);
    vf.frame_index++;
    vf.time_seconds += 0.016;
    vf.right_aim = identity_pose_at(0.6f, 1.35f, 0.6f);
    threeos_tick(&vf, &vd);
    for (int i = 0; i < 4; ++i) {  // ~0.07 s total hold with open frame
      threeos_interaction_set_hands(&L, &R);
      vf.frame_index++;
      vf.time_seconds += 1.0 / 72.0;
      threeos_tick(&vf, &vd);
    }
    R.secondary_pressed = 0;
    threeos_interaction_set_hands(&L, &R);
    vf.frame_index++;
    vf.time_seconds += 0.016;
    threeos_tick(&vf, &vd);
    threeos_voodoo_get_session(1, &sess);
    ok = expect_eq("portal.short_hold_cancels", sess.open == 0 ? 1u : 0u, 1u) && ok;
    ok = expect_eq("portal.short_hold_count", threeos_voodoo_portal_count(), before_short) && ok;

    // Fill orbit until capacity; further creates must refuse (no overlap dump).
    auto park_free_at = [&](float x, float y, float z) {
      fill_hand(&R, x, y, z, 0);
      R.secondary_pressed = 1;
      threeos_interaction_set_hands(&L, &R);
      vf.frame_index++;
      vf.time_seconds += 0.016;
      vf.right_aim = identity_pose_at(x, y, z);
      threeos_tick(&vf, &vd);
      for (int i = 0; i < 22; ++i) {
        threeos_interaction_set_hands(&L, &R);
        vf.frame_index++;
        vf.time_seconds += 1.0 / 72.0;
        threeos_tick(&vf, &vd);
      }
      R.secondary_pressed = 0;
      threeos_interaction_set_hands(&L, &R);
      vf.frame_index++;
      vf.time_seconds += 0.016;
      threeos_tick(&vf, &vd);
    };
    // Hands well outside near-BB radius so hover refuse does not mask capacity.
    const float ring[8][3] = {
        {0.90f, 1.55f, 0.35f},  {-0.90f, 1.55f, 0.35f}, {0.35f, 1.55f, 0.90f},
        {-0.35f, 1.55f, 0.90f}, {0.90f, 1.55f, -0.35f}, {-0.90f, 1.55f, -0.35f},
        {0.0f, 1.55f, 0.95f},   {0.0f, 1.55f, -0.90f},
    };
    // Pack around the body; ellipse arc should allow several before refuse.
    for (int i = 0; i < 8; ++i) {
      park_free_at(ring[i][0], ring[i][1], ring[i][2]);
      for (int s = 0; s < 20; ++s) {
        vf.frame_index++;
        vf.time_seconds += 1.0 / 72.0;
        threeos_tick(&vf, &vd);
      }
    }
    const uint32_t packed = threeos_voodoo_portal_count();
    ok = expect_eq("portal.orbit_packed_ge3", packed >= 3u ? 1u : 0u, 1u) && ok;
    // Further create into a dense cluster: count must not exceed hard cap.
    ok = expect_eq("portal.creation_under_hardcap",
                   threeos_voodoo_creation_allowed() == 1u || packed >= 12u ? 1u : 0u, 1u) &&
         ok;
    const uint32_t full_count = packed;
    // Let bump lerps settle before overlap audit.
    for (int i = 0; i < 24; ++i) {
      vf.frame_index++;
      vf.time_seconds += 1.0 / 72.0;
      threeos_tick(&vf, &vd);
    }
    ThreeOS_VoodooPortal pa{};
    ThreeOS_VoodooPortal pb{};
    bool any_overlap = false;
    for (uint32_t i = 0; i < full_count && !any_overlap; ++i) {
      threeos_voodoo_get_portal(i, &pa);
      for (uint32_t j = i + 1; j < full_count; ++j) {
        threeos_voodoo_get_portal(j, &pb);
        const float dx = pa.near_bb.pose.position.x - pb.near_bb.pose.position.x;
        const float dz = pa.near_bb.pose.position.z - pb.near_bb.pose.position.z;
        const float ra =
            0.5f * std::sqrt(pa.near_bb.extents.x * pa.near_bb.extents.x +
                             pa.near_bb.extents.z * pa.near_bb.extents.z);
        const float rb =
            0.5f * std::sqrt(pb.near_bb.extents.x * pb.near_bb.extents.x +
                             pb.near_bb.extents.z * pb.near_bb.extents.z);
        if ((dx * dx + dz * dz) < (ra + rb + 0.04f) * (ra + rb + 0.04f)) {
          any_overlap = true;
          break;
        }
      }
    }
    ok = expect_eq("portal.orbit_no_overlap", any_overlap ? 0u : 1u, 1u) && ok;
    std::printf("OK   portals ghosts orbit tint dismiss hold-gate capacity\n");
  }

  // --- Phase 0.6 Task 6.5: ghost drag, summon, temp BB unchanged ---
  {
    threeos_storage_clear();
    threeos_god_clear();

    ThreeOS_Pose anchor{};
    anchor.orientation = {0.f, 0.f, 0.f, 1.f};
    uint64_t godA = 0;
    threeos_god_register("DragA", &anchor, &godA);
    uint64_t o1 = 0;
    uint64_t o2 = 0;
    threeos_storage_add_object("d1.glb", "d1.glb", &o1);
    threeos_storage_add_object("d2.glb", "d2.glb", &o2);
    threeos_storage_set_god_root(o1, godA);
    threeos_storage_set_god_root(o2, godA);
    ThreeOS_Pose lay = identity_pose_at(0.f, 1.4f, 2.0f);
    threeos_storage_layout_god(godA, &lay);

    auto fill_hand = [](ThreeOS_HandSample* h, float x, float y, float z, int primary) {
      *h = ThreeOS_HandSample{};
      h->tracked = 1;
      h->tip_flags = 0x1Fu;
      h->thumb_tip = {x, y, z};
      h->index_tip = {x + 0.02f, y, z};
      h->index_proximal = {x, y, z};
      h->palm_mode = THREEOS_PALM_DOWN;
      h->palm_mode_valid = 1;
      h->primary_pressed = primary ? 1u : 0u;
      h->primary_value = primary ? 1.f : 0.f;
    };

    ThreeOS_HandSample L{};
    ThreeOS_HandSample R{};
    fill_hand(&R, 0.25f, 1.2f, -0.25f, 1);
    threeos_interaction_set_hands(&L, &R);

    ThreeOS_InteropInputFrame vf{};
    vf.frame_index = 11000;
    vf.time_seconds = 90.0;
    vf.tracking_flags =
        THREEOS_TRACK_HEAD | THREEOS_TRACK_LEFT_AIM | THREEOS_TRACK_RIGHT_AIM;
    vf.head = identity_pose_at(0.f, 1.6f, 0.f);
    vf.head.orientation = {0.f, 0.f, 0.f, 1.f};
    vf.right_aim = identity_pose_at(0.25f, 1.2f, -0.25f);
    ThreeOS_InteropTransformDelta vd{};
    threeos_tick(&vf, &vd);

    ThreeOS_VoodooFieldParams fp{};
    threeos_voodoo_get_field_params(&fp);
    ThreeOS_VoodooSession sess{};
    threeos_voodoo_get_session(1, &sess);
    const float rest_x = sess.far_bb.pose.position.x;
    const float rest_y = sess.far_bb.pose.position.y;
    const float rest_z = sess.far_bb.pose.position.z;
    ThreeOS_Centroid ca{};
    for (uint32_t i = 0; i < threeos_centroid_count(); ++i) {
      ThreeOS_Centroid cx{};
      threeos_centroid_get(i, &cx);
      if (cx.id == godA) {
        ca = cx;
        break;
      }
    }
    const float gain_h = fp.field_radius_m / fp.arm_reach_m;
    const float gain_v = fp.field_height_m / fp.arm_reach_m;
    const float sx = (ca.position.x - rest_x) / gain_h;
    const float sy = (ca.position.y - rest_y) / gain_v;
    const float sz = (ca.position.z - rest_z) / gain_h;
    fill_hand(&R, 0.25f + sx, 1.2f + sy, -0.25f + sz, 1);
    vf.right_aim = identity_pose_at(0.25f + sx, 1.2f + sy, -0.25f + sz);
    for (int i = 0; i < 22; ++i) {
      threeos_interaction_set_hands(&L, &R);
      vf.frame_index++;
      vf.time_seconds += 1.0 / 72.0;
      threeos_tick(&vf, &vd);
    }
    fill_hand(&R, 0.25f + sx, 1.2f + sy, -0.25f + sz, 0);
    threeos_interaction_set_hands(&L, &R);
    vf.frame_index++;
    vf.time_seconds += 0.016;
    threeos_tick(&vf, &vd);

    ok = expect_eq("gdrag.portal1", threeos_voodoo_portal_count(), 1u) && ok;
    ThreeOS_VoodooPortal portal{};
    threeos_voodoo_get_portal(0, &portal);
    ok = expect_eq("gdrag.ghosts", portal.ghost_count >= 2u ? 1u : 0u, 1u) && ok;

    uint64_t ghost_ent = 0;
    float ghost_scale = 1.f;
    ThreeOS_Pose far0{};
    ThreeOS_Pose ghost_near0{};
    for (uint32_t gi = 0; gi < portal.ghost_count; ++gi) {
      ThreeOS_VoodooGhost g{};
      threeos_voodoo_get_portal_ghost(portal.id, gi, &g);
      if (g.is_parent_box == 0) {
        ghost_ent = g.entity_id;
        ghost_scale = g.scale;
        far0 = g.far_pose;
        ghost_near0 = g.near_pose;
        break;
      }
    }
    ok = expect_eq("gdrag.has_ent", ghost_ent != 0 ? 1u : 0u, 1u) && ok;
    threeos_selection_set(ghost_ent);

    const float nx = portal.near_bb.pose.position.x;
    const float ny = portal.near_bb.pose.position.y;
    const float nz = portal.near_bb.pose.position.z;
    ok = expect_eq("gdrag.portal_at",
                   threeos_voodoo_portal_at_near_point(nx, ny, nz) == portal.id ? 1u : 0u, 1u) &&
         ok;
    ok = expect_eq("gdrag.portal_for",
                   threeos_voodoo_portal_for_ghost(ghost_ent) == portal.id ? 1u : 0u, 1u) &&
         ok;

    // Cursor = tip + forward(7") + up(3") + right-hand lateral(4"). With identity head
    // (floor fwd +Z, right +X): tip = target_cursor - (0.1016, 0.0762, 0.1778).
    const float tip_dx = 0.1016f;
    const float tip_dy = 0.0762f;
    const float tip_dz = 0.1778f;

    // --- Direct grab: cursor on the near ghost → 1:1 follow; no velocity sticks ---
    {
      const float dtip_x = ghost_near0.position.x - tip_dx;
      const float dtip_y = ghost_near0.position.y - tip_dy;
      const float dtip_z = ghost_near0.position.z - tip_dz;
      fill_hand(&R, dtip_x, dtip_y, dtip_z, 1);
      threeos_interaction_set_hands(&L, &R);
      vf.right_aim = identity_pose_at(dtip_x, dtip_y, dtip_z);
      vf.frame_index++;
      vf.time_seconds += 0.016;
      threeos_tick(&vf, &vd);
      ok = expect_eq("gdrag.direct_begin",
                     threeos_voodoo_ghost_drag_begin(1, ghost_ent) == 0 ? 1u : 0u, 1u) &&
           ok;
      const float move = 0.04f;
      fill_hand(&R, dtip_x + move, dtip_y, dtip_z, 1);
      vf.right_aim = identity_pose_at(dtip_x + move, dtip_y, dtip_z);
      for (int i = 0; i < 8; ++i) {
        threeos_interaction_set_hands(&L, &R);
        vf.frame_index++;
        vf.time_seconds += 1.0 / 72.0;
        threeos_tick(&vf, &vd);
      }
      ThreeOS_VoodooDragArrow darrow{};
      threeos_voodoo_get_ghost_drag_arrow(1, &darrow);
      ok = expect_eq("gdrag.direct_no_arrow", darrow.active, 0u) && ok;
      ThreeOS_VoodooGhost g_direct{};
      for (uint32_t gi = 0; gi < threeos_voodoo_portal_ghost_count(portal.id); ++gi) {
        ThreeOS_VoodooGhost g{};
        threeos_voodoo_get_portal_ghost(portal.id, gi, &g);
        if (g.entity_id == ghost_ent) {
          g_direct = g;
          break;
        }
      }
      ok = expect_eq("gdrag.direct_follow_x",
                     std::fabs(g_direct.near_pose.position.x - (ghost_near0.position.x + move)) <
                             0.015f
                         ? 1u
                         : 0u,
                     1u) &&
           ok;
      // Snap targeting uses near ghost pose but must not teleport the far real onto the ghost.
      {
        ThreeOS_WorkspaceItem wi{};
        bool found = false;
        for (uint32_t ii = 0; ii < threeos_storage_item_count(); ++ii) {
          threeos_storage_get_item(ii, &wi);
          if (wi.entity_id == ghost_ent) {
            found = true;
            break;
          }
        }
        const float dxn = wi.pose.position.x - portal.near_bb.pose.position.x;
        const float dzn = wi.pose.position.z - portal.near_bb.pose.position.z;
        const float dxf = wi.pose.position.x - portal.far_bb.pose.position.x;
        const float dzf = wi.pose.position.z - portal.far_bb.pose.position.z;
        const float near_d = std::sqrt(dxn * dxn + dzn * dzn);
        const float far_d = std::sqrt(dxf * dxf + dzf * dzf);
        ok = expect_eq("gdrag.direct_far_stays_far",
                       (found && far_d + 0.05f < near_d) ? 1u : 0u, 1u) &&
             ok;
        ok = expect_eq("gdrag.direct_far_pose_ok",
                       (std::fabs(g_direct.far_pose.position.x - wi.pose.position.x) < 0.02f &&
                        std::fabs(g_direct.far_pose.position.z - wi.pose.position.z) < 0.02f)
                           ? 1u
                           : 0u,
                       1u) &&
             ok;
      }
      threeos_voodoo_ghost_drag_end(1, 1);
      // Refresh ghost_near0 / far0 after direct drag.
      for (uint32_t gi = 0; gi < threeos_voodoo_portal_ghost_count(portal.id); ++gi) {
        ThreeOS_VoodooGhost g{};
        threeos_voodoo_get_portal_ghost(portal.id, gi, &g);
        if (g.entity_id == ghost_ent) {
          ghost_near0 = g.near_pose;
          far0 = g.far_pose;
          ghost_scale = g.scale;
          break;
        }
      }
    }

    // --- Velocity stick: cursor inside near BB but outside the ghost glyph ---
    float stick_cx = nx;
    float stick_cy = ny;
    float stick_cz = nz;
    {
      const float gdx = stick_cx - ghost_near0.position.x;
      const float gdy = stick_cy - ghost_near0.position.y;
      const float gdz = stick_cz - ghost_near0.position.z;
      const float gd = std::sqrt(gdx * gdx + gdy * gdy + gdz * gdz);
      const float grab_r = 0.09f * std::max(ghost_scale, 0.15f) + 0.02f;
      if (gd < grab_r) {
        // Nudge toward a near-BB corner so we are clearly outside the glyph.
        stick_cx = nx + portal.near_bb.extents.x * 0.35f;
        stick_cy = ny + portal.near_bb.extents.y * 0.25f;
        stick_cz = nz;
      }
    }
    const float tip_x = stick_cx - tip_dx;
    const float tip_y = stick_cy - tip_dy;
    const float tip_z = stick_cz - tip_dz;
    fill_hand(&R, tip_x, tip_y, tip_z, 1);
    threeos_interaction_set_hands(&L, &R);
    vf.right_aim = identity_pose_at(tip_x, tip_y, tip_z);
    vf.frame_index++;
    vf.time_seconds += 0.016;
    threeos_tick(&vf, &vd);
    ThreeOS_PointCursor cur{};
    threeos_interaction_get_cursor(1, &cur);
    ok = expect_eq("gdrag.cursor_in",
                   threeos_voodoo_point_in_near_bb(cur.position.x, cur.position.y, cur.position.z),
                   1u) &&
         ok;
    ok = expect_eq("gdrag.begin", threeos_voodoo_ghost_drag_begin(1, ghost_ent) == 0 ? 1u : 0u,
                   1u) &&
         ok;
    ok = expect_eq("gdrag.active", threeos_voodoo_ghost_drag_active(1), 1u) && ok;

    threeos_voodoo_get_portal(0, &portal);
    const float far_ext_x_park = portal.far_bb.extents.x;
    const float near_ext_x_park = portal.near_bb.extents.x;
    const float scale_park = ghost_scale;

    // Stick +0.08 m in X (past deadzone) for several frames → far should move ~stick/scale.
    fill_hand(&R, tip_x + 0.08f, tip_y, tip_z, 1);
    vf.right_aim = identity_pose_at(tip_x + 0.08f, tip_y, tip_z);
    for (int i = 0; i < 36; ++i) {
      threeos_interaction_set_hands(&L, &R);
      vf.frame_index++;
      vf.time_seconds += 1.0 / 72.0;
      threeos_tick(&vf, &vd);
    }

    ThreeOS_VoodooDragArrow blue{};
    threeos_voodoo_get_ghost_drag_arrow(1, &blue);
    ok = expect_eq("gdrag.arrow_active", blue.active, 1u) && ok;

    ThreeOS_VoodooGhost g_after{};
    for (uint32_t gi = 0; gi < threeos_voodoo_portal_ghost_count(portal.id); ++gi) {
      ThreeOS_VoodooGhost g{};
      threeos_voodoo_get_portal_ghost(portal.id, gi, &g);
      if (g.entity_id == ghost_ent) {
        g_after = g;
        break;
      }
    }
    const float far_dx = g_after.far_pose.position.x - far0.position.x;
    ok = expect_eq("gdrag.far_moved", std::fabs(far_dx) > 0.02f ? 1u : 0u, 1u) && ok;
    // Ghost scale < 1 → far travel exceeds near stick (~0.08); allow some slack.
    if (ghost_scale < 0.95f) {
      ok = expect_eq("gdrag.far_amplified", std::fabs(far_dx) > 0.05f ? 1u : 0u, 1u) && ok;
    }

    // Drive further so the real would leave the parked far BB — far+near grow at
    // lock ratio; ghost scale stays put; near pose stays inside the near OBB.
    fill_hand(&R, tip_x + 0.35f, tip_y, tip_z, 1);
    vf.right_aim = identity_pose_at(tip_x + 0.35f, tip_y, tip_z);
    for (int i = 0; i < 72; ++i) {
      threeos_interaction_set_hands(&L, &R);
      vf.frame_index++;
      vf.time_seconds += 1.0 / 72.0;
      threeos_tick(&vf, &vd);
    }
    threeos_voodoo_get_portal(0, &portal);
    for (uint32_t gi = 0; gi < threeos_voodoo_portal_ghost_count(portal.id); ++gi) {
      ThreeOS_VoodooGhost g{};
      threeos_voodoo_get_portal_ghost(portal.id, gi, &g);
      if (g.entity_id == ghost_ent) {
        g_after = g;
        break;
      }
    }
    ok = expect_eq("gdrag.far_bb_grew",
                   portal.far_bb.extents.x > far_ext_x_park + 1e-3f ? 1u : 0u, 1u) &&
         ok;
    ok = expect_eq("gdrag.scale_locked",
                   std::fabs(g_after.scale - scale_park) < 1e-3f ? 1u : 0u, 1u) &&
         ok;
    ok = expect_eq("gdrag.near_bb_grew",
                   portal.near_bb.extents.x > near_ext_x_park + 1e-4f ? 1u : 0u, 1u) &&
         ok;
    ok = expect_eq("gdrag.near_in_bb",
                   threeos_voodoo_point_in_near_bb(g_after.near_pose.position.x,
                                                  g_after.near_pose.position.y,
                                                  g_after.near_pose.position.z),
                   1u) &&
         ok;

    // Yellow far twin + blue near arrow both active during velocity ghost drag.
    ThreeOS_VoodooDragArrow yellow{};
    threeos_voodoo_get_drag_arrow(1, &yellow);
    threeos_voodoo_get_ghost_drag_arrow(1, &blue);
    ok = expect_eq("gdrag.yellow_far_arrow", yellow.active, 1u) && ok;
    ok = expect_eq("gdrag.blue_near_arrow", blue.active, 1u) && ok;

    threeos_voodoo_ghost_drag_end(1, 1);
    ok = expect_eq("gdrag.ended", threeos_voodoo_ghost_drag_active(1), 0u) && ok;
    threeos_voodoo_get_portal(0, &portal);
    for (uint32_t gi = 0; gi < threeos_voodoo_portal_ghost_count(portal.id); ++gi) {
      ThreeOS_VoodooGhost g{};
      threeos_voodoo_get_portal_ghost(portal.id, gi, &g);
      if (g.entity_id == ghost_ent) {
        g_after = g;
        break;
      }
    }
    ok = expect_eq("gdrag.near_in_bb_after_end",
                   threeos_voodoo_point_in_near_bb(g_after.near_pose.position.x,
                                                  g_after.near_pose.position.y,
                                                  g_after.near_pose.position.z),
                   1u) &&
         ok;

    // Open-air summon: pinch outside near BB; far extents stay put.
    const float far_ext_x0 = portal.far_bb.extents.x;
    const float far_ext_y0 = portal.far_bb.extents.y;
    const float far_ext_z0 = portal.far_bb.extents.z;
    const float pinch_x = portal.near_bb.pose.position.x + portal.near_bb.extents.x * 0.5f + 0.35f;
    const float pinch_y = portal.near_bb.pose.position.y;
    const float pinch_z = portal.near_bb.pose.position.z;
    ok = expect_eq("gdrag.summon_out",
                   threeos_voodoo_point_in_near_bb(pinch_x, pinch_y, pinch_z) == 0u ? 1u : 0u,
                   1u) &&
         ok;
    ok = expect_eq("gdrag.summon_ok",
                   threeos_voodoo_summon(ghost_ent, pinch_x, pinch_y, pinch_z) == 0 ? 1u : 0u,
                   1u) &&
         ok;
    ok = expect_eq("gdrag.summon_gone",
                   threeos_voodoo_portal_for_ghost(ghost_ent) == 0 ? 1u : 0u, 1u) &&
         ok;
    threeos_voodoo_get_portal(0, &portal);
    ok = expect_eq("gdrag.temp_no_grow_x",
                   std::fabs(portal.far_bb.extents.x - far_ext_x0) < 1e-4f ? 1u : 0u, 1u) &&
         ok;
    ok = expect_eq("gdrag.temp_no_grow_y",
                   std::fabs(portal.far_bb.extents.y - far_ext_y0) < 1e-4f ? 1u : 0u, 1u) &&
         ok;
    ok = expect_eq("gdrag.temp_no_grow_z",
                   std::fabs(portal.far_bb.extents.z - far_ext_z0) < 1e-4f ? 1u : 0u, 1u) &&
         ok;

    // Fast-lerp (~0.20 s) to pinch — advance ticks then check destination.
    for (int i = 0; i < 24; ++i) {
      threeos_interaction_set_hands(&L, &R);
      vf.frame_index++;
      vf.time_seconds += 1.0 / 72.0;
      threeos_tick(&vf, &vd);
    }
    ThreeOS_WorkspaceItem item{};
    bool found = false;
    for (uint32_t i = 0; i < threeos_storage_item_count(); ++i) {
      threeos_storage_get_item(i, &item);
      if (item.entity_id == ghost_ent) {
        found = true;
        break;
      }
    }
    ok = expect_eq("gdrag.summon_pose", found ? 1u : 0u, 1u) && ok;
    if (found) {
      ok = expect_eq("gdrag.summon_x",
                     std::fabs(item.pose.position.x - pinch_x) < 0.02f ? 1u : 0u, 1u) &&
           ok;
    }

    std::printf("OK   ghost drag summon temp-no-grow\n");
  }

  threeos_shutdown();
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
