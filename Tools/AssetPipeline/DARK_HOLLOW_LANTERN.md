# Dark Hollow lantern — production integration

Implemented in `E:/Spyro Fangame Engine/Spyro-Blowhards-Revenge`.

## Use in Unreal

Search the Content Browser for **BP_DarkHollowLantern** and drag it into the desired level. It is saved under:

`/Game/OT_Ports/S1/S1_Objects/Home_00_Artisans/02_DarkHollow/Interactive_Lantern`

This is a ready-to-place actor. No existing level was changed or populated automatically.

## Behaviour

- Uses the existing lantern skeletal mesh, skeleton and its materials.
- Holds `Lantern_Anim0` at rest.
- Uses `Damageable_Com` and its `Call Deal_Damage` dispatcher.
- Assigns a box hitbox fitted to the mesh bounds to `Object's Hitbox Component`.
- Plays `Lantern_Anim1` once on an accepted attack. The imported reaction lasts 3.9375 seconds.
- Ignores duplicate callbacks during playback, then restores the resting pose and accepts another attack.
- Does not destroy the prop, reduce health or spawn rewards. Ordinary collision alone does not trigger a reaction.

The Blueprint inherits `AMMALantern` in the existing MMAGameplay runtime module. Mesh and animations are configured in Blueprint defaults. Actor ticking is enabled only during a reaction.

## Animation correction

The original mesh reference root has a +90 degree roll. Default animation import produced an identity root. Both clips now import with **Import Rotation Roll = 90**, Pitch/Yaw = 0, against the existing skeleton. The original mesh and skeleton were not modified.

Keep these settings when reimporting. Scripted reimport must explicitly replace existing import settings to apply new options; replacing animation data alone retains previous settings.

The generic FBX transform warning can still appear due to the source coordinate conversion. Runtime tests verify that the resulting root position, rotation and scale match the production reference pose.

## Validation

UE 4.27 editor build succeeded. The saved Blueprint reloads and compiles, and both animations reference the original skeleton.

**Spyro.Props.DarkHollowLantern passed on 2026-09-12.** The automation test spawns the production Blueprint in a separate game world and checks:

- Damage dispatcher and hitbox initialization.
- The real `Deal Damage` function with **Ram** and **Burn** damage types.
- Duplicate hit gating, recovery, survival and idle tick disabling.
- Finite evaluated bone transforms and reference root alignment at five points through the reaction.

These are automated runtime tests, not a manual playthrough of Spyro charging or flaming a placed lantern. Placement and a visual in-level check remain for the level designer.

Evidence in the workspace: `Inspection/runtime_tests.log`, `Inspection/production_install.json`, `Inspection/production_verify.log`.

## Production files

- `Plugins/MMAEditorTools/Source/MMAGameplay/Public/MMALantern.h`
- `Plugins/MMAEditorTools/Source/MMAGameplay/Private/MMALantern.cpp`
- `Plugins/MMAEditorTools/Source/MMAGameplay/Private/MMALanternTests.cpp`
- `Content/Python/install_lantern.py` — initial installation; refuses an existing destination.
- `Content/Python/verify_lantern.py` — checks installed assets.

The local editor module has been rebuilt. Rebuild the editor target on another machine after bringing over the native source.
