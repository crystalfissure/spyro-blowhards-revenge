"""Read-only focused inspection of the preserved Level 5 totals actor."""

from __future__ import print_function

import unreal


LEVEL = "/Game/Spyro64/Levels/05_Level5"


if not unreal.EditorLevelLibrary.load_level(LEVEL):
    raise RuntimeError("unable to load " + LEVEL)
actors = list(unreal.EditorLevelLibrary.get_all_level_actors())
world = unreal.EditorLevelLibrary.get_editor_world()
try:
    persistent_level = world.get_editor_property("persistent_level")
    level_script = persistent_level.get_editor_property("level_script_actor")
    if level_script is not None:
        actors.append(level_script)
except Exception as error:
    unreal.log_warning("SM64_TOTALS_LEVEL_SCRIPT_UNREADABLE=" + str(error))
for actor in actors:
    if actor.get_class().get_name() != "BP_Total_Gems_C" and not isinstance(actor, unreal.LevelScriptActor):
        continue
    for name in (
        "my_level",
        "current_adventure_info",
        "item_type",
        "level",
        "level_index",
        "inventory_index",
    ):
        try:
            value = actor.get_editor_property(name)
            unreal.log_warning(
                "SM64_TOTALS_PROPERTY actor={} name={} type={} value={}".format(
                    actor.get_path_name(), name, type(value).__name__, str(value)
                )
            )
        except Exception as error:
            unreal.log_warning("SM64_TOTALS_PROPERTY name={} unreadable={}".format(name, error))
