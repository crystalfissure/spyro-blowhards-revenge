"""Read-only acceptance check for the WF homeworld mission selector integration."""

from __future__ import print_function

import json
import unreal


LEVEL = "/Game/Spyro64/Levels/00_Homeworld"
COURSE = "/Game/Spyro64/SM64/WhompsFortress/Data/PDA_WF_CourseDefinition"
STABLE_TAG = "SM64StableId=homeworld/portal/wf_course_select"


def validate():
    if not unreal.EditorLevelLibrary.load_level(LEVEL):
        raise RuntimeError("Unable to load " + LEVEL)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    presentations = [actor for actor in actors if actor.get_name() == "BP_Portal5"]
    if len(presentations) != 1:
        raise AssertionError("Expected one preserved BP_Portal5, found {}".format(len(presentations)))
    presentation = presentations[0]
    surfaces = {
        component.get_name(): component
        for component in presentation.get_components_by_class(unreal.PrimitiveComponent)
        if component.get_name() in ("Portal_Surface", "Portal_Surface_Backwards")
    }
    if set(surfaces) != {"Portal_Surface", "Portal_Surface_Backwards"}:
        raise AssertionError("Legacy travel surface set changed")
    for name, component in surfaces.items():
        if component.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
            raise AssertionError(name + " still performs direct legacy travel")

    selectors = [
        actor for actor in actors
        if STABLE_TAG in [str(tag) for tag in actor.get_editor_property("tags")]
    ]
    if len(selectors) != 1 or selectors[0].get_class().get_name() != "SM64CoursePortal":
        raise AssertionError("WF course selector identity/class mismatch")
    selector = selectors[0]
    if selector.get_editor_property("course_definition") != unreal.load_asset(COURSE):
        raise AssertionError("WF course selector has the wrong course definition")
    if str(selector.get_editor_property("target_level")) != "/Game/Spyro64/Levels/05_Level5":
        raise AssertionError("WF course selector has the wrong destination")
    if not selector.get_editor_property("require_sequential_unlock"):
        raise AssertionError("WF missions are not sequentially gated")
    report = {
        "status": "PASS",
        "presentation": presentation.get_path_name(),
        "selector": selector.get_path_name(),
        "target": str(selector.get_editor_property("target_level")),
    }
    unreal.log_warning("SM64_WF_PORTAL_VALIDATION=" + json.dumps(report, sort_keys=True))


validate()
