"""Idempotently repair the Spyro64 foundation used by Whomp's Fortress.

The script is intentionally independent of SM64Runtime so it can be run before
or after that plugin is installed.  It duplicates the surviving Level 5 shell
through Unreal, preserves its functional actor instances, removes only known
template decoration, fixes the Sparx component template, and repairs references
that are exposed as ordinary Blueprint properties.

Run with ``--snapshot-only`` for a mutation-free report.  Every property write
is preceded by a read and is recorded in the final JSON log entry.
"""

from __future__ import print_function

import argparse
import json
import sys
import traceback

import unreal


TEMPLATE_MAP = "/Game/Spyro64/Levels/05_Level5_PreWhomps"
LEVEL5_MAP = "/Game/Spyro64/Levels/05_Level5"
HOMEWORLD_MAP = "/Game/Spyro64/Levels/00_Homeworld"
TITLE_MAP = "/Game/Spyro64/64_TitleScreen"
ADVENTURE_INFO = "/Game/Spyro64/AdventureInfo_64"
SAVE_BASE = "/Game/Spyro64/64_SaveData"
SAVE_SLOT = "/Game/Spyro64/64_SaveData_S1"
SPARX_BLUEPRINT = "/Game/SpyroContent/Global_Assets/Global_Characters/Sparx/Sparx_BP"

LEVEL_MAPS = [
    "/Game/Spyro64/Levels/{:02d}_Level{}".format(index, index)
    for index in range(1, 8)
]

ADVENTURE_PROPERTY_NAMES = (
    "current_adventure_info",
    "Current_AdventureInfo",
    "current adventure info",
)
WORLD_REFERENCE_PROPERTY_NAMES = (
    "my_level",
    "My_Level",
    "leads_to_level",
    "Leads_to_Level",
    "destination_level",
    "Destination_Level",
    "homeworld",
    "Homeworld",
    "current_level",
    "Current_Level",
)

SHELL_RULES = {
    "spyro": ("BP_Spyro_C", "BP_S3_Spyro"),
    "skybox": ("BP_Skybox_C", "BP_Skybox"),
    "return_portal": ("BP_Portal_ReturnHome_C", "BP_Portal_ReturnHome"),
    "kill_plane": ("BP_KillPlane_C", "BP_KillPlane"),
    "total_gems": ("BP_Total_Gems_C", "BP_Total_Gems"),
}

SHELL_LOCATIONS = {
    # The source start is (2600, 256, 5120).  UE receives (x, z, y), and the
    # Spyro capsule origin is 48 cm above the course floor.
    "spyro": (2600.0, 5120.0, 304.0),
    "skybox": (0.0, 0.0, 0.0),
    "return_portal": (2600.0, 5600.0, 256.0),
    "kill_plane": (0.0, 0.0, -3071.0),
    "total_gems": (0.0, 0.0, 0.0),
}


class MutationLog(object):
    def __init__(self):
        self.entries = []
        self.warnings = []

    def add(self, action, target, before=None, after=None, detail=None):
        record = {"action": action, "target": target}
        if before is not None:
            record["before"] = before
        if after is not None:
            record["after"] = after
        if detail is not None:
            record["detail"] = detail
        self.entries.append(record)
        unreal.log("SM64 foundation: {} {}".format(action, target))

    def warn(self, message):
        self.warnings.append(message)
        unreal.log_warning("SM64 foundation: " + message)


def object_path(value):
    try:
        return value.get_path_name()
    except Exception:
        return None


def class_name(value):
    try:
        return value.get_class().get_name()
    except Exception:
        return ""


def vector_record(value):
    return [float(value.x), float(value.y), float(value.z)]


def rotator_record(value):
    return [float(value.pitch), float(value.yaw), float(value.roll)]


def actor_record(actor):
    transform = actor.get_actor_transform()
    record = {
        "name": actor.get_name(),
        "label": actor.get_actor_label(),
        "class": class_name(actor),
        "location": vector_record(transform.translation),
        "rotation": rotator_record(transform.rotation.rotator()),
        "scale": vector_record(transform.scale3d),
        "tags": [str(tag) for tag in actor.tags],
    }
    get_guid = getattr(actor, "get_actor_guid", None)
    if callable(get_guid):
        try:
            record["actor_guid"] = str(get_guid())
        except Exception:
            pass
    return record


def load_object_asset(asset_path):
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if asset:
        return asset
    name = asset_path.rsplit("/", 1)[-1]
    return unreal.load_object(None, "{}.{}".format(asset_path, name))


def load_map(map_path):
    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    if not world:
        raise RuntimeError("Unable to load map: {}".format(map_path))
    return world


def resolve_template_map(log=None, allow_rename=True):
    """Resolve the loose PreWhomps package without copying its .umap file.

    UE's asset registry can omit a loose map copied into Content even when the
    package's internal identity is valid.  A synchronous file/path scan followed
    by the map loader resolves that state.  If a differently named registered
    asset is found, it is renamed through AssetTools rather than at filesystem
    level so package metadata and redirectors remain valid.
    """
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    try:
        registry.scan_paths_synchronous(
            ["/Game/Spyro64/Levels"], force_rescan=True
        )
    except TypeError:
        registry.scan_paths_synchronous(["/Game/Spyro64/Levels"], True)
    physical_file = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_content_dir()
        + "Spyro64/Levels/05_Level5_PreWhomps.umap"
    )
    scan_files = getattr(registry, "scan_files_synchronous", None)
    if callable(scan_files):
        try:
            scan_files([physical_file], True)
        except TypeError:
            try:
                scan_files([physical_file])
            except Exception:
                pass
        except Exception:
            pass

    try:
        world = unreal.EditorLoadingAndSavingUtils.load_map(TEMPLATE_MAP)
    except Exception:
        world = None
    if world:
        if log:
            log.add(
                "resolve_template_map",
                TEMPLATE_MAP,
                after={"object": object_path(world), "physical_file": physical_file},
            )
        return world

    if not allow_rename:
        raise RuntimeError(
            "Preflight could not resolve {} after a synchronous registry/file "
            "scan. Apply mode can rename a discovered mismatched package through "
            "Unreal, but preflight is intentionally mutation-free.".format(TEMPLATE_MAP)
        )

    # Deterministic registered-asset fallback for a package whose filename was
    # renamed without Unreal.  No raw map file is copied or rewritten here.
    candidates = []
    try:
        asset_data = registry.get_assets_by_path(
            "/Game/Spyro64/Levels", recursive=False
        )
    except TypeError:
        asset_data = registry.get_assets_by_path("/Game/Spyro64/Levels", False)
    for data in asset_data:
        asset_name = str(data.asset_name)
        package_name = str(data.package_name)
        if "05_Level5_PreWhomps" in asset_name or "05_Level5_PreWhomps" in package_name:
            asset = data.get_asset()
            if asset:
                candidates.append(asset)
    for asset in candidates:
        source_path = object_path(asset).split(".", 1)[0]
        if source_path != TEMPLATE_MAP:
            renamed = unreal.EditorAssetLibrary.rename_asset(source_path, TEMPLATE_MAP)
            if not renamed:
                continue
            if log:
                log.add("rename_template_through_unreal", source_path, after=TEMPLATE_MAP)
        world = unreal.EditorLoadingAndSavingUtils.load_map(TEMPLATE_MAP)
        if world:
            return world
    raise RuntimeError(
        "PreWhomps exists at {!r} but Unreal could not resolve its package as {}. "
        "The script intentionally refuses filesystem copying; open/resave or rename "
        "the map through Unreal once, then rerun.".format(physical_file, TEMPLATE_MAP)
    )


def all_actors(world):
    return [
        actor
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
        if actor
    ]


def level_script_actors(world):
    try:
        return [
            actor
            for actor in unreal.GameplayStatics.get_all_actors_of_class(
                world, unreal.LevelScriptActor
            )
            if actor
        ]
    except Exception:
        return []


def snapshot_world(map_path, load=False):
    world = load_map(map_path) if load else load_object_asset(map_path)
    if not world:
        return {"map": map_path, "exists": False}
    actors = all_actors(world)
    settings = unreal.GameplayStatics.get_actor_of_class(world, unreal.WorldSettings)
    result = {
        "map": map_path,
        "exists": True,
        "object": object_path(world),
        "actors": [actor_record(actor) for actor in actors],
        "force_no_precomputed_lighting": None,
        "references": [],
    }
    if settings:
        try:
            result["force_no_precomputed_lighting"] = bool(
                settings.get_editor_property("force_no_precomputed_lighting")
            )
        except Exception:
            pass
    for actor in actors + level_script_actors(world):
        for property_name in ADVENTURE_PROPERTY_NAMES + WORLD_REFERENCE_PROPERTY_NAMES:
            try:
                value = actor.get_editor_property(property_name)
            except Exception:
                continue
            path = object_path(value)
            if path is None and value is not None:
                path = str(value)
            result["references"].append(
                {
                    "actor": object_path(actor),
                    "property": property_name,
                    "value": path,
                }
            )
    return result


def is_playing_in_editor():
    for owner in (unreal.EditorLevelLibrary, unreal.EditorLoadingAndSavingUtils):
        for name in ("is_playing", "is_playing_in_editor"):
            method = getattr(owner, name, None)
            if callable(method):
                try:
                    if method():
                        return True
                except Exception:
                    pass
    return False


def ensure_level5_map(log):
    existing = load_object_asset(LEVEL5_MAP)
    if existing:
        log.add("reuse_map", LEVEL5_MAP, after=object_path(existing))
        return load_map(LEVEL5_MAP)

    source_world = resolve_template_map(log)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    duplicated = None
    try:
        duplicated = tools.duplicate_asset(
            "05_Level5", "/Game/Spyro64/Levels", source_world
        )
    except Exception as error:
        log.warn("AssetTools duplicate failed: {}".format(error))
    if not duplicated:
        try:
            duplicated = unreal.EditorAssetLibrary.duplicate_asset(
                TEMPLATE_MAP, LEVEL5_MAP
            )
        except Exception as error:
            log.warn("EditorAssetLibrary duplicate failed: {}".format(error))
    if not duplicated:
        # Save Map As is Unreal's map-aware duplication path and preserves the
        # persistent-level actor GUIDs and Level Blueprint references.
        if not unreal.EditorLoadingAndSavingUtils.save_map(source_world, LEVEL5_MAP):
            raise RuntimeError(
                "All Unreal map duplication methods failed for {}".format(LEVEL5_MAP)
            )
        duplicated = load_object_asset(LEVEL5_MAP)
    log.add(
        "duplicate_map",
        LEVEL5_MAP,
        before=TEMPLATE_MAP,
        after=object_path(duplicated),
    )
    world = load_map(LEVEL5_MAP)
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL5_MAP):
        raise RuntimeError("Duplicated map could not be saved: {}".format(LEVEL5_MAP))
    return world


def find_shell_actor(actors, class_token, label_token):
    class_matches = [actor for actor in actors if class_token in class_name(actor)]
    if class_matches:
        return class_matches
    lowered = label_token.lower()
    return [
        actor
        for actor in actors
        if lowered in actor.get_actor_label().lower() or lowered in actor.get_name().lower()
    ]


def shell_identity_snapshot(world):
    actors = all_actors(world)
    result = {}
    for role, (class_token, label_token) in SHELL_RULES.items():
        matches = find_shell_actor(actors, class_token, label_token)
        result[role] = [actor_record(actor) for actor in matches]
    return result


def assert_exact_shell(world, expected_identity=None):
    snapshot = shell_identity_snapshot(world)
    failures = []
    for role in sorted(SHELL_RULES):
        matches = snapshot[role]
        if len(matches) != 1:
            failures.append("{} count was {} (expected 1)".format(role, len(matches)))
            continue
        if expected_identity and role in expected_identity:
            expected = expected_identity[role]
            if len(expected) != 1:
                failures.append(
                    "source {} count was {} (expected 1)".format(role, len(expected))
                )
                continue
            # Duplicate/Save-As must preserve the persistent actor name and
            # generated class.  GUID is compared when this UE build exposes it.
            for key in ("name", "class", "actor_guid"):
                if key in expected[0] and matches[0].get(key) != expected[0].get(key):
                    failures.append(
                        "{} {} changed from {!r} to {!r}".format(
                            role, key, expected[0].get(key), matches[0].get(key)
                        )
                    )
    if failures:
        raise RuntimeError("Shell identity assertion failed: " + "; ".join(failures))
    return snapshot


def repair_shell(world, log):
    actors = all_actors(world)
    shell = {}
    for role, (class_token, label_token) in SHELL_RULES.items():
        matches = find_shell_actor(actors, class_token, label_token)
        if not matches:
            raise RuntimeError("Required shell actor is missing: {}".format(role))
        if len(matches) > 1:
            log.warn(
                "Multiple shell actors match {}: {}".format(
                    role, [object_path(actor) for actor in matches]
                )
            )
        shell[role] = matches[0]

    for role, actor in shell.items():
        before = actor_record(actor)
        coords = SHELL_LOCATIONS[role]
        actor.set_actor_location(unreal.Vector(*coords), False, True)
        if role == "spyro":
            actor.set_actor_rotation(
                unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0), True
            )
        after = actor_record(actor)
        if before != after:
            log.add("move_shell_actor", object_path(actor), before=before, after=after)

    decoration = []
    for actor in actors:
        name = actor.get_name()
        label = actor.get_actor_label()
        actor_class = class_name(actor)
        if "Dragon_Candle_BP_C" in actor_class:
            decoration.append(actor)
        elif label == "Plane" or name == "Plane":
            decoration.append(actor)
        elif label == "Level_spawn_helper" or name == "Level_spawn_helper":
            decoration.append(actor)
    for actor in decoration:
        record = actor_record(actor)
        if not unreal.EditorLevelLibrary.destroy_actor(actor):
            log.warn("Could not remove decoration actor {}".format(record["name"]))
        else:
            log.add("remove_template_decoration", record["name"], before=record)

    # Gold_Text actors are child-actor prompts owned by the return portal.  They
    # are intentionally not included in the direct-decoration deletion list.
    gold_text = [
        actor_record(actor)
        for actor in all_actors(world)
        if "Gold_Text" in actor.get_name() or "Gold_Text" in actor.get_actor_label()
    ]
    if gold_text:
        log.add("preserve_portal_child_prompts", LEVEL5_MAP, after=gold_text)

    settings = unreal.GameplayStatics.get_actor_of_class(world, unreal.WorldSettings)
    if not settings:
        raise RuntimeError("Level 5 has no WorldSettings actor")
    before = bool(settings.get_editor_property("force_no_precomputed_lighting"))
    settings.set_editor_property("force_no_precomputed_lighting", True)
    log.add(
        "set_force_no_precomputed_lighting",
        object_path(settings),
        before=before,
        after=True,
    )

    if not unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL5_MAP):
        raise RuntimeError("Failed to save repaired Level 5 shell")
    return shell


def find_sparx_capsule_template(generated_class):
    candidates = (
        "{}:Capsule_GEN_VARIABLE".format(generated_class.get_path_name()),
        "{}.Default__{}:Capsule_GEN_VARIABLE".format(
            generated_class.get_outer().get_path_name(), generated_class.get_name()
        ),
    )
    for path in candidates:
        component = unreal.find_object(None, path)
        if not component:
            component = unreal.load_object(None, path)
        if component:
            return component
    default_object = unreal.get_default_object(generated_class)
    try:
        for component in default_object.get_components_by_class(unreal.SceneComponent):
            if component.get_name() in ("Capsule", "Capsule_GEN_VARIABLE"):
                return component
    except Exception:
        pass
    return None


def fix_sparx_template(log):
    blueprint = unreal.EditorAssetLibrary.load_asset(SPARX_BLUEPRINT)
    generated_class = unreal.EditorAssetLibrary.load_blueprint_class(SPARX_BLUEPRINT)
    if not blueprint or not generated_class:
        raise RuntimeError("Sparx Blueprint/class could not be loaded")
    component = find_sparx_capsule_template(generated_class)
    if not component:
        raise RuntimeError("Sparx Capsule component template could not be reflected")
    before = {
        "path": object_path(component),
        "absolute_location": bool(component.get_editor_property("absolute_location")),
        "relative_location": vector_record(
            component.get_editor_property("relative_location")
        ),
    }
    component.set_editor_property("absolute_location", False)
    component.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, 0.0))

    compiler = getattr(unreal, "MMAEditorAnimationLibrary", None)
    compile_method = getattr(compiler, "compile_blueprint", None) if compiler else None
    if callable(compile_method):
        compile_method(blueprint)
    else:
        log.warn(
            "MMAEditorAnimationLibrary.compile_blueprint is unavailable; saving the "
            "component-template mutation without an explicit compile"
        )

    # Compilation can reconstruct the SCS templates.  Resolve and enforce the
    # setting a second time before saving.
    component = find_sparx_capsule_template(generated_class)
    if not component:
        raise RuntimeError("Sparx Capsule disappeared after Blueprint compile")
    component.set_editor_property("absolute_location", False)
    component.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, 0.0))
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False):
        raise RuntimeError("Sparx Blueprint failed to save")
    after = {
        "path": object_path(component),
        "absolute_location": bool(component.get_editor_property("absolute_location")),
        "relative_location": vector_record(
            component.get_editor_property("relative_location")
        ),
    }
    if after["absolute_location"] or after["relative_location"] != [0.0, 0.0, 0.0]:
        raise RuntimeError("Sparx Capsule template verification failed: {}".format(after))
    log.add("fix_sparx_capsule_template", object_path(component), before, after)


def sparx_capsule_record(actor):
    capsules = []
    try:
        components = actor.get_components_by_class(unreal.SceneComponent)
    except Exception:
        components = []
    for component in components:
        if "capsule" not in component.get_name().lower():
            continue
        capsules.append(
            {
                "path": object_path(component),
                "absolute_location": bool(
                    component.get_editor_property("absolute_location")
                ),
                "relative_location": vector_record(
                    component.get_editor_property("relative_location")
                ),
            }
        )
    return capsules


def verify_sparx_in_fresh_map(map_path):
    world = load_map(map_path)
    sparx_actors = [
        actor for actor in all_actors(world) if "Sparx_BP_C" in class_name(actor)
    ]
    if not sparx_actors:
        raise RuntimeError("Fresh map {} contains no Sparx_BP_C instance".format(map_path))
    records = []
    for actor in sparx_actors:
        capsules = sparx_capsule_record(actor)
        if len(capsules) != 1:
            raise RuntimeError(
                "{} in {} has {} Capsule components".format(
                    object_path(actor), map_path, len(capsules)
                )
            )
        capsule = capsules[0]
        if capsule["absolute_location"] or capsule["relative_location"] != [0.0, 0.0, 0.0]:
            raise RuntimeError(
                "Fresh Sparx Capsule verification failed in {}: {}".format(
                    map_path, capsule
                )
            )
        records.append({"actor": object_path(actor), "capsule": capsule})
    return records


def verify_sparx_template_and_instances():
    generated_class = unreal.EditorAssetLibrary.load_blueprint_class(SPARX_BLUEPRINT)
    component = find_sparx_capsule_template(generated_class)
    if not component:
        raise RuntimeError("Sparx Capsule template is missing during verification")
    template = {
        "path": object_path(component),
        "absolute_location": bool(component.get_editor_property("absolute_location")),
        "relative_location": vector_record(
            component.get_editor_property("relative_location")
        ),
    }
    if template["absolute_location"] or template["relative_location"] != [0.0, 0.0, 0.0]:
        raise RuntimeError("Sparx template verification failed: {}".format(template))
    return {
        "template": template,
        "level5_instances": verify_sparx_in_fresh_map(LEVEL5_MAP),
        "existing_level_instances": verify_sparx_in_fresh_map(LEVEL_MAPS[0]),
    }


def value_record(value):
    path = object_path(value)
    if path:
        return path
    if value is None:
        return None
    return str(value)


def typed_replacement(old_value, asset_path, prefer_class=False):
    """Resolve one replacement with the same reflected type as the old value.

    UE4.27 can access-violate inside BlueprintGraph when set_editor_property is
    handed a series of incompatible UObject candidates.  Never probe property
    types by writing: inspect the current value and perform exactly one write.
    """
    old_class = ""
    try:
        old_class = old_value.get_class().get_path_name()
    except Exception:
        pass
    if old_class == "/Script/Engine.World":
        # Loading a map through EditorLoadingAndSavingUtils here would unload
        # the owner map and invalidate every actor in the active iteration.
        object_name = asset_path.rsplit("/", 1)[-1]
        return unreal.load_object(None, asset_path + "." + object_name)
    if old_class == "/Script/Engine.BlueprintGeneratedClass" or prefer_class:
        return unreal.EditorAssetLibrary.load_blueprint_class(asset_path)
    if old_class == "/Script/Engine.Blueprint":
        return unreal.EditorAssetLibrary.load_asset(asset_path)
    if isinstance(old_value, str):
        return asset_path
    if type(old_value).__name__ == "Name":
        return unreal.Name(asset_path)
    if type(old_value).__name__ == "SoftObjectPath":
        return unreal.SoftObjectPath(asset_path)
    if old_value is None:
        return None
    return load_object_asset(asset_path)


def set_exposed_reference(
    owner,
    property_names,
    target_path,
    log,
    prefer_class=False,
    required_old_fragment=None,
):
    for property_name in property_names:
        try:
            old_value = owner.get_editor_property(property_name)
        except Exception:
            continue
        old_record = value_record(old_value)
        if required_old_fragment and required_old_fragment not in str(old_record):
            continue
        if target_path in str(old_record):
            return True
        replacement = typed_replacement(old_value, target_path, prefer_class)
        if replacement is None:
            log.warn(
                "Property {}.{} is null, so its UObject type cannot be safely "
                "inferred in UE4.27".format(object_path(owner), property_name)
            )
            continue
        try:
            owner.set_editor_property(property_name, replacement)
            new_value = owner.get_editor_property(property_name)
        except Exception as error:
            log.warn(
                "Type-matched property write failed for {}.{}: {}".format(
                    object_path(owner), property_name, error
                )
            )
            continue
        new_record = value_record(new_value)
        if target_path not in str(new_record):
            log.warn(
                "Type-matched property write read back {!r} for {}.{}".format(
                    new_record, object_path(owner), property_name
                )
            )
            continue
        log.add(
            "repair_reference",
            "{}.{}".format(object_path(owner), property_name),
            before=old_record,
            after=new_record,
        )
        return True
    return False


def repair_adventure_reference(world, log):
    owners = level_script_actors(world)
    if not owners:
        log.warn("No reflected LevelScriptActor in {}".format(object_path(world)))
        return False
    changed = False
    for owner in owners:
        changed = (
            set_exposed_reference(
                owner,
                ADVENTURE_PROPERTY_NAMES,
                ADVENTURE_INFO,
                log,
                prefer_class=True,
            )
            or changed
        )
    return changed


def repair_return_portal_reference(world, log):
    changed = False
    for actor in all_actors(world):
        if "BP_Portal_ReturnHome_C" not in class_name(actor):
            continue
        changed = (
            set_exposed_reference(
                actor,
                WORLD_REFERENCE_PROPERTY_NAMES,
                HOMEWORLD_MAP,
                log,
                required_old_fragment="/Game/ExampleAdventure/Levels/00_Homeworld",
            )
            or changed
        )
    return changed


def repair_homeworld_portals(world, log):
    changed = False
    for actor in all_actors(world):
        label = actor.get_actor_label()
        name = actor.get_name()
        index = None
        for candidate in range(2, 8):
            token = "BP_Portal{}".format(candidate)
            if token in label or token in name:
                index = candidate
                break
        if index is None:
            continue
        target = "/Game/Spyro64/Levels/{:02d}_Level{}".format(index, index)
        old_fragment = "/Game/ExampleAdventure/Levels/{:02d}_Level{}".format(
            index, index
        )
        changed = (
            set_exposed_reference(
                actor,
                WORLD_REFERENCE_PROPERTY_NAMES,
                target,
                log,
                required_old_fragment=old_fragment,
            )
            or changed
        )
    return changed


def repair_map_references(map_path, log, is_homeworld=False):
    world = load_map(map_path)
    changed = repair_adventure_reference(world, log)
    changed = repair_return_portal_reference(world, log) or changed
    if is_homeworld:
        changed = repair_homeworld_portals(world, log) or changed
    if changed and not unreal.EditorLoadingAndSavingUtils.save_map(world, map_path):
        raise RuntimeError("Failed to save reference repairs in {}".format(map_path))
    return changed


def repair_save_default(log):
    asset = unreal.EditorAssetLibrary.load_asset(SAVE_SLOT)
    generated_class = unreal.EditorAssetLibrary.load_blueprint_class(SAVE_SLOT)
    if not asset:
        asset = load_object_asset(SAVE_SLOT)
    if not asset:
        log.warn("Could not load {} for default Current_Level repair".format(SAVE_SLOT))
        return False
    # 64_SaveData_S1 is a Blueprint in the current project, but supporting a
    # concrete SaveGame/DataAsset instance keeps this repair deterministic if
    # the project later converts that asset without changing its content path.
    owner = unreal.get_default_object(generated_class) if generated_class else asset
    changed = set_exposed_reference(
        owner,
        ("current_level", "Current_Level", "current level"),
        HOMEWORLD_MAP,
        log,
        required_old_fragment="/Game/ExampleAdventure",
    )
    if changed:
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
            raise RuntimeError("Failed to save repaired {}".format(SAVE_SLOT))
    return changed


def repair_references(log):
    map_paths = [TITLE_MAP, HOMEWORLD_MAP] + LEVEL_MAPS
    # Level 5 does not exist under its final name until ensure_level5_map runs.
    map_paths[2 + 4] = LEVEL5_MAP
    results = {}
    for map_path in map_paths:
        if not load_object_asset(map_path):
            log.warn("Reference repair skipped missing map {}".format(map_path))
            results[map_path] = False
            continue
        results[map_path] = repair_map_references(
            map_path, log, is_homeworld=(map_path == HOMEWORLD_MAP)
        )
    results[SAVE_SLOT] = repair_save_default(log)
    return results


def assert_no_authoritative_example_references(log):
    authoritative_packages = set(
        [TITLE_MAP, HOMEWORLD_MAP, LEVEL5_MAP, SAVE_SLOT]
        + [path for path in LEVEL_MAPS if path != LEVEL_MAPS[4]]
    )
    failures = []
    for map_path in sorted(authoritative_packages):
        if map_path == SAVE_SLOT or not load_object_asset(map_path):
            continue
        record = snapshot_world(map_path, load=True)
        for reference in record.get("references", []):
            value = str(reference.get("value"))
            if (
                "/Game/ExampleAdventure/AdventureInfo_EX" in value
                or "/Game/ExampleAdventure/Levels/" in value
            ):
                failures.append(
                    "{}.{} -> {}".format(
                        reference.get("actor"), reference.get("property"), value
                    )
                )

    old_targets = ["/Game/ExampleAdventure/AdventureInfo_EX"] + [
        "/Game/ExampleAdventure/Levels/{:02d}_Level{}".format(index, index)
        for index in range(1, 8)
    ] + ["/Game/ExampleAdventure/Levels/00_Homeworld"]
    find_referencers = getattr(
        unreal.EditorAssetLibrary, "find_package_referencers_for_asset", None
    )
    if callable(find_referencers):
        for old_target in old_targets:
            try:
                referencers = find_referencers(old_target, True)
            except TypeError:
                referencers = find_referencers(old_target)
            for referencer in referencers:
                package = str(referencer).split(".", 1)[0]
                if package in authoritative_packages:
                    failures.append("{} still depends on {}".format(package, old_target))
    else:
        log.warn(
            "EditorAssetLibrary.find_package_referencers_for_asset is unavailable; "
            "reference assertion used reflected property readback only"
        )

    if failures:
        raise RuntimeError(
            "Authoritative ExampleAdventure references remain after repair: "
            + "; ".join(sorted(set(failures)))
        )
    return True


def assert_level5_foundation(world, expected_identity):
    shell = assert_exact_shell(world, expected_identity)
    failures = []
    for role, expected_location in SHELL_LOCATIONS.items():
        actual = shell[role][0]["location"]
        if any(abs(actual[index] - expected_location[index]) > 0.01 for index in range(3)):
            failures.append(
                "{} location {} != {}".format(role, actual, list(expected_location))
            )
    spyro_rotation = shell["spyro"][0]["rotation"]
    if abs(spyro_rotation[1] - (-90.0)) > 0.01:
        failures.append("Spyro yaw {} != -90".format(spyro_rotation[1]))
    decoration = []
    for actor in all_actors(world):
        if "Dragon_Candle_BP_C" in class_name(actor):
            decoration.append(actor.get_name())
        if actor.get_name() in ("Plane", "Level_spawn_helper"):
            decoration.append(actor.get_name())
        if actor.get_actor_label() in ("Plane", "Level_spawn_helper"):
            decoration.append(actor.get_actor_label())
    if decoration:
        failures.append("template decoration remains: {}".format(decoration))
    settings = unreal.GameplayStatics.get_actor_of_class(world, unreal.WorldSettings)
    if not settings or not settings.get_editor_property("force_no_precomputed_lighting"):
        failures.append("Force No Precomputed Lighting is not enabled")
    if failures:
        raise RuntimeError("Level 5 foundation assertion failed: " + "; ".join(failures))
    return shell


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--snapshot-only", action="store_true")
    parser.add_argument("--skip-references", action="store_true")
    parser.add_argument("--skip-sparx", action="store_true")
    return parser.parse_args(sys.argv[1:])


def main():
    args = parse_args()
    if is_playing_in_editor() and not args.snapshot_only:
        raise RuntimeError("Refusing foundation mutation while PIE is active")

    log = MutationLog()
    source_world = resolve_template_map(log, allow_rename=not args.snapshot_only)
    source_identity = assert_exact_shell(source_world)
    report = {
        "mode": "snapshot-only" if args.snapshot_only else "repair",
        "before": {
            "template": snapshot_world(TEMPLATE_MAP, load=True),
            "template_shell_identity": source_identity,
            "level5": snapshot_world(LEVEL5_MAP),
        },
        "mutations": log.entries,
        "warnings": log.warnings,
    }
    if args.snapshot_only:
        unreal.log_warning("SM64_FOUNDATION_REPORT=" + json.dumps(report, sort_keys=True))
        return

    try:
        world = ensure_level5_map(log)
        assert_exact_shell(world, source_identity)
        repair_shell(world, log)
        assert_level5_foundation(world, source_identity)
        if not args.skip_sparx:
            fix_sparx_template(log)
        if not args.skip_references:
            report["reference_results"] = repair_references(log)
            report["reference_verification"] = assert_no_authoritative_example_references(
                log
            )
        if not args.skip_sparx:
            report["sparx_verification"] = verify_sparx_template_and_instances()
        # Leave the editor/commandlet on the final Level 5 package and ensure
        # the latest reference pass did not discard its shell changes.
        world = load_map(LEVEL5_MAP)
        report["shell_verification"] = assert_level5_foundation(
            world, source_identity
        )
        if not unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL5_MAP):
            raise RuntimeError("Final Level 5 save failed")
        report["after"] = {
            "level5": snapshot_world(LEVEL5_MAP, load=True),
            "sparx": {
                "blueprint": SPARX_BLUEPRINT,
                "template": object_path(
                    find_sparx_capsule_template(
                        unreal.EditorAssetLibrary.load_blueprint_class(SPARX_BLUEPRINT)
                    )
                ),
            },
        }
        report["success"] = True
    except Exception as error:
        report["success"] = False
        report["error"] = str(error)
        report["traceback"] = traceback.format_exc()
        unreal.log_error(report["traceback"])
        raise
    finally:
        report["mutations"] = log.entries
        report["warnings"] = log.warnings
        unreal.log_warning("SM64_FOUNDATION_REPORT=" + json.dumps(report, sort_keys=True))


if __name__ == "__main__":
    main()
