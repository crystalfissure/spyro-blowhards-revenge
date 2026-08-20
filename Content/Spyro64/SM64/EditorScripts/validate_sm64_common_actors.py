"""Read-only Unreal acceptance checks for the exact SM64 common actor catalog."""

from __future__ import print_function

import json
import os
import unreal


SOURCE_ROOT = os.path.normpath(
    r"C:\Users\adace\Desktop\spyro-blowhards-revenge\Content\Spyro64"
    r"\SM64\Source\Actors"
)
ACTOR_ROOT = "/Game/Spyro64/SM64/Common/Actors"


def title_name(value):
    return "".join(part.capitalize() for part in value.split("_"))


def load_json(path):
    with open(path, "r") as stream:
        return json.load(stream)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def material_slot_count(mesh):
    if isinstance(mesh, unreal.SkeletalMesh):
        return len(mesh.get_editor_property("materials"))
    return len(mesh.get_editor_property("static_materials"))


def assigned_materials(mesh):
    if isinstance(mesh, unreal.SkeletalMesh):
        return [
            slot.get_editor_property("material_interface")
            for slot in mesh.get_editor_property("materials")
        ]
    return [mesh.get_material(index) for index in range(material_slot_count(mesh))]


def animation_frame_count(asset):
    for method_name in ("get_number_of_frames", "get_number_of_sampled_keys"):
        method = getattr(asset, method_name, None)
        if method is not None:
            return int(method())
    # UE4.27 does not expose NumFrames as a Python property in all builds.
    # Every bridge take is sampled at exactly 30 Hz, so the serialized play
    # length is an equivalent independent check.
    length = float(asset.get_editor_property("sequence_length"))
    return int(round(length * 30.0)) + 1


def validate_actor(actor):
    actor_dir = os.path.join(SOURCE_ROOT, actor)
    exports = load_json(os.path.join(actor_dir, actor + ".exports.json"))
    primary = next(
        item for item in exports["exports"]
        if item["variant"] == "primary" and item["role"] in ("skeletal", "static")
    )
    title = title_name(actor)
    skeletal = primary["role"] == "skeletal"
    category = "Skeletal" if skeletal else "Static"
    prefix = "SK" if skeletal else "SM"
    mesh_path = "{}/{}/{}/{}_SM64_{}".format(
        ACTOR_ROOT, title, category, prefix, title
    )
    mesh = unreal.load_asset(mesh_path)
    expected_type = unreal.SkeletalMesh if skeletal else unreal.StaticMesh
    require(isinstance(mesh, expected_type), "Missing/wrong mesh type: " + mesh_path)
    require(material_slot_count(mesh) > 0, actor + " has no assigned material slots")
    for index, material in enumerate(assigned_materials(mesh)):
        require(material is not None, "{} material {} is null".format(actor, index))

    skeleton_path = None
    animation_rows = []
    expected_actions = exports.get("rig", {}).get("actions", [])
    if skeletal:
        skeleton = mesh.get_editor_property("skeleton")
        require(skeleton is not None, actor + " has no skeleton")
        skeleton_path = skeleton.get_path_name()
        require(unreal.EditorAssetLibrary.does_asset_exist(skeleton_path), actor + " skeleton is not serialized")
        animation_assets = []
        animation_folder = "{}/{}/Animations".format(ACTOR_ROOT, title)
        for path in unreal.EditorAssetLibrary.list_assets(animation_folder, recursive=False):
            asset = unreal.load_asset(path)
            if isinstance(asset, unreal.AnimSequence):
                animation_assets.append(asset)
        require(
            len(animation_assets) == len(expected_actions),
            "{} expected {} animation assets, found {}".format(
                actor, len(expected_actions), len(animation_assets)
            ),
        )
        expected_frames = sorted(int(item["frame_count"]) for item in expected_actions)
        actual_frames = sorted(animation_frame_count(item) for item in animation_assets)
        # FBX samples both endpoints, so UE may report either the source key
        # count or one additional terminal sample depending on take length.
        require(
            all(actual in (expected, expected + 1) for actual, expected in zip(actual_frames, expected_frames)),
            "{} frame counts differ: expected {}, found {}".format(
                actor, expected_frames, actual_frames
            ),
        )
        for asset in sorted(animation_assets, key=lambda item: item.get_name()):
            require(asset.get_editor_property("skeleton") == skeleton, asset.get_name() + " uses the wrong skeleton")
            animation_rows.append(
                {
                    "asset": asset.get_path_name(),
                    "frames": animation_frame_count(asset),
                    "seconds": float(asset.get_editor_property("sequence_length")),
                }
            )

    collision = next(
        (item for item in exports["exports"] if item["role"] == "collision"), None
    )
    collision_path = None
    collision_hulls = 0
    if collision:
        collision_path = "{}/{}/Collision/SM64_COL_{}".format(ACTOR_ROOT, title, title)
        collision_mesh = unreal.load_asset(collision_path)
        require(isinstance(collision_mesh, unreal.StaticMesh), "Missing collision asset " + collision_path)
        collision_hulls = int(
            unreal.SM64EditorReferenceLibrary.get_convex_collision_hull_count(collision_mesh)
        )
        require(collision_hulls > 0, actor + " collision has no convex hull")

    return {
        "actor": actor,
        "mesh": mesh.get_path_name(),
        "role": primary["role"],
        "source_triangles": int(primary["triangle_count"]),
        "expected_bones": int(primary.get("bone_count", 0)),
        "material_slots": material_slot_count(mesh),
        "skeleton": skeleton_path,
        "animations": animation_rows,
        "collision": collision_path,
        "collision_hulls": collision_hulls,
    }


def main():
    rows = []
    actors = sorted(
        name for name in os.listdir(SOURCE_ROOT)
        if os.path.isfile(os.path.join(SOURCE_ROOT, name, name + ".exports.json"))
    )
    require(len(actors) == 22, "Expected 22 source actors, found {}".format(len(actors)))
    for actor in actors:
        rows.append(validate_actor(actor))
    require(sum(len(row["animations"]) for row in rows) == 20, "Expected 20 animations")
    require(sum(1 for row in rows if row["skeleton"]) == 6, "Expected 6 skeletons")
    require(sum(1 for row in rows if row["collision"]) == 8, "Expected 8 collision actors")
    unreal.log_warning("SM64_COMMON_ACTOR_VALIDATION=" + json.dumps(rows, sort_keys=True))


main()
