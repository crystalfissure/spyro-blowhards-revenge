"""Verify the additive SM64 progress target on the real Spyro64 save class."""

from __future__ import print_function

import unreal


SAVE_BLUEPRINT = "/Game/Spyro64/64_SaveData_S1"


def inspect():
    blueprint = unreal.load_asset(SAVE_BLUEPRINT)
    if blueprint is None:
        raise RuntimeError("unable to load " + SAVE_BLUEPRINT)
    generated_class = unreal.EditorAssetLibrary.load_blueprint_class(SAVE_BLUEPRINT)
    if generated_class is None:
        raise RuntimeError("save Blueprint has no generated class")
    default_object = unreal.get_default_object(generated_class)
    candidates = (
        "s1_treasure",
        "sm64_course_progress",
        "permanent_level_gem_counts",
        "levels",
        "s3_levels",
        "list_of_levels",
        "level_item_info",
    )
    report = []
    for name in candidates:
        try:
            value = default_object.get_editor_property(name)
            size = len(value) if hasattr(value, "__len__") else None
            report.append("{}:{}:{}".format(name, type(value).__name__, size))
        except Exception as error:
            report.append("{}:unreadable:{}".format(name, error))
    map_valid = unreal.SM64EditorReferenceLibrary.ensure_name_int_map_variable(
        blueprint, "SM64_CourseProgress"
    )
    if not map_valid:
        raise AssertionError("SM64_CourseProgress is not a Name->int map")
    unreal.log_warning(
        "SM64_SAVE_LINEAGE_SHAPE class={} map_valid={} properties={}".format(
            generated_class.get_path_name(), map_valid, ";".join(report)
        )
    )


inspect()
