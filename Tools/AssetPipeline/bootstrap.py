"""
One-time setup: turn on what the import pipeline needs in this project.

    python bootstrap.py            # show what would change
    python bootstrap.py --apply    # write the changes

It enables the Python Editor Script Plugin in the .uproject (the engine ships
it, this project just never switched it on) and makes sure remote execution is
enabled in Config/DefaultEngine.ini. Both files are backed up first.

Restart the editor afterwards for the plugin change to take effect.
"""

import argparse
import json
import os
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))


def _project_dir():
    """Nearest ancestor holding a .uproject (this lives in <project>/Tools/AssetPipeline)."""
    directory = HERE
    while True:
        if any(n.lower().endswith(".uproject") for n in os.listdir(directory)):
            return directory
        parent = os.path.dirname(directory)
        if parent == directory:
            raise SystemExit("no .uproject found above %s" % HERE)
        directory = parent


PROJECT_DIR = _project_dir()

REQUIRED_PLUGINS = [
    "PythonScriptPlugin",        # the `unreal` module + remote execution
    "EditorScriptingUtilities",  # EditorAssetLibrary and friends
]


def find_uproject():
    for name in sorted(os.listdir(PROJECT_DIR)):
        if name.lower().endswith(".uproject"):
            return os.path.join(PROJECT_DIR, name)
    raise SystemExit("no .uproject found in %s" % PROJECT_DIR)


def backup(path):
    target = path + ".bak"
    if not os.path.exists(target):
        shutil.copy2(path, target)
        print("  backed up -> %s" % os.path.basename(target))


def fix_uproject(path, apply_changes):
    with open(path, "r", encoding="utf-8") as handle:
        data = json.load(handle)

    plugins = data.setdefault("Plugins", [])
    by_name = {p.get("Name"): p for p in plugins}
    changes = []

    for name in REQUIRED_PLUGINS:
        existing = by_name.get(name)
        if existing is None:
            changes.append("enable %s (not listed)" % name)
            if apply_changes:
                plugins.append({"Name": name, "Enabled": True})
        elif not existing.get("Enabled", False):
            changes.append("enable %s (currently disabled)" % name)
            if apply_changes:
                existing["Enabled"] = True

    if changes and apply_changes:
        backup(path)
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(data, handle, indent="\t")
            handle.write("\n")
    return changes


def fix_engine_ini(path, apply_changes):
    section = "[/Script/PythonScriptPlugin.PythonScriptPluginSettings]"
    with open(path, "r", encoding="utf-8-sig") as handle:
        lines = handle.read().splitlines()

    changes = []
    try:
        start = lines.index(section)
    except ValueError:
        changes.append("add %s with bRemoteExecution=True" % section)
        if apply_changes:
            backup(path)
            lines += ["", section, "bRemoteExecution=True"]
            with open(path, "w", encoding="utf-8-sig") as handle:
                handle.write("\n".join(lines) + "\n")
        return changes

    end = len(lines)
    for index in range(start + 1, len(lines)):
        if lines[index].startswith("["):
            end = index
            break

    body = lines[start + 1:end]
    if not any(line.strip().lower().startswith("bremoteexecution=true") for line in body):
        changes.append("set bRemoteExecution=True")
        if apply_changes:
            backup(path)
            kept = [line for line in body
                    if not line.strip().lower().startswith("bremoteexecution=")]
            lines[start + 1:end] = ["bRemoteExecution=True"] + kept
            with open(path, "w", encoding="utf-8-sig") as handle:
                handle.write("\n".join(lines) + "\n")
    return changes


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--apply", action="store_true", help="write the changes")
    args = parser.parse_args(argv)

    uproject = find_uproject()
    engine_ini = os.path.join(PROJECT_DIR, "Config", "DefaultEngine.ini")

    print("project: %s" % uproject)
    changes = fix_uproject(uproject, args.apply)
    for change in changes:
        print("  uproject: %s" % change)

    if os.path.isfile(engine_ini):
        ini_changes = fix_engine_ini(engine_ini, args.apply)
        changes += ini_changes
        for change in ini_changes:
            print("  DefaultEngine.ini: %s" % change)
    else:
        print("  DefaultEngine.ini not found - skipped")

    if not changes:
        print("\nalready set up, nothing to do.")
    elif args.apply:
        print("\napplied. Restart the Unreal editor to pick up the plugin change.")
    else:
        print("\ndry run - re-run with --apply to write these changes.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
