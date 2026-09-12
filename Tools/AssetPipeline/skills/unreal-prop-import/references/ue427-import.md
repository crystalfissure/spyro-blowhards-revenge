# UE 4.27 import and reimport

These notes describe the verified 4.27 workflow. For another engine version, confirm the corresponding local APIs instead of assuming parity.

## Before changing assets

Record the existing mesh/skeleton/material paths and repository status. Keep original FBX files intact. For a known-correct production mesh, import only the missing animations. For unfamiliar geometry, inspect a separate import before replacing it. A staging skeleton is not a replacement for a shared production skeleton.

For textures/materials, preserve the intended UVs, vertex colours, alpha mode, normals and slot assignments. Check the actual source texture rather than inferring everything from a suffix. Do not impose PS1 filtering, unlit shading or a vertex-colour multiplier on unrelated assets.

## Explicit import settings

| Intent | UE settings / invariant |
| --- | --- |
| Static geometry | `FBXIT_STATIC_MESH`; make a deliberate collision choice |
| New skinned geometry | `FBXIT_SKELETAL_MESH`, `import_as_skeletal=True`; decide whether to import clips |
| Clips for an existing mesh | `FBXIT_ANIMATION`, `import_mesh=False`, `import_animations=True`, `skeleton=<verified existing Skeleton>` |
| Known import route | `automated_import_should_detect_type=False` |
| Reuse materials/textures | `import_materials=False`, `import_textures=False` |
| Keep existing reference pose | Do not update the skeleton reference pose or use frame zero as a new reference pose unless specifically intended |
| Replace newly created clips with changed settings | `replace_existing=True` and `replace_existing_settings=True`; check their stored `asset_import_data` too |
| Save deliberately | Import with `save=False`, validate results, then save the intended loaded assets; avoid save-all |

Use `set_editor_property` for editor-only properties. In 4.27, direct assignment to `FbxSkeletalMeshImportData.update_skeleton_reference_pose` raised `AttributeError`, while `set_editor_property('update_skeleton_reference_pose', False)` worked. Determine each property's owning import-data object; animation corrections belong on `anim_sequence_import_data`, not only the mesh options.

Inspect the imported objects rather than assuming a filename yields exactly one asset. Assert the expected classes, clip count, skeleton identity and durations. Fail clearly when a required skeleton is missing; never quietly create a new one for an animation-only repair.

## Root corrections and reimport cache

1. Compare the mesh's reference root with evaluated animation transforms. Log named Pitch/Yaw/Roll, translation and scale, rather than guessing from an unlabeled vector.
2. Determine whether the discrepancy comes from axis conversion, a wrapper root, units, root motion, or an asset-specific import transform. Do not rotate the actor to hide an incorrect animation import.
3. Apply the measured correction to the relevant import options. Preserving local transforms is an option to investigate, not a universal fix.
4. For existing clips, explicitly replace existing settings. The lantern repair also updated each clip's `asset_import_data.import_rotation` before reimport. Merely supplying new options with `replace_existing=True` retained old settings in that run.
5. Reload/evaluate the saved result. Check all affected axes, representative frames and geometry, then rerun the interaction test if playback changed.

**Lantern case, not a default:** the original reference root had Roll approximately +90 degrees; imported clips had identity roots. `unreal.Rotator(pitch=0, yaw=0, roll=90)` on the animation import matched that mesh. Retaining old import settings prevented the correction until they were explicitly replaced. The generic FBX warning persisted, but runtime root comparisons passed. This does not prove that all other transform warnings are harmless.

One-frame Anim0 imported as 0.0625 seconds; Anim1 spanned 3.875 seconds in Blender and 3.9375 seconds in Unreal. Those are observations from this FBX, not universal sampling settings. Do not truncate a clip or hard-code its timer to a DCC-reported duration without inspecting the sampled result.

## Running Unreal reliably on Windows

- Prefer an existing editor connection after confirming its project/version. If no editor is running, use the project's commandlet workflow. Avoid simultaneous writers to the same assets.
- The tested Python module pattern is a uniquely named module under the project's `Content/Python`, invoked with `-run=pythonscript` and `'-Script=import module_name'`. Use the actual module name. Keep temporary helpers clearly task-owned and clean them up afterward, or retain useful repeatable scripts intentionally.
- Raw filesystem paths with spaces passed as a Script value were interpreted as Python code in this session. Use the verified module form or correctly quoted code; do not improvise shell escaping around apostrophes/backslashes.
- Do not assume a Python commandlet has an editor world. `EditorLevelLibrary.spawn_actor_from_class` crashed after the lantern assets had already been saved. Inspect asset defaults without spawning; create/load an appropriate world when actor tests are required. The native automation test uses an isolated game world.
- In 4.27, load a Blueprint class with `unreal.load_class(None, '/Game/Folder/BP_Name.BP_Name_C')` and get its CDO with `unreal.get_default_object(cls)`. The Python `Blueprint` object did not expose `generated_class()` in this environment.
- Read the actual log after execution. Require script assertions/reports, Blueprint compile results and `Test Completed. Result={Passed}` for the expected test. Exit code zero alone did not establish test success.
- If Unreal cannot write a cache outside the workspace, use a permitted cache location/configuration or the normal authorized execution route. Do not mistake cache-access failure for an animation defect.
- If a new native source file is omitted from an apparently up-to-date build, refresh UBT's gathered source list (the tested `-gather` invocation did so) and inspect the compile actions. Do not delete build folders indiscriminately.

## Existing repository tooling

`Tools/AssetPipeline/ue_asset_pipeline.py` is a mesh/material batch importer. Its mesh path sets `import_mesh=True`; it is not an animation-only repair path. It also warns and creates a new skeleton if an explicitly named skeleton cannot be found, so callers requiring reuse must preflight that reference. Its mesh transform settings do not automatically establish the corresponding animation-import correction.

`Content/Python/install_lantern.py` is a tested asset-specific example, not a generic importer: it assumes the lantern's paths, two clips, +90 roll and `AMMALantern` parent. It refuses an existing destination. `verify_lantern.py` checks the saved references and compilation. Adapt these assumptions deliberately for a new prop rather than changing only its filename.
