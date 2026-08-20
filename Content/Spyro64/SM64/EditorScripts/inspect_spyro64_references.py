"""Read-only reflection inventory for Spyro64 map and save references."""

from __future__ import print_function

import json
import unreal


MAPS = (
    "/Game/Spyro64/64_TitleScreen",
    "/Game/Spyro64/Levels/00_Homeworld",
    "/Game/Spyro64/Levels/01_Level1",
    "/Game/Spyro64/Levels/02_Level2",
    "/Game/Spyro64/Levels/03_Level3",
    "/Game/Spyro64/Levels/04_Level4",
    "/Game/Spyro64/Levels/05_Level5",
    "/Game/Spyro64/Levels/06_Level6",
    "/Game/Spyro64/Levels/07_Level7",
)

PROPERTIES = (
    "current_adventure_info",
    "my_level",
    "leads_to_level",
    "destination_level",
    "homeworld",
    "current_level",
)


def object_path(value):
    try:
        return value.get_path_name()
    except Exception:
        return None


def value_record(value):
    record = {
        "python_type": type(value).__name__,
        "rendered": str(value),
        "object_path": object_path(value),
        "object_class": None,
    }
    try:
        record["object_class"] = object_path(value.get_class())
    except Exception:
        pass
    return record


def owner_record(owner, map_path):
    result = {
        "map": map_path,
        "owner": object_path(owner),
        "owner_class": object_path(owner.get_class()),
        "properties": {},
    }
    for property_name in PROPERTIES:
        try:
            value = owner.get_editor_property(property_name)
        except Exception:
            continue
        result["properties"][property_name] = value_record(value)
    return result


def main():
    for map_path in MAPS:
        world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
        if not world:
            unreal.log_error("SM64_REFERENCE_INSPECT missing map " + map_path)
            continue
        owners = list(
            unreal.GameplayStatics.get_all_actors_of_class(
                world, unreal.LevelScriptActor
            )
        )
        owners.extend(
            actor
            for actor in unreal.GameplayStatics.get_all_actors_of_class(
                world, unreal.Actor
            )
            if actor and "Portal" in actor.get_class().get_name()
        )
        for owner in owners:
            record = owner_record(owner, map_path)
            if record["properties"]:
                unreal.log_warning(
                    "SM64_REFERENCE_INSPECT=" + json.dumps(record, sort_keys=True)
                )

    for asset_path in (
        "/Game/Spyro64/AdventureInfo_64",
        "/Game/Spyro64/64_SaveData_S1",
    ):
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        generated_class = unreal.EditorAssetLibrary.load_blueprint_class(asset_path)
        record = {
            "asset": asset_path,
            "asset_object": value_record(asset),
            "generated_class": value_record(generated_class),
        }
        if generated_class:
            default_object = unreal.get_default_object(generated_class)
            record["default_object"] = owner_record(default_object, asset_path)
        unreal.log_warning(
            "SM64_REFERENCE_ASSET=" + json.dumps(record, sort_keys=True)
        )


if __name__ == "__main__":
    main()
