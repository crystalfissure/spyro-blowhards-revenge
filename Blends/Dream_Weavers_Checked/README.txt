Dream Weavers Edit — checked Blender/FBX and repaired Unreal asset

The existing Unreal asset was repaired in place:
  /Game/_CF_Project/Meshes/Levels/Dream_Weavers_Edit
Its material now uses the supplied extended atlas, and Full Precision UVs is on.
A separate Dream_Weavers_Edit_Checked asset is also available in the same folder.
The original shared V2.7 texture asset was not changed.

The missing-texture/import problems identified were:
- Unreal's material was using original V2.7 instead of the extended atlas. The
  added original-texture patches therefore sampled empty or unrelated pixels.
- Half-precision UVs could move atlas coordinates by up to two pixels.
- Six coincident triangles were discarded in an FBX round-trip. Several have
  different UV mappings on their opposite sides. Their vertices are now split
  so both sides and mappings survive.

Geometry and normals:
- Explicit Blender triangulation preserves the intended tessellation for Unreal.
- Eleven triangles with zero world-space area at the file's precision were
  removed. The resulting mesh contains 29,667 triangles.
- Flat normals were generated from the explicit triangles and verified finite.
- Face winding, including the user's edits, was retained. The visible reverse
  faces around the cave include inward-facing ceilings and cliff surfaces.
  A blanket Recalculate Outside would incorrectly turn these surfaces around.
- Original open edges/non-manifold junctions were not welded or filled. This is
  an open game level, not a watertight solid; an edge-consistency audit alone
  cannot determine intended visibility for every sheet.
- UVs and vertex colours were copied per corner. Collapsed UV mappings that
  intentionally sample a point or line were retained.

Validation:
- FBX exported and imported back into Blender: 89,001 UV corners, zero UV error;
  minimum face-normal agreement dot product 0.99999978.
- Saved Unreal assets reopened: all 29,667 triangles / 89,001 UV corners present;
  UV coordinates match Blender's coordinate set at float32 precision.
- Existing Unreal bounds preserved within 0.006 cm. The current Blender source
  was smaller than the previously imported mesh, so a 1.22 build scale preserves
  the level's existing size. The checked FBX is registered as the reimport source.
- Existing collision configuration and convex hull count preserved.
- No interactive Unreal viewport visual comparison was performed; verification
  used its saved mesh buffers, settings, texture assignments, and Blender renders.

Files:
  Dream Weavers Edit - Checked.blend : editable model with packed atlas
  Dream Weavers Edit - Checked.fbx   : verified Unreal export
  Spyro_Giga_Texture_Atlas_V2.7_Dream_Weavers_Extended.png : correct atlas
  verification.json / unreal_verification.json : detailed check results
  Unreal_Backup/ : original mesh and material .uassets from before repair

The Downloads source .blend was left untouched. Keep the atlas PNG beside the
checked blend. In Unreal, use nearest filtering and the current no-mip setting
for the intended pixel-art sampling. Preserve Full Precision UVs on reimport.
