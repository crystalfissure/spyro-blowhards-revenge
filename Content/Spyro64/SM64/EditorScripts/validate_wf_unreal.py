"""Read-only Unreal acceptance checks for the assembled Whomp's Fortress milestone."""

from __future__ import print_function

import json
import math
import os
import unreal


LEVEL = "/Game/Spyro64/Levels/05_Level5"
COURSE_ASSET = "/Game/Spyro64/SM64/WhompsFortress/Data/PDA_WF_CourseDefinition"
SPARX_BLUEPRINT = "/Game/SpyroContent/Global_Assets/Global_Characters/Sparx/Sparx_BP"
SOURCE_ROOT = os.path.normpath(
    r"C:\Users\adace\Desktop\spyro-blowhards-revenge\Content\Spyro64"
    r"\SM64\Source\WhompsFortress"
)
PLACEMENTS_FILE = os.path.join(SOURCE_ROOT, "Manifest", "wf_placements.json")
TAG_PREFIX = "SM64StableId="

SHELL_PATHS = {
    "BP_Spyro_C": LEVEL + ".05_Level5:PersistentLevel.BP_S3_Spyro_2",
    "BP_Skybox_C": LEVEL + ".05_Level5:PersistentLevel.BP_Skybox_2",
    "BP_Portal_ReturnHome_C": LEVEL + ".05_Level5:PersistentLevel.BP_Portal_ReturnHome_2",
    "BP_KillPlane_C": LEVEL + ".05_Level5:PersistentLevel.BP_KillPlane_2",
    "BP_Total_Gems_C": LEVEL + ".05_Level5:PersistentLevel.BP_Total_Gems_2",
}

STATIC_IDS = {
    "wf/render/static_base",
    "wf/render/static_props",
    "wf/collision/terrain/default",
    "wf/collision/terrain/very_slippery",
    "wf/collision/terrain/slippery",
    "wf/collision/terrain/not_slippery",
    "wf/collision/terrain/wall_misc",
    "wf/collision/terrain/noise_default",
    "wf/object/bhvtower/000",
    "wf/object/bhvbulletbillcannon/000",
    "wf/render/water",
}
MOVER_IDS = {
    "wf/object/bhvsmallbomp/000",
    "wf/object/bhvsmallbomp/001",
    "wf/object/bhvlargebomp/000",
    "wf/object/bhvwfrotatingwoodenplatform/000",
    "wf/object/bhvwfslidingplatform/000",
    "wf/object/bhvwfslidingplatform/001",
    "wf/object/bhvwfslidingplatform/002",
    "wf/object/bhvrotatingplatform/000",
    "wf/object/bhvrotatingplatform/001",
    "wf/object/bhvrotatingplatform/002",
}
CLASS_BY_ID = dict((stable_id, "SM64StaticCourseActor") for stable_id in STATIC_IDS)
CLASS_BY_ID.update(dict((stable_id, "SM64MovingPlatformBase") for stable_id in MOVER_IDS))
CLASS_BY_ID.update(
    {
        "wf/system/course_manager": "SM64CourseManager",
        "wf/object/bhvtumblingbridge/000": "WFTumblingBridgeController",
        "wf/object/bhvtowerplatformgroup/000": "WFTowerPlatformGroup",
        "wf/object/bhvkickableboard/000": "WFKickableBoard",
        "wf/object/bhvwfbreakablewallright/000": "SM64BreakableActor",
        "wf/object/bhvwfbreakablewallleft/000": "SM64BreakableActor",
        "wf/object/bhvtowerdoor/000": "SM64BreakableActor",
        "wf/object/bhvgiantpole/000": "SM64ClimbableActor",
        "wf/special/special_bubble_tree/000": "SM64ClimbableActor",
        "wf/system/water_volume": "SM64WaterVolume",
        "wf/warp/0B": "SM64Warp",
        "wf/warp/0C": "SM64Warp",
    }
)

WHOMP_IDS = {
    "wf/object/bhvsmallwhomp/000",
    "wf/object/bhvsmallwhomp/001",
    "wf/object/bhvwhompkingboss/000",
}
THWOMP_IDS = {"wf/object/bhvthwomp2/000", "wf/object/bhvthwomp/000"}
PIRANHA_IDS = {"wf/object/bhvpiranhaplant/{:03d}".format(index) for index in range(3)}
STAR_IDS = {
    "wf/star/mission_01_boss",
    "wf/object/bhvstar/000",
    "wf/object/bhvstar/001",
    "wf/object/bhvhiddenredcoinstar/000",
    "wf/object/bhvstar/002",
    "wf/object/bhvstar/003",
    "wf/star/100_coin",
}
RED_COIN_IDS = {"wf/macro/macro_red_coin/{:03d}".format(index) for index in range(8)}
YELLOW_COIN_IDS = {"wf/macro/macro_yellow_coin_1/{:03d}".format(index) for index in range(4)}
BLUE_COIN_IDS = {"wf/macro/macro_hidden_blue_coin/{:03d}".format(index) for index in range(4)}
FORMATION_IDS = {
    "wf/macro/macro_coin_line_horizontal/{:03d}".format(index) for index in range(4)
} | {
    "wf/macro/macro_coin_ring_horizontal/{:03d}".format(index) for index in range(3)
} | {
    "wf/macro/macro_coin_ring_horizontal_flying/000",
    "wf/macro/macro_coin_arrow/000",
}
SIGN_IDS = {"wf/macro/macro_wooden_signpost/{:03d}".format(index) for index in range(8)}
BOX_IDS = {"wf/macro/macro_breakable_box_small/{:03d}".format(index) for index in range(2)}
METAL_CAP_BOX_ID = "wf/macro/macro_box_metal_cap/000"
BUTTERFLY_IDS = {"wf/object/bhvbutterfly/{:03d}".format(index) for index in range(10)}
TRIPLET_BUTTERFLY_IDS = {
    "wf/macro/macro_butterfly_triplet_no_bombs/000",
    "wf/runtime/butterfly_triplet_no_bombs/001",
    "wf/runtime/butterfly_triplet_no_bombs/002",
}
HIDDEN_ONE_UP_IDS = {
    "wf/macro/macro_hidden_1up/000",
    "wf/macro/macro_hidden_1up_in_pole/000",
}
HIDDEN_ONE_UP_TRIGGER_IDS = {
    "wf/macro/macro_hidden_1up_trigger/000",
    "wf/macro/macro_hidden_1up_trigger/001",
    "wf/runtime/hidden_1up_pole_trigger/000",
    "wf/runtime/hidden_1up_pole_trigger/001",
}

CLASS_BY_ID.update(dict((identity, "WFWhomp") for identity in WHOMP_IDS))
CLASS_BY_ID.update(dict((identity, "WFThwomp") for identity in THWOMP_IDS))
CLASS_BY_ID.update(dict((identity, "WFPiranhaPlant") for identity in PIRANHA_IDS))
CLASS_BY_ID.update(dict((identity, "SM64PowerStar") for identity in STAR_IDS))
CLASS_BY_ID.update(dict((identity, "SM64Collectible") for identity in RED_COIN_IDS | YELLOW_COIN_IDS))
CLASS_BY_ID.update(dict((identity, "SM64TimedBlueCoin") for identity in BLUE_COIN_IDS))
CLASS_BY_ID.update(dict((identity, "SM64CoinFormation") for identity in FORMATION_IDS))
CLASS_BY_ID.update(dict((identity, "SM64SignActor") for identity in SIGN_IDS))
CLASS_BY_ID.update(dict((identity, "SM64BreakableActor") for identity in BOX_IDS))
CLASS_BY_ID[METAL_CAP_BOX_ID] = "SM64BreakableActor"
CLASS_BY_ID.update(dict((identity, "WFButterfly") for identity in BUTTERFLY_IDS | TRIPLET_BUTTERFLY_IDS))
CLASS_BY_ID.update(dict((identity, "SM64HiddenOneUp") for identity in HIDDEN_ONE_UP_IDS))
CLASS_BY_ID.update(dict((identity, "SM64HiddenOneUpTrigger") for identity in HIDDEN_ONE_UP_TRIGGER_IDS))
CLASS_BY_ID.update(
    {
        "wf/system/player_adapter": "SM64PlayerAdapter",
        "wf/object/bhvbulletbill/000": "WFBulletBill",
        "wf/object/bhvcheckerboardelevatorgroup/000": "WFCheckerboardElevatorPair",
        "wf/macro/macro_cannon_closed/000": "WFCannon",
        "wf/object/bhvbobombbuddyopenscannon/000": "WFBobombBuddy",
        "wf/object/bhvhoot/000": "WFHoot",
        "wf/macro/macro_blue_coin_switch/000": "SM64BlueCoinSwitch",
        "wf/object/bhv1up/000": "SM64Collectible",
        "wf/system/death_surface": "SM64CourseTrigger",
        "wf/system/camera/boss_fight": "SM64CameraSurfaceVolume",
        "wf/system/camera/middle_lower": "SM64CameraSurfaceVolume",
        "wf/system/camera/middle_upper": "SM64CameraSurfaceVolume",
    }
)


def stable_id(actor):
    values = [
        str(tag)[len(TAG_PREFIX) :]
        for tag in actor.get_editor_property("tags")
        if str(tag).startswith(TAG_PREFIX)
    ]
    if len(values) > 1:
        raise AssertionError("multiple stable-ID tags on " + actor.get_path_name())
    return values[0] if values else None


def close(a, b, tolerance=0.02):
    return abs(float(a) - float(b)) <= tolerance


def vector_tuple(value):
    return (float(value.x), float(value.y), float(value.z))


def assert_transform(actor, record):
    expected_location = record["unreal_transform"]["location_cm"]
    actual_location = vector_tuple(actor.get_actor_location())
    if not all(close(actual_location[index], expected_location[index]) for index in range(3)):
        raise AssertionError(
            "{} location {} != {}".format(stable_id(actor), actual_location, expected_location)
        )
    expected_rotation = record["unreal_transform"]["rotation_deg"]
    actual = actor.get_actor_rotation()
    actual_rotation = (float(actual.pitch), float(actual.yaw), float(actual.roll))
    for index in range(3):
        delta = (actual_rotation[index] - float(expected_rotation[index]) + 180.0) % 360.0 - 180.0
        if abs(delta) > 0.02:
            raise AssertionError(
                "{} rotation {} != {}".format(stable_id(actor), actual_rotation, expected_rotation)
            )


def require_component_mesh(actor, component_property, require_simple=False, require_complex_as_simple=False):
    try:
        component = actor.get_editor_property(component_property)
    except Exception:
        return
    if component is None:
        raise AssertionError("{}.{} is null".format(stable_id(actor), component_property))
    mesh = component.get_editor_property("static_mesh")
    if mesh is None:
        raise AssertionError("{}.{} has no mesh".format(stable_id(actor), component_property))
    if component_property == "collision_mesh":
        if require_simple:
            collision_count = int(
                unreal.SM64EditorReferenceLibrary.get_convex_collision_hull_count(mesh)
            )
            if collision_count < 1:
                raise AssertionError("{} collision mesh has no simple hull".format(stable_id(actor)))
        if require_complex_as_simple:
            body_setup = mesh.get_editor_property("body_setup")
            flag = body_setup.get_editor_property("collision_trace_flag") if body_setup else None
            if flag != unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE:
                raise AssertionError("{} terrain collision is not complex-as-simple".format(stable_id(actor)))


def require_component_asset(actor, component_property, asset_property="static_mesh"):
    component = actor.get_editor_property(component_property)
    if component is None or component.get_editor_property(asset_property) is None:
        raise AssertionError("{}.{} has no {}".format(
            stable_id(actor), component_property, asset_property
        ))


def verify_sparx():
    generated_class = unreal.EditorAssetLibrary.load_blueprint_class(SPARX_BLUEPRINT)
    if generated_class is None:
        raise AssertionError("missing Sparx generated class")
    template_path = generated_class.get_path_name() + ":Capsule_GEN_VARIABLE"
    template = unreal.find_object(None, template_path) or unreal.load_object(None, template_path)
    if template is None:
        raise AssertionError("missing Sparx Capsule template")
    relative = vector_tuple(template.get_editor_property("relative_location"))
    if template.get_editor_property("absolute_location") or any(abs(v) > 0.001 for v in relative):
        raise AssertionError("Sparx Capsule template is detached: {} {}".format(
            template.get_editor_property("absolute_location"), relative
        ))
    instances = []
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_class().get_name() != "Sparx_BP_C":
            continue
        capsules = [
            component for component in actor.get_components_by_class(unreal.SceneComponent)
            if component.get_name() == "Capsule"
        ]
        if len(capsules) != 1:
            raise AssertionError("Sparx instance has {} Capsules".format(len(capsules)))
        capsule = capsules[0]
        relative = vector_tuple(capsule.get_editor_property("relative_location"))
        if capsule.get_editor_property("absolute_location") or any(abs(v) > 0.001 for v in relative):
            raise AssertionError("Level 5 Sparx Capsule is detached")
        instances.append(actor.get_path_name())
    if len(instances) != 1:
        raise AssertionError("expected exactly one Level 5 Sparx, found {}".format(len(instances)))
    return {"template": template.get_path_name(), "instances": instances}


def validate():
    if not unreal.EditorLevelLibrary.load_level(LEVEL):
        raise RuntimeError("unable to load " + LEVEL)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    by_id = {}
    for actor in actors:
        identity = stable_id(actor)
        if not identity:
            continue
        if identity in by_id:
            raise AssertionError("duplicate stable ID " + identity)
        by_id[identity] = actor
    if set(by_id) != set(CLASS_BY_ID):
        raise AssertionError("generated ID mismatch: missing={} extra={}".format(
            sorted(set(CLASS_BY_ID) - set(by_id)), sorted(set(by_id) - set(CLASS_BY_ID))
        ))

    with open(PLACEMENTS_FILE, "r") as stream:
        placement_data = json.load(stream)
    placements = dict((item["stable_id"], item) for item in placement_data["placements"])
    for identity, expected_class in sorted(CLASS_BY_ID.items()):
        actor = by_id[identity]
        if actor.get_class().get_name() != expected_class:
            raise AssertionError("{} class {} != {}".format(
                identity, actor.get_class().get_name(), expected_class
            ))
        if identity in placements:
            assert_transform(actor, placements[identity])
            expected_mask = sum(1 << (int(act) - 1) for act in placements[identity].get("acts", range(1, 7)))
            try:
                actual_mask = int(actor.get_editor_property("act_mask"))
            except Exception:
                actual_mask = expected_mask
            if actual_mask != expected_mask:
                raise AssertionError("{} act mask {} != {}".format(identity, actual_mask, expected_mask))

    manager = by_id["wf/system/course_manager"]
    if not manager.get_editor_property("use_spyro_save_lineage"):
        raise AssertionError("course manager does not prefer the Spyro64 save lineage")
    if str(manager.get_editor_property("spyro_progress_map_property")) != "SM64_CourseProgress":
        raise AssertionError("course manager is not targeting 64_SaveData_S1.SM64_CourseProgress")
    if not manager.get_editor_property("use_standalone_save_fallback"):
        raise AssertionError("course manager lost its resilient standalone save fallback")

    for identity in STATIC_IDS:
        actor = by_id[identity]
        if identity.startswith("wf/collision/"):
            require_component_mesh(actor, "collision_mesh", require_complex_as_simple=True)
        elif identity not in ("wf/render/water",):
            require_component_mesh(actor, "visual_mesh")
    for identity in MOVER_IDS:
        require_component_mesh(by_id[identity], "visual_mesh")
        require_component_mesh(by_id[identity], "collision_mesh", require_simple=True)
        if not close(by_id[identity].get_editor_property("simulation_hz"), 30.0):
            raise AssertionError(identity + " is not 30 Hz")
    for identity in (
        "wf/object/bhvtower/000",
        "wf/object/bhvbulletbillcannon/000",
        "wf/object/bhvwfbreakablewallright/000",
        "wf/object/bhvwfbreakablewallleft/000",
        "wf/object/bhvtowerdoor/000",
    ):
        require_component_mesh(by_id[identity], "collision_mesh", require_simple=True)

    for identity in WHOMP_IDS:
        require_component_asset(by_id[identity], "character_mesh", "skeletal_mesh")
        require_component_asset(by_id[identity], "exact_collision_mesh")
        if by_id[identity].get_editor_property("walk_animation") is None or by_id[identity].get_editor_property("prepare_jump_animation") is None:
            raise AssertionError(identity + " is missing exact Whomp animations")
        if by_id[identity].get_editor_property("drop_coin_class") is None:
            raise AssertionError(identity + " has no yellow-coin reward class")
    for identity in THWOMP_IDS:
        require_component_asset(by_id[identity], "mesh")
        require_component_asset(by_id[identity], "exact_collision_mesh")
    for identity in PIRANHA_IDS:
        require_component_asset(by_id[identity], "character_mesh", "skeletal_mesh")
        if len(by_id[identity].get_editor_property("animations")) != 10:
            raise AssertionError(identity + " does not have the ten-entry piranha animation table")
        if by_id[identity].get_editor_property("blue_coin_class") is None:
            raise AssertionError(identity + " has no blue-coin reward class")
    selected_triplet_rewards = 0
    for identity in BUTTERFLY_IDS | TRIPLET_BUTTERFLY_IDS:
        require_component_asset(by_id[identity], "character_mesh", "skeletal_mesh")
        if by_id[identity].get_editor_property("flight_animation") is None or by_id[identity].get_editor_property("rest_animation") is None:
            raise AssertionError(identity + " is missing exact butterfly animations")
        if identity in TRIPLET_BUTTERFLY_IDS:
            if not by_id[identity].get_editor_property("triplet_butterfly"):
                raise AssertionError(identity + " is not configured as a triplet butterfly")
            if by_id[identity].get_editor_property("selected_for_one_up"):
                selected_triplet_rewards += 1
                if by_id[identity].get_editor_property("one_up_class") is None:
                    raise AssertionError(identity + " cannot spawn its guaranteed 1UP")
    if selected_triplet_rewards != 1:
        raise AssertionError("NO_BOMBS butterfly triplet must select exactly one 1UP reward")
    require_component_asset(by_id["wf/object/bhvbulletbill/000"], "mesh")
    require_component_asset(by_id["wf/object/bhvbobombbuddyopenscannon/000"], "character_mesh", "skeletal_mesh")
    require_component_asset(by_id["wf/object/bhvhoot/000"], "character_mesh", "skeletal_mesh")
    if by_id["wf/object/bhvbobombbuddyopenscannon/000"].get_editor_property("idle_animation") is None:
        raise AssertionError("Bob-omb Buddy idle animation is missing")
    hoot_actor = by_id["wf/object/bhvhoot/000"]
    if hoot_actor.get_editor_property("free_flight_animation") is None or hoot_actor.get_editor_property("carry_animation") is None:
        raise AssertionError("Hoot animations are missing")
    checker = by_id["wf/object/bhvcheckerboardelevatorgroup/000"]
    require_component_asset(checker, "render_mesh_a")
    require_component_asset(checker, "collision_mesh_a")
    cannon = by_id["wf/macro/macro_cannon_closed/000"]
    for component_name in ("lid_mesh", "lid_collision_mesh", "base_mesh", "barrel_mesh"):
        require_component_asset(cannon, component_name)
    for identity in STAR_IDS:
        require_component_asset(by_id[identity], "mesh")
    for identity in RED_COIN_IDS | YELLOW_COIN_IDS | {"wf/object/bhv1up/000"} | HIDDEN_ONE_UP_IDS:
        require_component_asset(by_id[identity], "mesh")
    if not by_id["wf/object/bhv1up/000"].get_editor_property("one_up"):
        raise AssertionError("generic act-gated 1UP does not award a life")
    for identity in HIDDEN_ONE_UP_IDS:
        if not by_id[identity].get_editor_property("one_up"):
            raise AssertionError(identity + " does not award a life")
        if int(by_id[identity].get_editor_property("required_trigger_count")) != 2:
            raise AssertionError(identity + " does not require two canonical triggers")
    if not by_id["wf/macro/macro_hidden_1up_in_pole/000"].get_editor_property("home_toward_player"):
        raise AssertionError("pole 1UP does not home toward the player")
    death = by_id["wf/system/death_surface"]
    if death.get_editor_property("trigger_type") != unreal.SM64CourseTriggerType.DEATH:
        raise AssertionError("decomp death surface is not a death trigger")
    if vector_tuple(death.get_editor_property("box_extent")) != (8191.5, 8191.5, 32.0):
        raise AssertionError("decomp death trigger bounds changed")
    if not death.get_editor_property("auto_retry_act_on_death"):
        raise AssertionError("fatal falls do not retry the selected act")
    camera_expectations = {
        "wf/system/camera/boss_fight": unreal.SM64CameraSurfaceMode.BOSS_CAMERA,
        "wf/system/camera/middle_lower": unreal.SM64CameraSurfaceMode.CAMERA_MIDDLE,
        "wf/system/camera/middle_upper": unreal.SM64CameraSurfaceMode.CAMERA_MIDDLE,
    }
    for identity, mode in camera_expectations.items():
        if by_id[identity].get_editor_property("camera_mode") != mode:
            raise AssertionError(identity + " has the wrong semantic camera mode")
    for identity in BLUE_COIN_IDS:
        require_component_asset(by_id[identity], "mesh")
    require_component_asset(by_id["wf/macro/macro_blue_coin_switch/000"], "switch_mesh")
    require_component_asset(by_id["wf/macro/macro_blue_coin_switch/000"], "exact_collision_mesh")
    for identity in SIGN_IDS:
        require_component_asset(by_id[identity], "mesh")
        require_component_asset(by_id[identity], "exact_collision_mesh")
    for identity in BOX_IDS:
        require_component_asset(by_id[identity], "mesh")
        require_component_asset(by_id[identity], "collision_mesh")
        if int(by_id[identity].get_editor_property("drop_coin_count")) != 3 or by_id[identity].get_editor_property("drop_coin_class") is None:
            raise AssertionError(identity + " does not drop three yellow coins")
    metal_cap_box = by_id[METAL_CAP_BOX_ID]
    require_component_asset(metal_cap_box, "mesh")
    require_component_asset(metal_cap_box, "collision_mesh")
    if metal_cap_box.get_editor_property("required_attack") != unreal.SM64AttackType.CHARGE:
        raise AssertionError("metal cap box is not wired to Spyro's physical charge interaction")
    expected_star_indices = {
        "wf/star/mission_01_boss": 0,
        "wf/object/bhvstar/000": 1,
        "wf/object/bhvstar/001": 2,
        "wf/object/bhvhiddenredcoinstar/000": 3,
        "wf/object/bhvstar/002": 4,
        "wf/object/bhvstar/003": 5,
        "wf/star/100_coin": 6,
    }
    for identity, expected_index in expected_star_indices.items():
        if int(by_id[identity].get_editor_property("star_index")) != expected_index:
            raise AssertionError(identity + " has the wrong star index")
    adapter = by_id["wf/system/player_adapter"]
    spyro = next(actor for actor in actors if actor.get_class().get_name() == "BP_Spyro_C")
    if adapter.get_editor_property("spyro_actor") != spyro:
        raise AssertionError("SM64 player adapter is not bound to the preserved BP_Spyro")
    hundred_coin_star = by_id["wf/star/100_coin"]
    hundred_coin_flag = None
    no_exit_flag = None
    for candidate in ("100_coin_star", "b100_coin_star"):
        try:
            hundred_coin_flag = bool(hundred_coin_star.get_editor_property(candidate))
            break
        except Exception:
            pass
    for candidate in ("no_exit", "bno_exit"):
        try:
            no_exit_flag = bool(hundred_coin_star.get_editor_property(candidate))
            break
        except Exception:
            pass
    if not hundred_coin_flag or not no_exit_flag:
        raise AssertionError("100-coin star is not configured as a non-ejecting bonus star")
    if by_id["wf/object/bhvbobombbuddyopenscannon/000"].get_editor_property("linked_cannon") != by_id["wf/macro/macro_cannon_closed/000"]:
        raise AssertionError("Bob-omb Buddy is not linked to the WF cannon")

    shell = {}
    for class_name, expected_path in SHELL_PATHS.items():
        matches = [actor for actor in actors if actor.get_class().get_name() == class_name]
        if len(matches) != 1 or matches[0].get_path_name() != expected_path:
            raise AssertionError("shell identity mismatch for {}: {}".format(
                class_name, [actor.get_path_name() for actor in matches]
            ))
        shell[class_name] = expected_path
    kill_plane = next(actor for actor in actors if actor.get_class().get_name() == "BP_KillPlane_C")
    legacy_pawn_surfaces = {
        component.get_name(): component.get_collision_enabled()
        for component in kill_plane.get_components_by_class(unreal.PrimitiveComponent)
        if component.get_name() in ("Box", "visualizer")
    }
    if set(legacy_pawn_surfaces) != {"Box", "visualizer"} or any(
        value != unreal.CollisionEnabled.NO_COLLISION for value in legacy_pawn_surfaces.values()
    ):
        raise AssertionError("legacy kill-plane pawn surfaces were not safely superseded")
    if not all(close(a, b) for a, b in zip(vector_tuple(spyro.get_actor_location()), (2600, 5120, 304))):
        raise AssertionError("Spyro course entrance is incorrect")

    decorations = []
    for actor in actors:
        if "Dragon_Candle_BP_C" in actor.get_class().get_name():
            decorations.append(actor.get_path_name())
        if actor.get_name() in ("Plane", "Level_spawn_helper"):
            decorations.append(actor.get_path_name())
        if actor.get_actor_label() in ("Plane", "Level_spawn_helper"):
            decorations.append(actor.get_path_name())
    if decorations:
        raise AssertionError("template decorations remain: " + repr(decorations))

    settings = unreal.EditorLevelLibrary.get_editor_world().get_world_settings()
    if not settings.get_editor_property("force_no_precomputed_lighting"):
        raise AssertionError("Force No Precomputed Lighting is disabled")

    course = unreal.load_asset(COURSE_ASSET)
    if course is None:
        raise AssertionError("course definition is missing")
    missions = course.get_editor_property("missions")
    course_placements = course.get_editor_property("placements")
    if len(missions) != 6 or len(course_placements) != 127:
        raise AssertionError("course data counts are wrong")
    names = [str(mission.get_editor_property("display_name")) for mission in missions]
    expected_names = [
        "Chip Off Whomp's Block",
        "To the Top of the Fortress",
        "Shoot into the Wild Blue",
        "Red Coins on the Floating Isle",
        "Fall onto the Caged Island",
        "Blast Away the Wall",
    ]
    if names != expected_names:
        raise AssertionError("mission names do not match decomp truth: " + repr(names))
    if str(course.get_editor_property("return_level")) != "/Game/Spyro64/Levels/00_Homeworld":
        raise AssertionError("course return destination is wrong")

    sparx = verify_sparx()
    try:
        unreal.SystemLibrary.execute_console_command(
            unreal.EditorLevelLibrary.get_editor_world(), "MAP CHECK NOTIFYOFF"
        )
    except Exception as error:
        unreal.log_warning("SM64_WF_MAPCHECK_COMMAND_UNAVAILABLE=" + str(error))

    report = {
        "status": "PASS",
        "level_actor_count": len(actors),
        "generated_actor_count": len(by_id),
        "course_placement_count": len(course_placements),
        "mission_count": len(missions),
        "shell": shell,
        "sparx": sparx,
    }
    unreal.log_warning("SM64_WF_UNREAL_VALIDATION=" + json.dumps(report, sort_keys=True))


validate()
