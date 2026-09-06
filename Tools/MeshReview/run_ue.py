import sys,pathlib
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'AssetPipeline'))
import send_to_editor as bridge
remote=bridge.load_remote_execution(r'C:\Program Files\Epic Games\UE_4.27')
execution,node=bridge.connect(remote,5)
try:
    code=pathlib.Path(sys.argv[1]).read_text()
    result=execution.run_command(code,unattended=True,exec_mode=remote.MODE_EXEC_FILE,raise_on_failure=False)
    bridge.report(result)
finally: execution.stop()
