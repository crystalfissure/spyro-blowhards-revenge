"""
Scan a drop folder of FBX + PNG files and write a manifest for
ue_asset_pipeline.py.

Runs on any Python 3 (stdlib only) - it does NOT need Unreal.

    python make_manifest.py <drop folder> --content-root /Game/MythsAwaken/Enemies
    python make_manifest.py <drop folder> -o manifests/enemies.json --preset ps1

Pairing rules
-------------
* Each `*.fbx` becomes one asset, named after the file stem.
* PNGs are matched to that asset when their stem equals the FBX stem, or
  starts with it followed by `_`/`-`/`.`, or when they sit alone beside the
  FBX in a per-asset subfolder.
* Texture slot comes from the PNG suffix (`_n`/`_normal` -> normal, and so on);
  anything unrecognised becomes base_color.

The manifest is meant to be edited afterwards - it is the label sheet.
"""

import argparse
import json
import os
import re
import sys

MESH_EXTENSIONS = (".fbx",)
TEXTURE_EXTENSIONS = (".png", ".tga", ".bmp", ".jpg", ".jpeg", ".psd", ".dds")

# Longest suffixes first so `_ao` never shadows `_normal` style matches.
SLOT_SUFFIXES = [
    ("ambient_occlusion", ("_ambientocclusion", "_occlusion", "_ao")),
    ("normal", ("_normal", "_norm", "_nrm", "_n")),
    ("emissive", ("_emissive", "_emission", "_emit", "_e", "_glow")),
    ("roughness", ("_roughness", "_rough", "_r")),
    ("metallic", ("_metallic", "_metal", "_m")),
    ("specular", ("_specular", "_spec", "_s")),
    ("opacity_mask", ("_mask", "_opacitymask")),
    ("opacity", ("_opacity", "_alpha", "_a")),
    ("base_color", ("_basecolor", "_albedo", "_diffuse", "_color", "_colour",
                    "_d", "_c", "_t", "_tex")),
]

PRESETS = {
    # Crisp, unfiltered, unlit-ish look for retro console rips.
    "ps1": {
        "texture": {
            "filter": "nearest",
            "mip_gen": "no_mipmaps",
            "compression": "uncompressed",
            "address": "wrap",
        },
        "material": {
            "mode": "new",
            "shading_model": "unlit",
            "blend_mode": "masked",
            "two_sided": True,
            "use_vertex_color": True,
            "vertex_color_scale": 2.0,
        },
        "mesh": {
            "mesh_type": "auto",
            "combine_meshes": True,
            "generate_lightmap_uvs": False,
            "auto_generate_collision": True,
            "vertex_color_import": "replace",
        },
    },
    # Ordinary lit PBR-ish import.
    "standard": {
        "texture": {
            "filter": "default",
            "mip_gen": "from_texture_group",
            "compression": "default",
            "address": "wrap",
        },
        "material": {
            "mode": "new",
            "shading_model": "default_lit",
            "blend_mode": "opaque",
            "two_sided": False,
            "use_vertex_color": False,
            "roughness": 0.8,
            "metallic": 0.0,
        },
        "mesh": {
            "mesh_type": "auto",
            "combine_meshes": True,
            "generate_lightmap_uvs": True,
            "auto_generate_collision": True,
            "vertex_color_import": "replace",
        },
    },
}


def sanitise(name):
    """UE-safe asset name: keep it recognisable, drop what UE would mangle."""
    cleaned = re.sub(r"[^A-Za-z0-9_]+", "_", name).strip("_")
    if not cleaned:
        cleaned = "Asset"
    if cleaned[0].isdigit():
        cleaned = "A_" + cleaned
    return cleaned


def slot_for(texture_stem, mesh_stem):
    """Infer the material slot from what the texture name adds to the mesh name."""
    lowered = texture_stem.lower()
    remainder = lowered
    if mesh_stem and lowered.startswith(mesh_stem.lower()):
        remainder = lowered[len(mesh_stem):]

    for slot, suffixes in SLOT_SUFFIXES:
        for suffix in suffixes:
            if remainder.endswith(suffix) or lowered.endswith(suffix):
                return slot
    return "base_color"


def find_files(folder):
    meshes, textures = [], []
    for root, dirs, names in os.walk(folder):
        dirs[:] = [d for d in dirs if not d.startswith(".")]
        for filename in sorted(names):
            ext = os.path.splitext(filename)[1].lower()
            path = os.path.join(root, filename)
            if ext in MESH_EXTENSIONS:
                meshes.append(path)
            elif ext in TEXTURE_EXTENSIONS:
                textures.append(path)
    return meshes, textures


def matches_by_name(texture_path, mesh_path):
    """True when the texture's filename names this mesh."""
    texture_stem = os.path.splitext(os.path.basename(texture_path))[0].lower()
    mesh_stem = os.path.splitext(os.path.basename(mesh_path))[0].lower()

    if texture_stem == mesh_stem:
        return True
    return (texture_stem.startswith(mesh_stem)
            and texture_stem[len(mesh_stem):len(mesh_stem) + 1] in ("_", "-", "."))


# Base colour first so the generated material graph reads top-down.
SLOT_PRIORITY = {"base_color": 0}
for _index, (_slot, _) in enumerate(SLOT_SUFFIXES, start=1):
    SLOT_PRIORITY.setdefault(_slot, _index)


def check_content_root(content_root):
    """Git Bash rewrites a bare /Game/... argument into a Windows path."""
    if re.match(r"^[A-Za-z]:[\\/]", content_root) or content_root.startswith("\\\\"):
        raise SystemExit(
            "--content-root looks like a filesystem path: %s\n"
            "Git Bash/MSYS rewrites arguments that start with '/'. Either run this\n"
            "from PowerShell or cmd, or prefix the command with MSYS_NO_PATHCONV=1."
            % content_root)
    if not content_root.startswith("/"):
        raise SystemExit("--content-root must be a UE package path starting with '/', "
                         "e.g. /Game/MythsAwaken/Enemies (got %r)" % content_root)


def build(folder, content_root, preset, flatten):
    folder = os.path.abspath(folder)
    check_content_root(content_root)
    meshes, textures = find_files(folder)
    if not meshes:
        print("no .fbx found under %s" % folder, file=sys.stderr)

    defaults = json.loads(json.dumps(PRESETS[preset]))  # deep copy

    # Directories holding exactly one mesh: there, an unnamed texture can only
    # belong to that mesh. Directories with several meshes get no such fallback,
    # otherwise the first mesh would swallow every sibling's textures.
    mesh_count = {}
    for mesh_path in meshes:
        directory = os.path.dirname(mesh_path)
        mesh_count[directory] = mesh_count.get(directory, 0) + 1
    sole_mesh_dirs = {d for d, count in mesh_count.items() if count == 1}

    # Pass 1: filename matches. Pass 2: sole-mesh-directory fallback for
    # whatever is still unclaimed.
    claimed = {}
    for mesh_path in meshes:
        for texture_path in textures:
            if texture_path not in claimed and matches_by_name(texture_path, mesh_path):
                claimed[texture_path] = mesh_path
    for mesh_path in meshes:
        if os.path.dirname(mesh_path) not in sole_mesh_dirs:
            continue
        for texture_path in textures:
            if texture_path not in claimed and os.path.dirname(texture_path) == os.path.dirname(mesh_path):
                claimed[texture_path] = mesh_path

    assets = []
    for mesh_path in meshes:
        mesh_stem = os.path.splitext(os.path.basename(mesh_path))[0]
        name = sanitise(mesh_stem)
        relative_mesh = os.path.relpath(mesh_path, folder).replace("\\", "/")

        if flatten:
            parts = [name]
        else:
            sub = os.path.dirname(relative_mesh)
            parts = [sanitise(p) for p in sub.split("/") if p] + [name]
        dest = "/".join([content_root.rstrip("/")] + parts)

        entry_textures = []
        for texture_path in textures:
            if claimed.get(texture_path) is not mesh_path:
                continue
            texture_stem = os.path.splitext(os.path.basename(texture_path))[0]
            entry_textures.append({
                "file": os.path.relpath(texture_path, folder).replace("\\", "/"),
                "slot": slot_for(texture_stem, mesh_stem),
            })
        entry_textures.sort(key=lambda t: SLOT_PRIORITY.get(t["slot"], 99))

        assets.append({
            "name": name,
            "dest": dest,
            "mesh": relative_mesh,
            "textures": entry_textures,
            "material": {"name": "%s_Mat" % name},
        })

    orphans = [os.path.relpath(t, folder).replace("\\", "/")
               for t in textures if t not in claimed]

    manifest = {
        "source_root": folder,
        "content_root": content_root,
        "replace_existing": True,
        "defaults": defaults,
        "assets": assets,
    }
    if orphans:
        manifest["_unmatched_textures"] = orphans
    return manifest


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("folder", help="folder containing the FBX/PNG files")
    parser.add_argument("-o", "--output", help="manifest path (default: <folder>/manifest.json)")
    parser.add_argument("--content-root", default="/Game/Imported",
                        help="UE package path the assets land under")
    parser.add_argument("--preset", choices=sorted(PRESETS), default="ps1",
                        help="default texture/material/mesh settings (default: ps1)")
    parser.add_argument("--flatten", action="store_true",
                        help="ignore subfolder structure; put every asset directly under --content-root")
    args = parser.parse_args(argv)

    manifest = build(args.folder, args.content_root, args.preset, args.flatten)

    output = args.output or os.path.join(os.path.abspath(args.folder), "manifest.json")
    output = os.path.abspath(output)
    os.makedirs(os.path.dirname(output), exist_ok=True)
    with open(output, "w", encoding="utf-8") as handle:
        json.dump(manifest, handle, indent=2)

    total_textures = sum(len(a["textures"]) for a in manifest["assets"])
    print("%d asset(s), %d texture(s) -> %s" % (
        len(manifest["assets"]), total_textures, output))
    for asset in manifest["assets"]:
        slots = ", ".join(t["slot"] for t in asset["textures"]) or "no textures"
        print("  %-32s %s  [%s]" % (asset["name"], asset["dest"], slots))
    for orphan in manifest.get("_unmatched_textures", []):
        print("  unmatched texture: %s" % orphan)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
