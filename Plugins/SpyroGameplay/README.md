# Spyro Gameplay

This project-owned plugin supplies runtime components used by existing Blueprints, including the Gnorc Thief. Keep it enabled when opening or packaging the project. The thief is not a standalone asset that can be migrated without its shared Spyro damage, player, gem and checkpoint Blueprints.

## Collaborator setup

Use **Unreal Engine 4.27.2 on Windows x64** for the checked-in editor binaries. Pull the repository and its Git LFS assets (`git lfs pull`) before opening `Spyro_Bunnited.uproject`. Both `UE4Editor-SpyroGameplay.dll` and `UE4Editor-SpyroEditor.dll`, plus their module manifest, are included as ordinary Git files. Opening the project with that engine does not require compiling this plugin locally. Other engine builds or platforms require rebuilding from the included C++ source with a compatible Unreal toolchain.

No Marketplace plugin, Codex addon, emulator, OpenPete checkout, Blender installation, external Python package, research folder or original sound WAV is required to run the Gnorc Thief. The saved assets contain the imported meshes, animations and audio. Python and Editor Scripting Utilities are engine-provided tools used by the project's existing import workflow; they are not runtime dependencies of the thief component. `SpyroEditor` is an editor-only module and is excluded from packaged runtime targets.

The project's other plugins support other project features. In particular, shared assets reference `PS1IsoGate` and `MMAGameplay`; removing those plugins as part of a thief cleanup would break existing references.

## Gnorc Thief

Blueprint: `/Game/OT_Ports/S1/S1_Enemies/Home_00_Artisans/00_Artisans/Gnorc_Thief/Gnorc_Thief_BP`.

The behavior component controls the original 30 Hz state timing, alert/facing, route steering, alternating run animations, three-hit reaction sequence, frame-timed sounds and checkpoint reset. The first two hits each drop one gem; the final hit drops three and plays the separate final mesh animation with a slowing slide. The custom animation instance belongs to the same runtime module.

`RoutePoints` are spawn-relative original-game coordinates, rotated by the placed actor's yaw and projected onto UE terrain. Adjust them for the destination level's layout. `WorldUnitsPerOriginalUnit` defaults to `0.146104`, calibrated for the supplied mesh scale. Explicit `Pursuer` assignment is optional; otherwise the component uses the local player character.

Validation for this revision: UE4.27 editor build and Blueprint compilation passed; 13 runtime checks passed at both 60 and 20 FPS render caps. Tests include the three hits, gem counts, sound sequence, moving rolls, final slide, checkpoint cleanup, and Spyro walking/charge contact without lifting the thief. Original meshes, animation assets and the prototype map were preserved.
