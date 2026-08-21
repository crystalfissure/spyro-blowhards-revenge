"""
Batch importer: FBX meshes + PNG textures + materials -> UE 4.27 content.

Runs INSIDE Unreal's Python (the `unreal` module must be importable).

Entry points
------------
headless :  UE4Editor-Cmd.exe <project>.uproject -run=pythonscript
                -script="<abs path to this file>"
            with the manifest supplied via env var UE_ASSET_MANIFEST.

live     :  from a running editor (Output Log > Cmd > Python), or via
            send_to_editor.py:
                import ue_asset_pipeline
                ue_asset_pipeline.run(r"C:/path/to/manifest.json")

The manifest format is documented in README.md next to this file.
"""

import json
import os
import sys
import traceback

import unreal


# ---------------------------------------------------------------------------
# enum lookup helpers
#
# Enum member names moved around a little between engine versions, so every
# lookup goes through _enum(): an unknown key logs a warning and falls back
# instead of aborting a 200-asset batch on one bad field.
# ---------------------------------------------------------------------------

def _enum(enum_type, member, fallback=None):
    value = getattr(enum_type, member, None)
    if value is None:
        _warn("unknown enum %s.%s - falling back" % (enum_type.__name__, member))
        return fallback
    return value


TEXTURE_FILTER = {
    "nearest": lambda: _enum(unreal.TextureFilter, "TF_NEAREST"),
    "bilinear": lambda: _enum(unreal.TextureFilter, "TF_BILINEAR"),
    "trilinear": lambda: _enum(unreal.TextureFilter, "TF_TRILINEAR"),
    "default": lambda: _enum(unreal.TextureFilter, "TF_DEFAULT"),
}

MIP_GEN = {
    "no_mipmaps": lambda: _enum(unreal.TextureMipGenSettings, "TMGS_NO_MIPMAPS"),
    "from_texture_group": lambda: _enum(unreal.TextureMipGenSettings, "TMGS_FROM_TEXTURE_GROUP"),
    "simple_average": lambda: _enum(unreal.TextureMipGenSettings, "TMGS_SIMPLE_AVERAGE"),
    "sharpen": lambda: _enum(unreal.TextureMipGenSettings, "TMGS_SHARPEN4"),
}

COMPRESSION = {
    "default": lambda: _enum(unreal.TextureCompressionSettings, "TC_DEFAULT"),
    "uncompressed": lambda: _enum(unreal.TextureCompressionSettings, "TC_VECTOR_DISPLACEMENTMAP"),
    "normalmap": lambda: _enum(unreal.TextureCompressionSettings, "TC_NORMALMAP"),
    "masks": lambda: _enum(unreal.TextureCompressionSettings, "TC_MASKS"),
    "grayscale": lambda: _enum(unreal.TextureCompressionSettings, "TC_GRAYSCALE"),
    "alpha": lambda: _enum(unreal.TextureCompressionSettings, "TC_ALPHA"),
    "bc7": lambda: _enum(unreal.TextureCompressionSettings, "TC_BC7"),
}

ADDRESS = {
    "wrap": lambda: _enum(unreal.TextureAddress, "TA_WRAP"),
    "clamp": lambda: _enum(unreal.TextureAddress, "TA_CLAMP"),
    "mirror": lambda: _enum(unreal.TextureAddress, "TA_MIRROR"),
}

SHADING_MODEL = {
    "default_lit": lambda: _enum(unreal.MaterialShadingModel, "MSM_DEFAULT_LIT"),
    "unlit": lambda: _enum(unreal.MaterialShadingModel, "MSM_UNLIT"),
    "subsurface": lambda: _enum(unreal.MaterialShadingModel, "MSM_SUBSURFACE"),
    "two_sided_foliage": lambda: _enum(unreal.MaterialShadingModel, "MSM_TWO_SIDED_FOLIAGE"),
}

BLEND_MODE = {
    "opaque": lambda: _enum(unreal.BlendMode, "BLEND_OPAQUE"),
    "masked": lambda: _enum(unreal.BlendMode, "BLEND_MASKED"),
    "translucent": lambda: _enum(unreal.BlendMode, "BLEND_TRANSLUCENT"),
    "additive": lambda: _enum(unreal.BlendMode, "BLEND_ADDITIVE"),
    "modulate": lambda: _enum(unreal.BlendMode, "BLEND_MODULATE"),
}

# manifest texture slot -> (material property, sampler type, sensible compression)
SLOT_SPEC = {
    "base_color": ("MP_BASE_COLOR", "SAMPLERTYPE_COLOR", "default"),
    "normal": ("MP_NORMAL", "SAMPLERTYPE_NORMAL", "normalmap"),
    "emissive": ("MP_EMISSIVE_COLOR", "SAMPLERTYPE_COLOR", "default"),
    "roughness": ("MP_ROUGHNESS", "SAMPLERTYPE_LINEAR_COLOR", "masks"),
    "metallic": ("MP_METALLIC", "SAMPLERTYPE_LINEAR_COLOR", "masks"),
    "specular": ("MP_SPECULAR", "SAMPLERTYPE_LINEAR_COLOR", "masks"),
    "ambient_occlusion": ("MP_AMBIENT_OCCLUSION", "SAMPLERTYPE_LINEAR_COLOR", "masks"),
    "opacity": ("MP_OPACITY", "SAMPLERTYPE_LINEAR_COLOR", "alpha"),
    "opacity_mask": ("MP_OPACITY_MASK", "SAMPLERTYPE_LINEAR_COLOR", "alpha"),
}


# ---------------------------------------------------------------------------
# logging
# ---------------------------------------------------------------------------

_LOG = []


def _log(message):
    _LOG.append(("info", message))
    unreal.log("[assetpipe] %s" % message)


def _warn(message):
    _LOG.append(("warn", message))
    unreal.log_warning("[assetpipe] %s" % message)


def _error(message):
    _LOG.append(("error", message))
    unreal.log_error("[assetpipe] %s" % message)


# ---------------------------------------------------------------------------
# small utilities
# ---------------------------------------------------------------------------

def _asset_tools():
    return unreal.AssetToolsHelpers.get_asset_tools()


def _merge(base, override):
    """Recursive dict merge; `override` wins. Neither input is mutated."""
    out = dict(base or {})
    for key, value in (override or {}).items():
        if isinstance(value, dict) and isinstance(out.get(key), dict):
            out[key] = _merge(out[key], value)
        else:
            out[key] = value
    return out


def _resolve(path, root):
    """Absolute source path from a manifest entry, relative to the batch root."""
    path = os.path.expandvars(os.path.expanduser(path))
    if not os.path.isabs(path):
        path = os.path.join(root, path)
    return os.path.normpath(path)


def _clean_package_path(path):
    return "/" + "/".join(part for part in path.replace("\\", "/").split("/") if part)


def _looks_skinned(fbx_path):
    """Heuristic skeletal-mesh detection.

    Both ASCII and binary FBX store class names as literal strings, so the
    presence of a skin Deformer is readable without a full FBX parse.
    """
    try:
        with open(fbx_path, "rb") as handle:
            blob = handle.read()
    except OSError as exc:
        _warn("could not read %s for skeletal detection (%s)" % (fbx_path, exc))
        return False
    return b"Deformer" in blob and (b"Skin" in blob or b"Cluster" in blob)


def _imported_objects(task):
    """Objects produced by an AssetImportTask, across API spellings."""
    objects = []
    try:
        paths = list(task.get_editor_property("imported_object_paths") or [])
    except Exception:
        paths = []
    for path in paths:
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if asset:
            objects.append(asset)
    if not objects and hasattr(task, "get_objects"):
        try:
            objects = [obj for obj in task.get_objects() if obj]
        except Exception:
            pass
    return objects


def _save(asset):
    try:
        unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    except Exception as exc:
        _warn("could not save %s (%s)" % (asset.get_name(), exc))


# ---------------------------------------------------------------------------
# textures
# ---------------------------------------------------------------------------

def import_texture(source, dest_path, dest_name, settings, replace=True):
    package = "%s/%s" % (_clean_package_path(dest_path), dest_name)

    if not replace and unreal.EditorAssetLibrary.does_asset_exist(package):
        _log("texture exists, keeping: %s" % package)
        return unreal.EditorAssetLibrary.load_asset(package)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", _clean_package_path(dest_path))
    task.set_editor_property("destination_name", dest_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", replace)
    task.set_editor_property("replace_existing_settings", False)
    task.set_editor_property("save", False)

    _asset_tools().import_asset_tasks([task])

    texture = None
    for obj in _imported_objects(task):
        if isinstance(obj, unreal.Texture):
            texture = obj
            break
    if texture is None:
        texture = unreal.EditorAssetLibrary.load_asset(package)
    if texture is None:
        _error("texture import failed: %s" % source)
        return None

    apply_texture_settings(texture, settings)
    _save(texture)
    _log("texture -> %s" % package)
    return texture


def apply_texture_settings(texture, settings):
    settings = settings or {}

    def assign(prop, value):
        if value is None:
            return
        try:
            texture.set_editor_property(prop, value)
        except Exception as exc:
            _warn("texture property %s rejected (%s)" % (prop, exc))

    if "srgb" in settings:
        assign("srgb", bool(settings["srgb"]))
    if settings.get("filter"):
        assign("filter", TEXTURE_FILTER.get(settings["filter"], TEXTURE_FILTER["default"])())
    if settings.get("mip_gen"):
        assign("mip_gen_settings", MIP_GEN.get(settings["mip_gen"], MIP_GEN["from_texture_group"])())
    if settings.get("compression"):
        assign("compression_settings", COMPRESSION.get(settings["compression"], COMPRESSION["default"])())
    if settings.get("address"):
        mode = ADDRESS.get(settings["address"], ADDRESS["wrap"])()
        assign("address_x", mode)
        assign("address_y", mode)
    if "flip_green_channel" in settings:
        assign("flip_green_channel", bool(settings["flip_green_channel"]))
    if "never_stream" in settings:
        assign("never_stream", bool(settings["never_stream"]))
    if settings.get("lod_group"):
        assign("lod_group", _enum(unreal.TextureGroup, settings["lod_group"]))


# ---------------------------------------------------------------------------
# meshes
# ---------------------------------------------------------------------------

def _build_fbx_options(mesh_settings, skeletal):
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_materials", bool(mesh_settings.get("import_fbx_materials", False)))
    options.set_editor_property("import_as_skeletal", skeletal)
    options.set_editor_property(
        "mesh_type_to_import",
        _enum(unreal.FBXImportType, "FBXIT_SKELETAL_MESH" if skeletal else "FBXIT_STATIC_MESH"),
    )
    options.set_editor_property("import_animations", bool(mesh_settings.get("import_animations", skeletal)))

    if skeletal and mesh_settings.get("skeleton"):
        skeleton = unreal.EditorAssetLibrary.load_asset(mesh_settings["skeleton"])
        if skeleton:
            options.set_editor_property("skeleton", skeleton)
        else:
            _warn("skeleton not found: %s (a new one will be created)" % mesh_settings["skeleton"])

    data = options.skeletal_mesh_import_data if skeletal else options.static_mesh_import_data

    def assign(prop, value):
        if value is None:
            return
        try:
            data.set_editor_property(prop, value)
        except Exception as exc:
            _warn("fbx option %s rejected (%s)" % (prop, exc))

    translation = mesh_settings.get("import_translation") or [0.0, 0.0, 0.0]
    rotation = mesh_settings.get("import_rotation") or [0.0, 0.0, 0.0]
    assign("import_translation", unreal.Vector(*[float(v) for v in translation]))
    assign("import_rotation", unreal.Rotator(*[float(v) for v in rotation]))
    assign("import_uniform_scale", float(mesh_settings.get("import_uniform_scale", 1.0)))
    assign("convert_scene", bool(mesh_settings.get("convert_scene", True)))
    assign("force_front_x_axis", bool(mesh_settings.get("force_front_x_axis", False)))
    assign("convert_scene_unit", bool(mesh_settings.get("convert_scene_unit", False)))

    normals = mesh_settings.get("normal_import_method", "import_normals_and_tangents")
    assign("normal_import_method", _enum(
        unreal.FBXNormalImportMethod,
        {
            "compute_normals": "FBXNIM_COMPUTE_NORMALS",
            "import_normals": "FBXNIM_IMPORT_NORMALS",
            "import_normals_and_tangents": "FBXNIM_IMPORT_NORMALS_AND_TANGENTS",
        }.get(normals, "FBXNIM_IMPORT_NORMALS_AND_TANGENTS"),
    ))

    vertex_colors = mesh_settings.get("vertex_color_import", "replace")
    assign("vertex_color_import_option", _enum(
        unreal.VertexColorImportOption,
        {"replace": "REPLACE", "ignore": "IGNORE", "override": "OVERRIDE"}.get(vertex_colors, "REPLACE"),
    ))

    if skeletal:
        assign("import_morph_targets", bool(mesh_settings.get("import_morph_targets", False)))
        assign("preserve_smoothing_groups", bool(mesh_settings.get("preserve_smoothing_groups", True)))
        assign("import_meshes_in_bone_hierarchy", bool(mesh_settings.get("import_meshes_in_bone_hierarchy", True)))
        assign("update_skeleton_reference_pose", False)
        assign("use_t0_as_ref_pose", bool(mesh_settings.get("use_t0_as_ref_pose", False)))
    else:
        assign("combine_meshes", bool(mesh_settings.get("combine_meshes", True)))
        assign("generate_lightmap_u_vs", bool(mesh_settings.get("generate_lightmap_uvs", True)))
        assign("auto_generate_collision", bool(mesh_settings.get("auto_generate_collision", True)))
        assign("remove_degenerates", bool(mesh_settings.get("remove_degenerates", True)))

    return options


def import_mesh(source, dest_path, dest_name, mesh_settings, replace=True):
    """Returns (mesh_asset, [all imported objects])."""
    mesh_type = (mesh_settings.get("mesh_type") or "auto").lower()
    if mesh_type == "auto":
        skeletal = _looks_skinned(source)
        _log("%s detected as %s mesh" % (os.path.basename(source), "skeletal" if skeletal else "static"))
    else:
        skeletal = mesh_type == "skeletal"

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", _clean_package_path(dest_path))
    task.set_editor_property("destination_name", dest_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", replace)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", False)
    task.set_editor_property("options", _build_fbx_options(mesh_settings, skeletal))

    _asset_tools().import_asset_tasks([task])

    objects = _imported_objects(task)
    wanted = unreal.SkeletalMesh if skeletal else unreal.StaticMesh
    mesh = next((obj for obj in objects if isinstance(obj, wanted)), None)
    if mesh is None:
        mesh = unreal.EditorAssetLibrary.load_asset(
            "%s/%s" % (_clean_package_path(dest_path), dest_name))
    if mesh is None:
        _error("mesh import failed: %s" % source)
        return None, objects

    for obj in objects:
        _save(obj)
    _log("mesh -> %s (%s)" % (mesh.get_path_name(), "skeletal" if skeletal else "static"))
    return mesh, objects


# ---------------------------------------------------------------------------
# materials
# ---------------------------------------------------------------------------

def _expr(material, class_, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(material, class_, x, y)


def _connect_property(expression, output, material, property_name):
    prop = _enum(unreal.MaterialProperty, property_name)
    if prop is None:
        return
    unreal.MaterialEditingLibrary.connect_material_property(expression, output, prop)


def build_material(name, dest_path, textures, settings, replace=True):
    """Create a standalone Material with a texture-driven graph."""
    package = "%s/%s" % (_clean_package_path(dest_path), name)

    if unreal.EditorAssetLibrary.does_asset_exist(package):
        if not replace:
            _log("material exists, keeping: %s" % package)
            return unreal.EditorAssetLibrary.load_asset(package)
        unreal.EditorAssetLibrary.delete_asset(package)

    material = _asset_tools().create_asset(
        name, _clean_package_path(dest_path), unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        _error("could not create material %s" % package)
        return None

    shading = (settings.get("shading_model") or "default_lit").lower()
    blend = (settings.get("blend_mode") or "opaque").lower()

    material.set_editor_property("shading_model", SHADING_MODEL.get(shading, SHADING_MODEL["default_lit"])())
    material.set_editor_property("blend_mode", BLEND_MODE.get(blend, BLEND_MODE["opaque"])())
    material.set_editor_property("two_sided", bool(settings.get("two_sided", False)))
    if blend == "masked":
        material.set_editor_property(
            "opacity_mask_clip_value", float(settings.get("opacity_mask_clip_value", 0.333)))

    # Unlit materials have no base colour input; colour goes to emissive.
    colour_property = "MP_EMISSIVE_COLOR" if shading == "unlit" else "MP_BASE_COLOR"

    row = 0
    base_sample = None
    for slot, texture in textures.items():
        if texture is None:
            continue
        prop, sampler_name, _ = SLOT_SPEC.get(slot, SLOT_SPEC["base_color"])
        sample = _expr(material, unreal.MaterialExpressionTextureSample, -760, row)
        sample.set_editor_property("texture", texture)
        sampler = _enum(unreal.MaterialSamplerType, sampler_name)
        if sampler is not None:
            sample.set_editor_property("sampler_type", sampler)
        row += 260

        if slot == "base_color":
            base_sample = sample
            continue
        if slot == "emissive" and shading == "unlit":
            # would fight the base colour chain for the emissive input
            _warn("emissive texture ignored on unlit material %s" % name)
            continue
        _connect_property(sample, "RGB", material, prop)

    if base_sample is not None:
        colour_source, colour_output = base_sample, "RGB"

        if settings.get("use_vertex_color"):
            vertex = _expr(material, unreal.MaterialExpressionVertexColor, -760, row)
            row += 260
            multiply = _expr(material, unreal.MaterialExpressionMultiply, -420, 0)
            unreal.MaterialEditingLibrary.connect_material_expressions(base_sample, "RGB", multiply, "A")
            unreal.MaterialEditingLibrary.connect_material_expressions(vertex, "RGB", multiply, "B")
            colour_source, colour_output = multiply, ""

            scale = float(settings.get("vertex_color_scale", 1.0))
            if scale != 1.0:
                constant = _expr(material, unreal.MaterialExpressionConstant, -420, 240)
                constant.set_editor_property("r", scale)
                scaled = _expr(material, unreal.MaterialExpressionMultiply, -200, 0)
                unreal.MaterialEditingLibrary.connect_material_expressions(multiply, "", scaled, "A")
                unreal.MaterialEditingLibrary.connect_material_expressions(constant, "", scaled, "B")
                colour_source, colour_output = scaled, ""

        _connect_property(colour_source, colour_output, material, colour_property)

        # Alpha from the base texture drives masking / translucency unless the
        # manifest supplied a dedicated opacity texture.
        if blend == "masked" and "opacity_mask" not in textures:
            _connect_property(base_sample, "A", material, "MP_OPACITY_MASK")
        elif blend in ("translucent", "additive", "modulate") and "opacity" not in textures:
            _connect_property(base_sample, "A", material, "MP_OPACITY")

    # Constant scalar overrides for anything with no texture driving it.
    constants = {
        "roughness": "MP_ROUGHNESS",
        "metallic": "MP_METALLIC",
        "specular": "MP_SPECULAR",
    }
    offset = 0
    for key, prop in constants.items():
        if key in settings and key not in textures:
            node = _expr(material, unreal.MaterialExpressionConstant, -420, 600 + offset)
            node.set_editor_property("r", float(settings[key]))
            _connect_property(node, "", material, prop)
            offset += 140

    unreal.MaterialEditingLibrary.recompile_material(material)
    _save(material)
    _log("material -> %s (%s, %s%s)" % (
        package, shading, blend, ", two-sided" if settings.get("two_sided") else ""))
    return material


def build_material_instance(name, dest_path, parent_path, textures, settings, replace=True):
    """Create a MaterialInstanceConstant parented to an existing master material."""
    package = "%s/%s" % (_clean_package_path(dest_path), name)

    parent = unreal.EditorAssetLibrary.load_asset(parent_path)
    if parent is None:
        _error("parent material not found: %s" % parent_path)
        return None

    if unreal.EditorAssetLibrary.does_asset_exist(package):
        if not replace:
            _log("material instance exists, keeping: %s" % package)
            return unreal.EditorAssetLibrary.load_asset(package)
        unreal.EditorAssetLibrary.delete_asset(package)

    instance = _asset_tools().create_asset(
        name, _clean_package_path(dest_path), unreal.MaterialInstanceConstant,
        unreal.MaterialInstanceConstantFactoryNew())
    if instance is None:
        _error("could not create material instance %s" % package)
        return None

    lib = unreal.MaterialEditingLibrary
    lib.set_material_instance_parent(instance, parent)

    for param, texture in (textures or {}).items():
        if texture is not None:
            lib.set_material_instance_texture_parameter_value(instance, param, texture)

    for param, value in (settings.get("scalars") or {}).items():
        lib.set_material_instance_scalar_parameter_value(instance, param, float(value))

    for param, value in (settings.get("vectors") or {}).items():
        if isinstance(value, (list, tuple)):
            rgba = list(value) + [1.0] * (4 - len(value))
            colour = unreal.LinearColor(*[float(c) for c in rgba[:4]])
        else:
            colour = value
        lib.set_material_instance_vector_parameter_value(instance, param, colour)

    switches = settings.get("switches") or {}
    if switches:
        setter = getattr(lib, "set_material_instance_static_switch_parameter_value", None)
        if setter is None:
            _warn("static switch parameters are not exposed to Python in this engine "
                  "build; set these by hand on %s: %s" % (package, ", ".join(switches)))
        else:
            for param, value in switches.items():
                setter(instance, param, bool(value))

    if "two_sided" in settings or "blend_mode" in settings:
        _apply_instance_overrides(instance, settings)

    _save(instance)
    _log("material instance -> %s (parent %s)" % (package, parent_path))
    return instance


def _apply_instance_overrides(instance, settings):
    """Material-instance base property overrides (two-sided / blend mode)."""
    try:
        overrides = instance.get_editor_property("base_property_overrides")
        if "two_sided" in settings:
            overrides.set_editor_property("override_two_sided", True)
            overrides.set_editor_property("two_sided", bool(settings["two_sided"]))
        if "blend_mode" in settings:
            overrides.set_editor_property("override_blend_mode", True)
            overrides.set_editor_property(
                "blend_mode", BLEND_MODE.get(settings["blend_mode"], BLEND_MODE["opaque"])())
        instance.set_editor_property("base_property_overrides", overrides)
    except Exception as exc:
        _warn("could not apply base property overrides (%s)" % exc)


# ---------------------------------------------------------------------------
# material assignment
# ---------------------------------------------------------------------------

def assign_material(mesh, material, slots=None):
    """Assign `material` to the given mesh slot indices (all slots if None)."""
    if mesh is None or material is None:
        return

    if isinstance(mesh, unreal.StaticMesh):
        entries = list(mesh.get_editor_property("static_materials"))
        prop = "static_materials"
    elif isinstance(mesh, unreal.SkeletalMesh):
        entries = list(mesh.get_editor_property("materials"))
        prop = "materials"
    else:
        _warn("do not know how to assign materials on %s" % type(mesh).__name__)
        return

    if not entries:
        _warn("%s has no material slots" % mesh.get_name())
        return

    targets = list(range(len(entries))) if slots is None else [
        i for i in slots if 0 <= i < len(entries)]

    for index in targets:
        entries[index].set_editor_property("material_interface", material)

    mesh.set_editor_property(prop, entries)
    _save(mesh)
    _log("assigned %s to %d slot(s) on %s" % (
        material.get_name(), len(targets), mesh.get_name()))


# ---------------------------------------------------------------------------
# one manifest entry
# ---------------------------------------------------------------------------

def _texture_name(asset_name, slot):
    suffix = {
        "base_color": "_T",
        "normal": "_N",
        "emissive": "_E",
        "roughness": "_R",
        "metallic": "_M",
        "specular": "_S",
        "ambient_occlusion": "_AO",
        "opacity": "_A",
        "opacity_mask": "_A",
    }.get(slot, "_T")
    return "%s%s" % (asset_name, suffix)


def process_asset(entry, defaults, source_root, content_root, replace, dry_run):
    name = entry.get("name") or os.path.splitext(os.path.basename(entry.get("mesh", "asset")))[0]

    dest = entry.get("dest") or "%s/%s" % (_clean_package_path(content_root), name)
    dest = _clean_package_path(dest)

    mesh_settings = _merge(defaults.get("mesh"), entry.get("mesh_settings"))
    texture_defaults = defaults.get("texture") or {}
    material_settings = _merge(defaults.get("material"), entry.get("material"))

    _log("--- %s -> %s" % (name, dest))

    if dry_run:
        _log("dry run: would import mesh=%s textures=%d material_mode=%s" % (
            entry.get("mesh"), len(entry.get("textures") or []),
            material_settings.get("mode", "new")))
        return {"name": name, "dest": dest, "dry_run": True}

    result = {"name": name, "dest": dest, "mesh": None, "textures": {}, "material": None}

    # --- textures -----------------------------------------------------------
    textures_by_slot = {}
    textures_by_param = {}
    for spec in entry.get("textures") or []:
        if isinstance(spec, str):
            spec = {"file": spec}
        source = _resolve(spec["file"], source_root)
        if not os.path.isfile(source):
            _error("texture missing on disk: %s" % source)
            continue

        slot = (spec.get("slot") or "base_color").lower()
        _, _, default_compression = SLOT_SPEC.get(slot, SLOT_SPEC["base_color"])

        settings = _merge(texture_defaults, spec.get("settings"))
        settings.setdefault("compression", default_compression)
        settings.setdefault("srgb", slot in ("base_color", "emissive"))

        texture_name = spec.get("name") or _texture_name(name, slot)
        texture = import_texture(source, dest, texture_name, settings, replace)
        if texture is None:
            continue

        result["textures"][slot] = texture.get_path_name()
        textures_by_slot[slot] = texture
        if spec.get("param"):
            textures_by_param[spec["param"]] = texture

    # --- mesh ---------------------------------------------------------------
    mesh = None
    if entry.get("mesh"):
        mesh_source = _resolve(entry["mesh"], source_root)
        if os.path.isfile(mesh_source):
            mesh_name = entry.get("mesh_name") or name
            mesh, _ = import_mesh(mesh_source, dest, mesh_name, mesh_settings, replace)
            if mesh:
                result["mesh"] = mesh.get_path_name()
        else:
            _error("mesh missing on disk: %s" % mesh_source)

    # --- material -----------------------------------------------------------
    mode = (material_settings.get("mode") or "new").lower()
    material = None

    if mode == "none":
        pass
    elif mode == "existing":
        material = unreal.EditorAssetLibrary.load_asset(material_settings.get("asset", ""))
        if material is None:
            _error("existing material not found: %s" % material_settings.get("asset"))
    elif mode == "instance":
        params = dict(textures_by_param)
        if not params and textures_by_slot.get("base_color"):
            default_param = material_settings.get("base_color_param", "BaseTexture")
            params[default_param] = textures_by_slot["base_color"]
        material = build_material_instance(
            material_settings.get("name") or ("MI_%s" % name),
            dest, material_settings.get("parent", ""), params, material_settings, replace)
    else:
        material = build_material(
            material_settings.get("name") or ("%s_Mat" % name),
            dest, textures_by_slot, material_settings, replace)

    if material is not None:
        result["material"] = material.get_path_name()
        assign_material(mesh, material, material_settings.get("slots"))

    return result


# ---------------------------------------------------------------------------
# entry point
# ---------------------------------------------------------------------------

def run(manifest_path, dry_run=False):
    manifest_path = os.path.abspath(manifest_path)
    with open(manifest_path, "r", encoding="utf-8") as handle:
        manifest = json.load(handle)

    source_root = manifest.get("source_root") or os.path.dirname(manifest_path)
    source_root = os.path.abspath(os.path.expandvars(source_root))
    content_root = manifest.get("content_root", "/Game")
    defaults = manifest.get("defaults") or {}
    replace = bool(manifest.get("replace_existing", True))
    dry_run = dry_run or bool(manifest.get("dry_run", False))
    assets = manifest.get("assets") or []

    _log("manifest %s" % manifest_path)
    _log("source root %s" % source_root)
    _log("%d asset(s), content root %s%s" % (
        len(assets), content_root, ", DRY RUN" if dry_run else ""))

    results, failures = [], []
    for entry in assets:
        try:
            results.append(process_asset(
                entry, defaults, source_root, content_root, replace, dry_run))
        except Exception:
            label = entry.get("name", "?")
            failures.append(label)
            _error("FAILED %s\n%s" % (label, traceback.format_exc()))

    if not dry_run:
        try:
            unreal.EditorAssetLibrary.save_directory(
                _clean_package_path(content_root), only_if_is_dirty=True, recursive=True)
        except Exception as exc:
            _warn("final save pass failed (%s)" % exc)

    warnings = sum(1 for level, _ in _LOG if level == "warn")
    errors = sum(1 for level, _ in _LOG if level == "error")
    _log("=== done: %d imported, %d failed, %d warning(s), %d error(s)" % (
        len(results), len(failures), warnings, errors))

    report_path = manifest.get("report")
    if report_path:
        report_path = _resolve(report_path, source_root)
        with open(report_path, "w", encoding="utf-8") as handle:
            json.dump({"results": results, "failures": failures,
                       "log": [{"level": l, "message": m} for l, m in _LOG]},
                      handle, indent=2)
        _log("report -> %s" % report_path)

    return {"results": results, "failures": failures}


def main():
    manifest = os.environ.get("UE_ASSET_MANIFEST")
    if not manifest:
        for arg in sys.argv[1:]:
            if arg.lower().endswith(".json"):
                manifest = arg
                break
    if not manifest:
        _error("no manifest: set UE_ASSET_MANIFEST or pass a .json path")
        return 1

    dry_run = os.environ.get("UE_ASSET_DRY_RUN", "").lower() in ("1", "true", "yes")
    outcome = run(manifest, dry_run=dry_run)
    return 1 if outcome["failures"] else 0


if __name__ == "__main__":
    main()
