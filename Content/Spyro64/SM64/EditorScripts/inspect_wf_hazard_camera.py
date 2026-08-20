"""Read-only inspection for the preserved kill plane and semantic surface assets."""

from __future__ import print_function

import json
import unreal


LEVEL = "/Game/Spyro64/Levels/05_Level5"
ASSETS = {
    "death": "/Game/Spyro64/SM64/WhompsFortress/Meshes/Collision/SM_WF_COL_Area1_wf_seg7_collision_070102D8_COL_WF_Area1_wf_seg7_collision_070102D8_01_SURFACE_DEATH_PLANE",
    "boss_camera": "/Game/Spyro64/SM64/WhompsFortress/Meshes/Collision/SM_WF_COL_Area1_wf_seg7_collision_070102D8_COL_WF_Area1_wf_seg7_collision_070102D8_07_SURFACE_BOSS_FIGHT_CAMERA",
    "camera_middle": "/Game/Spyro64/SM64/WhompsFortress/Meshes/Collision/SM_WF_COL_Area1_wf_seg7_collision_070102D8_COL_WF_Area1_wf_seg7_collision_070102D8_08_SURFACE_CAMERA_MIDDLE",
}


def vector(value):
    return [float(value.x), float(value.y), float(value.z)]


if not unreal.EditorLevelLibrary.load_level(LEVEL):
    raise RuntimeError("Unable to load " + LEVEL)

report = {"assets": {}, "kill_plane": None}
for key, path in ASSETS.items():
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing semantic surface asset " + path)
    bounds = asset.get_bounds()
    report["assets"][key] = {
        "origin": vector(bounds.origin),
        "box_extent": vector(bounds.box_extent),
        "sphere_radius": float(bounds.sphere_radius),
    }

for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if actor.get_class().get_name() != "BP_KillPlane_C":
        continue
    components = []
    for component in actor.get_components_by_class(unreal.PrimitiveComponent):
        bounds_origin, bounds_extent, _ = unreal.SystemLibrary.get_component_bounds(component)
        components.append({
            "name": component.get_name(),
            "class": component.get_class().get_name(),
            "collision": str(component.get_collision_enabled()),
            "bounds_origin": vector(bounds_origin),
            "bounds_extent": vector(bounds_extent),
        })
    report["kill_plane"] = {
        "path": actor.get_path_name(),
        "location": vector(actor.get_actor_location()),
        "components": components,
    }
    break

if report["kill_plane"] is None:
    raise AssertionError("Preserved BP_KillPlane is missing")
unreal.log_warning("SM64_WF_HAZARD_CAMERA_INSPECTION=" + json.dumps(report, sort_keys=True))
