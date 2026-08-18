"""Run an Unreal Python script in the already-open UE 4.27 editor.

Epic's UE 4.27 ``remote_execution.py`` still passes the removed ``encoding``
argument to ``json.loads``.  This wrapper applies a process-local compatibility
shim, discovers the editor for this project, runs a script, and forwards the
structured Unreal result to stdout.

This utility intentionally lives beside the editor automation so the exact
same scripts can be run either remotely or with the PythonScript commandlet.
"""

from __future__ import print_function

import argparse
import json
import os
import sys
import time


DEFAULT_REMOTE_MODULE = os.path.join(
    r"C:\Program Files\Epic Games\UE_4.27",
    "Engine",
    "Plugins",
    "Experimental",
    "PythonScriptPlugin",
    "Content",
    "Python",
)


def _load_remote_execution(module_dir):
    sys.path.insert(0, module_dir)
    import remote_execution  # pylint: disable=import-error,import-outside-toplevel

    original_loads = remote_execution._json.loads  # pylint: disable=protected-access

    def loads_compat(value, encoding=None, **kwargs):  # pylint: disable=unused-argument
        return original_loads(value, **kwargs)

    remote_execution._json.loads = loads_compat  # pylint: disable=protected-access
    return remote_execution


def _discover_node(session, project_name, timeout_seconds):
    deadline = time.time() + timeout_seconds
    while time.time() < deadline:
        matches = [
            node
            for node in session.remote_nodes
            if not project_name or node.get("project_name") == project_name
        ]
        if matches:
            return matches[0]
        time.sleep(0.2)
    raise RuntimeError(
        "No UE Python remote node found for project {!r} within {:.1f}s".format(
            project_name, timeout_seconds
        )
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("script", help="Python file visible to the Unreal editor")
    parser.add_argument("script_args", nargs="*", help="Arguments passed to the script")
    parser.add_argument("--project", default="Spyro_Bunnited")
    parser.add_argument("--timeout", type=float, default=8.0)
    parser.add_argument("--remote-module", default=DEFAULT_REMOTE_MODULE)
    args = parser.parse_args()

    script_path = os.path.abspath(args.script)
    if not os.path.isfile(script_path):
        raise RuntimeError("Script does not exist: {}".format(script_path))

    remote_execution = _load_remote_execution(args.remote_module)
    session = remote_execution.RemoteExecution()
    session.start()
    try:
        node = _discover_node(session, args.project, args.timeout)
        session.open_command_connection(node["node_id"])
        quoted = [json.dumps(script_path)] + [json.dumps(value) for value in args.script_args]
        command = "{} {}".format(quoted[0], " ".join(quoted[1:])).rstrip()
        result = session.run_command(
            command,
            unattended=True,
            exec_mode=remote_execution.MODE_EXEC_FILE,
            raise_on_failure=False,
        )
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0 if result.get("success") else 1
    finally:
        session.stop()


if __name__ == "__main__":
    sys.exit(main())
