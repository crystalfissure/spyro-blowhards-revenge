"""Idempotently import the validated Whomp's Fortress PNG/FBX deliverables."""

from __future__ import print_function

import json
import os
import unreal


SOURCE_ROOT = os.path.normpath(
    r"C:\Users\adace\Desktop\spyro-blowhards-revenge\Content\Spyro64\SM64\Source\WhompsFortress"
)
DEST_ROOT = "/Game/Spyro64/SM64/WhompsFortress"
SOURCE_TEXTURE_ASSET_ROOT = "/Game/Spyro64/SM64/Source/WhompsFortress/Textures"


def import_textures():
    texture_dir = os.path.join(SOURCE_ROOT, "Textures")
    tasks = []
    for filename in sorted(os.listdir(texture_dir)):
        if not filename.lower().endswith(".png"):
            continue
        task = unreal.AssetImportTask()
        task.filename = os.path.join(texture_dir, filename)
        task.destination_path = DEST_ROOT + "/Textures"
        task.destination_name = os.path.splitext(filename)[0]
        task.automated = True
        task.replace_existing = True
        task.replace_existing_settings = True
        task.save = True
        tasks.append(task)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    imported = []
    for task in tasks:
        imported.extend(str(path) for path in task.imported_object_paths)

    for object_path in imported:
        texture = unreal.load_asset(object_path.split(".", 1)[0])
        if not isinstance(texture, unreal.Texture2D):
            continue
        texture.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
        texture.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
        texture.set_editor_property("address_y", unreal.TextureAddress.TA_WRAP)
        texture.set_editor_property("lod_bias", 0)
        texture.modify()
        unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    return sorted(set(path.split(".", 1)[0] for path in imported))


def make_fbx_options(combine_meshes, generate_collision):
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    static_options = options.static_mesh_import_data
    static_options.combine_meshes = combine_meshes
    static_options.generate_lightmap_u_vs = False
    static_options.auto_generate_collision = generate_collision
    static_options.import_mesh_lo_ds = False
    static_options.remove_degenerates = False
    # Blender scene coordinates are authoritative metres; SM64/UE placements are cm.
    static_options.import_uniform_scale = 100.0
    return options


def import_fbx_directory(category, combine_meshes, generate_collision=False):
    source_dir = os.path.join(SOURCE_ROOT, "FBX", category)
    destination = DEST_ROOT + "/Meshes/" + category
    tasks = []
    for filename in sorted(os.listdir(source_dir)):
        if not filename.lower().endswith(".fbx"):
            continue
        task = unreal.AssetImportTask()
        task.filename = os.path.join(source_dir, filename)
        task.destination_path = destination
        task.destination_name = os.path.splitext(filename)[0]
        task.automated = True
        task.replace_existing = True
        task.replace_existing_settings = True
        task.save = True
        task.options = make_fbx_options(combine_meshes, generate_collision)
        tasks.append(task)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    paths = []
    for task in tasks:
        paths.extend(str(path).split(".", 1)[0] for path in task.imported_object_paths)
    return sorted(set(paths))


def configure_collision_assets(asset_paths):
    configured = []
    for asset_path in asset_paths:
        mesh = unreal.load_asset(asset_path)
        if not isinstance(mesh, unreal.StaticMesh):
            continue
        collision_sections = int(
            unreal.SM64EditorReferenceLibrary.configure_complex_as_simple_collision(mesh)
        )
        if collision_sections < 1:
            raise RuntimeError("No complex collision sections were enabled for " + asset_path)
        mesh.modify()
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
        configured.append(asset_path)
    return configured


def remove_accidental_source_texture_assets(texture_asset_paths):
    removed = []
    for target_path in texture_asset_paths:
        name = target_path.rsplit("/", 1)[-1]
        source_path = SOURCE_TEXTURE_ASSET_ROOT + "/" + name
        if unreal.EditorAssetLibrary.does_asset_exist(source_path):
            if unreal.EditorAssetLibrary.delete_asset(source_path):
                removed.append(source_path)
    return removed


def main():
    if not os.path.isfile(os.path.join(SOURCE_ROOT, "Manifest", "wf_placements.json")):
        raise RuntimeError("Validated WF source root is incomplete: " + SOURCE_ROOT)

    report = {
        "textures": import_textures(),
        "meshes": {},
        "removed_accidental_source_assets": [],
    }
    report["meshes"]["Static"] = import_fbx_directory("Static", True)
    report["meshes"]["Water"] = import_fbx_directory("Water", True)
    report["meshes"]["Movers"] = import_fbx_directory("Movers", True)
    report["meshes"]["Conditional"] = import_fbx_directory("Conditional", True)
    collision_assets = import_fbx_directory("Collision", False, False)
    report["meshes"]["Collision"] = configure_collision_assets(collision_assets)
    report["removed_accidental_source_assets"] = remove_accidental_source_texture_assets(
        report["textures"]
    )

    expected_fbx = sum(
        1
        for root, _, files in os.walk(os.path.join(SOURCE_ROOT, "FBX"))
        for filename in files
        if filename.lower().endswith(".fbx")
    )
    report["expected_fbx_files"] = expected_fbx
    report["imported_mesh_assets"] = sum(
        len(paths) for paths in report["meshes"].values()
    )
    unreal.log_warning("SM64_WF_IMPORT=" + json.dumps(report, sort_keys=True))


main()
