# Whomp's Fortress offline acceptance

This gate validates generated source without launching Blender or Unreal. It uses
a second parser to reconstruct all 127 stable placements directly from the SM64
decomp, consumes `wf_course_definition.json`, hashes all 35 FBX exports, audits
the 22 exact actor packages, and scans the native `SM64Runtime` reflection source.

The mover test is independent of render rate. Nine motion scenarios run for 420
integer simulation frames through equal elapsed-time schedules at 30, 60, and
120 FPS plus a repeatable hitch mix. The gate compares 27 full histories and 513
action/transform checkpoints, then verifies nine canonical motion invariants.

## Rerun

From `C:\Users\adace\Desktop\spyro-blowhards-revenge\Content\Spyro64`:

```powershell
& 'C:\Users\adace\AppData\Local\Programs\Python\Python313\python.exe' '.\SM64\Validation\validate_wf_acceptance.py'
```

The command returns exit code `0` only on a complete pass and updates:

- `SM64\Validation\Reports\wf_acceptance_report.json`
- `SM64\Validation\Reports\wf_acceptance_report.txt`

Read-only CI/probing run:

```powershell
& 'C:\Users\adace\AppData\Local\Programs\Python\Python313\python.exe' '.\SM64\Validation\validate_wf_acceptance.py' --no-write
```

Run the test suite:

```powershell
& 'C:\Users\adace\AppData\Local\Programs\Python\Python313\python.exe' -m unittest discover -s '.\SM64\Validation\Tests' -v
```

Run the focused UE4.27 Windows cook for the title screen, Spyro64 homeworld,
and Whomp's Fortress after closing every interactive editor instance:

```powershell
& '.\SM64\EditorScripts\cook_wf_acceptance.ps1'
```

Override `--decomp-root`, `--dae-dir`, `--content-root`, or `--plugin-root` when
the workspaces are moved. `expected_truth.json` is the reviewed immutable hash
snapshot; update it only after intentionally accepting regenerated artifacts.
