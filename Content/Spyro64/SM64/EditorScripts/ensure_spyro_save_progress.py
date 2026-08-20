"""Idempotently add the shared SM64 progress map to 64_SaveData_S1."""

from __future__ import print_function

import unreal


SAVE_BLUEPRINT = "/Game/Spyro64/64_SaveData_S1"
PROPERTY_NAME = "SM64_CourseProgress"


def ensure():
    blueprint = unreal.load_asset(SAVE_BLUEPRINT)
    if blueprint is None:
        raise RuntimeError("unable to load " + SAVE_BLUEPRINT)
    if not unreal.SM64EditorReferenceLibrary.ensure_name_int_map_variable(
        blueprint, PROPERTY_NAME
    ):
        raise RuntimeError("unable to create or validate " + PROPERTY_NAME)
    if not unreal.EditorAssetLibrary.save_asset(SAVE_BLUEPRINT, only_if_is_dirty=False):
        raise RuntimeError("unable to save " + SAVE_BLUEPRINT)
    unreal.log_warning(
        "SM64_SAVE_PROGRESS_SAVED asset={} property={}".format(
            SAVE_BLUEPRINT, PROPERTY_NAME
        )
    )


ensure()
