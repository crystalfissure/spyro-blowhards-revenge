"""Idempotently import and configure the unplaced Green Druid asset set.

Run inside UE 4.27's PythonScript commandlet. This script deliberately never
loads or saves the production Magic Crafters map.
"""

import importlib.util
import os
import re

import unreal


PROJECT_ROOT = r"C:\Users\adace\Desktop\spyro-blowhards-revenge"
SOURCE_ROOT = (
    r"C:\Users\adace\Downloads\Spyro OT Asset Dump\Spyro OT Assets 2026-01-02"
)
ENEMY_SOURCE = os.path.join(
    SOURCE_ROOT,
    r"S1\Enemies\Home_02_Magic_Crafters\00_MagicCrafters\Green Druid.fbx",
)
TEXTURE_SOURCE = os.path.join(
    SOURCE_ROOT,
    r"S1\Enemies\Home_02_Magic_Crafters\00_MagicCrafters\Green Druid_T.png",
)
LEVEL_SOURCE = os.path.join(
    SOURCE_ROOT,
    r"S1\Levels\Home_02_Magic_Crafters\00_MagicCrafters\Magic Crafters_EDIT.fbx",
)

ENEMY_ROOT = (
    "/Game/OT_Ports/S1/S1_Enemies/Home_02_Magic_Crafters/"
    "00_MagicCrafters/Green_Druid"
)
GEOMETRY_ROOT = ENEMY_ROOT + "/Moving_Geometry"
RAW_GEOMETRY_ROOT = GEOMETRY_ROOT + "/Raw"
BASE_ENEMY = (
    "/Game/SpyroContent/Global_Assets/Global_Characters/"
    "AI_Characters/Actors/Enemies/Base_Enemy_BP"
)
GREEN_DRUID_BP = ENEMY_ROOT + "/Green_Druid_BP"
LEVEL_MATERIAL = (
    "/Game/OT_Ports/S1/S1_Levels/Home_02_Magic_Crafters/"
    "00_MagicCrafters/Textures/MI_Magic_Crafters"
)


def log(message):
    unreal.log_warning("[green-druid] " + str(message))


def require_file(path):
    if not os.path.isfile(path):
        raise RuntimeError("Required source file is missing: {}".format(path))


def load_pipeline():
    path = os.path.join(PROJECT_ROOT, "Tools", "AssetPipeline", "ue_asset_pipeline.py")
    spec = importlib.util.spec_from_file_location("green_druid_asset_pipeline", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def assets_under(folder):
    result = []
    for path in unreal.EditorAssetLibrary.list_assets(folder, recursive=True, include_folder=False):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if asset:
            result.append(asset)
    return result


def load_if_exists(path):
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        return None
    return unreal.EditorAssetLibrary.load_asset(path)


def import_skeletal_fbx(source, destination, destination_name):
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_as_skeletal", True)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    options.set_editor_property("import_animations", True)
    data = options.get_editor_property("skeletal_mesh_import_data")
    data.set_editor_property("import_morph_targets", False)
    data.set_editor_property("preserve_smoothing_groups", True)
    data.set_editor_property("import_meshes_in_bone_hierarchy", True)
    data.set_editor_property("update_skeleton_reference_pose", False)
    data.set_editor_property("use_t0_as_ref_pose", False)
    data.set_editor_property("import_uniform_scale", 1.0)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", destination_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", False)
    task.set_editor_property("options", options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = list(task.get_editor_property("imported_object_paths"))
    log("FBX import produced {} objects from {}".format(len(imported), os.path.basename(source)))
    return imported


def ensure_enemy_assets(pipeline):
    current = assets_under(ENEMY_ROOT)
    meshes = [asset for asset in current if isinstance(asset, unreal.SkeletalMesh)]
    takes = [asset for asset in current if isinstance(asset, unreal.AnimSequence)]
    # Geometry assets live below ENEMY_ROOT too, so distinguish by package path.
    meshes = [mesh for mesh in meshes if "/Moving_Geometry/" not in mesh.get_path_name()]
    takes = [take for take in takes if "/Moving_Geometry/" not in take.get_path_name()]
    if len(meshes) != 1 or len(takes) < 10:
        import_skeletal_fbx(ENEMY_SOURCE, ENEMY_ROOT, "Green_Druid")
        current = assets_under(ENEMY_ROOT)
        meshes = [asset for asset in current if isinstance(asset, unreal.SkeletalMesh)
                  and "/Moving_Geometry/" not in asset.get_path_name()]
        takes = [asset for asset in current if isinstance(asset, unreal.AnimSequence)
                 and "/Moving_Geometry/" not in asset.get_path_name()]
    if len(meshes) != 1:
        raise RuntimeError("Expected one Green Druid skeletal mesh, found {}".format(len(meshes)))
    if len(takes) != 10:
        raise RuntimeError("Expected ten Green Druid raw takes, found {}".format(len(takes)))

    texture = load_if_exists(ENEMY_ROOT + "/Green_Druid_T")
    if not texture:
        texture = pipeline.import_texture(
            TEXTURE_SOURCE,
            ENEMY_ROOT,
            "Green_Druid_T",
            {
                "filter": "nearest",
                "mip_gen": "no_mipmaps",
                "compression": "uncompressed",
                "address": "wrap",
                "srgb": True,
            },
            replace=True,
        )
    material = load_if_exists(ENEMY_ROOT + "/Green_Druid_Mat")
    if not material:
        material = pipeline.build_material(
            "Green_Druid_Mat",
            ENEMY_ROOT,
            {"base_color": texture},
            {
                "shading_model": "unlit",
                "blend_mode": "masked",
                "opacity_mask_clip_value": 0.333,
                "two_sided": True,
                "use_vertex_color": True,
                "vertex_color_scale": 2.0,
            },
            replace=False,
        )
    pipeline.assign_material(meshes[0], material)

    def take_number(asset):
        matches = re.findall(r"Anim(\d+)", asset.get_name(), re.IGNORECASE)
        return int(matches[-1]) if matches else 999

    by_number = {take_number(take): take for take in takes}
    missing = [number for number in range(10) if number not in by_number]
    if missing:
        raise RuntimeError("Green Druid raw take numbers missing: {}".format(missing))
    for number in sorted(by_number):
        take = by_number[number]
        length = take.get_play_length() if hasattr(take, "get_play_length") else "unknown"
        log("raw take Anim{}: {} seconds ({})".format(number, length, take.get_path_name()))

    # The chosen slots retain their raw Anim numbers in the assigned paths.
    slots = {
        "idle": by_number[0],
        "raise": by_number[9],
        "lower": by_number[9],
        "death": by_number[8],
    }
    log("semantic slots: idle=Anim0, raise=Anim9, lower=Anim9 reversed, death=Anim8")
    return meshes[0], slots


def ensure_geometry_assets(pipeline):
    current = assets_under(RAW_GEOMETRY_ROOT)
    meshes = [asset for asset in current if isinstance(asset, unreal.SkeletalMesh)]
    if len(meshes) != 2:
        import_skeletal_fbx(LEVEL_SOURCE, RAW_GEOMETRY_ROOT, "MC_Druid_Channel")
        current = assets_under(RAW_GEOMETRY_ROOT)
        meshes = [asset for asset in current if isinstance(asset, unreal.SkeletalMesh)]
    static_meshes = [asset for asset in current if isinstance(asset, unreal.StaticMesh)]
    if static_meshes:
        raise RuntimeError(
            "Level FBX import unexpectedly created static/full-terrain meshes: {}".format(
                [asset.get_path_name() for asset in static_meshes]
            )
        )
    if len(meshes) != 2:
        raise RuntimeError("Expected exactly two skinned channel meshes, found {}".format(len(meshes)))

    level_material = unreal.EditorAssetLibrary.load_asset(LEVEL_MATERIAL)
    if level_material:
        for mesh in meshes:
            pipeline.assign_material(mesh, level_material)

    animations = [asset for asset in current if isinstance(asset, unreal.AnimSequence)]
    variants = []
    for letter, mesh in zip(("A", "B"), sorted(meshes, key=lambda asset: asset.get_name())):
        skeleton = mesh.get_editor_property("skeleton")
        related = sorted(
            [animation for animation in animations
             if animation.get_editor_property("skeleton") == skeleton],
            key=lambda asset: asset.get_name(),
        )
        if not related:
            raise RuntimeError("No lift tracks found for channel {}".format(mesh.get_path_name()))
        semantic_path = GEOMETRY_ROOT + "/MC_Druid_Channel_{}_Lift".format(letter)
        lift = load_if_exists(semantic_path)
        if not lift:
            if not unreal.EditorAssetLibrary.duplicate_asset(related[0].get_path_name(), semantic_path):
                raise RuntimeError("Could not create {}".format(semantic_path))
            lift = unreal.EditorAssetLibrary.load_asset(semantic_path)
        for additional in related[1:]:
            unreal.MMAEditorAnimationLibrary.merge_animation_tracks(lift, additional)
        lift_bone = unreal.MMAEditorAnimationLibrary.find_largest_translation_track_bone(lift)
        if not lift_bone or str(lift_bone) == "None":
            raise RuntimeError("No animated lift bone found for channel {}".format(letter))
        unreal.EditorAssetLibrary.save_loaded_asset(lift, only_if_is_dirty=False)
        variants.append((letter, mesh, lift, lift_bone, related))
        log("channel {}: mesh={}, sources={}, lift_bone={}".format(
            letter,
            mesh.get_path_name(),
            [asset.get_name() for asset in related],
            lift_bone,
        ))
    return variants


def ensure_platform_blueprint(letter, mesh, lift, lift_bone):
    path = GEOMETRY_ROOT + "/BP_Green_Druid_Channel_{}".format(letter)
    blueprint = load_if_exists(path)
    if not blueprint:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.MMAGreenDruidPlatform)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "BP_Green_Druid_Channel_{}".format(letter),
            GEOMETRY_ROOT,
            unreal.Blueprint,
            factory,
        )
    if not blueprint:
        raise RuntimeError("Could not create {}".format(path))
    if not unreal.MMAEditorAnimationLibrary.compile_blueprint(blueprint):
        raise RuntimeError("Initial compile failed for {}".format(path))
    if not unreal.MMAEditorAnimationLibrary.configure_mma_green_druid_platform(
        blueprint, mesh, lift, lift_bone
    ):
        raise RuntimeError("Could not configure {}".format(path))
    if not unreal.MMAEditorAnimationLibrary.compile_blueprint(blueprint):
        raise RuntimeError("Configured compile failed for {}".format(path))
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
    log(unreal.MMAEditorAnimationLibrary.describe_mma_green_druid_blueprint(blueprint))
    return blueprint


def ensure_enemy_blueprint(mesh, slots):
    blueprint = load_if_exists(GREEN_DRUID_BP)
    if not blueprint:
        if not unreal.EditorAssetLibrary.duplicate_asset(BASE_ENEMY, GREEN_DRUID_BP):
            raise RuntimeError("Could not duplicate Base_Enemy_BP")
        blueprint = unreal.EditorAssetLibrary.load_asset(GREEN_DRUID_BP)
    if not blueprint:
        raise RuntimeError("Could not load Green_Druid_BP")
    if not unreal.MMAEditorAnimationLibrary.add_mma_green_druid_behavior_component(blueprint):
        raise RuntimeError("Could not add Green Druid behavior component")
    if not unreal.MMAEditorAnimationLibrary.configure_mma_green_druid_behavior(
        blueprint,
        slots["idle"],
        slots["raise"],
        slots["lower"],
        slots["death"],
        None,
    ):
        raise RuntimeError("Could not configure Green Druid behavior")
    if not unreal.MMAEditorAnimationLibrary.compile_blueprint(blueprint):
        raise RuntimeError("Green_Druid_BP failed its first compile")
    if not unreal.MMAEditorAnimationLibrary.configure_mma_hedge_trimmer_mesh(
        blueprint,
        mesh,
        slots["idle"],
        unreal.Vector(0.0, 0.0, -88.0),
        unreal.Rotator(0.0, -90.0, 0.0),
        unreal.Vector(1.0, 1.0, 1.0),
    ):
        raise RuntimeError("Could not assign Green Druid skeletal mesh")
    if not unreal.MMAEditorAnimationLibrary.compile_blueprint(blueprint):
        raise RuntimeError("Green_Druid_BP failed its configured compile")
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
    log(unreal.MMAEditorAnimationLibrary.describe_mma_green_druid_blueprint(blueprint))
    return blueprint


def main():
    for source in (ENEMY_SOURCE, TEXTURE_SOURCE, LEVEL_SOURCE):
        require_file(source)
    pipeline = load_pipeline()
    mesh, slots = ensure_enemy_assets(pipeline)
    variants = ensure_geometry_assets(pipeline)
    ensure_enemy_blueprint(mesh, slots)
    for letter, channel_mesh, lift, lift_bone, _ in variants:
        ensure_platform_blueprint(letter, channel_mesh, lift, lift_bone)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("GREEN_DRUID_SETUP_OK")


main()
