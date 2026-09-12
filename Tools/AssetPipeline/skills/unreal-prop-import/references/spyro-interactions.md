# Spyro prop interactions

## Locate the current project

The project used for the lantern was `E:/Spyro Fangame Engine/Spyro-Blowhards-Revenge/Spyro_Bunnited.uproject`, EngineAssociation 4.27. A separate `Spaghetti_2026-04-22` tree contained the original FBX dump. These paths distinguish source data from the target project; rediscover them if unavailable.

The source lantern was under `Content/Spyro_OT_Assets_June/S1/Objects/Home_00_Artisans/02_DarkHollow/Lantern_Dark_Hollow.fbx` in that dump tree. The existing production assets were under `/Game/OT_Ports/S1/S1_Objects/Home_00_Artisans/02_DarkHollow`.

## Use the real damage contract

The component asset is `/Game/SpyroContent/Global_Assets/Global_Components/Damageable_Com`. Inspect its graph and a relevant existing prop when integrating a different item; do not rely on extracted strings alone.

- Non-character owners must assign **Object's Hitbox Component**. Supplying a visible mesh alone does not satisfy this contract.
- The **Deal Damage** function receives a project damage enum, attacker's forward vector, super-damage flag, source actor and magnetize flag.
- The project enum uses **Ram** for charge-type damage and **Burn** for fire damage. Tests looking only for labels such as Charge or Flame exercised no attack types. Resolve the actual enum rather than relying on numeric ordinals.
- **Call Deal_Damage** is a parameterless dispatcher used by the lantern after the component's accepted-damage path. Other dispatchers include attempted/resisted/successful damage and are not interchangeable.
- Check delegate signatures before binding. Set the required hitbox early enough for runtime attacks. Use relevant component defaults/resistances rather than assuming every prop is damageable in the same way.

A non-destructible reaction prop can respond to accepted damage without decrementing health or using an enemy/treasure-container death path. A chest may require destruction and loot; foliage may only loop. Match the requested item.

## Behaviour to specify

Write down idle/rest, accepted trigger types, repeated-hit policy, completion/reset and any sound/light/material changes actually requested or present in the source. Do not add effects because a lantern could plausibly have them.

For the Dark Hollow lantern, the chosen policy was: hold a pose, play the reaction once, ignore duplicate callbacks while playing, return to rest, and then allow another reaction. Restarting or blending successive hits may be appropriate for another item. An ordinary overlap or physics hit is not inherently a Spyro attack.

Keep collision aligned with the resting mesh and intended contact area, including placement scale. Use a suitable proxy if the skeletal mesh has no useful physics asset. Check the attack's trace/object channels; a box existing in the actor is not proof that the player's attack can reach it.

## Reuse without overgeneralizing

The production lantern Blueprint is `Interactive_Lantern/BP_DarkHollowLantern` under the Dark Hollow object folder. It uses the original mesh and materials, with `Lantern_Anim0` and `Lantern_Anim1` on the original skeleton.

Its parent `AMMALantern` lives in `Plugins/MMAEditorTools/Source/MMAGameplay`. It creates the mesh, bounds-fitted box and Damageable component, validates/binds the dispatcher, plays the reaction, and only ticks the actor during playback. Native code was a practical implementation route here, not a requirement for all props. Prefer a suitable existing Blueprint or native abstraction when one is available; do not duplicate a full enemy architecture for scenery.

The editor helper `unreal.MMAEditorAnimationLibrary` can describe Blueprint graphs/class functions and compile Blueprints. This is project functionality, not a stock Unreal API. Inspect availability before using it in another project.

## Verification and honest handoff

The test `Spyro.Props.DarkHollowLantern` creates a game world and spawns the saved production Blueprint. It calls the actual Damageable **Deal Damage** function with Ram and Burn, checks reaction state, duplicate-hit gating, return to rest, survival and disabled idle ticking. It evaluates bone transforms at five times and compares the root with the original reference.

For future items, also verify the displayed pose and deformation, completion continuity, collision reachability, and any item-specific effects. An animation can have the correct root while child bones or weights are wrong.

Distinguish these results:

| Evidence | Establishes |
| --- | --- |
| Source contains curves/takes | Animation data exists |
| Import and asset reload succeed | Packages and references are usable |
| Blueprint compiles | Its graph/class is valid for compilation |
| Direct damage-function runtime test passes | The actor responds correctly to the tested project callback |
| Real-player collision/attack test passes | Attack detection reaches the actor in the tested scene |
| Visual playback reviewed | Appearance and deformation were actually inspected |

The lantern integration completed the actor and automated callback tests; it did not place instances in an existing level or perform a manual Spyro playthrough. Do not turn that historical limitation into a rule to stop future authorized placement or testing.
