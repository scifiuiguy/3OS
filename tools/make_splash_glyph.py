"""Build the "3OS" splash glyph in Blender and render a stereo OS splash PNG.

Meta Quest expects one double-wide PNG: left eye = left half, right eye = right half.
Each half is a full 1024×1024 view from a slightly offset camera (stereo parallax).

Typography: Myriad Pro (place under assets/fonts/ or set MYRIAD_PRO_FONT).

Usage (headless):
  blender -b -P tools/make_splash_glyph.py -- <out.png> <out_glyph.glb> [font.otf|ttf] [swap]

Pass ``swap`` to swap left/right halves (fixes reversed stereo).
"""
import os
import sys

import bpy

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
MYRIAD_CANDIDATE_NAMES = (
    "MyriadPro-Regular.otf",
    "Myriad Pro Regular.otf",
    "MYRIADPRO-REGULAR.OTF",
    "Myriad Pro Regular.ttf",
    "MyriadPro-Regular.ttf",
)

# Shipped with Adobe Acrobat DC (typical Windows install).
ACROBAT_FONT_DIRS = (
    r"C:\Program Files\Adobe\Acrobat DC\Resource\Font",
    r"C:\Program Files (x86)\Adobe\Acrobat DC\Resource\Font",
)


def resolve_myriad_font(cli_font):
    candidates = []
    if cli_font:
        candidates.append(os.path.expanduser(cli_font))
    env = os.environ.get("MYRIAD_PRO_FONT")
    if env:
        candidates.append(os.path.expanduser(env))
    for adobe_dir in ACROBAT_FONT_DIRS:
        for name in MYRIAD_CANDIDATE_NAMES:
            candidates.append(os.path.join(adobe_dir, name))
    fonts_dir = os.path.join(REPO_ROOT, "assets", "fonts")
    for name in MYRIAD_CANDIDATE_NAMES:
        candidates.append(os.path.join(fonts_dir, name))
    for path in candidates:
        if path and os.path.isfile(path):
            return path
    raise SystemExit(
        "Myriad Pro font not found. Copy Myriad Pro Regular.otf to assets/fonts/, "
        "set MYRIAD_PRO_FONT, or pass the .otf/.ttf path as the 3rd CLI argument."
    )


argv = sys.argv[sys.argv.index("--") + 1:]
out_png, out_glb = argv[0], argv[1]
extra = argv[2:]
swap_stereo = any(a.lower() in ("swap", "1", "true", "--swap-stereo") for a in extra)
cli_font = next(
    (a for a in extra if a.lower() not in ("swap", "1", "true", "--swap-stereo")), None
)
font_path = resolve_myriad_font(cli_font)
print(f"SPLASH_FONT {font_path}")

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene

curve = bpy.data.curves.new("3OS_text", "FONT")
curve.font = bpy.data.fonts.load(font_path)
curve.body = "3OS"
curve.align_x = "CENTER"
curve.align_y = "CENTER"
curve.extrude = 0.08
curve.bevel_depth = 0.02
curve.bevel_resolution = 4
obj = bpy.data.objects.new("3OS_glyph", curve)
scene.collection.objects.link(obj)
bpy.context.view_layer.objects.active = obj
obj.select_set(True)
bpy.ops.object.convert(target="MESH")
bpy.ops.object.shade_smooth()

mat = bpy.data.materials.new("3OS_glyph_mat")
mat.use_nodes = True
bsdf = mat.node_tree.nodes["Principled BSDF"]
bsdf.inputs["Base Color"].default_value = (0.86, 0.93, 1.0, 1.0)
bsdf.inputs["Metallic"].default_value = 0.6
bsdf.inputs["Roughness"].default_value = 0.3
obj.data.materials.append(mat)

bpy.ops.object.origin_set(type="ORIGIN_GEOMETRY", center="BOUNDS")
obj.location = (0, 0, 0)
dx, dy, _ = obj.dimensions

world = bpy.data.worlds.new("w")
world.use_nodes = True
# Quest OSSplash: keep logo alpha; manifest com.oculus.ossplash.background=black composites the field.
world.node_tree.nodes["Background"].inputs["Color"].default_value = (
    0.12156863,
    0.12156863,
    0.1254902,
    1.0,
)
world.node_tree.nodes["Background"].inputs["Strength"].default_value = 1.0
scene.world = world

sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
sun.data.energy = 4.0
sun.rotation_euler = (0.9, 0.2, 0.6)
scene.collection.objects.link(sun)

half_w, half_h = 1024, 1024
res_x, res_y = half_w * 2, half_h
ipd_half = 0.032

cam_data = bpy.data.cameras.new("cam")
cam_data.type = "ORTHO"
cam_data.ortho_scale = max(dx / 0.8, dy / 0.8 * (half_w / half_h))
cam = bpy.data.objects.new("cam", cam_data)
cam.location = (0, 0, 5)
scene.collection.objects.link(cam)
scene.camera = cam

scene.render.engine = "BLENDER_EEVEE"
scene.render.resolution_x = half_w
scene.render.resolution_y = half_h
# Must be true or “transparent” pixels keep world RGB (~190) and Quest OSSplash shows an empty plate.
scene.render.film_transparent = True
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA"

import os
import tempfile

tmp_dir = tempfile.mkdtemp(prefix="3os_splash_")
paths = {}
for eye, x_off in (("L", -ipd_half), ("R", ipd_half)):
    cam.location.x = x_off
    path = os.path.join(tmp_dir, f"eye_{eye}.png")
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    paths[eye] = path

# Compose double-wide: default L | R; swap puts R on the left half.
order = ("R", "L") if swap_stereo else ("L", "R")
comp = bpy.data.images.new("splash_comp", width=res_x, height=res_y, alpha=True)
comp.file_format = "PNG"

for i, eye in enumerate(order):
    eye_img = bpy.data.images.load(paths[eye])
    eye_w, eye_h = eye_img.size
    if eye_w != half_w or eye_h != half_h:
        raise RuntimeError(f"Unexpected render size {eye_w}x{eye_h} for eye {eye}")
    src = list(eye_img.pixels[:])
    dst = list(comp.pixels[:])
    x0 = i * half_w
    for y in range(half_h):
        for x in range(half_w):
            si = (y * half_w + x) * 4
            di = (y * res_x + (x0 + x)) * 4
            dst[di : di + 4] = src[si : si + 4]
    comp.pixels = dst
    bpy.data.images.remove(eye_img)

comp.filepath_raw = out_png
comp.save()
bpy.data.images.remove(comp)

bpy.ops.object.select_all(action="DESELECT")
obj.select_set(True)
bpy.context.view_layer.objects.active = obj
bpy.ops.export_scene.gltf(filepath=out_glb, export_format="GLB", use_selection=True)
print(f"SPLASH_OK dims=({dx:.3f},{dy:.3f}) swap_stereo={swap_stereo}")
