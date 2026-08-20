"""Read-only focused report for Spyro64 AdventureInfo level inventory arrays."""

from __future__ import print_function

import unreal


ASSET = "/Game/Spyro64/AdventureInfo_64"


blueprint = unreal.load_asset(ASSET)
generated_class = unreal.EditorAssetLibrary.load_blueprint_class(ASSET)
if blueprint is None or generated_class is None:
    raise RuntimeError("unable to load " + ASSET)
default_object = unreal.get_default_object(generated_class)
for name in (
    "levels_inventory_info",
    "homeworld_names",
    "save_slot_names",
    "current_level",
):
    try:
        value = default_object.get_editor_property(name)
        unreal.log_warning(
            "SM64_ADVENTURE_PROPERTY name={} type={} value={}".format(
                name, type(value).__name__, str(value)
            )
        )
    except Exception as error:
        unreal.log_warning("SM64_ADVENTURE_PROPERTY name={} unreadable={}".format(name, error))
