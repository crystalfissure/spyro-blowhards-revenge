"""Read-only reflection report for Spyro state and Spyro64 save integration.

This script deliberately does not load or save a map, compile a Blueprint, or
set a property.  It is safe to run against an editor that already has the
project loaded (including PIE) when native integration needs the exact names
exported by Blueprint-generated classes.
"""

from __future__ import print_function

import json
import unreal


ASSETS = {
    "spyro": "/Game/SpyroContent/Global_Assets/Global_Characters/Playable_Characters/Spyro/BP_Spyro",
    "save_base": "/Game/Spyro64/64_SaveData",
    "save_slot": "/Game/Spyro64/64_SaveData_S1",
    "adventure": "/Game/Spyro64/AdventureInfo_64",
}

KEYWORDS = (
    "state",
    "charge",
    "flame",
    "fire",
    "breath",
    "head",
    "bash",
    "cannon",
    "air",
    "land",
    "ground",
    "jump",
    "fall",
    "save",
    "slot",
    "level",
    "adventure",
    "progress",
    "star",
    "coin",
    "gem",
    "player",
)


def object_path(value):
    try:
        return value.get_path_name()
    except Exception:
        return None


def summarize(value, depth=0):
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    path = object_path(value)
    if path:
        return {"object": path, "class": object_path(value.get_class())}
    if isinstance(value, (list, tuple)):
        if depth > 1:
            return {"count": len(value)}
        return [summarize(item, depth + 1) for item in list(value)[:32]]
    if isinstance(value, dict):
        if depth > 1:
            return {"count": len(value)}
        result = {}
        for index, (key, item) in enumerate(value.items()):
            if index >= 32:
                break
            result[str(key)] = summarize(item, depth + 1)
        return result
    try:
        return str(value)
    except Exception:
        return "<unprintable {}>".format(type(value).__name__)


def matching_names(value):
    names = []
    for name in dir(value):
        lowered = name.lower()
        if any(keyword in lowered for keyword in KEYWORDS):
            names.append(name)
    return sorted(set(names))


def reflected_members(value):
    result = {}
    for name in matching_names(value):
        try:
            member = getattr(value, name)
        except Exception as error:
            result[name] = {"error": str(error)}
            continue
        if callable(member):
            result[name] = {"callable": True}
            continue
        result[name] = summarize(member)
    return result


def candidate_property_reads(value):
    # Blueprint properties are not guaranteed to appear in dir() in UE 4.27.
    # Probe likely sanitized names read-only and report only successful names.
    words = (
        "Player State",
        "Player_State",
        "Gameplay State",
        "Gameplay_State",
        "Environmental State",
        "Environmental_State",
        "Ground State",
        "Ground_State",
        "During Charge",
        "During_Charge",
        "Charge Speed",
        "Charge_Speed",
        "Cant Charge",
        "Cant_Charge",
        "Can Initiate Supercharge",
        "Can_Initiate_Supercharge",
        "Breathing Fire",
        "Breathing_Fire",
        "Breathed Fire",
        "Breathed_Fire",
        "isHeadbashing",
        "Is Headbashing",
        "Is_Headbashing",
        "Currently Fairy Falling",
        "Currently_Fairy_Falling",
        "Player in Portal",
        "Player_in_Portal",
        "Current AdventureInfo",
        "Current_AdventureInfo",
        "Current SaveSlot",
        "Current_SaveSlot",
        "Current Level",
        "Current_Level",
        "Levels Inventory Info",
        "Levels_InventoryInfo",
        "Save Data Type",
        "SaveData_Type",
        "Save Slot Names",
        "SaveSlot_Names",
        "Number of Gems",
        "NumberofGems",
    )
    result = {}
    for display_name in words:
        variants = {
            display_name,
            display_name.lower(),
            display_name.replace(" ", "_"),
            display_name.replace(" ", "_").lower(),
        }
        for name in sorted(variants):
            try:
                result[name] = summarize(value.get_editor_property(name))
            except Exception:
                continue
    return result


def class_lineage(generated_class):
    result = []
    current = generated_class
    for _unused in range(16):
        if not current:
            break
        result.append(object_path(current))
        next_class = None
        for method_name in ("get_super_class", "get_super_struct"):
            method = getattr(current, method_name, None)
            if callable(method):
                try:
                    next_class = method()
                except Exception:
                    next_class = None
                if next_class:
                    break
        if not next_class:
            try:
                next_class = current.get_editor_property("super_struct")
            except Exception:
                pass
        if not next_class or next_class == current:
            break
        current = next_class
    return result


def inspect_asset(asset_path):
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    generated_class = unreal.EditorAssetLibrary.load_blueprint_class(asset_path)
    record = {
        "asset_path": asset_path,
        "asset": object_path(asset),
        "generated_class": object_path(generated_class),
    }
    if not generated_class:
        return record
    default_object = unreal.get_default_object(generated_class)
    record.update(
        {
            "class_lineage": class_lineage(generated_class),
            "default_object": object_path(default_object),
            "matching_members": reflected_members(default_object),
            "successful_property_reads": candidate_property_reads(default_object),
        }
    )
    return record


def inspect_loaded_spyro_instances(spyro_class):
    result = []
    if not spyro_class:
        return result
    for world in unreal.EngineLibrary.get_engine_subsystem(
        unreal.UnrealEditorSubsystem
    ).get_all_level_actors() if False else []:
        # Kept unreachable because UnrealEditorSubsystem was introduced after
        # UE 4.27.  PIE worlds are located through loaded object iteration below.
        del world
    try:
        instances = unreal.ObjectLibrary.create_library(spyro_class, True, True)
        instances.load_blueprint_asset_data_from_path("/Game/Spyro64")
    except Exception:
        instances = None
    # UE 4.27 does not expose a reliable all-world iterator to Python.  The
    # current editor world still yields the selected map's non-PIE instance.
    try:
        world = unreal.EditorLevelLibrary.get_editor_world()
        actors = unreal.GameplayStatics.get_all_actors_of_class(world, spyro_class)
    except Exception:
        actors = []
    for actor in actors:
        result.append(
            {
                "object": object_path(actor),
                "matching_members": reflected_members(actor),
                "successful_property_reads": candidate_property_reads(actor),
            }
        )
    del instances
    return result


def inspect_unreal_enum_exports():
    result = {}
    for name in sorted(dir(unreal)):
        lowered = name.lower()
        if not any(key in lowered for key in ("state", "breath", "charge", "attack")):
            continue
        try:
            value = getattr(unreal, name)
            members = [member for member in dir(value) if member.isupper()]
        except Exception:
            continue
        if members:
            result[name] = members
    return result


def main():
    records = {name: inspect_asset(path) for name, path in ASSETS.items()}
    spyro_class = unreal.EditorAssetLibrary.load_blueprint_class(ASSETS["spyro"])
    report = {
        "assets": records,
        "editor_world_spyro_instances": inspect_loaded_spyro_instances(spyro_class),
        "matching_unreal_enum_exports": inspect_unreal_enum_exports(),
    }
    unreal.log_warning("SM64_SPYRO_RUNTIME_REFLECTION=" + json.dumps(report, sort_keys=True))


if __name__ == "__main__":
    main()
