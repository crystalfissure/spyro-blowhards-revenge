"""Import one merged, simple-collision asset per WF dynamic collision FBX."""

from __future__ import print_function

import json
import os
import unreal


SOURCE_DIR = os.path.normpath(
    r"C:\Users\adace\Desktop\spyro-blowhards-revenge\Content\Spyro64"
    r"\SM64\Source\WhompsFortress\FBX\Collision"
)
DESTINATION = "/Game/Spyro64/SM64/WhompsFortress/Meshes/CollisionDynamic"
SKIP_PREFIXES = ("SM_WF_COL_Area1_",)


def import_options():
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    data = options.static_mesh_import_data
    data.combine_meshes = True
    data.generate_lightmap_u_vs = False
    data.auto_generate_collision = False
    data.remove_degenerates = False
    data.import_uniform_scale = 100.0
    return options


def main():
    tasks = []
    for filename in sorted(os.listdir(SOURCE_DIR)):
        if not filename.lower().endswith(".fbx") or filename.startswith(SKIP_PREFIXES):
            continue
        task = unreal.AssetImportTask()
        task.filename = os.path.join(SOURCE_DIR, filename)
        task.destination_path = DESTINATION
        task.destination_name = os.path.splitext(filename)[0]
        task.automated = True
        task.replace_existing = True
        task.replace_existing_settings = True
        task.save = True
        task.options = import_options()
        tasks.append(task)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    report = []
    for task in tasks:
        for object_path in task.imported_object_paths:
            asset_path = str(object_path).split(".", 1)[0]
            mesh = unreal.load_asset(asset_path)
            if not isinstance(mesh, unreal.StaticMesh):
                continue
            # These are already low-poly authoritative collision sources. The
            # editor helper builds one hull per source section and gives flat
            # SM64 collision sheets only enough thickness for PhysX cooking.
            generated_simple = int(
                unreal.SM64EditorReferenceLibrary.build_simple_convex_collision_from_source(
                    mesh, 2.0
                )
            )
            body_setup = mesh.get_editor_property("body_setup")
            if body_setup:
                body_setup.set_editor_property(
                    "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT
                )
            mesh.modify()
            unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
            report.append({"asset": asset_path, "simple_hulls": generated_simple})

    unreal.log_warning("SM64_WF_DYNAMIC_COLLISION=" + json.dumps(report, sort_keys=True))


main()
