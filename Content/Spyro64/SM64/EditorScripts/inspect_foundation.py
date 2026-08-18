"""Read-only inventory for the Spyro64/Whomp's Fortress UE foundation."""

from __future__ import print_function

import json
import unreal


TEMPLATE_MAP = "/Game/Spyro64/Levels/05_Level5_PreWhomps"
LEVEL5_MAP = "/Game/Spyro64/Levels/05_Level5"


def object_path(value):
    return value.get_path_name() if value else None


def class_path(value):
    cls = value.get_class() if value else None
    return object_path(cls)


def vector(value):
    return [value.x, value.y, value.z]


def rotator(value):
    return [value.pitch, value.yaw, value.roll]


def actor_record(actor):
    transform = actor.get_actor_transform()
    return {
        "name": actor.get_name(),
        "label": actor.get_actor_label(),
        "class": class_path(actor),
        "location": vector(transform.translation),
        "rotation": rotator(transform.rotation.rotator()),
        "scale": vector(transform.scale3d),
        "tags": [str(tag) for tag in actor.tags],
    }


def world_record(path):
    world = unreal.EditorAssetLibrary.load_asset(path)
    if not world:
        object_name = path.rsplit("/", 1)[-1]
        world = unreal.load_object(None, path + "." + object_name)
    if not world:
        return {"path": path, "exists": False}
    actors = [
        actor_record(actor)
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
        if actor
    ]
    settings = unreal.GameplayStatics.get_actor_of_class(world, unreal.WorldSettings)
    return {
        "path": path,
        "exists": True,
        "world": object_path(world),
        "actor_count": len(actors),
        "actors": actors,
        "force_no_precomputed_lighting": bool(
            settings and settings.get_editor_property("force_no_precomputed_lighting")
        ),
    }


def find_assets():
    expected = (
        "/Game/Spyro64/AdventureInfo_64",
        "/Game/ExampleAdventure/Utils/BP_Total_Gems",
        "/Game/SpyroContent/Global_Assets/Global_Characters/Playable_Characters/Spyro/BP_Spyro",
        "/Game/SpyroContent/Global_Assets/Global_Characters/Sparx/Sparx_BP",
        "/Game/SpyroContent/Global_Assets/Global_Level_Items/BP_KillPlane",
        "/Game/SpyroContent/Global_Assets/Global_Level_Items/Portals/Actors/BP_Portal_ReturnHome",
        "/Game/SpyroContent/Global_Assets/Global_Level_Items/Portals/Actors/BP_Skybox",
    )
    return {
        path: bool(unreal.EditorAssetLibrary.does_asset_exist(path)) for path in expected
    }


def component_record(component):
    record = {
        "name": component.get_name(),
        "class": class_path(component),
    }
    if isinstance(component, unreal.SceneComponent):
        record.update(
            {
                "relative_location": vector(
                    component.get_editor_property("relative_location")
                ),
                "absolute_location": bool(
                    component.get_editor_property("absolute_location")
                ),
            }
        )
    return record


def inspect_blueprint_components(asset_path):
    generated_class = unreal.EditorAssetLibrary.load_blueprint_class(asset_path)
    if not generated_class:
        return {"path": asset_path, "exists": False}
    default_object = unreal.get_default_object(generated_class)
    components = default_object.get_components_by_class(unreal.ActorComponent)
    result = {
        "path": asset_path,
        "exists": True,
        "generated_class": object_path(generated_class),
        "components": [component_record(component) for component in components],
    }
    blueprint = unreal.EditorAssetLibrary.load_asset(asset_path)
    try:
        script = blueprint.get_editor_property("simple_construction_script")
        nodes = script.get_all_nodes()
        result["scs_components"] = [
            component_record(node.get_editor_property("component_template"))
            for node in nodes
        ]
    except Exception as error:  # UE 4.27 does not expose SCS on every build.
        result["scs_error"] = str(error)
        result["blueprint_component_members"] = [
            name
            for name in dir(blueprint)
            if "component" in name.lower() or "construction" in name.lower()
        ]
    return result


def main():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(["/Game/Spyro64/Levels"], force_rescan=True)
    assets = find_assets()
    sparx_candidates = [
        "/Game/SpyroContent/Global_Assets/Global_Characters/Sparx/Sparx_BP"
    ]
    current_world = unreal.EditorLevelLibrary.get_editor_world()
    report = {
        "template": world_record(TEMPLATE_MAP),
        "level5": world_record(LEVEL5_MAP),
        "registered_level_assets": list(
            unreal.EditorAssetLibrary.list_assets(
                "/Game/Spyro64/Levels", recursive=True, include_folder=False
            )
        ),
        "expected_assets": assets,
        "current_world": object_path(current_world),
        "sparx": [inspect_blueprint_components(path) for path in sparx_candidates],
    }
    unreal.log_warning("SM64_FOUNDATION_INSPECT=" + json.dumps(report, sort_keys=True))


if __name__ == "__main__":
    main()
