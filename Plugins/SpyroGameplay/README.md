# Spyro Gameplay

This project-owned plugin supplies runtime components used by existing Blueprints, including the Gnorc Thief. Keep it enabled when opening or packaging the project. The thief is not a standalone asset that can be migrated without its shared Spyro damage, player, gem and checkpoint Blueprints.

## Collaborator setup

Use **Unreal Engine 4.27.2 on Windows x64** for the checked-in editor binaries. Pull the repository and its Git LFS assets (`git lfs pull`) before opening `Spyro_Bunnited.uproject`. Both `UE4Editor-SpyroGameplay.dll` and `UE4Editor-SpyroEditor.dll`, plus their module manifest, are included as ordinary Git files. Opening the project with that engine does not require compiling this plugin locally. Other engine builds or platforms require rebuilding from the included C++ source with a compatible Unreal toolchain.

No Marketplace plugin, Codex addon, emulator, OpenPete checkout, Blender installation, external Python package, research folder or original sound WAV is required to run the Gnorc Thief. The saved assets contain the imported meshes, animations and audio. Python and Editor Scripting Utilities are engine-provided tools used by the project's existing import workflow; they are not runtime dependencies of the thief component. `SpyroEditor` is an editor-only module and is excluded from packaged runtime targets.

The project's other plugins support other project features. In particular, shared assets reference `PS1IsoGate` and `MMAGameplay`; removing those plugins as part of a thief cleanup would break existing references.

The separate [Bull and Toreador guide](TOWN_SQUARE.md) covers Town Square enemy placement, explicit pairing, route settings, reference evidence and validation limits.

The [Sleeping Dog and Toasty guide](TOASTY.md) covers the Toasty assets, staged guard assignments, flame phases, original timing and Unreal adaptation limits.

The [Giant Pansy guide](GIANT_PANSY.md) covers stationary and roaming variants, flower contact, original timing, rewards, and checkpoint reset.

## Gnorc Thief

Blueprint: `/Game/OT_Ports/S1/S1_Enemies/Home_00_Artisans/00_Artisans/Gnorc_Thief/Gnorc_Thief_BP`.

The behavior component controls the original 30 Hz state timing, alert/facing, route steering, alternating run animations, three-hit reaction sequence, frame-timed sounds and checkpoint reset. The first two hits each drop one gem; the final hit drops three and plays the separate final mesh animation with a slowing slide. The custom animation instance belongs to the same runtime module.

Select the **GnorcThiefBehavior** component and open **Thief > Roaming** to set **Roam Radius**. It defaults to **1,200 cm (12 metres)**, with a 300 cm minimum. The horizontal boundary stays centered on the actor's starting position, applies to running and both hit rolls, and reserves clearance for the collision body. A cyan preview sphere appears when the actor is selected in the editor; it has no collision and is hidden in play. Set the radius in the Blueprint defaults or override it on a placed instance before playing. **Limit Roaming** enables this feature; disabling it restores the unbounded route behavior.

`RoutePoints` are spawn-relative original-game coordinates, rotated by the placed actor's yaw and projected onto UE terrain. With roaming limited, the XY route is uniformly reduced if needed to fit inside the radius with turning clearance. The radius does not change alert distance, turning rate, running speed or roll speed. It is a UE placement safeguard; the original game used its authored route and terrain to keep the thief in the intended area. Adjust route points if obstacles inside the circle make a destination unreachable. `WorldUnitsPerOriginalUnit` defaults to `0.146104`, calibrated for the supplied mesh scale. Explicit `Pursuer` assignment is optional; otherwise the component uses the local player character.

### Placing a thief in another level

Drag `Gnorc_Thief_BP` from the Content Browser into your level, with its capsule resting on blocking ground. Set the placed actor's yaw to orient the route, then adjust **GnorcThiefBehavior > Thief > Roaming > Roam Radius** for the available space. The default remains 1,200 cm; approximately 3,650 cm allows the supplied original route to run without compression. The radius is centered on the starting position, rather than following the thief.

The radius limits movement; it does not find a path around every obstacle inside the circle. Use **Thief > Route > Route Points** to fit your level's terrain. Points use original-game units (one unit is approximately 0.146104 cm), relative to the starting transform. No NavMesh is required. Enable **Thief > Debug > Draw Movement Debug** during play to inspect the route, fitted scale, body clearance and obstruction status. Keep this disabled for ordinary play.

The thief already placed in `01_Artisans_BR` uses a 3,650 cm radius and 240-degree yaw. Four of its nine route points are adjusted around this level's fountain and raised dividers. These are instance overrides: the Blueprint's default route, default radius, meshes, animations and sounds are preserved.

Player contact now allows bounded sliding and safe separation. If a fleeing thief remains blocked, he can retrace the same route segment, including when Spyro intercepts the return trip. This recovery respects the original turn rate and does not change the speed or timing of either hit roll. A fully blocked passage or an outward roll at the hard boundary still limits travel.

Death cleanup hides the final mesh and retains the existing cleanup sound and checkpoint event without spawning the shared white dust ring. Shared enemy Blueprints and the gem/save contracts are unchanged. Research scripts, test fixtures and demonstration recordings are kept outside the repository and are not required by collaborators.

### Validation

The UE4.27 editor modules build successfully, and the thief plus four other Blueprints using this plugin compile. Sixteen live collision scenarios passed at 60 FPS, with focused repeats at 20 and 30 FPS. Coverage includes standing/walking Spyro, initial overlap, repeated interception, close walls, 20-degree slopes, travelling rolls and 300/900/1,200 cm boundaries. The Artisans chase reached every route node during 45 seconds of play, stayed contained and preserved the starting center. The saved instance was reloaded to verify its overrides.

The lifecycle checks cover alert/facing, all eight original sound cue events, charge/flame hits, final slide, death cleanup and early/late checkpoint reset. These are scripted PIE checks with the actual Spyro and thief Blueprints; a manual gameplay review is still useful. Packaging and other platforms were not tested in this revision.

## Ice Cavern Snow Gnorc (11 October 2026)

The new `Snow_Gnorc_BP` uses the original class 198 guard/punch behavior, ten animations, eleven sound samples, charge resistance and one green gem. See [SNOW_GNORC.md](SNOW_GNORC.md) for placement, source details, Unreal adaptations and validation.

## Safe live route edits and optional Blueprint signals (3 October 2026)

Use `SetRoutePoints` to replace a thief route, or `SetRouteConfiguration` when changing both the route and its unit scale. Check the returned success flag and Error string. A route needs at least two finite, usable coordinates and a finite positive unit scale; repeated points remain allowed. Rejection leaves the current route unchanged. An accepted live replacement restarts route progress at node 0 without teleporting the thief or restarting its health, combat state, animation or spawn-relative origin.

Existing direct Blueprint writes remain supported. The next native update checks them and restores the last accepted configuration if they are invalid. `LastRouteError` records the latest rejection until a successful explicit setter or changed valid configuration clears it. Before play, `ValidateConfiguration` returns configuration errors, including missing required meshes, animations, shared components and body/sensor setup; `ValidateInEditor` prints them in the Output Log. Invalid initial configuration prevents this adapter from starting. Runtime route safety does not make an obstructed route navigable.

The Thief, Town Square, Sleeping Dog and Toasty behavior components expose the optional Blueprint-assignable `OnEnemySignal` dispatcher. Bind it from the owning Blueprint or encounter script, then switch on `Signal`; `Behavior` identifies the component and `Detail` has the meanings below. No listener is required for normal gameplay.

| Signal | Components | Detail |
| --- | --- | --- |
| AttackCommitted | Town Square, Sleeping Dog, Toasty | Native attack clip index |
| StageChanged | Toasty | Newly entered stage (0-based) |
| GuardsReleased | Toasty | Stage whose observed living guards have all been defeated |
| RecoveryStarted | Thief, Town Square | Recovery attempt count |
| RecoveryStarted | Toasty | Current stage when a supported detour is chosen |
| RouteChanged | Thief | Restarted route node (0) |
| ResetCompleted | All four | Reset stage for Toasty (0); otherwise 0 |

These notifications support Blueprint sound, VFX, camera, UI and encounter responses. Native code continues to own movement, pose evaluation, attack windows, damage and reset state. AttackCommitted is a wind-up/pounce transition, not proof of a landed hit: do not apply additional damage from it. RecoveryStarted does not guarantee successful escape. GuardsReleased observes a true-to-false living-guard transition within a stage; a stage that starts without living guards does not emit it. ResetCompleted means this native adapter reset has completed; the shared dropper's broader reset guard may still be active.

Dispatch occurs at the end of an authoritative component update, including paused reset/fear updates. A reset or destroyed owner cancels the remaining queued notices from the old lifecycle; newly queued callback notices wait for a subsequent update. Multiple listeners on the currently broadcasting notice still follow Unreal's multicast behavior. These are local notifications, not replicated network events. They do not replay history when a listener binds late or when component ticking is disabled. Reuse the existing shared damage/dropper contracts for hit and death notifications.

Validation for this patch: 59 native route/contact/event cases pass, including actual saved enemy Blueprints, physical wall filtering, bad live writes and Blueprint-compatible delegate reset/destruction. Existing Dog contact/color, Dog death and Toasty terrain suites also pass. Final editor/game compilation and fresh class checks are recorded in the external workshop handoff. These automated checks do not replace a controller playthrough or prove packaged launch behavior.
