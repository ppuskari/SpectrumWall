#!/usr/bin/env python3
"""Export a Blender scene to a private SpectrumWall OBJ mesh/animation folder.

Run with Blender's bundled Python, not the Windows system Python:
  blender -b "DancingBaby.blend" -P tools/export_blender_retro3d.py -- dancer "C:/SpectrumWall/assets/retro3d/dancer"
  blender -b "CokeCan.blend" -P tools/export_blender_retro3d.py -- can "C:/SpectrumWall/assets/retro3d/can"

Supports Blender 4.x's built-in wavefront OBJ exporter; for older Blender
3.x use the bundled io_scene_obj add-on if available. Do not commit these
exported models or textures to the public project.
"""
import pathlib
import sys

import bpy


def export_obj(path: pathlib.Path):
    path.parent.mkdir(parents=True, exist_ok=True)
    if hasattr(bpy.ops.wm, "obj_export"):
        op = bpy.ops.wm.obj_export
        available = {p.identifier for p in op.get_rna_type().properties}
        settings = {
            "filepath": str(path),
            "export_selected_objects": False,
            "apply_modifiers": True,
            "export_materials": True,
            "path_mode": "COPY",
        }
        op(**{key: value for key, value in settings.items() if key in available})
    elif hasattr(bpy.ops.export_scene, "obj"):
        bpy.ops.export_scene.obj(
            filepath=str(path), use_selection=False,
            use_mesh_modifiers=True, use_materials=True,
            path_mode="COPY",
        )
    else:
        raise RuntimeError(
            "OBJ export is not available. Enable Blender's Wavefront OBJ exporter."
        )


def main():
    if "--" not in sys.argv:
        raise SystemExit("Expected: -- dancer|can OUTPUT_DIRECTORY [MAX_FRAMES]")
    args = sys.argv[sys.argv.index("--") + 1 :]
    if len(args) < 2:
        raise SystemExit("Expected: -- dancer|can OUTPUT_DIRECTORY [MAX_FRAMES]")
    kind = args[0].lower()
    output = pathlib.Path(args[1]).expanduser().resolve()
    maximum = max(1, min(96, int(args[2]) if len(args) > 2 else 72))
    scene = bpy.context.scene

    # Blender loads an animated character as an armature + evaluated meshes.
    # Exporting each evaluated frame retains the exact deformations without
    # requiring SpectrumWall to interpret legacy Character Studio BIP rigs.
    if kind == "dancer":
        folder = output / "frames"
        frame_first = int(scene.frame_start)
        frame_last = int(scene.frame_end)
        if frame_last < frame_first:
            frame_last = frame_first
        frames = min(maximum, frame_last - frame_first + 1)
        for frame in range(frame_first, frame_first + frames):
            scene.frame_set(frame)
            path = folder / f"frame_{frame-frame_first:04d}.obj"
            export_obj(path)
            print(f"[{frame-frame_first+1}/{frames}] {path}")
        print(f"Exported {frames} private dance frames")
    elif kind == "can":
        export_obj(output / "can.obj")
        print("Exported private rotating can model")
    else:
        raise SystemExit(f"Unknown model kind: {kind}")

    print("Keep the complete OBJ + MTL + texture folder together.")
    print("Do not add your private model folders to Git.")


if __name__ == "__main__":
    main()
