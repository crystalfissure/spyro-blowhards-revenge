"""Build unlit SM64 material masters/instances and assign every WF render slot."""

from __future__ import print_function

import json
import re
import unreal


ROOT = "/Game/Spyro64/SM64/WhompsFortress"
MASTER_DIR = "/Game/Spyro64/SM64/Common/Materials/Masters"
INSTANCE_DIR = ROOT + "/Materials/Instances"
TEXTURE_DIR = ROOT + "/Textures"
MESH_ROOT = ROOT + "/Meshes"


def get_or_create(name, package_path, asset_class, factory):
    asset_path = package_path + "/" + name
    asset = unreal.load_asset(asset_path)
    if asset:
        return asset
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, package_path, asset_class, factory
    )


def expression(material, expression_class, x, y):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material, expression_class, x, y
    )
    if not node:
        raise RuntimeError("Unable to create {} in {}".format(expression_class, material))
    return node


def build_master(name, blend_mode, two_sided=False, animated_uv=False):
    material = get_or_create(name, MASTER_DIR, unreal.Material, unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("blend_mode", blend_mode)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", two_sided)
    material.set_editor_property("use_material_attributes", False)
    if blend_mode == unreal.BlendMode.BLEND_MASKED:
        material.set_editor_property("opacity_mask_clip_value", 0.333)

    texture = expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -700, -50)
    texture.set_editor_property("parameter_name", "Texture")
    vertex = expression(material, unreal.MaterialExpressionVertexColor, -700, 180)
    color_mul = expression(material, unreal.MaterialExpressionMultiply, -320, -20)
    unreal.MaterialEditingLibrary.connect_material_expressions(texture, "RGB", color_mul, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(vertex, "RGB", color_mul, "B")
    tint = expression(material, unreal.MaterialExpressionVectorParameter, -320, 120)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    tinted_color = expression(material, unreal.MaterialExpressionMultiply, -40, -20)
    unreal.MaterialEditingLibrary.connect_material_expressions(color_mul, "", tinted_color, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(tint, "", tinted_color, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        tinted_color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )

    if animated_uv:
        texcoord = expression(material, unreal.MaterialExpressionTextureCoordinate, -1100, -160)
        panner = expression(material, unreal.MaterialExpressionPanner, -900, -160)
        panner.set_editor_property("speed_x", 0.025)
        panner.set_editor_property("speed_y", 0.0125)
        unreal.MaterialEditingLibrary.connect_material_expressions(texcoord, "", panner, "Coordinate")
        unreal.MaterialEditingLibrary.connect_material_expressions(panner, "", texture, "UVs")

    if blend_mode in (unreal.BlendMode.BLEND_MASKED, unreal.BlendMode.BLEND_TRANSLUCENT):
        alpha_mul = expression(material, unreal.MaterialExpressionMultiply, -310, 230)
        unreal.MaterialEditingLibrary.connect_material_expressions(texture, "A", alpha_mul, "A")
        unreal.MaterialEditingLibrary.connect_material_expressions(vertex, "A", alpha_mul, "B")
        alpha_output = alpha_mul
        if blend_mode == unreal.BlendMode.BLEND_TRANSLUCENT:
            alpha_scale = expression(material, unreal.MaterialExpressionScalarParameter, -310, 390)
            alpha_scale.set_editor_property("parameter_name", "AlphaMultiplier")
            alpha_scale.set_editor_property("default_value", 1.0)
            scaled_alpha = expression(material, unreal.MaterialExpressionMultiply, -40, 260)
            unreal.MaterialEditingLibrary.connect_material_expressions(alpha_mul, "", scaled_alpha, "A")
            unreal.MaterialEditingLibrary.connect_material_expressions(alpha_scale, "", scaled_alpha, "B")
            alpha_output = scaled_alpha
        unreal.MaterialEditingLibrary.connect_material_property(
            alpha_output,
            "",
            unreal.MaterialProperty.MP_OPACITY_MASK
            if blend_mode == unreal.BlendMode.BLEND_MASKED
            else unreal.MaterialProperty.MP_OPACITY,
        )

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def parent_for_slot(slot_name, masters):
    if slot_name.endswith("_masked_two_sided"):
        return masters["masked"], 1.0
    if slot_name.endswith("_translucent_yellow"):
        return masters["translucent"], 120.0 / 255.0
    if slot_name.endswith("_water"):
        return masters["water"], 120.0 / 255.0
    return masters["opaque"], 1.0


def create_instance(slot_name, masters):
    match = re.match(r"^M_WF_(tex_[0-9A-Fa-f]+)_", slot_name)
    if not match:
        raise RuntimeError("Unrecognized WF material slot " + slot_name)
    texture = unreal.load_asset(TEXTURE_DIR + "/" + match.group(1))
    if not texture:
        raise RuntimeError("Missing texture for " + slot_name)
    parent, alpha = parent_for_slot(slot_name, masters)
    instance = get_or_create(
        slot_name,
        INSTANCE_DIR,
        unreal.MaterialInstanceConstant,
        unreal.MaterialInstanceConstantFactoryNew(),
    )
    instance.set_editor_property("parent", parent)
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
        instance, "Texture", texture
    )
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
        instance, "AlphaMultiplier", alpha
    )
    unreal.EditorAssetLibrary.save_loaded_asset(instance, only_if_is_dirty=False)
    return instance


def main():
    masters = {
        "opaque": build_master("M_SM64_Opaque", unreal.BlendMode.BLEND_OPAQUE),
        "masked": build_master(
            "M_SM64_MaskedTwoSided", unreal.BlendMode.BLEND_MASKED, two_sided=True
        ),
        "translucent": build_master(
            "M_SM64_Translucent", unreal.BlendMode.BLEND_TRANSLUCENT, two_sided=True
        ),
        "water": build_master(
            "M_SM64_Water", unreal.BlendMode.BLEND_TRANSLUCENT, two_sided=True, animated_uv=True
        ),
    }

    instances = {}
    assignments = {}
    for asset_path in sorted(unreal.EditorAssetLibrary.list_assets(MESH_ROOT, recursive=True)):
        if "/Collision/" in asset_path or "/CollisionDynamic/" in asset_path:
            continue
        mesh = unreal.load_asset(asset_path)
        if not isinstance(mesh, unreal.StaticMesh):
            continue
        slots = mesh.get_editor_property("static_materials")
        assigned = []
        for index, slot in enumerate(slots):
            slot_name = str(slot.get_editor_property("material_slot_name"))
            if slot_name not in instances:
                instances[slot_name] = create_instance(slot_name, masters)
            mesh.set_material(index, instances[slot_name])
            assigned.append(slot_name)
        mesh.modify()
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
        assignments[asset_path.split(".", 1)[0]] = assigned

    report = {
        "masters": sorted(master.get_path_name() for master in masters.values()),
        "instances": sorted(instances),
        "assignments": assignments,
    }
    unreal.log_warning("SM64_WF_MATERIALS=" + json.dumps(report, sort_keys=True))


main()
