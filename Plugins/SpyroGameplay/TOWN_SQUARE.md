# Bull and Toreador

UE 4.27.2 assets:

- `/Game/OT_Ports/S1/S1_Enemies/Home_00_Artisans/03_TownSquare/Bull_BP`
- `/Game/OT_Ports/S1/S1_Enemies/Home_00_Artisans/03_TownSquare/Toreador_BP`

Both derive from the project's `Base_Enemy_BP`. Their behavior and animation classes are in `SpyroGameplay`; `SpyroEditor` supplies configuration and isolated test helpers. All ten source animation takes per enemy, textures, skeletons, materials and nine original sound samples are saved game assets. Runtime needs neither the research folder nor Blender, Python, an emulator or the original disc. Use the repository's UE 4.27.2 Windows editor binaries, or rebuild the included source for another engine/toolchain. The normal project and shared Spyro dependencies still apply.

## Placement

Place either Blueprint with its capsule resting on blocking ground (root approximately 92 cm above a horizontal floor at default scale). They work independently. To create a pair, select the Toreador's **ToreadorBehavior** component and explicitly assign **Town Square > Pair > Linked Bull** to the placed Bull. Leave it empty for standalone cape combat. There is no nearest-enemy pairing. Each Bull accepts one Toreador; `Pair Conflict` and `Validate Placement` expose invalid assignments. Destroying a partner clears its reciprocal runtime link. Defeating one changes the surviving member's behavior without deleting the authored assignment, so checkpoint reset can restore the pair.

For a pair, the Toreador owns the shared territory, route and route orientation. Set its **Roam Radius**, **Route Points** and **Route Yaw**. The Bull uses those settings while linked. For a standalone enemy, its own starting transform owns the territory. Radius defaults to 1,800 cm and cannot be smaller than 300 cm. The center stays at the starting position; it does not follow the enemy. Place both members inside the circle with body clearance. Changing the radius cannot make an already invalid starting placement valid.

Route points are local coordinates in original-game units, converted by `0.146104` cm per unit, rotated by the territory owner's placed yaw plus Route Yaw. The default three points reproduce the first Town Square pair's route. Routes are uniformly reduced when necessary, reserving body and turning clearance; movement speed and animation timing remain unchanged. Z follows blocking terrain. The selected editor preview shows territory and the fitted route; **Draw Movement Debug** adds runtime route, body and obstruction information. Preview terrain height is schematic. Reselect/reconstruct actors after changing another actor's pairing settings to refresh their previews.

No NavMesh is needed. Author routes around obstacles and avoid narrow dead ends. Sweeps, bounded sliding, ground support probes and a circular boundary constrain movement; repeated obstruction reverses route direction. These safeguards do not provide general pathfinding. The default placement scale is calibrated independently for each model; arbitrary nonuniform actor scaling is not a supported placement workflow.

### Artisans placement review

The existing user-placed Artisans pair has a valid explicit link. Its revised setup keeps the 1,800 cm territory and existing yaw settings, places the Bull at XY `(6500, 9000)` and the Toreador at `(6900, 9400)`, and uses a four-point circuit around the spring chest. The route's world XY points are `(6750, 9650)`, `(6000, 9400)`, `(6200, 8700)`, then the Toreador's home `(6900, 9400)`. Both starting heights are projected onto the actual ground. Edit the Toreador's route for this pair; the Bull's own route override is unused while linked.

A 40-second unsaved candidate test reached all four route points for both enemies with no obstruction reversals or sampled requested-motion stalls, at full route scale. The prior saved layout repeatedly reversed and fitted its route to approximately 89%. The revision moves only the two enemies and their attached shadows and changes the Toreador's route; it does not move the chest or other independent actors.

Both child Blueprints must use their skeletal meshes' material slots. `Base_Enemy_BP` supplies an inherited untextured slot-1 override which obscured the face textures despite correct mesh previews. That override is cleared on these two Blueprints, and the configuration helper now clears inherited mesh overrides when configuring a Town Square enemy. The mesh's current body material remains intact.

## Combat and reset

The behavior uses the existing Damageable, Drops Items, player damage and checkpoint contracts. Bull charge immediately awards its one reward and transitions through inversion to horns-stuck behavior. Repeated charge cannot award another gem. Flame removes an active or stuck Bull; flame after charge does not pay again. Toreador charge and flame both use its death reaction and one reward. The default reward is one green gem, matching the first reference pair; change the inherited dropper's **Items to Drop** first entry to choose a different gem value. Additional array entries are not emitted by these one-reward enemies.

An incapacitated Bull no longer drives the Toreador's flight; the Toreador can use its cape attack. A Bull without a live Toreador uses standalone pursuit. Root physics launches and the inherited generic walking/attack controller are disabled only on these actors. Frame-driven sounds stop during reset/cleanup. Checkpoint reset restores health, collision, starting transforms, animation state, reward guards and pair behavior; gem identity and permanent collection filtering remain with the shared project dropper.

## Reference basis and fidelity limits

The reference is the local NTSC Spyro 1 Town Square disc data and the public decompilation at commit `46b51d3a5c34dd2585b4fe1de74ac9b74c76390d`. All 9,849 assembly words in the consulted level update listing matched that disc's overlay. Model classes are Bull 23 and Toreador 395. Independently decoded model scales are 2 and 4. Animation counts, original per-update interpolation, sound tables/pitches and the cape collision-active frames (5–7 of Anim8) were inspected directly. The implementation advances at 30 Hz and interpolates presentation between updates.

This is a source-informed UE port, not a verified bit-identical emulation. Authored waypoint steering, circular containment, floor support, capsule/box collision and attack range/cones adapt the behavior to UE terrain and Spyro's existing collision contracts. The original route helper, exact collision volumes, SPU reverb/mixing, random state timing and every branch of the original state machine have not been proven equivalent. No synchronized emulator/gameplay comparison or listening review has been completed. Unused imported takes are retained rather than forced into the state machine. Multiplayer behavior is not validated.

The disposable `Bull_Toreador_Test` map, source audit, scripts, logs and screenshots are outside the game repository in `C:\Users\adace\Desktop\Scripts\bull_toreador_research`. See that folder's README and reports for test scope and results. The initial addition did not place enemies in production maps; the subsequent Artisans review adjusts the user's existing pair as described above.

## Validation

UE editor and runtime sources compile. Both enemy Blueprints and the unchanged thief Blueprint compile. All 167 final scripted assertions pass: 30 lifecycle checks at each of 20/30/60 FPS, 13 charge/contact/territory checks, four native combat/pair-conflict checks, nine focused terrain checks and 51 existing thief regression checks. During uninterrupted motion the measured simulation count stays within one tick of 30 Hz at each tested frame rate. The wall, unsupported ledge and 20-degree slope fixtures use the actual Spyro and enemy assets.

The WindowsNoEditor cook succeeds with zero errors and 80 warnings, producing all 42 Town Square asset packages. Shared dependencies pull in existing project content; warnings include missing legacy ExampleAdventure damage components, a missing Sunrise Spring map, Beast Makers skybox references, MLSDK and invalid trimesh data. Those unrelated assets were not repaired. A packaged executable and a full collected-gem save/reload cycle were not tested. The original-game fidelity limits above still apply even where integration assertions pass.
