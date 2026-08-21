"""Read-only Whomp placement, component-transform, and imported-bounds audit."""

from __future__ import print_function

import json
import unreal


LEVEL = "/Game/Spyro64/Levels/05_Level5"
SKELETAL = "/Game/Spyro64/SM64/Common/Actors/Whomp/Skeletal/SK_SM64_Whomp"
COLLISION = "/Game/Spyro64/SM64/Common/Actors/Whomp/Collision/SM64_COL_Whomp"


def vector(value):
    return [float(value.x), float(value.y), float(value.z)]


def rotator(value):
    return [float(value.pitch), float(value.yaw), float(value.roll)]


def transform(value):
    return {
        "location": vector(value.translation),
        "rotation": rotator(value.rotation.rotator()),
        "scale": vector(value.scale3d),
    }


def main():
    result = {"assets": {}, "actors": []}
    skeletal = unreal.load_asset(SKELETAL)
    collision = unreal.load_asset(COLLISION)
    if skeletal:
        result["assets"]["skeletal"] = {
            "path": skeletal.get_path_name(),
        }
    if collision:
        result["assets"]["collision"] = {
            "path": collision.get_path_name(),
        }

    if not unreal.EditorLevelLibrary.load_level(LEVEL):
        raise RuntimeError("Unable to load " + LEVEL)
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if not isinstance(actor, unreal.WFWhomp):
            continue
        record = {
            "name": actor.get_name(),
            "actor_transform": transform(actor.get_actor_transform()),
            "king": bool(actor.get_editor_property("king_whomp")),
            "components": {},
        }
        actor_origin, actor_extent = actor.get_actor_bounds(False, True)
        record["actor_bounds"] = {
            "origin": vector(actor_origin),
            "extent": vector(actor_extent),
        }
        for component_name in ("character_mesh", "exact_collision_mesh", "collision_box"):
            component = actor.get_editor_property(component_name)
            record["components"][component_name] = {
                "relative": transform(component.get_relative_transform()),
                "world": transform(component.get_world_transform()),
            }
        result["actors"].append(record)
    unreal.log_warning("SM64_WF_WHOMP_ALIGNMENT=" + json.dumps(result, sort_keys=True))


main()
