"""Idempotently put the reusable WF mission selector behind the existing Level 5 portal art."""

from __future__ import print_function

import json
import unreal


LEVEL = "/Game/Spyro64/Levels/00_Homeworld"
COURSE = "/Game/Spyro64/SM64/WhompsFortress/Data/PDA_WF_CourseDefinition"
TARGET = "/Game/Spyro64/Levels/05_Level5"
STABLE_TAG = "SM64StableId=homeworld/portal/wf_course_select"


def stable_actor():
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if STABLE_TAG in [str(tag) for tag in actor.get_editor_property("tags")]:
            return actor
    return None


def integrate():
    if not unreal.EditorLevelLibrary.load_level(LEVEL):
        raise RuntimeError("Unable to load " + LEVEL)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    presentation = next(
        (actor for actor in actors if actor.get_name() == "BP_Portal5" and actor.get_class().get_name() == "BP_Portal_C"),
        None,
    )
    if presentation is None:
        raise RuntimeError("The preserved Spyro64 BP_Portal5 presentation actor is missing")

    disabled = []
    for target_name in ("Portal_Surface", "Portal_Surface_Backwards"):
        component = next(
            (
                item for item in presentation.get_components_by_class(unreal.PrimitiveComponent)
                if item.get_name() == target_name
            ),
            None,
        )
        if component is None:
            raise RuntimeError("Unable to locate legacy portal travel surface " + target_name)
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        disabled.append(target_name)

    portal = stable_actor()
    created = False
    if portal is not None and portal.get_class().get_name() != "SM64CoursePortal":
        unreal.EditorLevelLibrary.destroy_actor(portal)
        portal = None
    if portal is None:
        portal = unreal.SM64EditorReferenceLibrary.spawn_actor_in_editor(
            unreal.SM64CoursePortal, presentation.get_actor_transform()
        )
        if portal is None:
            raise RuntimeError("Unable to spawn SM64CoursePortal")
        created = True
    portal.set_actor_location_and_rotation(
        presentation.get_actor_location(), presentation.get_actor_rotation(), False, False
    )
    portal.set_editor_property("course_definition", unreal.load_asset(COURSE))
    portal.set_editor_property("target_level", TARGET)
    portal.set_editor_property("require_sequential_unlock", True)
    portal.get_editor_property("trigger").set_box_extent(unreal.Vector(230.0, 230.0, 220.0))
    portal.set_actor_label("SM64_WF_MissionSelector", mark_dirty=True)
    tags = [str(tag) for tag in portal.get_editor_property("tags") if not str(tag).startswith("SM64StableId=")]
    portal.set_editor_property("tags", tags + ["SM64Generated", STABLE_TAG])

    unreal.EditorLevelLibrary.save_current_level()
    report = {
        "status": "PASS",
        "created": created,
        "presentation": presentation.get_path_name(),
        "selector": portal.get_path_name(),
        "disabled_legacy_surfaces": sorted(disabled),
        "target": TARGET,
    }
    unreal.log_warning("SM64_WF_PORTAL_INTEGRATION=" + json.dumps(report, sort_keys=True))


integrate()
