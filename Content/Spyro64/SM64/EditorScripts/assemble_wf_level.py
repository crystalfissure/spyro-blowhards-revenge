"""Idempotently assemble the DAE-backed Whomp's Fortress milestone in Level 5."""

from __future__ import print_function

import json
import os
import re
import unreal


LEVEL = "/Game/Spyro64/Levels/05_Level5"
SOURCE_ROOT = os.path.normpath(
    r"C:\Users\adace\Desktop\spyro-blowhards-revenge\Content\Spyro64"
    r"\SM64\Source\WhompsFortress"
)
PLACEMENTS_FILE = os.path.join(SOURCE_ROOT, "Manifest", "wf_placements.json")
COURSE_FILE = os.path.join(SOURCE_ROOT, "Manifest", "wf_course_definition.json")
CONTENT = "/Game/Spyro64/SM64/WhompsFortress"
DATA_PATH = CONTENT + "/Data"
PHYSICS_PATH = CONTENT + "/Physics"
STATIC = CONTENT + "/Meshes/Static"
MOVERS = CONTENT + "/Meshes/Movers"
CONDITIONAL = CONTENT + "/Meshes/Conditional"
COLLISION = CONTENT + "/Meshes/Collision"
COLLISION_DYNAMIC = CONTENT + "/Meshes/CollisionDynamic"
WATER = CONTENT + "/Meshes/Water"
COMMON_ACTORS = "/Game/Spyro64/SM64/Common/Actors"
TAG_PREFIX = "SM64StableId="
GENERATED_TAG = "SM64Generated"


STATIC_RENDER = {
    "wf/render/static_base": STATIC + "/SM_WF_Static_Base",
    "wf/render/static_props": STATIC + "/SM_WF_Static_Props",
}

TERRAIN_COLLISION = {
    "default": COLLISION + "/SM_WF_COL_Area1_wf_seg7_collision_070102D8_COL_WF_Area1_wf_seg7_collision_070102D8_00_SURFACE_DEFAULT",
    "death": COLLISION + "/SM_WF_COL_Area1_wf_seg7_collision_070102D8_COL_WF_Area1_wf_seg7_collision_070102D8_01_SURFACE_DEATH_PLANE",
    "very_slippery": COLLISION + "/SM_WF_COL_Area1_wf_seg7_collision_070102D8_COL_WF_Area1_wf_seg7_collision_070102D8_02_SURFACE_VERY_SLIPPERY",
    "slippery": COLLISION + "/SM_WF_COL_Area1_wf_seg7_collision_070102D8_COL_WF_Area1_wf_seg7_collision_070102D8_03_SURFACE_SLIPPERY",
    "not_slippery": COLLISION + "/SM_WF_COL_Area1_wf_seg7_collision_070102D8_COL_WF_Area1_wf_seg7_collision_070102D8_04_SURFACE_NOT_SLIPPERY",
    "wall_misc": COLLISION + "/SM_WF_COL_Area1_wf_seg7_collision_070102D8_COL_WF_Area1_wf_seg7_collision_070102D8_05_SURFACE_WALL_MISC",
    "noise_default": COLLISION + "/SM_WF_COL_Area1_wf_seg7_collision_070102D8_COL_WF_Area1_wf_seg7_collision_070102D8_06_SURFACE_NOISE_DEFAULT",
    "boss_camera": COLLISION + "/SM_WF_COL_Area1_wf_seg7_collision_070102D8_COL_WF_Area1_wf_seg7_collision_070102D8_07_SURFACE_BOSS_FIGHT_CAMERA",
    "camera_middle": COLLISION + "/SM_WF_COL_Area1_wf_seg7_collision_070102D8_COL_WF_Area1_wf_seg7_collision_070102D8_08_SURFACE_CAMERA_MIDDLE",
}

PHYSICS = {
    "default": ("PM_SM64_Default", 0.80),
    "very_slippery": ("PM_SM64_VerySlippery", 0.05),
    "slippery": ("PM_SM64_Slippery", 0.20),
    "not_slippery": ("PM_SM64_NotSlippery", 1.20),
    "wall_misc": ("PM_SM64_WallMisc", 0.80),
    "noise_default": ("PM_SM64_NoiseDefault", 0.80),
}

RENDER = {
    "PushyRock": MOVERS + "/SM_WF_PushyRock",
    "PushyRockBig": MOVERS + "/SM_WF_PushyRockBig",
    "RotatingBridge": MOVERS + "/SM_WF_RotatingBridge",
    "SlidingPlatform": MOVERS + "/SM_WF_SlidingPlatform",
    "GrassPlatform": MOVERS + "/SM_WF_GrassPlatform",
    "BridgePiece": MOVERS + "/SM_WF_BridgePiece",
    "TowerPlatform": MOVERS + "/SM_WF_TowerPlatform",
    "KickWood": MOVERS + "/SM_WF_KickWood",
    "KickWoodDown": MOVERS + "/SM_WF_KickWoodDown",
    "Tower": CONDITIONAL + "/SM_WF_Tower",
    "BillBlaster": CONDITIONAL + "/SM_WF_BillBlaster",
    "TowerWall": CONDITIONAL + "/SM_WF_TowerWall",
    "CornerStar": CONDITIONAL + "/SM_WF_CornerStar",
    "CornerBreak": CONDITIONAL + "/SM_WF_CornerBreak",
    "Pole": CONDITIONAL + "/SM_WF_Pole",
    "Tree": CONDITIONAL + "/SM_WF_Tree",
}

DYNAMIC_COLLISION = {
    "PushyRock": COLLISION_DYNAMIC + "/SM_WF_COL_PushyRock_wf_seg7_collision_small_bomp",
    "PushyRockBig": COLLISION_DYNAMIC + "/SM_WF_COL_PushyRockBig_wf_seg7_collision_large_bomp",
    "RotatingBridge": COLLISION_DYNAMIC + "/SM_WF_COL_RotatingBridge_wf_seg7_collision_clocklike_rotation",
    "SlidingPlatform": COLLISION_DYNAMIC + "/SM_WF_COL_SlidingPlatform_wf_seg7_collision_sliding_brick_platform",
    "GrassPlatform": COLLISION_DYNAMIC + "/SM_WF_COL_GrassPlatform_wf_seg7_collision_rotating_platform",
    "BridgePiece": COLLISION_DYNAMIC + "/SM_WF_COL_BridgePiece_wf_seg7_collision_tumbling_bridge",
    "TowerPlatform": COLLISION_DYNAMIC + "/SM_WF_COL_TowerPlatform_wf_seg7_collision_platform",
    "KickWood": COLLISION_DYNAMIC + "/SM_WF_COL_KickWood_wf_seg7_collision_kickable_board",
    "Tower": COLLISION_DYNAMIC + "/SM_WF_COL_Tower_wf_seg7_collision_tower",
    "BillBlaster": COLLISION_DYNAMIC + "/SM_WF_COL_BillBlaster_wf_seg7_collision_bullet_bill_cannon",
    "TowerWall": COLLISION_DYNAMIC + "/SM_WF_COL_TowerWall_wf_seg7_collision_tower_door",
    "CornerStar": COLLISION_DYNAMIC + "/SM_WF_COL_CornerStar_wf_seg7_collision_breakable_wall",
    "CornerBreak": COLLISION_DYNAMIC + "/SM_WF_COL_CornerBreak_wf_seg7_collision_breakable_wall_2",
}


def common_actor_asset(actor, role="Static"):
    title = "".join(part.capitalize() for part in actor.split("_"))
    prefix = "SK" if role == "Skeletal" else "SM"
    return "{}/{}/{}/{}_SM64_{}".format(COMMON_ACTORS, title, role, prefix, title)


def common_collision(actor):
    title = "".join(part.capitalize() for part in actor.split("_"))
    return "{}/{}/Collision/SM64_COL_{}".format(COMMON_ACTORS, title, title)


def common_animations(actor, expected_count):
    title = "".join(part.capitalize() for part in actor.split("_"))
    folder = "{}/{}/Animations".format(COMMON_ACTORS, title)
    assets = []
    for path in unreal.EditorAssetLibrary.list_assets(folder, recursive=False):
        asset = unreal.load_asset(path)
        if asset is not None and asset.get_class().get_name() == "AnimSequence":
            assets.append(asset)
    assets.sort(key=lambda item: item.get_name())
    if len(assets) != expected_count:
        raise RuntimeError(
            "{} expected {} animations, found {}".format(actor, expected_count, len(assets))
        )
    return assets


def load_json(path):
    with open(path, "r") as stream:
        return json.load(stream)


PLACEMENT_DATA = load_json(PLACEMENTS_FILE)
COURSE_DATA = load_json(COURSE_FILE)
PLACEMENTS = {item["stable_id"]: item for item in PLACEMENT_DATA["placements"]}


def require_asset(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Required asset is missing: " + path)
    return asset


def maybe_asset(path):
    return unreal.load_asset(path) if path else None


def act_mask(item):
    value = 0
    for act in item.get("acts", [1, 2, 3, 4, 5, 6]):
        value |= 1 << (int(act) - 1)
    return value


def location(item):
    xyz = item["unreal_transform"]["location_cm"]
    return unreal.Vector(float(xyz[0]), float(xyz[1]), float(xyz[2]))


def rotation(item):
    xyz = item["unreal_transform"]["rotation_deg"]
    return unreal.Rotator(pitch=float(xyz[0]), yaw=float(xyz[1]), roll=float(xyz[2]))


def transform_from(record, unreal_space=True):
    key = "unreal_transform" if unreal_space else "source_transform"
    loc_key = "location_cm"
    xyz = record[key][loc_key]
    angles = record[key]["rotation_deg"]
    return unreal.Transform(
        location=unreal.Vector(float(xyz[0]), float(xyz[1]), float(xyz[2])),
        rotation=unreal.Rotator(
            pitch=float(angles[0]), yaw=float(angles[1]), roll=float(angles[2])
        ),
        scale=unreal.Vector(1.0, 1.0, 1.0),
    )


def class_name(actor):
    return actor.get_class().get_name()


def actor_stable_id(actor):
    for tag in actor.get_editor_property("tags"):
        value = str(tag)
        if value.startswith(TAG_PREFIX):
            return value[len(TAG_PREFIX) :]
    return None


def label_for(stable_id):
    return "SM64_" + re.sub(r"[^A-Za-z0-9_]+", "_", stable_id).strip("_")


def add_identity(actor, stable_id):
    tags = [
        str(tag)
        for tag in actor.get_editor_property("tags")
        if not str(tag).startswith(TAG_PREFIX) and str(tag) != GENERATED_TAG
    ]
    tags.extend([GENERATED_TAG, TAG_PREFIX + stable_id])
    actor.set_editor_property("tags", tags)
    actor.set_actor_label(label_for(stable_id), mark_dirty=True)
    try:
        actor.set_editor_property("stable_id", stable_id)
    except Exception:
        pass


def update_actor_index():
    result = {}
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        stable_id = actor_stable_id(actor)
        if stable_id:
            if stable_id in result:
                raise RuntimeError("Duplicate SM64 stable ID in map: " + stable_id)
            result[stable_id] = actor
    return result


ACTORS = {}
REPORT = {"created": [], "updated": [], "replaced": [], "assets": [], "shell": {}}


def ensure_actor(stable_id, actor_class, actor_location=None, actor_rotation=None):
    global ACTORS
    actor_location = actor_location or unreal.Vector(0.0, 0.0, 0.0)
    actor_rotation = actor_rotation or unreal.Rotator(0.0, 0.0, 0.0)
    actor = ACTORS.get(stable_id)
    expected_name = actor_class.static_class().get_name()
    unreal.log_warning("SM64_WF_STEP ensure_actor {} {}".format(stable_id, expected_name))
    if actor is not None and class_name(actor) != expected_name:
        old_name = class_name(actor)
        unreal.EditorLevelLibrary.destroy_actor(actor)
        REPORT["replaced"].append(
            {"stable_id": stable_id, "old_class": old_name, "new_class": expected_name}
        )
        actor = None
    if actor is None:
        unreal.log_warning("SM64_WF_STEP spawning " + stable_id)
        actor = unreal.SM64EditorReferenceLibrary.spawn_actor_in_editor(
            actor_class,
            unreal.Transform(
                location=actor_location,
                rotation=actor_rotation,
                scale=unreal.Vector(1.0, 1.0, 1.0),
            ),
        )
        unreal.log_warning("SM64_WF_STEP spawned " + stable_id)
        if actor is None:
            raise RuntimeError("Failed to spawn {} as {}".format(stable_id, expected_name))
        REPORT["created"].append(stable_id)
        ACTORS[stable_id] = actor
    else:
        actor.set_actor_location_and_rotation(
            actor_location, actor_rotation, False, False
        )
        REPORT["updated"].append(stable_id)
    unreal.log_warning("SM64_WF_STEP identity " + stable_id)
    add_identity(actor, stable_id)
    unreal.log_warning("SM64_WF_STEP ensured " + stable_id)
    return actor


def rerun(actor):
    try:
        actor.rerun_construction_scripts()
    except Exception:
        pass


def set_first_editor_property(target, candidates, value):
    errors = []
    for candidate in candidates:
        try:
            target.set_editor_property(candidate, value)
            return candidate
        except Exception as error:
            errors.append("{}={}".format(candidate, error))
    raise RuntimeError("Unable to set any property candidate: " + "; ".join(errors))


def ensure_physical_material(name, friction):
    asset_path = PHYSICS_PATH + "/" + name
    material = unreal.load_asset(asset_path)
    if material is None:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name,
            PHYSICS_PATH,
            unreal.PhysicalMaterial,
            unreal.PhysicalMaterialFactoryNew(),
        )
    if material is None:
        raise RuntimeError("Unable to create physical material " + name)
    material.set_editor_property("friction", float(friction))
    material.set_editor_property("restitution", 0.0)
    material.modify()
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def ensure_blueprint_class(name, parent_class, defaults):
    path = "/Game/Spyro64/SM64/Common/Blueprints"
    blueprint = unreal.load_asset(path + "/" + name)
    if blueprint is None:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, path, unreal.Blueprint, factory
        )
    if not isinstance(blueprint, unreal.Blueprint):
        raise RuntimeError("Unable to create Blueprint " + name)
    generated_class = unreal.load_object(
        None, "{0}/{1}.{1}_C".format(path, name)
    )
    if generated_class is None:
        raise RuntimeError("Blueprint generated class is missing for " + name)
    default_object = unreal.get_default_object(generated_class)
    for property_name, value in defaults.items():
        default_object.set_editor_property(property_name, value)
    default_object.modify()
    blueprint.modify()
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
    return generated_class


def ensure_course_definition():
    name = "PDA_WF_CourseDefinition"
    path = DATA_PATH + "/" + name
    asset = unreal.load_asset(path)
    if asset is None:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.SM64CourseDefinition)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, DATA_PATH, unreal.SM64CourseDefinition, factory
        )
    if asset is None:
        raise RuntimeError("Unable to create " + path)

    asset.set_editor_property("course_index", int(COURSE_DATA["spyro_level_index"]))
    asset.set_editor_property("course_id", COURSE_DATA["course_id"])
    asset.set_editor_property("warp_key", COURSE_DATA["course_id"])
    asset.set_editor_property("course_name", unreal.Text(COURSE_DATA["display_name"]))
    asset.set_editor_property("canonical_course_coin_total", 100)
    asset.set_editor_property("death_plane_height", float(COURSE_DATA["death_plane_height_cm"]))
    asset.set_editor_property("water_surface_height", float(COURSE_DATA["water"]["surface_height_cm"]))
    start = COURSE_DATA["entrance"]["mario_base_start"]
    entrance = COURSE_DATA["entrance"]["warp_0A"]
    asset.set_editor_property("course_start", unreal.Vector(*start["unreal_location_cm"]))
    asset.set_editor_property(
        "course_start_rotation", unreal.Rotator(pitch=0.0, yaw=float(start["unreal_yaw_deg"]), roll=0.0)
    )
    asset.set_editor_property("entrance_warp_location", unreal.Vector(*entrance["unreal_location_cm"]))
    asset.set_editor_property(
        "entrance_warp_rotation",
        unreal.Rotator(pitch=0.0, yaw=float(entrance["unreal_yaw_deg"]), roll=0.0),
    )
    asset.set_editor_property("use_spin_airborne_entrance", False)
    asset.set_editor_property("return_level", COURSE_DATA["return_level"])

    missions = []
    for item in COURSE_DATA["missions"]:
        mission = unreal.SM64MissionDefinition()
        mission.set_editor_property("star_index", int(item["star_index"]))
        mission.set_editor_property("display_name", unreal.Text(item["display_name"]))
        mission.set_editor_property("preferred_act", int(item["preferred_act"]))
        mission.set_editor_property("visible_act_mask", int(item["visible_act_mask"]))
        mission.set_editor_property("star_location", unreal.Vector(*item["unreal_location_cm"]))
        mission.set_editor_property("eject_player_on_collect", bool(item["eject_on_collect"]))
        missions.append(mission)
    asset.set_editor_property("missions", missions)

    warps = []
    for item in COURSE_DATA["warps"]:
        warp = unreal.SM64WarpDefinition()
        warp.set_editor_property("warp_id", item["warp_id"])
        warp.set_editor_property("destination_warp_id", item["destination_warp_id"])
        warp.set_editor_property("destination_level", "")
        warp.set_editor_property("act_mask", 0x3F)
        warp.set_editor_property(
            "transform",
            unreal.Transform(
                location=unreal.Vector(*item["unreal_location_cm"]),
                rotation=unreal.Rotator(
                    pitch=0.0, yaw=float(item["unreal_yaw_deg"]), roll=0.0
                ),
                scale=unreal.Vector(1.0, 1.0, 1.0),
            ),
        )
        warps.append(warp)
    asset.set_editor_property("warps", warps)

    surface_enum = {
        "SURFACE_DEFAULT": unreal.SM64SurfaceType.DEFAULT,
        "SURFACE_DEATH_PLANE": unreal.SM64SurfaceType.DEATH,
        "SURFACE_VERY_SLIPPERY": unreal.SM64SurfaceType.VERY_SLIPPERY,
        "SURFACE_SLIPPERY": unreal.SM64SurfaceType.SLIPPERY,
        "SURFACE_NOT_SLIPPERY": unreal.SM64SurfaceType.NOT_SLIPPERY,
        "SURFACE_WALL_MISC": unreal.SM64SurfaceType.WALL_MISC,
        "SURFACE_NOISE_DEFAULT": unreal.SM64SurfaceType.NOISE_DEFAULT,
        "SURFACE_BOSS_FIGHT_CAMERA": unreal.SM64SurfaceType.BOSS_CAMERA,
        "SURFACE_CAMERA_MIDDLE": unreal.SM64SurfaceType.CAMERA_MIDDLE,
    }
    surfaces = []
    for item in COURSE_DATA["terrain_surface_mappings"]:
        mapping = unreal.SM64SurfaceMapping()
        mapping.set_editor_property("surface", surface_enum[item["surface"]])
        mapping.set_editor_property("physical_material_id", item.get("physical_material", ""))
        mapping.set_editor_property("hazard", bool(item.get("hazard", False)))
        mapping.set_editor_property("camera_surface", "camera_mode" in item)
        surfaces.append(mapping)
    asset.set_editor_property("surface_mappings", surfaces)

    placement_records = []
    for item in PLACEMENT_DATA["placements"]:
        record = unreal.SM64Placement()
        record.set_editor_property("stable_id", item["stable_id"])
        record.set_editor_property("behavior_id", item.get("behavior") or "")
        record.set_editor_property("source_transform", transform_from(item, False))
        record.set_editor_property("unreal_transform", transform_from(item, True))
        record.set_editor_property("act_mask", act_mask(item))
        record.set_editor_property("collision_source", item.get("asset") or "")
        record.set_editor_property("category", item.get("collection") or "")
        numeric = {}
        for key, value in item.get("metadata", {}).items():
            if isinstance(value, (int, float)) and not isinstance(value, bool):
                numeric[str(key)] = float(value)
        record.set_editor_property("numeric_parameters", numeric)
        placement_records.append(record)
    asset.set_editor_property("placements", placement_records)
    asset.modify()
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    return asset


def configure_static_actor(actor, mesh_path=None, collision_path=None, mask=0x3F, physical=None):
    actor.set_editor_property("default_mesh", maybe_asset(mesh_path))
    actor.set_editor_property("default_collision_mesh", maybe_asset(collision_path))
    actor.set_editor_property("act_mask", int(mask))
    rerun(actor)
    if physical:
        actor.get_editor_property("collision_mesh").set_phys_material_override(physical)


def configure_mover(actor, item, motion, speed=15.0, travel=510.0, degrees=0.703125):
    asset_name = item["asset"]
    actor.set_editor_property("default_mesh", require_asset(RENDER[asset_name]))
    actor.set_editor_property("default_collision_mesh", require_asset(DYNAMIC_COLLISION[asset_name]))
    actor.set_editor_property("act_mask", act_mask(item))
    actor.set_editor_property("motion", motion)
    actor.set_editor_property("simulation_hz", 30.0)
    actor.set_editor_property("speed_per_frame", float(speed))
    actor.set_editor_property("travel_distance", float(travel))
    actor.set_editor_property("rotation_degrees_per_frame", float(degrees))
    actor.set_editor_property("manual_rider_conveyance", True)
    actor.set_editor_property("rider_sensor_extent", unreal.Vector(420.0, 420.0, 120.0))
    actor.set_editor_property("rider_sensor_offset", unreal.Vector(0.0, 0.0, 100.0))
    rerun(actor)


def verify_shell():
    requirements = {
        "BP_Spyro_C": None,
        "BP_Skybox_C": None,
        "BP_Portal_ReturnHome_C": None,
        "BP_KillPlane_C": None,
        "BP_Total_Gems_C": None,
    }
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if class_name(actor) in requirements and requirements[class_name(actor)] is None:
            requirements[class_name(actor)] = actor
    missing = [name for name, actor in requirements.items() if actor is None]
    if missing:
        raise RuntimeError("Level 5 integration shell is missing: " + ", ".join(missing))
    for name, actor in requirements.items():
        REPORT["shell"][name] = actor.get_path_name()
    spyro = requirements["BP_Spyro_C"]
    spyro.set_actor_location_and_rotation(
        unreal.Vector(2600.0, 5120.0, 304.0),
        unreal.Rotator(pitch=0.0, yaw=-90.0, roll=0.0),
        False,
        False,
    )
    return requirements


def assemble():
    global ACTORS
    if not unreal.EditorLevelLibrary.load_level(LEVEL):
        raise RuntimeError("Unable to load " + LEVEL)
    shell = verify_shell()
    world = unreal.EditorLevelLibrary.get_editor_world()
    world.get_world_settings().set_editor_property("force_no_precomputed_lighting", True)
    ACTORS = update_actor_index()

    physical = {
        key: ensure_physical_material(name, friction)
        for key, (name, friction) in PHYSICS.items()
    }
    course_definition = ensure_course_definition()
    unreal.log_warning("SM64_WF_STEP course_definition_ready")

    for stable_id, mesh_path in STATIC_RENDER.items():
        unreal.log_warning("SM64_WF_STEP static_render " + stable_id)
        actor = ensure_actor(stable_id, unreal.SM64StaticCourseActor)
        configure_static_actor(actor, mesh_path, None)

    for surface in ("default", "very_slippery", "slippery", "not_slippery", "wall_misc", "noise_default"):
        stable_id = "wf/collision/terrain/" + surface
        actor = ensure_actor(stable_id, unreal.SM64StaticCourseActor)
        configure_static_actor(actor, None, TERRAIN_COLLISION[surface], 0x3F, physical[surface])

    manager = ensure_actor("wf/system/course_manager", unreal.SM64CourseManager)
    manager.set_editor_property("course_definition", course_definition)
    manager.set_editor_property("current_act", 1)
    manager.set_editor_property("session_seed", 0x6405)
    manager.set_editor_property("use_spyro_save_lineage", True)
    manager.set_editor_property("spyro_progress_map_property", "SM64_CourseProgress")
    manager.set_editor_property("use_standalone_save_fallback", True)
    adapter = ensure_actor(
        "wf/system/player_adapter",
        unreal.SM64PlayerAdapter,
        shell["BP_Spyro_C"].get_actor_location(),
        shell["BP_Spyro_C"].get_actor_rotation(),
    )
    adapter.set_editor_property("spyro_actor", shell["BP_Spyro_C"])
    adapter.set_editor_property("attack_radius", 180.0)
    adapter.set_editor_property("auto_detect_spyro_attacks", True)

    # Preserve the shell kill-plane actor because the Level Blueprint owns a
    # serialized reference to it, but transfer pawn death to the reusable SM64
    # course trigger.  The separate deep enemy-catcher remains intact.
    legacy_kill_plane = shell["BP_KillPlane_C"]
    for component in legacy_kill_plane.get_components_by_class(unreal.PrimitiveComponent):
        if component.get_name() in ("Box", "visualizer"):
            component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)

    death_surface = ensure_actor(
        "wf/system/death_surface",
        unreal.SM64CourseTrigger,
        unreal.Vector(0.5, 0.5, -3103.0),
        unreal.Rotator(),
    )
    death_surface.set_editor_property("act_mask", 0x3F)
    death_surface.set_editor_property("trigger_type", unreal.SM64CourseTriggerType.DEATH)
    death_surface.set_editor_property("box_extent", unreal.Vector(8191.5, 8191.5, 32.0))
    death_surface.set_editor_property("auto_retry_act_on_death", True)
    death_surface.set_editor_property("use_latest_checkpoint", False)
    rerun(death_surface)

    # Camera floor semantics are represented as volumes above the exact source
    # polygons. CAMERA_MIDDLE is disconnected in the decomp, so it becomes two
    # tight boxes rather than one oversized combined asset bound.
    camera_specs = (
        (
            "wf/system/camera/boss_fight",
            unreal.Vector(256.5, -211.5, 3834.0),
            unreal.Vector(1279.5, 1747.5, 250.0),
            unreal.SM64CameraSurfaceMode.BOSS_CAMERA,
            "WF_BossFight",
            10,
        ),
        (
            "wf/system/camera/middle_lower",
            unreal.Vector(3277.0, -3583.0, 1325.0),
            unreal.Vector(819.0, 512.0, 250.0),
            unreal.SM64CameraSurfaceMode.CAMERA_MIDDLE,
            "WF_Middle",
            5,
        ),
        (
            "wf/system/camera/middle_upper",
            unreal.Vector(3405.0, -255.5, 2554.0),
            unreal.Vector(179.0, 255.5, 250.0),
            unreal.SM64CameraSurfaceMode.CAMERA_MIDDLE,
            "WF_Middle",
            5,
        ),
    )
    for stable_id, actor_location, extent, mode, profile, priority in camera_specs:
        camera = ensure_actor(stable_id, unreal.SM64CameraSurfaceVolume, actor_location)
        camera.set_editor_property("act_mask", 0x3F)
        camera.set_editor_property("box_extent", extent)
        camera.set_editor_property("camera_mode", mode)
        camera.set_editor_property("camera_profile", profile)
        camera.set_editor_property("priority", priority)
        camera.set_editor_property("blend_time", 0.25)
        rerun(camera)

    mover_specs = [
        ("wf/object/bhvsmallbomp/000", unreal.SM64PlatformMotion.SMALL_BOMP, 15.0, 510.0, 0.0),
        ("wf/object/bhvsmallbomp/001", unreal.SM64PlatformMotion.SMALL_BOMP, 15.0, 510.0, 0.0),
        ("wf/object/bhvlargebomp/000", unreal.SM64PlatformMotion.LARGE_BOMP, 15.0, 510.0, 0.0),
        ("wf/object/bhvwfrotatingwoodenplatform/000", unreal.SM64PlatformMotion.ROTATING_WOOD, 0.0, 0.0, 1.40625),
        ("wf/object/bhvwfslidingplatform/000", unreal.SM64PlatformMotion.SLIDING, 15.0, 510.0, 0.0),
        ("wf/object/bhvwfslidingplatform/001", unreal.SM64PlatformMotion.SLIDING, 10.0, 510.0, 0.0),
        ("wf/object/bhvwfslidingplatform/002", unreal.SM64PlatformMotion.SLIDING, 20.0, 510.0, 0.0),
        ("wf/object/bhvrotatingplatform/000", unreal.SM64PlatformMotion.ROTATING_CONTINUOUS, 0.0, 0.0, 0.703125),
        ("wf/object/bhvrotatingplatform/001", unreal.SM64PlatformMotion.ROTATING_CONTINUOUS, 0.0, 0.0, 0.703125),
        ("wf/object/bhvrotatingplatform/002", unreal.SM64PlatformMotion.ROTATING_CONTINUOUS, 0.0, 0.0, 0.703125),
    ]
    for stable_id, motion, speed, travel, degrees in mover_specs:
        item = PLACEMENTS[stable_id]
        actor = ensure_actor(stable_id, unreal.SM64MovingPlatformBase, location(item), rotation(item))
        configure_mover(actor, item, motion, speed, travel, degrees)

    bridge_item = PLACEMENTS["wf/object/bhvtumblingbridge/000"]
    bridge = ensure_actor(
        bridge_item["stable_id"], unreal.WFTumblingBridgeController, location(bridge_item), rotation(bridge_item)
    )
    bridge.set_editor_property("act_mask", act_mask(bridge_item))
    bridge.set_editor_property("piece_mesh", require_asset(RENDER["BridgePiece"]))
    bridge.set_editor_property("piece_collision_mesh", require_asset(DYNAMIC_COLLISION["BridgePiece"]))
    bridge.set_editor_property("piece_count", 9)
    bridge.set_editor_property("first_piece_offset", -512.0)
    bridge.set_editor_property("piece_spacing", 128.0)
    bridge.set_editor_property("spawn_distance", 1000.0)
    bridge.set_editor_property("reset_distance", 1200.0)
    rerun(bridge)

    tower_group_item = PLACEMENTS["wf/object/bhvtowerplatformgroup/000"]
    tower_group = ensure_actor(
        tower_group_item["stable_id"],
        unreal.WFTowerPlatformGroup,
        location(tower_group_item),
        rotation(tower_group_item),
    )
    tower_group.set_editor_property("act_mask", act_mask(tower_group_item))
    tower_group.set_editor_property("platform_mesh", require_asset(RENDER["TowerPlatform"]))
    tower_group.set_editor_property("elevator_mesh", require_asset(RENDER["TowerPlatform"]))
    tower_group.set_editor_property(
        "platform_collision_mesh", require_asset(DYNAMIC_COLLISION["TowerPlatform"])
    )
    tower_group.set_editor_property("radius", 704.0)
    tower_group.set_editor_property("platform_height_step", 100.0)
    tower_group.set_editor_property("activation_height_below_root", 700.0)

    board_item = PLACEMENTS["wf/object/bhvkickableboard/000"]
    board = ensure_actor(board_item["stable_id"], unreal.WFKickableBoard, location(board_item), rotation(board_item))
    board.set_editor_property("act_mask", act_mask(board_item))
    board.set_editor_property("upright_mesh", require_asset(RENDER["KickWood"]))
    board.set_editor_property("felled_mesh", require_asset(RENDER["KickWoodDown"]))
    board.set_editor_property("collision_asset", require_asset(DYNAMIC_COLLISION["KickWood"]))
    board.set_editor_property("simulation_hz", 30.0)
    rerun(board)

    for stable_id, required_attack in (
        ("wf/object/bhvwfbreakablewallright/000", unreal.SM64AttackType.CANNON),
        ("wf/object/bhvwfbreakablewallleft/000", unreal.SM64AttackType.CANNON),
        ("wf/object/bhvtowerdoor/000", unreal.SM64AttackType.CHARGE),
    ):
        item = PLACEMENTS[stable_id]
        asset_name = item["asset"]
        actor = ensure_actor(stable_id, unreal.SM64BreakableActor, location(item), rotation(item))
        actor.set_editor_property("act_mask", act_mask(item))
        actor.set_editor_property("default_mesh", require_asset(RENDER[asset_name]))
        actor.set_editor_property("default_collision_mesh", require_asset(DYNAMIC_COLLISION[asset_name]))
        actor.set_editor_property("required_attack", required_attack)
        actor.set_editor_property("destroy_actor_on_break", False)
        rerun(actor)

    for stable_id in ("wf/object/bhvtower/000", "wf/object/bhvbulletbillcannon/000"):
        item = PLACEMENTS[stable_id]
        asset_name = item["asset"]
        actor = ensure_actor(stable_id, unreal.SM64StaticCourseActor, location(item), rotation(item))
        configure_static_actor(
            actor, RENDER[asset_name], DYNAMIC_COLLISION[asset_name], act_mask(item), physical["default"]
        )

    climb_specs = (
        ("wf/object/bhvgiantpole/000", unreal.SM64ClimbableType.GIANT_WF_POLE),
        ("wf/special/special_bubble_tree/000", unreal.SM64ClimbableType.TREE),
    )
    for stable_id, climb_type in climb_specs:
        item = PLACEMENTS[stable_id]
        actor = ensure_actor(stable_id, unreal.SM64ClimbableActor, location(item), rotation(item))
        actor.set_editor_property("act_mask", act_mask(item))
        actor.set_editor_property("default_mesh", require_asset(RENDER[item["asset"]]))
        actor.set_editor_property("climbable_type", climb_type)
        actor.set_editor_property("apply_vanilla_metadata_on_construction", True)
        rerun(actor)

    water_render = ensure_actor("wf/render/water", unreal.SM64StaticCourseActor)
    configure_static_actor(water_render, WATER + "/SM_WF_Water", None)
    bounds = COURSE_DATA["water"]["unreal_bounds_xy_cm"]
    min_x, min_y, max_x, max_y = [float(value) for value in bounds]
    surface_z = float(COURSE_DATA["water"]["surface_height_cm"])
    bottom_z = float(COURSE_DATA["death_plane_height_cm"])
    water_center = unreal.Vector(
        (min_x + max_x) * 0.5,
        (min_y + max_y) * 0.5,
        (surface_z + bottom_z) * 0.5,
    )
    water_volume = ensure_actor("wf/system/water_volume", unreal.SM64WaterVolume, water_center)
    water_volume.set_editor_property("act_mask", 0x3F)
    water_surface = water_volume.get_editor_property("surface_mesh")
    water_surface.set_editor_property("static_mesh", require_asset(WATER + "/SM_WF_Water"))
    water_surface.set_visibility(False, True)
    water_surface.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    water_surface.set_editor_property("cast_dynamic_shadow", False)
    water_volume.get_editor_property("water_trigger").set_box_extent(
        unreal.Vector((max_x - min_x) * 0.5, (max_y - min_y) * 0.5, (surface_z - bottom_z) * 0.5)
    )

    warp_actors = {}
    for warp_data in COURSE_DATA["warps"]:
        stable_id = "wf/warp/" + warp_data["warp_id"]
        actor = ensure_actor(
            stable_id,
            unreal.SM64Warp,
            unreal.Vector(*warp_data["unreal_location_cm"]),
            unreal.Rotator(pitch=0.0, yaw=float(warp_data["unreal_yaw_deg"]), roll=0.0),
        )
        actor.set_editor_property("warp_id", warp_data["warp_id"])
        actor.set_editor_property("act_mask", 0x3F)
        warp_actors[warp_data["warp_id"]] = actor
    for warp_data in COURSE_DATA["warps"]:
        warp_actors[warp_data["warp_id"]].set_editor_property(
            "destination", warp_actors[warp_data["destination_warp_id"]]
        )

    yellow_mesh = require_asset(common_actor_asset("yellow_coin"))
    blue_mesh = require_asset(common_actor_asset("blue_coin"))
    yellow_coin_class = ensure_blueprint_class(
        "BP_SM64_YellowCoin",
        unreal.SM64Collectible,
        {"default_mesh": yellow_mesh, "coin_value": 1, "red_coin": False},
    )
    blue_coin_class = ensure_blueprint_class(
        "BP_SM64_BlueCoin",
        unreal.SM64Collectible,
        {"default_mesh": blue_mesh, "coin_value": 5, "red_coin": False},
    )
    one_up_mesh = require_asset(common_actor_asset("mushroom_1up"))
    one_up_class = ensure_blueprint_class(
        "BP_SM64_OneUp",
        unreal.SM64Collectible,
        {"default_mesh": one_up_mesh, "coin_value": 0, "one_up": True},
    )

    # Exact common actor art is imported separately from the location DAEs.
    # These runtime actors intentionally remain independent map instances so
    # act gating, reset, save identity, and future reimports stay deterministic.
    whomp_mesh = require_asset(common_actor_asset("whomp", "Skeletal"))
    whomp_collision = require_asset(common_collision("whomp"))
    whomp_animations = common_animations("whomp", 2)
    for stable_id in (
        "wf/object/bhvsmallwhomp/000",
        "wf/object/bhvsmallwhomp/001",
        "wf/object/bhvwhompkingboss/000",
    ):
        item = PLACEMENTS[stable_id]
        actor = ensure_actor(stable_id, unreal.WFWhomp, location(item), rotation(item))
        actor.set_editor_property("act_mask", act_mask(item))
        actor.set_editor_property("default_skeletal_mesh", whomp_mesh)
        actor.set_editor_property("default_collision_mesh", whomp_collision)
        actor.set_editor_property("walk_animation", whomp_animations[0])
        actor.set_editor_property("prepare_jump_animation", whomp_animations[1])
        actor.set_editor_property("drop_coin_class", yellow_coin_class)
        actor.set_editor_property("king_whomp", item["behavior"] == "bhvWhompKingBoss")
        actor.set_editor_property("boss_star_index", 0)
        actor.set_editor_property("boss_star_location", unreal.Vector(180.0, 340.0, 3880.0))
        rerun(actor)

    thwomp_mesh = require_asset(common_actor_asset("thwomp"))
    thwomp_collision = require_asset(common_collision("thwomp"))
    for stable_id in ("wf/object/bhvthwomp2/000", "wf/object/bhvthwomp/000"):
        item = PLACEMENTS[stable_id]
        actor = ensure_actor(stable_id, unreal.WFThwomp, location(item), rotation(item))
        actor.set_editor_property("act_mask", act_mask(item))
        actor.set_editor_property("default_mesh", thwomp_mesh)
        actor.set_editor_property("thwomp_collision_asset", thwomp_collision)
        actor.set_editor_property("thwomp2_collision_asset", thwomp_collision)
        actor.set_editor_property("use_thwomp2_collision", item["behavior"] == "bhvThwomp2")
        rerun(actor)

    piranha_mesh = require_asset(common_actor_asset("piranha_plant", "Skeletal"))
    piranha_animations = common_animations("piranha_plant", 10)
    for index in range(3):
        stable_id = "wf/object/bhvpiranhaplant/{:03d}".format(index)
        item = PLACEMENTS[stable_id]
        actor = ensure_actor(stable_id, unreal.WFPiranhaPlant, location(item), rotation(item))
        actor.set_editor_property("act_mask", act_mask(item))
        actor.set_editor_property("default_skeletal_mesh", piranha_mesh)
        actor.set_editor_property("animations", piranha_animations)
        actor.set_editor_property("blue_coin_class", blue_coin_class)
        rerun(actor)

    butterfly_mesh = require_asset(common_actor_asset("butterfly", "Skeletal"))
    butterfly_animations = common_animations("butterfly", 2)
    for index in range(10):
        stable_id = "wf/object/bhvbutterfly/{:03d}".format(index)
        item = PLACEMENTS[stable_id]
        actor = ensure_actor(stable_id, unreal.WFButterfly, location(item), rotation(item))
        actor.set_editor_property("act_mask", act_mask(item))
        actor.set_editor_property("default_skeletal_mesh", butterfly_mesh)
        actor.set_editor_property("flight_animation", butterfly_animations[0])
        actor.set_editor_property("rest_animation", butterfly_animations[1])
        actor.set_editor_property("triplet_butterfly", False)
        rerun(actor)

    triplet_item = PLACEMENTS["wf/macro/macro_butterfly_triplet_no_bombs/000"]
    for index in range(3):
        stable_id = triplet_item["stable_id"] if index == 0 else (
            "wf/runtime/butterfly_triplet_no_bombs/{:03d}".format(index)
        )
        actor = ensure_actor(stable_id, unreal.WFButterfly, location(triplet_item), rotation(triplet_item))
        actor.set_editor_property("act_mask", act_mask(triplet_item))
        actor.set_editor_property("default_skeletal_mesh", butterfly_mesh)
        actor.set_editor_property("flight_animation", butterfly_animations[0])
        actor.set_editor_property("rest_animation", butterfly_animations[1])
        actor.set_editor_property("triplet_butterfly", True)
        actor.set_editor_property("selected_for_one_up", index == 0)
        actor.set_editor_property("one_up_class", one_up_class)
        actor.set_editor_property("base_yaw_degrees", float(index * 120))
        rerun(actor)

    bullet_item = PLACEMENTS["wf/object/bhvbulletbill/000"]
    bullet = ensure_actor(
        bullet_item["stable_id"], unreal.WFBulletBill, location(bullet_item), rotation(bullet_item)
    )
    bullet.set_editor_property("act_mask", act_mask(bullet_item))
    bullet.set_editor_property("default_mesh", require_asset(common_actor_asset("bullet_bill")))
    rerun(bullet)

    checker_item = PLACEMENTS["wf/object/bhvcheckerboardelevatorgroup/000"]
    checker = ensure_actor(
        checker_item["stable_id"],
        unreal.WFCheckerboardElevatorPair,
        location(checker_item),
        rotation(checker_item),
    )
    checker.set_editor_property("act_mask", act_mask(checker_item))
    checker.set_editor_property(
        "default_render_mesh", require_asset(common_actor_asset("checkerboard_platform"))
    )
    checker.set_editor_property(
        "default_collision_mesh", require_asset(common_collision("checkerboard_platform"))
    )
    checker.set_editor_property("group_variant", 0)
    checker.set_editor_property("move_duration_parameter", 0)
    rerun(checker)

    cannon_item = PLACEMENTS["wf/macro/macro_cannon_closed/000"]
    cannon = ensure_actor(
        cannon_item["stable_id"], unreal.WFCannon, location(cannon_item), rotation(cannon_item)
    )
    cannon.set_editor_property("act_mask", act_mask(cannon_item))
    cannon.set_editor_property("default_lid_mesh", require_asset(common_actor_asset("cannon_lid")))
    cannon.set_editor_property(
        "default_lid_collision_mesh", require_asset(common_collision("cannon_lid"))
    )
    cannon.set_editor_property("default_base_mesh", require_asset(common_actor_asset("cannon_base")))
    cannon.set_editor_property(
        "default_barrel_mesh", require_asset(common_actor_asset("cannon_barrel"))
    )
    cannon.set_editor_property("initial_barrel_yaw_degrees", 90.0)
    rerun(cannon)

    buddy_item = PLACEMENTS["wf/object/bhvbobombbuddyopenscannon/000"]
    buddy = ensure_actor(
        buddy_item["stable_id"], unreal.WFBobombBuddy, location(buddy_item), rotation(buddy_item)
    )
    buddy.set_editor_property("act_mask", act_mask(buddy_item))
    buddy.set_editor_property(
        "default_skeletal_mesh", require_asset(common_actor_asset("bobomb_buddy", "Skeletal"))
    )
    buddy.set_editor_property("idle_animation", common_animations("bobomb_buddy", 2)[0])
    buddy.set_editor_property("linked_cannon", cannon)
    buddy.set_editor_property("first_dialog_id", 47)
    buddy.set_editor_property("ready_dialog_id", 106)
    rerun(buddy)

    hoot_item = PLACEMENTS["wf/object/bhvhoot/000"]
    hoot = ensure_actor(hoot_item["stable_id"], unreal.WFHoot, location(hoot_item), rotation(hoot_item))
    hoot.set_editor_property("act_mask", act_mask(hoot_item))
    hoot.set_editor_property("default_skeletal_mesh", require_asset(common_actor_asset("hoot", "Skeletal")))
    hoot_animations = common_animations("hoot", 2)
    hoot.set_editor_property("free_flight_animation", hoot_animations[0])
    hoot.set_editor_property("carry_animation", hoot_animations[1])
    rerun(hoot)

    star_mesh = require_asset(common_actor_asset("star"))
    star_specs = (
        ("wf/star/mission_01_boss", unreal.Vector(180.0, 340.0, 3880.0), unreal.Rotator(), 0x01, 0, False),
        ("wf/object/bhvstar/000", None, None, act_mask(PLACEMENTS["wf/object/bhvstar/000"]), 1, True),
        ("wf/object/bhvstar/001", None, None, act_mask(PLACEMENTS["wf/object/bhvstar/001"]), 2, True),
        ("wf/object/bhvhiddenredcoinstar/000", None, None, 0x3F, 3, False),
        ("wf/object/bhvstar/002", None, None, act_mask(PLACEMENTS["wf/object/bhvstar/002"]), 4, True),
        ("wf/object/bhvstar/003", None, None, act_mask(PLACEMENTS["wf/object/bhvstar/003"]), 5, True),
    )
    for stable_id, star_location, star_rotation, mask, star_index, start_available in star_specs:
        if star_location is None:
            item = PLACEMENTS[stable_id]
            star_location, star_rotation = location(item), rotation(item)
        actor = ensure_actor(stable_id, unreal.SM64PowerStar, star_location, star_rotation)
        actor.set_editor_property("act_mask", int(mask))
        actor.set_editor_property("default_mesh", star_mesh)
        actor.set_editor_property("star_index", int(star_index))
        actor.set_editor_property("start_available", bool(start_available))
        actor.set_editor_property("wait_for_blueprint_collection_cutscene", False)
        actor.set_editor_property("home_location", star_location)
        rerun(actor)

    hundred_coin_location = unreal.Vector(2600.0, 5120.0, 504.0)
    hundred_coin_star = ensure_actor(
        "wf/star/100_coin", unreal.SM64PowerStar, hundred_coin_location, unreal.Rotator()
    )
    hundred_coin_star.set_editor_property("act_mask", 0x3F)
    hundred_coin_star.set_editor_property("default_mesh", star_mesh)
    hundred_coin_star.set_editor_property("star_index", 6)
    hundred_coin_star.set_editor_property("start_available", False)
    hundred_coin_star.set_editor_property("wait_for_blueprint_collection_cutscene", False)
    hundred_coin_star.set_editor_property("home_location", hundred_coin_location)
    set_first_editor_property(
        hundred_coin_star, ("100_coin_star", "b100_coin_star"), True
    )
    set_first_editor_property(hundred_coin_star, ("no_exit", "bno_exit"), True)
    rerun(hundred_coin_star)

    red_mesh = require_asset(common_actor_asset("red_coin"))
    for index in range(8):
        stable_id = "wf/macro/macro_red_coin/{:03d}".format(index)
        item = PLACEMENTS[stable_id]
        actor = ensure_actor(stable_id, unreal.SM64Collectible, location(item), rotation(item))
        actor.set_editor_property("act_mask", act_mask(item))
        actor.set_editor_property("default_mesh", red_mesh)
        actor.set_editor_property("coin_value", 2)
        actor.set_editor_property("red_coin", True)
        rerun(actor)
    for index in range(4):
        stable_id = "wf/macro/macro_yellow_coin_1/{:03d}".format(index)
        item = PLACEMENTS[stable_id]
        actor = ensure_actor(stable_id, unreal.SM64Collectible, location(item), rotation(item))
        actor.set_editor_property("act_mask", act_mask(item))
        actor.set_editor_property("default_mesh", yellow_mesh)
        actor.set_editor_property("coin_value", 1)
        rerun(actor)

    formation_types = {
        "macro_coin_line_horizontal": unreal.SM64CoinFormationType.HORIZONTAL_LINE,
        "macro_coin_ring_horizontal": unreal.SM64CoinFormationType.HORIZONTAL_RING,
        "macro_coin_ring_horizontal_flying": unreal.SM64CoinFormationType.HORIZONTAL_RING,
        "macro_coin_arrow": unreal.SM64CoinFormationType.ARROW,
    }
    for item in PLACEMENT_DATA["placements"]:
        behavior = item.get("behavior")
        if behavior not in formation_types:
            continue
        actor = ensure_actor(item["stable_id"], unreal.SM64CoinFormation, location(item), rotation(item))
        actor.set_editor_property("act_mask", act_mask(item))
        actor.set_editor_property("formation_type", formation_types[behavior])
        actor.set_editor_property("flying", behavior == "macro_coin_ring_horizontal_flying")
        actor.set_editor_property("coin_class", yellow_coin_class)

    switch_item = PLACEMENTS["wf/macro/macro_blue_coin_switch/000"]
    blue_switch = ensure_actor(
        switch_item["stable_id"], unreal.SM64BlueCoinSwitch, location(switch_item), rotation(switch_item)
    )
    blue_switch.set_editor_property("act_mask", act_mask(switch_item))
    blue_switch.set_editor_property(
        "default_mesh", require_asset(common_actor_asset("blue_coin_switch"))
    )
    blue_switch.set_editor_property(
        "default_collision_mesh", require_asset(common_collision("blue_coin_switch"))
    )
    blue_switch.set_editor_property("challenge_id", "WF_BlueCoins")
    rerun(blue_switch)
    for index in range(4):
        stable_id = "wf/macro/macro_hidden_blue_coin/{:03d}".format(index)
        item = PLACEMENTS[stable_id]
        actor = ensure_actor(stable_id, unreal.SM64TimedBlueCoin, location(item), rotation(item))
        actor.set_editor_property("act_mask", act_mask(item))
        actor.set_editor_property("default_mesh", blue_mesh)
        actor.set_editor_property("challenge_id", "WF_BlueCoins")
        actor.set_editor_property("coin_value", 5)
        rerun(actor)

    sign_mesh = require_asset(common_actor_asset("wooden_signpost"))
    sign_collision = require_asset(common_collision("wooden_signpost"))
    for item in PLACEMENT_DATA["placements"]:
        if item.get("behavior") != "macro_wooden_signpost":
            continue
        dialog_match = re.search(r"(\d+)", item.get("behavior_param", "0"))
        actor = ensure_actor(item["stable_id"], unreal.SM64SignActor, location(item), rotation(item))
        actor.set_editor_property("act_mask", act_mask(item))
        actor.set_editor_property("default_mesh", sign_mesh)
        actor.set_editor_property("default_collision_mesh", sign_collision)
        actor.set_editor_property("dialog_id", int(dialog_match.group(1)) if dialog_match else 0)
        rerun(actor)

    box_mesh = require_asset(common_actor_asset("breakable_box"))
    box_collision = require_asset(common_collision("breakable_box"))
    for index in range(2):
        stable_id = "wf/macro/macro_breakable_box_small/{:03d}".format(index)
        item = PLACEMENTS[stable_id]
        actor = ensure_actor(stable_id, unreal.SM64BreakableActor, location(item), rotation(item))
        actor.set_editor_property("act_mask", act_mask(item))
        actor.set_editor_property("default_mesh", box_mesh)
        actor.set_editor_property("default_collision_mesh", box_collision)
        actor.set_editor_property("required_attack", unreal.SM64AttackType.CHARGE)
        actor.set_editor_property("destroy_actor_on_break", False)
        actor.set_editor_property("drop_coin_class", yellow_coin_class)
        actor.set_editor_property("drop_coin_count", 3)
        rerun(actor)

    metal_cap_id = "wf/macro/macro_box_metal_cap/000"
    metal_cap_item = PLACEMENTS[metal_cap_id]
    metal_cap_mesh = require_asset(
        "/Game/Spyro64/SM64/Common/Actors/ExclamationBox/Static/"
        "SM_SM64_ExclamationBox_MetalCap"
    )
    metal_cap_box = ensure_actor(
        metal_cap_id,
        unreal.SM64BreakableActor,
        location(metal_cap_item),
        rotation(metal_cap_item),
    )
    metal_cap_box.set_editor_property("act_mask", act_mask(metal_cap_item))
    metal_cap_box.set_editor_property("default_mesh", metal_cap_mesh)
    metal_cap_box.set_editor_property("default_collision_mesh", metal_cap_mesh)
    metal_cap_box.set_editor_property("required_attack", unreal.SM64AttackType.CHARGE)
    metal_cap_box.set_editor_property("destroy_actor_on_break", False)
    metal_cap_box.set_editor_property("drop_coin_count", 0)
    rerun(metal_cap_box)

    one_up_item = PLACEMENTS["wf/object/bhv1up/000"]
    one_up = ensure_actor(
        one_up_item["stable_id"], unreal.SM64Collectible, location(one_up_item), rotation(one_up_item)
    )
    one_up.set_editor_property("act_mask", act_mask(one_up_item))
    one_up.set_editor_property("default_mesh", one_up_mesh)
    one_up.set_editor_property("coin_value", 0)
    one_up.set_editor_property("one_up", True)
    rerun(one_up)

    hidden_pair_item = PLACEMENTS["wf/macro/macro_hidden_1up/000"]
    hidden_pair = ensure_actor(
        hidden_pair_item["stable_id"],
        unreal.SM64HiddenOneUp,
        location(hidden_pair_item),
        rotation(hidden_pair_item),
    )
    hidden_pair.set_editor_property("act_mask", act_mask(hidden_pair_item))
    hidden_pair.set_editor_property("default_mesh", one_up_mesh)
    hidden_pair.set_editor_property("trigger_group", "WF_HiddenPair")
    hidden_pair.set_editor_property("required_trigger_count", 2)
    hidden_pair.set_editor_property("home_toward_player", False)
    hidden_pair.set_editor_property("home_location", location(hidden_pair_item))
    hidden_pair.set_editor_property("reveal_offset", unreal.Vector())
    hidden_pair.set_editor_property("one_up", True)
    rerun(hidden_pair)
    for index in range(2):
        stable_id = "wf/macro/macro_hidden_1up_trigger/{:03d}".format(index)
        item = PLACEMENTS[stable_id]
        trigger = ensure_actor(
            stable_id, unreal.SM64HiddenOneUpTrigger, location(item), rotation(item)
        )
        trigger.set_editor_property("act_mask", act_mask(item))
        trigger.set_editor_property("trigger_group", "WF_HiddenPair")
        trigger.set_editor_property("trigger_index", index)
        trigger.set_editor_property("box_extent", unreal.Vector(100.0, 100.0, 50.0))
        rerun(trigger)

    pole_item = PLACEMENTS["wf/macro/macro_hidden_1up_in_pole/000"]
    pole_reward = ensure_actor(
        pole_item["stable_id"], unreal.SM64HiddenOneUp, location(pole_item), rotation(pole_item)
    )
    pole_reward.set_editor_property("act_mask", act_mask(pole_item))
    pole_reward.set_editor_property("default_mesh", one_up_mesh)
    pole_reward.set_editor_property("trigger_group", "WF_PoleOneUp")
    pole_reward.set_editor_property("required_trigger_count", 2)
    pole_reward.set_editor_property("home_toward_player", True)
    pole_reward.set_editor_property("home_location", location(pole_item))
    pole_reward.set_editor_property("reveal_offset", unreal.Vector(0.0, 0.0, 50.0))
    pole_reward.set_editor_property("one_up", True)
    rerun(pole_reward)
    pole_base = location(pole_item)
    for index, height_offset in enumerate((0.0, -200.0)):
        trigger = ensure_actor(
            "wf/runtime/hidden_1up_pole_trigger/{:03d}".format(index),
            unreal.SM64HiddenOneUpTrigger,
            pole_base + unreal.Vector(0.0, 0.0, height_offset),
            unreal.Rotator(),
        )
        trigger.set_editor_property("act_mask", 0x3F)
        trigger.set_editor_property("trigger_group", "WF_PoleOneUp")
        trigger.set_editor_property("trigger_index", index)
        trigger.set_editor_property("box_extent", unreal.Vector(100.0, 100.0, 50.0))
        rerun(trigger)

    # The preserved template skybox and kill-plane use large movable helper
    # meshes.  They do not need dynamic shadows, and UE's Map Check correctly
    # flags the otherwise very expensive pre-shadow path.
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        for component in actor.get_components_by_class(unreal.StaticMeshComponent):
            if (
                class_name(actor) in ("BP_Skybox_C", "BP_KillPlane_C")
                or component.get_name().lower() == "visualizer"
            ):
                component.set_editor_property("cast_dynamic_shadow", False)

    # The preserved BP_KillPlane still exists for its serialized Level Blueprint
    # reference; pawn retry is owned by wf/system/death_surface above.
    unreal.EditorLevelLibrary.save_current_level()
    REPORT["generated_actor_count"] = len(update_actor_index())
    REPORT["level_actor_count"] = len(unreal.EditorLevelLibrary.get_all_level_actors())
    REPORT["course_placement_count"] = len(PLACEMENT_DATA["placements"])
    unreal.log_warning("SM64_WF_ASSEMBLY=" + json.dumps(REPORT, sort_keys=True))


assemble()
