"""
Push an import batch into an ALREADY-RUNNING UE 4.27 editor.

This is the fast path: no editor startup cost, and the assets appear in the
content browser as they land. It talks to the editor over the Python plugin's
remote-execution channel (UDP multicast), which this project already enables
via `bRemoteExecution=True` in Config/DefaultEngine.ini.

    python send_to_editor.py <manifest.json>
    python send_to_editor.py <manifest.json> --dry-run
    python send_to_editor.py --ping

Requirements in the editor: Edit > Plugins > "Python Editor Script Plugin"
enabled, and Project Settings > Plugins > Python > "Enable Remote Execution"
ticked. Run bootstrap.py once to set both.

If no editor is listening, use Import.bat instead (headless commandlet).
"""

import argparse
import json
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_ENGINE = r"C:\Unreal Engine\UE_4.27"


def _patch_json_for_modern_python():
    """Epic's remote_execution.py was written for Python 3.7.

    It calls json.loads(..., encoding=...), and that keyword was removed in
    Python 3.9 - every message then fails to deserialize and the editor looks
    like it never answered. Dropping the dead keyword keeps the engine file
    untouched and costs nothing on 3.7.
    """
    import json

    if sys.version_info < (3, 9):
        return

    original = json.loads

    def loads(*args, **kwargs):
        kwargs.pop("encoding", None)
        return original(*args, **kwargs)

    json.loads = loads


def load_remote_execution(engine_dir):
    module_dir = os.path.join(
        engine_dir, "Engine", "Plugins", "Experimental", "PythonScriptPlugin",
        "Content", "Python")
    candidate = os.path.join(module_dir, "remote_execution.py")
    if not os.path.isfile(candidate):
        raise SystemExit(
            "remote_execution.py not found at %s\n"
            "Point --engine at your UE 4.27 install." % candidate)
    _patch_json_for_modern_python()
    sys.path.insert(0, module_dir)
    import remote_execution  # noqa: E402
    return remote_execution


def connect(remote, timeout):
    execution = remote.RemoteExecution(remote.RemoteExecutionConfig())
    execution.start()

    deadline = time.time() + timeout
    while time.time() < deadline:
        if execution.remote_nodes:
            break
        time.sleep(0.25)
    else:
        execution.stop()
        raise SystemExit(
            "no Unreal editor answered on the remote-execution channel "
            "within %ss.\n"
            "  * is the editor open with this project?\n"
            "  * is 'Python Editor Script Plugin' enabled?\n"
            "  * is Project Settings > Python > Enable Remote Execution ticked?\n"
            "  * otherwise use Import.bat (headless)." % timeout)

    node = execution.remote_nodes[0]
    node_id = node["node_id"] if isinstance(node, dict) else node.node_id
    execution.open_command_connection(node_id)
    return execution, node_id


def build_command(manifest_path, dry_run):
    """Python executed inside the editor: load the module fresh, then run."""
    return "\n".join([
        "import sys, importlib",
        "_dir = %r" % HERE,
        "sys.path.insert(0, _dir) if _dir not in sys.path else None",
        "import ue_asset_pipeline",
        "importlib.reload(ue_asset_pipeline)",
        "ue_asset_pipeline.run(%r, dry_run=%r)" % (manifest_path, bool(dry_run)),
    ])


def report(result):
    if not isinstance(result, dict):
        print(result)
        return 0

    for entry in result.get("output") or []:
        text = entry.get("output", "")
        if entry.get("type") == "Error":
            print("ERROR  %s" % text, file=sys.stderr)
        elif entry.get("type") == "Warning":
            print("WARN   %s" % text)
        else:
            print(text)

    if not result.get("success", False):
        print("\nremote command reported failure", file=sys.stderr)
        return 1
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("manifest", nargs="?", help="manifest .json to import")
    parser.add_argument("--dry-run", action="store_true",
                        help="report what would be imported, touch nothing")
    parser.add_argument("--ping", action="store_true",
                        help="just check that an editor is listening")
    parser.add_argument("--engine", default=os.environ.get("UE_ENGINE_DIR", DEFAULT_ENGINE),
                        help="UE 4.27 install root")
    parser.add_argument("--timeout", type=float, default=10.0,
                        help="seconds to wait for an editor to answer")
    args = parser.parse_args(argv)

    if not args.ping and not args.manifest:
        parser.error("give a manifest, or --ping")

    remote = load_remote_execution(args.engine)
    execution, node_id = connect(remote, args.timeout)

    try:
        if args.ping:
            print("editor listening: %s" % node_id)
            result = execution.run_command(
                "import unreal; print(unreal.SystemLibrary.get_project_directory())",
                unattended=True, exec_mode=remote.MODE_EXEC_FILE, raise_on_failure=False)
            return report(result)

        manifest_path = os.path.abspath(args.manifest)
        if not os.path.isfile(manifest_path):
            raise SystemExit("manifest not found: %s" % manifest_path)
        with open(manifest_path, "r", encoding="utf-8") as handle:
            manifest = json.load(handle)

        print("editor: %s" % node_id)
        print("importing %d asset(s) from %s%s" % (
            len(manifest.get("assets") or []), manifest_path,
            "  [DRY RUN]" if args.dry_run else ""))

        result = execution.run_command(
            build_command(manifest_path, args.dry_run),
            unattended=True, exec_mode=remote.MODE_EXEC_FILE, raise_on_failure=False)
        return report(result)
    finally:
        try:
            execution.stop()
        except Exception:
            pass


if __name__ == "__main__":
    raise SystemExit(main())
