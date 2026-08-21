"""Idempotently import the validated SM64 actor FBX/texture catalog into UE4.27."""

from __future__ import print_function

import json
import os
import re
import unreal


SOURCE_ROOT = os.path.normpath(
    r"C:\Users\adace\Desktop\spyro-blowhards-revenge\Content\Spyro64"
    r"\SM64\Source\Actors"
)
CONTENT_ROOT = "/Game/Spyro64/SM64/Common"
ACTOR_ROOT = CONTENT_ROOT + "/Actors"
MASTER_ROOT = CONTENT_ROOT + "/Materials/Masters"
INSTANCE_ROOT = CONTENT_ROOT + "/Materials/Instances"
WHITE_TEXTURE = "/Engine/EngineResources/WhiteSquareTexture"
ACTOR_TINTS = {
    "yellow_coin": unreal.LinearColor(1.0, 0.82, 0.0, 1.0),
    "blue_coin": unreal.LinearColor(0.10, 0.35, 1.0, 1.0),
    "red_coin": unreal.LinearColor(1.0, 0.12, 0.08, 1.0),
}


def safe_name(value):
    return re.sub(r"[^A-Za-z0-9_]+", "_", value).strip("_")


def title_name(value):
    return "".join(part.capitalize() for part in value.split("_"))


def load_json(path):
    with open(path, "r") as stream:
        return json.load(stream)


def require_asset(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing required asset " + path)
    return asset


def import_texture(actor, filename):
    source = os.path.join(SOURCE_ROOT, actor, "textures", filename)
    destination = ACTOR_ROOT + "/" + title_name(actor) + "/Textures"
    name = "T_SM64_{}_{}".format(actor, safe_name(os.path.splitext(filename)[0]))
    task = unreal.AssetImportTask()
    task.filename = source
    task.destination_path = destination
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(destination + "/" + name)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError("Texture import failed: " + source)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("srgb", True)
    texture.modify()
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    return texture


def mesh_import_options(skeletal):
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = bool(skeletal)
    options.import_materials = False
    options.import_textures = False
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = (
        unreal.FBXImportType.FBXIT_SKELETAL_MESH
        if skeletal
        else unreal.FBXImportType.FBXIT_STATIC_MESH
    )
    if skeletal:
        # Import animation takes in a second, explicit pass.  UE4's automated
        # skeletal-mesh importer creates AnimSequence objects while processing
        # an FBX with multiple takes, but only returns/saves the mesh package.
        options.import_animations = False
        options.skeleton = None
        data = options.skeletal_mesh_import_data
        data.import_uniform_scale = 100.0
    else:
        data = options.static_mesh_import_data
        data.combine_meshes = True
        data.generate_lightmap_u_vs = False
        data.auto_generate_collision = False
        data.remove_degenerates = False
        data.import_uniform_scale = 100.0
    return options


def import_mesh(actor, record, output_name=None):
    role = record["role"]
    skeletal = role == "skeletal"
    category = "Skeletal" if skeletal else "Static"
    destination = ACTOR_ROOT + "/" + title_name(actor) + "/" + category
    prefix = "SK" if skeletal else "SM"
    name = output_name or "{}_SM64_{}".format(prefix, title_name(actor))
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SOURCE_ROOT, actor, record["path"].replace("/", os.sep))
    task.destination_path = destination
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = True
    task.options = mesh_import_options(skeletal)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(destination + "/" + name)
    expected_class = unreal.SkeletalMesh if skeletal else unreal.StaticMesh
    if not isinstance(mesh, expected_class):
        raise RuntimeError("{} import failed for {}".format(role, actor))
    return mesh, [str(path) for path in task.imported_object_paths]


def import_animations(actor, record, mesh, expected_actions):
    """Persist the FBX takes as AnimSequence packages on the mesh skeleton."""
    if not expected_actions:
        return [], None
    skeleton = mesh.get_editor_property("skeleton")
    if skeleton is None:
        raise RuntimeError("Skeletal mesh has no skeleton: " + actor)
    skeleton.modify()
    if not unreal.EditorAssetLibrary.save_loaded_asset(skeleton, only_if_is_dirty=False):
        raise RuntimeError("Unable to save skeleton for " + actor)

    destination = ACTOR_ROOT + "/" + title_name(actor) + "/Animations"
    options = unreal.FbxImportUI()
    options.import_mesh = False
    options.import_as_skeletal = True
    options.import_materials = False
    options.import_textures = False
    options.import_animations = True
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    options.skeleton = skeleton
    options.anim_sequence_import_data.import_uniform_scale = 100.0

    task = unreal.AssetImportTask()
    task.filename = os.path.join(SOURCE_ROOT, actor, record["path"].replace("/", os.sep))
    task.destination_path = destination
    task.destination_name = "A_SM64_" + title_name(actor)
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = True
    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    animation_paths = []
    for path in unreal.EditorAssetLibrary.list_assets(destination, recursive=False):
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.AnimSequence):
            asset.modify()
            unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
            animation_paths.append(asset.get_path_name())
    if len(animation_paths) != len(expected_actions):
        raise RuntimeError(
            "{} expected {} animations, imported {}: {}".format(
                actor, len(expected_actions), len(animation_paths), animation_paths
            )
        )
    return sorted(animation_paths), skeleton.get_path_name()


def import_collision(actor, record):
    destination = ACTOR_ROOT + "/" + title_name(actor) + "/Collision"
    name = "SM64_COL_" + title_name(actor)
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SOURCE_ROOT, actor, record["path"].replace("/", os.sep))
    task.destination_path = destination
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = True
    task.options = mesh_import_options(False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(destination + "/" + name)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Collision import failed for " + actor)
    hulls = int(
        unreal.SM64EditorReferenceLibrary.build_simple_convex_collision_from_source(mesh, 2.0)
    )
    if hulls < 1:
        raise RuntimeError("No simple collision hulls generated for " + actor)
    mesh.modify()
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
    return mesh, hulls


def material_states(actor_data):
    states = {}
    index = 0
    for render in actor_data["render_meshes"].values():
        for draw in render["draw_calls"]:
            states[index] = draw["material"]
            index += 1
    return states


def ensure_instance(actor, material_index, state, textures):
    name = "MI_SM64_{}_{:03d}".format(actor, material_index)
    path = INSTANCE_ROOT + "/" + title_name(actor)
    instance = unreal.load_asset(path + "/" + name)
    if instance is None:
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name,
            path,
            unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew(),
        )
    if instance is None:
        raise RuntimeError("Unable to create material instance " + name)
    layer = state.get("layer", "LAYER_OPAQUE")
    parent_name = "M_SM64_MaskedTwoSided" if layer == "LAYER_ALPHA" else "M_SM64_Opaque"
    instance.set_editor_property("parent", require_asset(MASTER_ROOT + "/" + parent_name))
    texture_asset = state.get("texture_asset")
    texture = textures.get(os.path.basename(texture_asset)) if texture_asset else None
    if texture is None:
        texture = require_asset(WHITE_TEXTURE)
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
        instance, "Texture", texture
    )
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        instance,
        "Tint",
        ACTOR_TINTS.get(actor, unreal.LinearColor(1.0, 1.0, 1.0, 1.0)),
    )
    instance.modify()
    unreal.EditorAssetLibrary.save_loaded_asset(instance, only_if_is_dirty=False)
    return instance


def assign_materials(actor, mesh, actor_data, textures):
    states = material_states(actor_data)
    if isinstance(mesh, unreal.StaticMesh):
        slots = mesh.get_editor_property("static_materials")
        assigned = []
        for index, slot in enumerate(slots):
            slot_name = str(slot.get_editor_property("material_slot_name"))
            match = re.match(r"^M_{}_(\d+)$".format(re.escape(actor)), slot_name)
            if not match:
                raise RuntimeError("Unexpected {} material slot {}".format(actor, slot_name))
            material_index = int(match.group(1))
            instance = ensure_instance(actor, material_index, states[material_index], textures)
            mesh.set_material(index, instance)
            assigned.append(slot_name)
    else:
        slots = list(mesh.get_editor_property("materials"))
        assigned = []
        for slot in slots:
            slot_name = str(slot.get_editor_property("material_slot_name"))
            match = re.match(r"^M_{}_(\d+)$".format(re.escape(actor)), slot_name)
            if not match:
                raise RuntimeError("Unexpected {} material slot {}".format(actor, slot_name))
            material_index = int(match.group(1))
            instance = ensure_instance(actor, material_index, states[material_index], textures)
            slot.set_editor_property("material_interface", instance)
            assigned.append(slot_name)
        mesh.set_editor_property("materials", slots)
    mesh.modify()
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
    return assigned


def main():
    require_asset(MASTER_ROOT + "/M_SM64_Opaque")
    require_asset(MASTER_ROOT + "/M_SM64_MaskedTwoSided")
    report = []
    for actor in sorted(
        name for name in os.listdir(SOURCE_ROOT)
        if os.path.isfile(os.path.join(SOURCE_ROOT, name, name + ".exports.json"))
    ):
        actor_dir = os.path.join(SOURCE_ROOT, actor)
        exports = load_json(os.path.join(actor_dir, actor + ".exports.json"))
        actor_data = load_json(os.path.join(actor_dir, actor + ".sm64actor.json"))
        textures = {}
        texture_dir = os.path.join(actor_dir, "textures")
        for filename in sorted(os.listdir(texture_dir)) if os.path.isdir(texture_dir) else []:
            if filename.lower().endswith(".png"):
                textures[filename] = import_texture(actor, filename)
        primary = next(
            record for record in exports["exports"]
            if record["role"] in ("skeletal", "static") and record["variant"] == "primary"
        )
        mesh, imported_paths = import_mesh(actor, primary)
        assigned = assign_materials(actor, mesh, actor_data, textures)
        variant_meshes = []
        if actor == "exclamation_box":
            metal_record = next(
                record for record in exports["exports"]
                if record["role"] == "static" and record["variant"] == "switch_00_case_01"
            )
            metal_mesh, metal_imported_paths = import_mesh(
                actor, metal_record, "SM_SM64_ExclamationBox_MetalCap"
            )
            metal_assigned = assign_materials(actor, metal_mesh, actor_data, textures)
            metal_hulls = int(
                unreal.SM64EditorReferenceLibrary.build_simple_convex_collision_from_source(
                    metal_mesh, 2.0
                )
            )
            if metal_hulls < 1:
                raise RuntimeError("No simple collision hull generated for metal cap box")
            unreal.EditorAssetLibrary.save_loaded_asset(metal_mesh, only_if_is_dirty=False)
            variant_meshes.append(
                {
                    "variant": metal_record["variant"],
                    "mesh": metal_mesh.get_path_name(),
                    "material_slots": metal_assigned,
                    "collision_hulls": metal_hulls,
                    "imported_objects": metal_imported_paths,
                }
            )
        expected_actions = exports.get("rig", {}).get("actions", [])
        animation_paths, skeleton_path = import_animations(
            actor, primary, mesh, expected_actions
        ) if primary["role"] == "skeletal" else ([], None)
        collision_record = next(
            (record for record in exports["exports"] if record["role"] == "collision"), None
        )
        collision_path = None
        hulls = 0
        if collision_record:
            collision, hulls = import_collision(actor, collision_record)
            collision_path = collision.get_path_name()
        report.append(
            {
                "actor": actor,
                "role": primary["role"],
                "mesh": mesh.get_path_name(),
                "triangles": int(primary["triangle_count"]),
                "bones": int(primary.get("bone_count", 0)),
                "animations": int(primary.get("animation_count", 0)),
                "animation_assets": animation_paths,
                "skeleton": skeleton_path,
                "texture_count": len(textures),
                "material_slots": assigned,
                "collision": collision_path,
                "collision_hulls": hulls,
                "imported_objects": imported_paths,
                "variant_meshes": variant_meshes,
            }
        )
    if len(report) != 22:
        raise RuntimeError("Expected 22 actors, imported {}".format(len(report)))
    unreal.log_warning("SM64_COMMON_ACTOR_IMPORT=" + json.dumps(report, sort_keys=True))


main()
