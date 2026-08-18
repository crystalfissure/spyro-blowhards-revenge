import unreal


ASSET_PATH = (
    "/Game/SpyroContent/Global_Assets/Global_Characters/"
    "AI_Characters/Actors/Enemies/ClubAttack_Enemy_BP"
)
BASE_AI_ASSET_PATH = (
    "/Game/SpyroContent/Global_Assets/Global_Components/Base_AI_Character"
)

blueprint = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
if not blueprint:
    raise RuntimeError("Could not load {}".format(ASSET_PATH))

if not unreal.MMAEditorAnimationLibrary.add_club_attack_alert_target_guard(blueprint):
    raise RuntimeError("Could not add the alert-target guard")

if not unreal.MMAEditorAnimationLibrary.compile_blueprint(blueprint):
    raise RuntimeError("ClubAttack_Enemy_BP did not compile after repair")

if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
    raise RuntimeError("Could not save ClubAttack_Enemy_BP")

unreal.log_warning("CLUB_ATTACK_GUARD_REPAIR_OK")

base_ai_blueprint = unreal.EditorAssetLibrary.load_asset(BASE_AI_ASSET_PATH)
if not base_ai_blueprint:
    raise RuntimeError("Could not load {}".format(BASE_AI_ASSET_PATH))

if not unreal.MMAEditorAnimationLibrary.add_nearest_alert_player_empty_array_guard(
    base_ai_blueprint
):
    raise RuntimeError("Could not add the nearest-player empty-array guard")

if not unreal.MMAEditorAnimationLibrary.compile_blueprint(base_ai_blueprint):
    raise RuntimeError("Base_AI_Character did not compile after repair")

if not unreal.EditorAssetLibrary.save_loaded_asset(
    base_ai_blueprint, only_if_is_dirty=False
):
    raise RuntimeError("Could not save Base_AI_Character")

unreal.log_warning("BASE_AI_NEAREST_PLAYER_GUARD_REPAIR_OK")
