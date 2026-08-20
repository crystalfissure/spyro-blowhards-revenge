"""Read-only inventory of the existing Spyro64 homeworld Level 5 portal."""

from __future__ import print_function

import json
import unreal


LEVEL = "/Game/Spyro64/Levels/00_Homeworld"


def safe_value(value):
    try:
        if isinstance(value, (bool, int, float, str)) or value is None:
            return value
        if isinstance(value, (list, tuple)):
            return [safe_value(item) for item in value]
        return str(value)
    except Exception:
        return "<unprintable>"


def inspect():
    if not unreal.EditorLevelLibrary.load_level(LEVEL):
        raise RuntimeError("Unable to load " + LEVEL)
    rows = []
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        identity = "{} {} {}".format(
            actor.get_name(), actor.get_actor_label(), actor.get_class().get_name()
        ).lower()
        if "portal" not in identity or not any(token in identity for token in ("5", "level5", "level_5")):
            continue
        properties = {}
        for name in dir(actor):
            lower = name.lower()
            if not any(token in lower for token in (
                "level", "destination", "warp", "portal", "world", "index", "title", "name"
            )):
                continue
            try:
                properties[name] = safe_value(actor.get_editor_property(name))
            except Exception:
                pass
        rows.append({
            "name": actor.get_name(),
            "label": actor.get_actor_label(),
            "class": actor.get_class().get_name(),
            "path": actor.get_path_name(),
            "location": list(actor.get_actor_location().to_tuple()),
            "rotation": list(actor.get_actor_rotation().to_tuple()),
            "properties": properties,
            "components": [
                {
                    "name": component.get_name(),
                    "class": component.get_class().get_name(),
                    "collision": safe_value(component.get_collision_enabled())
                    if isinstance(component, unreal.PrimitiveComponent) else None,
                    "overlap": safe_value(component.get_editor_property("generate_overlap_events"))
                    if isinstance(component, unreal.PrimitiveComponent) else None,
                }
                for component in actor.get_components_by_class(unreal.ActorComponent)
            ],
        })
    unreal.log_warning("SM64_HOMEWORLD_LEVEL5_PORTAL=" + json.dumps(rows, sort_keys=True))


inspect()
