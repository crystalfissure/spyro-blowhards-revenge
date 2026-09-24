"""
Minimal MCP server (stdio) that runs Python inside a RUNNING UE 4.27 editor.

No Unreal plugin is added. It uses the engine's own Python remote-execution
channel (UDP multicast) - the same one Tools/AssetPipeline/send_to_editor.py
uses. Requires: Python Editor Script Plugin enabled (built in) and
Project Settings > Plugins > Python > Enable Remote Execution ticked.

Standard library only. Register it in Claude desktop's MCP config, e.g.
    "Unreal": {
      "command": "python",
      "args": ["E:\\Spyro Fangame Engine\\Spyro-Blowhards-Revenge\\Tools\\UnrealMCP\\unreal_mcp_server.py"]
    }

Tools:
    unreal_ping         - check an editor is listening, return project dir
    unreal_run_python   - execute Python in the editor, return its output
"""

import contextlib
import json
import os
import sys
import time

ENGINE_DIR = os.environ.get("UE_ENGINE_DIR", r"C:\Unreal Engine\UE_4.27")
PROTOCOL_VERSION = "2024-11-05"

# stdout carries JSON-RPC only; keep a handle and send everything else to stderr.
_RPC_OUT = sys.stdout
sys.stdout = sys.stderr


def _patch_json():
    # Epic's remote_execution.py passes json.loads(encoding=...), removed in 3.9.
    if sys.version_info < (3, 9):
        return
    original = json.loads

    def loads(*args, **kwargs):
        kwargs.pop("encoding", None)
        return original(*args, **kwargs)

    json.loads = loads


_remote = None


def _remote_module():
    global _remote
    if _remote is None:
        module_dir = os.path.join(ENGINE_DIR, "Engine", "Plugins", "Experimental",
                                  "PythonScriptPlugin", "Content", "Python")
        if not os.path.isfile(os.path.join(module_dir, "remote_execution.py")):
            raise RuntimeError("remote_execution.py not found under %s (set UE_ENGINE_DIR)" % module_dir)
        _patch_json()
        sys.path.insert(0, module_dir)
        import remote_execution  # noqa: E402
        _remote = remote_execution
    return _remote


def _run_in_editor(code, timeout=10.0):
    remote = _remote_module()
    execution = remote.RemoteExecution(remote.RemoteExecutionConfig())
    execution.start()
    try:
        deadline = time.time() + timeout
        while not execution.remote_nodes and time.time() < deadline:
            time.sleep(0.2)
        if not execution.remote_nodes:
            raise RuntimeError(
                "No Unreal editor answered within %ss. Is the editor open with "
                "Enable Remote Execution ticked?" % timeout)
        node = execution.remote_nodes[0]
        node_id = node["node_id"] if isinstance(node, dict) else node.node_id
        execution.open_command_connection(node_id)
        result = execution.run_command(code, unattended=True,
                                       exec_mode=remote.MODE_EXEC_FILE,
                                       raise_on_failure=False)
    finally:
        with contextlib.suppress(Exception):
            execution.stop()

    lines, ok = [], True
    if isinstance(result, dict):
        ok = bool(result.get("success", False))
        for entry in result.get("output") or []:
            kind = entry.get("type", "Info")
            text = entry.get("output", "")
            lines.append(text if kind == "Info" else "%s: %s" % (kind.upper(), text))
        if result.get("result") not in (None, "None", ""):
            lines.append("result: %s" % result.get("result"))
    else:
        lines.append(str(result))
    return ok, "\n".join(lines) or "(no output)"


TOOLS = [
    {
        "name": "unreal_ping",
        "description": "Check that a UE 4.27 editor is listening and return its project directory.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "unreal_run_python",
        "description": ("Execute Python code inside the running Unreal editor (the `unreal` module is "
                        "available). Returns printed output and errors. Use print() to report values."),
        "inputSchema": {
            "type": "object",
            "properties": {
                "code": {"type": "string", "description": "Python source to execute in the editor."},
                "timeout": {"type": "number", "description": "Seconds to wait for the editor (default 10)."},
            },
            "required": ["code"],
        },
    },
]


def _call_tool(name, args):
    if name == "unreal_ping":
        return _run_in_editor("import unreal\nprint(unreal.SystemLibrary.get_project_directory())")
    if name == "unreal_run_python":
        return _run_in_editor(args.get("code", ""), float(args.get("timeout", 10.0)))
    raise RuntimeError("unknown tool: %s" % name)


def _send(message):
    _RPC_OUT.write(json.dumps(message) + "\n")
    _RPC_OUT.flush()


def _handle(request):
    method = request.get("method")
    req_id = request.get("id")
    if req_id is None:  # notification
        return
    if method == "initialize":
        result = {
            "protocolVersion": request.get("params", {}).get("protocolVersion", PROTOCOL_VERSION),
            "capabilities": {"tools": {}},
            "serverInfo": {"name": "unreal-remote-python", "version": "0.1"},
        }
    elif method == "tools/list":
        result = {"tools": TOOLS}
    elif method == "tools/call":
        params = request.get("params", {})
        try:
            ok, text = _call_tool(params.get("name"), params.get("arguments") or {})
            result = {"content": [{"type": "text", "text": text}], "isError": not ok}
        except Exception as exc:  # report, never crash the server
            result = {"content": [{"type": "text", "text": "error: %s" % exc}], "isError": True}
    elif method == "ping":
        result = {}
    else:
        _send({"jsonrpc": "2.0", "id": req_id,
               "error": {"code": -32601, "message": "method not found: %s" % method}})
        return
    _send({"jsonrpc": "2.0", "id": req_id, "result": result})


def main():
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            request = json.loads(line)
        except ValueError:
            continue
        _handle(request)


if __name__ == "__main__":
    main()
