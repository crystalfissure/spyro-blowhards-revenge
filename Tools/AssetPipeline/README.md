# Asset import pipeline (UE 4.27)

Drop a folder of FBX + PNG files, get meshes, textures, materials and material
assignments in the project. Batch-driven, repeatable, no clicking through the
FBX import dialog.

| File | What it is |
| --- | --- |
| `bootstrap.py` | One-time setup. Enables the Python plugin in the `.uproject`. |
| `make_manifest.py` | Scans a drop folder, writes a manifest. Plain Python, no Unreal. |
| `ue_asset_pipeline.py` | The importer. Runs inside Unreal. |
| `send_to_editor.py` | Pushes a manifest into a **running** editor. The fast path. |
| `Import.bat` | Runs a manifest headless, editor **closed**. |

## First time

```bash
python Tools/AssetPipeline/bootstrap.py --apply
```

Then restart the editor. This enables `PythonScriptPlugin`; `.uproject` and
`DefaultEngine.ini` are backed up to `.bak` first.

## Everyday use

**1. Scan the folder**

```bash
python Tools/AssetPipeline/make_manifest.py "D:\drop\enemies" --content-root /Game/MythsAwaken/Enemies
```

Writes `manifest.json` in the drop folder and prints what it paired up. Run it
from PowerShell or cmd — Git Bash rewrites `/Game/...` into a Windows path
(the script catches this and says so).

**2. Edit the manifest** — this is the label sheet. Rename assets, retarget
destinations, set material parameters.

**3. Import**

Editor open (seconds, assets appear live):

```bash
python Tools/AssetPipeline/send_to_editor.py "D:\drop\enemies\manifest.json"
```

Editor closed (full commandlet startup, a few minutes on this project):

```bash
Tools\AssetPipeline\Import.bat "D:\drop\enemies\manifest.json"
```

Add `--dry-run` to either to see the plan without writing anything.

## How files get paired

* Every `*.fbx` becomes one asset, named after the file stem.
* A PNG is attached when its name matches the FBX stem (`Vase.fbx` +
  `Vase_normal.png`), **or** when it sits in a subfolder that contains exactly
  one FBX. A folder with several FBXs gets no such fallback, so meshes never
  steal each other's textures.
* Slot comes from the suffix: `_n`/`_normal`, `_ao`, `_e`/`_emissive`,
  `_r`/`_rough`, `_m`/`_metal`, `_s`/`_spec`, `_mask`, `_a`/`_alpha`.
  Anything else is base colour.
* Textures nothing claimed are listed under `_unmatched_textures` in the
  manifest rather than silently dropped.
* Static vs skeletal is auto-detected by looking for a skin deformer in the
  FBX. Override per asset with `mesh_settings.mesh_type`.

## Manifest format

```jsonc
{
  "source_root": "D:/drop/enemies",       // relative paths resolve against this
  "content_root": "/Game/MythsAwaken/Enemies",
  "replace_existing": true,               // false = keep assets already in the project
  "report": "import_report.json",         // optional, written next to source_root

  "defaults": {                           // applied to every asset, overridable per asset
    "texture":  { "filter": "nearest", "mip_gen": "no_mipmaps",
                  "compression": "uncompressed", "address": "wrap" },
    "mesh":     { "mesh_type": "auto", "combine_meshes": true,
                  "generate_lightmap_uvs": false, "auto_generate_collision": true },
    "material": { "mode": "new", "shading_model": "unlit", "blend_mode": "masked",
                  "two_sided": true, "use_vertex_color": true, "vertex_color_scale": 2.0 }
  },

  "assets": [
    {
      "name": "Tin_Guard",
      "dest": "/Game/MythsAwaken/Enemies/Tin_Guard",
      "mesh": "Tin_Guard/Tin_Guard.fbx",
      "textures": [
        { "file": "Tin_Guard/Tin_Guard.png",   "slot": "base_color" },
        { "file": "Tin_Guard/Tin_Guard_n.png", "slot": "normal" }
      ],
      "material": { "name": "Tin_Guard_Mat", "two_sided": true }
    }
  ]
}
```

### Material modes

`"mode": "new"` — build a Material from the textures.

```jsonc
"material": {
  "mode": "new",
  "name": "Tin_Guard_Mat",
  "shading_model": "unlit",          // default_lit | unlit | subsurface | two_sided_foliage
  "blend_mode": "masked",            // opaque | masked | translucent | additive | modulate
  "opacity_mask_clip_value": 0.333,
  "two_sided": true,
  "use_vertex_color": true,          // multiply base colour by vertex colour
  "vertex_color_scale": 2.0,         // PS1 rips usually want 2x
  "roughness": 0.8, "metallic": 0.0  // constants, only where no texture drives the slot
}
```

On an unlit material the colour chain goes to Emissive, since Unlit has no
Base Color input. Base-colour alpha drives the opacity mask on `masked` (or
Opacity on the translucent modes) unless a dedicated opacity texture is given.

`"mode": "instance"` — a Material Instance of one of the project's masters.
This is the one to use for anything that should stay consistent with existing
content, e.g. `/Game/OT_Ports/Global/Materials/M_PS1_Spyro_Metal`.

```jsonc
"material": {
  "mode": "instance",
  "name": "MI_Tin_Guard",
  "parent": "/Game/OT_Ports/Global/Materials/M_PS1_Spyro_Metal",
  "scalars": { "Roughness": 0.4 },
  "vectors": { "Tint": [1.0, 0.9, 0.8, 1.0] },
  "switches": { "UseVertexColor": true },
  "two_sided": true
}
```

Texture parameter names come from the `param` field on each texture entry:

```jsonc
"textures": [ { "file": "tin.png", "slot": "base_color", "param": "BaseTexture" } ]
```

With no `param`, the base-colour texture goes to `BaseTexture`; change that
with `"base_color_param": "Diffuse"`.

`"mode": "existing"` — assign a material that already exists, import nothing:
`{ "mode": "existing", "asset": "/Game/.../Some_Mat" }`.

`"mode": "none"` — import mesh and textures, wire up no material.

By default the material lands on every slot of the mesh. Restrict it with
`"slots": [0, 2]`.

### Texture settings

`filter`: `nearest` | `bilinear` | `trilinear` | `default`
`mip_gen`: `no_mipmaps` | `from_texture_group` | `simple_average` | `sharpen`
`compression`: `uncompressed` (RGBA8, no DXT blocking — right for small
retro textures) | `default` | `normalmap` | `masks` | `grayscale` | `alpha` | `bc7`
`address`: `wrap` | `clamp` | `mirror`
plus `srgb`, `flip_green_channel`, `never_stream`, `lod_group`.

sRGB defaults to on for base colour and emissive, off for everything else.

### Mesh settings

`mesh_type` (`auto`/`static`/`skeletal`), `import_uniform_scale`,
`import_rotation`, `import_translation`, `combine_meshes`,
`generate_lightmap_uvs`, `auto_generate_collision`, `vertex_color_import`
(`replace`/`ignore`/`override`), `normal_import_method`, `convert_scene`,
`force_front_x_axis`, and for skeletal meshes `skeleton` (package path of an
existing skeleton to bind to), `import_animations`, `import_morph_targets`.

## Presets

`make_manifest.py --preset ps1` (default) — nearest filter, no mipmaps,
uncompressed, unlit, masked, two-sided, vertex colours at 2x. Matches how the
retro rips in this project are meant to look.

`--preset standard` — ordinary lit import with mipmaps and lightmap UVs.

## Notes

* Re-running a manifest overwrites the same assets in place, so fixing a
  texture and re-importing is safe. Set `"replace_existing": false` to keep
  whatever is already in the project.
* `Import.bat` refuses to run while `UE4Editor.exe` is open — a headless
  commandlet and the editor would fight over the same package files.
* Everything the pipeline logs is prefixed `[assetpipe]` in the Unreal log.
* Static switch parameters on material instances are not exposed to Python in
  4.27; the importer warns and names the ones you need to set by hand.
* This project's path contains a space ("Spyro Fangame Engine"), and UE 4.27
  resolves `-script=` by splitting at the first space — so `Import.bat` stages
  a copy of the importer under `%TEMP%\UEAssetPipeline` and passes that. Edit
  `ue_asset_pipeline.py` here; the staged copy is refreshed on every run.
* `send_to_editor.py` works on any Python 3. Epic's `remote_execution.py` uses
  a `json.loads(encoding=...)` argument that Python 3.9 removed; the script
  patches around it at import time rather than editing the engine file.

