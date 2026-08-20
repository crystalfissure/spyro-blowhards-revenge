"""Read-only inventory of imported WF meshes, slots, bounds, and LOD triangle counts."""

import json
import unreal


ROOT = "/Game/Spyro64/SM64/WhompsFortress/Meshes"


def main():
    report = []
    for path in sorted(unreal.EditorAssetLibrary.list_assets(ROOT, recursive=True)):
        asset = unreal.load_asset(path)
        if not isinstance(asset, unreal.StaticMesh):
            continue
        slots = []
        for material in asset.get_editor_property("static_materials"):
            slots.append(str(material.get_editor_property("material_slot_name")))
        bounds = asset.get_bounds()
        report.append(
            {
                "asset": path.split(".", 1)[0],
                "slots": slots,
                "bounds_origin": [bounds.origin.x, bounds.origin.y, bounds.origin.z],
                "bounds_extent": [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z],
            }
        )
    unreal.log_warning("SM64_WF_IMPORT_INSPECT=" + json.dumps(report, sort_keys=True))


main()
