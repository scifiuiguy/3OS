"""Build the "3OS" splash glyph in Blender and render a transparent PNG.

Usage (headless):
  blender -b -P tools/make_splash_glyph.py -- <out_half.png> <out_glyph.glb> [font.otf|ttf]
"""
import sys

import bpy

argv = sys.argv[sys.argv.index("--") + 1:]
out_png, out_glb = argv[0], argv[1]
font_path = argv[2] if len(argv) > 2 else None

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene

curve = bpy.data.curves.new("3OS_text", "FONT")
if font_path:
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

# Centre the mesh on the origin so the camera framing is symmetric.
bpy.ops.object.origin_set(type="ORIGIN_GEOMETRY", center="BOUNDS")
obj.location = (0, 0, 0)
dx, dy, _ = obj.dimensions

world = bpy.data.worlds.new("w")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.6, 0.7, 0.9, 1)
world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.8
scene.world = world

sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
sun.data.energy = 4.0
sun.rotation_euler = (0.9, 0.2, 0.6)
scene.collection.objects.link(sun)

cam_data = bpy.data.cameras.new("cam")
cam_data.type = "ORTHO"
res_x, res_y = 2048, 1024
# Fit the text to 80% of the frame width or height, whichever is tighter.
cam_data.ortho_scale = max(dx / 0.8, dy / 0.8 * (res_x / res_y))
cam = bpy.data.objects.new("cam", cam_data)
cam.location = (0, 0, 5)
scene.collection.objects.link(cam)
scene.camera = cam

scene.render.engine = "BLENDER_EEVEE"
scene.render.resolution_x = res_x
scene.render.resolution_y = res_y
scene.render.film_transparent = True
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA"
scene.render.filepath = out_png
bpy.ops.render.render(write_still=True)

bpy.ops.export_scene.gltf(filepath=out_glb, export_format="GLB", use_selection=True)
print(f"SPLASH_OK dims=({dx:.3f},{dy:.3f})")
