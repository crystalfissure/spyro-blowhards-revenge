"""Read-only compact property/component inspection for Spyro64 level actors."""

from __future__ import print_function

import json
import sys
import unreal


DEFAULT_MAPS = ["/Game/Spyro64/Levels/05_Level5_PreWhomps"]
PROPERTY_TERMS = (
    "adventure",
    "capsule",
    "course",
    "destination",
    "gem",
    "home",
    "index",
    "level",
    "map",
    "name",
    "portal",
    "return",
    "skybox",
    "sparx",
    "total",
    "warp",
    "world",
)


def path(value):
    try:
        return value.get_path_name() if value else None
    except Exception:
        return None


def json_value(value, depth=0):
    if value is None or isinstance(value, (bool, float, int, str)):
        return value
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if isinstance(value, unreal.Vector):
        return [value.x, value.y, value.z]
    if isinstance(value, unreal.Rotator):
        return [value.pitch, value.yaw, value.roll]
    if depth < 2 and hasattr(value, "__iter__"):
        try:
            return [json_value(item, depth + 1) for item in list(value)[:32]]
        except Exception:
            pass
    rendered = str(value)
    return rendered[:500]


def matching_properties(value):
    result = {}
    for name in dir(value):
        lowered = name.lower()
        if name.startswith("_") or not any(term in lowered for term in PROPERTY_TERMS):
            continue
        try:
            property_value = value.get_editor_property(name)
        except Exception:
            continue
        result[name] = json_value(property_value)
    return result


def component_record(component):
    result = {
        "name": component.get_name(),
        "class": path(component.get_class()),
        "properties": matching_properties(component),
    }
    if isinstance(component, unreal.SceneComponent):
        result["relative_location"] = json_value(
            component.get_editor_property("relative_location")
        )
        result["absolute_location"] = bool(
            component.get_editor_property("absolute_location")
        )
    return result


def interesting_actor(actor):
    class_name = path(actor.get_class()).lower()
    label = actor.get_actor_label().lower()
    return any(
        term in class_name or term in label
        for term in (
            "adventure",
            "killplane",
            "level5",
            "portal",
            "skybox",
            "sparx",
            "spyro",
            "total_gems",
        )
    )


def inspect_map(map_path):
    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    if not world:
        unreal.log_error("SM64_LEVEL_PROPERTY map-not-found " + map_path)
        return
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    unreal.log_warning(
        "SM64_LEVEL_PROPERTY_STATUS map={} world={} actors={}".format(
            map_path, path(world), len(actors)
        )
    )
    for actor in actors:
        if not actor or not interesting_actor(actor):
            continue
        record = {
            "map": map_path,
            "label": actor.get_actor_label(),
            "name": actor.get_name(),
            "class": path(actor.get_class()),
            "location": json_value(actor.get_actor_location()),
            "properties": matching_properties(actor),
            "components": [
                component_record(component)
                for component in actor.get_components_by_class(unreal.ActorComponent)
            ],
        }
        unreal.log_warning("SM64_LEVEL_PROPERTY=" + json.dumps(record, sort_keys=True))


def main():
    maps = sys.argv[1:] or DEFAULT_MAPS
    for map_path in maps:
        inspect_map(map_path)


if __name__ == "__main__":
    main()
