"""Remove only generated render imports created before the 100 cm scale fix."""

import json
import unreal


ROOTS = (
    "/Game/Spyro64/SM64/WhompsFortress/Meshes/Static",
    "/Game/Spyro64/SM64/WhompsFortress/Meshes/Water",
    "/Game/Spyro64/SM64/WhompsFortress/Meshes/Movers",
    "/Game/Spyro64/SM64/WhompsFortress/Meshes/Conditional",
)


def main():
    removed = []
    for root in ROOTS:
        for asset_path in sorted(unreal.EditorAssetLibrary.list_assets(root, recursive=True)):
            mesh = unreal.load_asset(asset_path)
            if not isinstance(mesh, unreal.StaticMesh):
                continue
            canonical_path = asset_path.split(".", 1)[0]
            if not unreal.EditorAssetLibrary.delete_asset(canonical_path):
                raise RuntimeError("Unable to remove generated import " + canonical_path)
            removed.append(canonical_path)
    unreal.log_warning("SM64_WF_SCALE_MIGRATION_REMOVED=" + json.dumps(removed))


main()
