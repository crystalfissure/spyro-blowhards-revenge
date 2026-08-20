"""Repair compiled Spyro64 AdventureInfo references with an exact native archive pass.

This script is intentionally narrow. It does not reconstruct Blueprint graphs or use
generic Python property probing. The transient compatibility asset exists only long
enough for UE to resolve the title screen's historical missing import.
"""

from __future__ import print_function

import json
import unreal


PACKAGES = (
    "/Game/Spyro64/64_TitleScreen",
    "/Game/Spyro64/Levels/00_Homeworld",
    "/Game/Spyro64/Levels/01_Level1",
    "/Game/Spyro64/Levels/02_Level2",
    "/Game/Spyro64/Levels/03_Level3",
    "/Game/Spyro64/Levels/04_Level4",
    "/Game/Spyro64/Levels/05_Level5",
    "/Game/Spyro64/Levels/06_Level6",
    "/Game/Spyro64/Levels/07_Level7",
)

TARGET = "/Game/Spyro64/AdventureInfo_64"
EXAMPLE = "/Game/ExampleAdventure/AdventureInfo_EX"
LEGACY = "/Game/Spyro64/AdventureInfo"


def object_path(asset_path):
    name = asset_path.rsplit("/", 1)[-1]
    return asset_path + "." + name


def class_path(asset_path):
    name = asset_path.rsplit("/", 1)[-1]
    return asset_path + "." + name + "_C"


def replace_pair(package_name, old_asset_path):
    counts = {}
    for kind, old_path, new_path in (
        ("asset", object_path(old_asset_path), object_path(TARGET)),
        ("class", class_path(old_asset_path), class_path(TARGET)),
    ):
        hard_count = unreal.SM64EditorReferenceLibrary.replace_hard_references_in_package(
            package_name, old_path, new_path
        )
        soft_count = unreal.SM64EditorReferenceLibrary.replace_soft_references_in_package(
            package_name, old_path, new_path
        )
        if hard_count < 0 or soft_count < 0:
            raise RuntimeError(
                "Native replacement rejected {} {} in {}".format(
                    kind, old_path, package_name
                )
            )
        counts[kind] = {"hard": int(hard_count), "soft_objects": int(soft_count)}
    return counts


def replace_legacy_duplicate_class(package_name):
    """The duplicated compatibility Blueprint retains AdventureInfo_64_C in UE4.27."""
    old_path = LEGACY + ".AdventureInfo_64_C"
    new_path = class_path(TARGET)
    hard_count = unreal.SM64EditorReferenceLibrary.replace_hard_references_in_package(
        package_name, old_path, new_path
    )
    soft_count = unreal.SM64EditorReferenceLibrary.replace_soft_references_in_package(
        package_name, old_path, new_path
    )
    if hard_count < 0 or soft_count < 0:
        raise RuntimeError(
            "Native replacement rejected retained legacy class {} in {}".format(
                old_path, package_name
            )
        )
    return {"hard": int(hard_count), "soft_objects": int(soft_count)}


def save_loaded_map(package_name):
    if not unreal.EditorLoadingAndSavingUtils.save_map(
        unreal.EditorLevelLibrary.get_editor_world(), package_name
    ):
        raise RuntimeError("Unable to save map " + package_name)


def ensure_legacy_alias():
    if unreal.EditorAssetLibrary.does_asset_exist(LEGACY):
        return False
    alias = unreal.EditorAssetLibrary.duplicate_asset(TARGET, LEGACY)
    if not alias:
        raise RuntimeError("Unable to create transient AdventureInfo compatibility alias")
    if not unreal.EditorAssetLibrary.save_asset(LEGACY, only_if_is_dirty=False):
        raise RuntimeError("Unable to save transient AdventureInfo compatibility alias")
    return True


def main():
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET):
        raise RuntimeError("Missing target " + TARGET)
    if not unreal.EditorAssetLibrary.does_asset_exist(EXAMPLE):
        raise RuntimeError("Missing source " + EXAMPLE)

    alias_created = ensure_legacy_alias()
    report = {"alias_created": alias_created, "packages": {}, "alias_deleted": False}

    for package_name in PACKAGES:
        world = unreal.EditorLoadingAndSavingUtils.load_map(package_name)
        if not world:
            raise RuntimeError("Unable to load map " + package_name)

        example_counts = replace_pair(package_name, EXAMPLE)
        legacy_counts = replace_pair(package_name, LEGACY)
        legacy_retained_class = replace_legacy_duplicate_class(package_name)
        save_loaded_map(package_name)
        report["packages"][package_name] = {
            "example": example_counts,
            "legacy": legacy_counts,
            "legacy_retained_class": legacy_retained_class,
            "total": sum(
                sum(kind_counts.values())
                for source_counts in (example_counts, legacy_counts)
                for kind_counts in source_counts.values()
            ) + sum(legacy_retained_class.values()),
        }

    # The alias was created by this script only to make the broken import resolvable.
    # Keep an existing user asset, but remove our transient one after all maps are saved.
    if alias_created:
        report["alias_deleted"] = bool(
            unreal.EditorAssetLibrary.delete_asset(LEGACY)
        )
        if not report["alias_deleted"]:
            raise RuntimeError("Unable to remove transient AdventureInfo compatibility alias")

    unreal.log_warning(
        "SM64_ADVENTURE_INFO_REPAIR=" + json.dumps(report, sort_keys=True)
    )


main()
