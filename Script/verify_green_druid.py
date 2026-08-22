"""Read-only package/contract verification for the Green Druid deliverable."""

import json
import unreal


ROOT = (
    "/Game/OT_Ports/S1/S1_Enemies/Home_02_Magic_Crafters/"
    "00_MagicCrafters/Green_Druid"
)
PRODUCTION_MAP_PACKAGE = "/Game/_CF_Project/Levels/03_Magic_Crafters_BR"
EXPECTED = [
    ROOT + "/Green_Druid_BP",
    ROOT + "/Green_Druid_T",
    ROOT + "/Green_Druid_Mat",
    ROOT + "/Moving_Geometry/MC_Druid_Channel_A_Lift",
    ROOT + "/Moving_Geometry/MC_Druid_Channel_B_Lift",
    ROOT + "/Moving_Geometry/BP_Green_Druid_Channel_A",
    ROOT + "/Moving_Geometry/BP_Green_Druid_Channel_B",
]


errors = []
for path in EXPECTED:
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        errors.append("missing asset: " + path)

all_paths = unreal.EditorAssetLibrary.list_assets(ROOT, recursive=True, include_folder=False)
loaded = [unreal.EditorAssetLibrary.load_asset(path) for path in all_paths]
raw_path_fragment = "/Moving_Geometry/Raw/"
raw_meshes = [asset for asset in loaded if isinstance(asset, unreal.SkeletalMesh)
              and raw_path_fragment in asset.get_path_name()]
raw_static = [asset for asset in loaded if isinstance(asset, unreal.StaticMesh)
              and raw_path_fragment in asset.get_path_name()]
enemy_takes = [asset for asset in loaded if isinstance(asset, unreal.AnimSequence)
               and "/Moving_Geometry/" not in asset.get_path_name()]
if len(raw_meshes) != 2:
    errors.append("expected 2 raw channel skeletal meshes, found {}".format(len(raw_meshes)))
if raw_static:
    errors.append("full/static terrain was imported: {}".format(
        [asset.get_path_name() for asset in raw_static]))
if len(enemy_takes) != 10:
    errors.append("expected 10 Green Druid raw takes, found {}".format(len(enemy_takes)))

descriptions = []
for path in (EXPECTED[0], EXPECTED[-2], EXPECTED[-1]):
    blueprint = unreal.EditorAssetLibrary.load_asset(path)
    if blueprint:
        if not unreal.MMAEditorAnimationLibrary.compile_blueprint(blueprint):
            errors.append("blueprint compile failed: " + path)
        description = json.loads(
            unreal.MMAEditorAnimationLibrary.describe_mma_green_druid_blueprint(blueprint)
        )
        descriptions.append(description)
        if "Error" in description.get("status", ""):
            errors.append("blueprint status error: " + path)

dirty_packages = [package.get_name() for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
if PRODUCTION_MAP_PACKAGE in dirty_packages:
    errors.append("production map package is dirty")

if errors:
    for error in errors:
        unreal.log_error("[green-druid-verify] " + error)
    raise RuntimeError("Green Druid verification failed: {}".format(errors))

unreal.log_warning("[green-druid-verify] {}".format(json.dumps({
    "raw_channel_skeletal_meshes": len(raw_meshes),
    "raw_channel_static_meshes": len(raw_static),
    "enemy_raw_takes": len(enemy_takes),
    "blueprints": descriptions,
}, sort_keys=True)))
unreal.log_warning("GREEN_DRUID_VERIFY_OK")

