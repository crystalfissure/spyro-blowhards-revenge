"""Read-only reflection probe for the SM64Runtime Unreal Python surface."""

from __future__ import print_function

import unreal


NAMES = (
    "SM64StaticCourseActor",
    "SM64MovingPlatformBase",
    "SM64CourseManager",
    "SM64CourseDefinition",
    "SM64MissionDefinition",
    "SM64WarpDefinition",
    "SM64SurfaceMapping",
    "SM64WaterVolume",
    "SM64Warp",
    "WFTowerPlatformGroup",
    "WFTumblingBridgeController",
    "WFKickableBoard",
    "SM64Climbable",
    "SM64BreakableActor",
)


for name in NAMES:
    value = getattr(unreal, name, None)
    unreal.log_warning("SM64_WF_REFLECT {}={}".format(name, value))
    if value is None:
        continue
    try:
        unreal.log_warning(
            "SM64_WF_REFLECT_HELP {}={}".format(name, unreal.get_type_from_name(name))
        )
    except Exception as error:
        unreal.log_warning("SM64_WF_REFLECT_HELP_ERROR {}={}".format(name, error))


for enum_name in ("SM64PlatformMotion", "SM64AttackType", "SM64SurfaceType"):
    value = getattr(unreal, enum_name, None)
    unreal.log_warning("SM64_WF_REFLECT_ENUM {}={}".format(enum_name, value))
    if value is not None:
        unreal.log_warning("SM64_WF_REFLECT_ENUM_DIR {}={}".format(enum_name, dir(value)))


collision_probe = unreal.load_asset(
    "/Game/Spyro64/SM64/WhompsFortress/Meshes/CollisionDynamic/"
    "SM_WF_COL_GrassPlatform_wf_seg7_collision_rotating_platform"
)
if collision_probe:
    count_fn = getattr(unreal.EditorStaticMeshLibrary, "get_simple_collision_count", None)
    unreal.log_warning(
        "SM64_WF_SIMPLE_COLLISION_COUNT={}".format(
            count_fn(collision_probe) if count_fn else "API_UNAVAILABLE"
        )
    )
    unreal.log_warning(
        "SM64_WF_CONVEX_DOC={}".format(
            getattr(
                unreal.EditorStaticMeshLibrary.set_convex_decomposition_collisions,
                "__doc__",
                "NO_DOC",
            )
        )
    )
