"""Remove the transient alias and count exact resolved AdventureInfo class refs."""

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

ALIAS = "/Game/Spyro64/AdventureInfo"
TARGET_CLASS = "/Game/Spyro64/AdventureInfo_64.AdventureInfo_64_C"
EXAMPLE_CLASS = "/Game/ExampleAdventure/AdventureInfo_EX.AdventureInfo_EX_C"


def main():
    removed_alias = False
    if unreal.EditorAssetLibrary.does_asset_exist(ALIAS):
        removed_alias = bool(unreal.EditorAssetLibrary.delete_asset(ALIAS))
        if not removed_alias:
            raise RuntimeError("Unable to remove transient " + ALIAS)

    report = {"removed_alias": removed_alias, "packages": {}}
    for package_name in PACKAGES:
        world = unreal.EditorLoadingAndSavingUtils.load_map(package_name)
        if not world:
            raise RuntimeError("Unable to load " + package_name)
        target_count = unreal.SM64EditorReferenceLibrary.count_hard_references_in_package(
            package_name, TARGET_CLASS
        )
        example_count = unreal.SM64EditorReferenceLibrary.count_hard_references_in_package(
            package_name, EXAMPLE_CLASS
        )
        report["packages"][package_name] = {
            "target_class_refs": int(target_count),
            "example_class_refs": int(example_count),
        }

    unreal.log_warning("SM64_ADVENTURE_INFO_VERIFY=" + json.dumps(report, sort_keys=True))


main()
