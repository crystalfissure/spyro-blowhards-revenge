---
name: unreal-prop-import
description: Import or repair static and animated scenery props from asset dumps in Unreal, including skeleton compatibility, reimport settings, collision, and hit-triggered animation. Includes tested UE 4.27 and Spyro project guidance. Use for props such as lanterns, chests, foliage, and moving scenery; not whole-level conversion or full character AI implementation.
---

# Unreal prop import

Produce the requested usable prop, with recorded import settings and evidence that its geometry, animation, and interaction work. Importing an FBX does not import its gameplay logic. A prop described as "static" may merely be motionless at rest.

## Choose the smallest correct route

| Source and intended behaviour | Route |
| --- | --- |
| Rigid geometry, no deformation | Static Mesh; use an actor/component transform or Timeline if it moves as a whole |
| Skinned geometry or authored bone animation | Skeletal Mesh with its animation clips; keep it at rest when inactive |
| Existing correct mesh, missing clips | Animation-only import against the verified existing skeleton |
| Several rigid parts with transform animation | Preserve part hierarchy/pivots; determine whether to use components, a sequence, or a skeletal representation |
| Bone data absent but deformation is expected | Investigate alternate dump files/vertex animation; do not invent a rig or claim animation survived |

Do not convert every prop to a skeletal mesh or create native code just because the lantern used those approaches.

## Repeatable workflow

1. **Locate and inspect.** Confirm the actual project/version and working tree. Search nearby asset dumps and existing assets before requesting a link. Record source file, mesh class, existing material/skeleton references, hierarchy, units, clips and durations. Binary strings are discovery hints, not proof of Blueprint execution or playable motion. Preview or evaluate source animations when behaviour is uncertain.
2. **Identify the scope.** Distinguish import-only, a usable actor, and level placement. Continue already authorized work without extra approval gates; do not silently expand import work into replacing shared skeletons or populating maps. Keep unknown behaviour explicitly unresolved rather than choosing destruction/loot by analogy to a chest.
3. **Import with explicit settings.** Read [UE 4.27 import and reimport](references/ue427-import.md) for this engine. Use an isolated destination for unfamiliar geometry or transform corrections. Reuse existing assets when compatible. Record asset-specific corrections; never apply the lantern's rotation to unrelated props. Verify the resulting asset classes and complete clip set before saving the intended packages.
4. **Connect gameplay when requested.** Read [Spyro interaction contract](references/spyro-interactions.md) in the Spyro project. Else inspect the target project's actual attack/collision mechanism. Define rest, trigger, repeat-hit behaviour, completion, and reset. Animation playback alone is not attack integration.
5. **Verify the requested outcome.** Check geometry/materials, skeleton identity, evaluated animation, collision, Blueprint compilation and runtime behaviour. A warning is neither automatic failure nor proof of harmlessness. Test the invariant implicated by it. Compare before/after references and do not save unrelated dirty packages.
6. **Leave a usable result.** Record the asset path, exact settings, clip mapping, runtime dependencies, test result and any unperformed placement/manual checks using [the handoff record](references/handoff-record.md). If work stops on a real blocker, name the remaining work; do not call a staging import a completed actor.

## Important decisions

- Determine animation roles from inspected motion, not `Anim0`/`Anim1` labels alone. A one-frame pose is valid. Blender and Unreal can report different sampled clip lengths; use the imported sequence's runtime duration.
- Compare bone names, parent relationships and reference transforms. An exporter root can make the Unreal bone count differ from the source joint count. Matching counts alone do not establish compatibility.
- Root position, orientation and scale must match the intended representation. Check start, middle and end; inspect deformation and return-to-rest continuity, not just the root. Test root motion only when the asset is intended to have it.
- Reimport changes may be ignored if cached asset import settings are retained. Verify the saved settings and resulting animation, not just the requested options.
- A successful process exit is not sufficient: Unreal automation can return zero while a test reports `Failed`. Require the named test's actual result and inspect script/import reports. A crash may occur after assets were saved; inventory the destination before retrying.
- Reuse the project's import scripts when their capabilities match the task. Do not treat a mesh importer as an animation-only importer, or silently accept a missing required skeleton and create a replacement.

## Local entry points

In the Spyro repository, start with `Tools/AssetPipeline/README.md`. The import helper, manifest workflow, and specific lantern scripts are described in the references. Discover current absolute paths; do not assume a prior machine's drive layout.

The repository copy of this skill lives at `Tools/AssetPipeline/skills/unreal-prop-import`. When updating it, keep the installed personal copy synchronized. Automatic selection remains enabled; explicit invocation is `$unreal-prop-import`.
