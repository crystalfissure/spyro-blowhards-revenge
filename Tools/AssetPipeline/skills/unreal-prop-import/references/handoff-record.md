# Import record

Save a short record beside the import scripts or asset notes. Fill only relevant fields, and label unknowns explicitly. This is a compact working record, not a required user questionnaire.

```text
Prop / intended behaviour:
Project / engine version:
Source file(s) / hash or identifying timestamp:
Destination asset paths:
Existing mesh / skeleton / materials reused:

Representation: static / skeletal / component hierarchy / other
Source clips -> Unreal clips -> roles:
Source frame rate and span / imported duration:
Import settings: units, axes, named Pitch/Yaw/Roll, scale, normals,
                vertex colours, materials, collision, reference-pose options
Reason and evidence for any correction:
Reimport policy: create-only / replace selected assets and settings

Actor parent / runtime module dependency:
Attack entry point / accepted types / required collision contract:
Rest -> reaction -> reset:
Repeat-hit policy:

Checks performed and result:
  saved references / compile / geometry and materials / animation poses
  root and child transforms / collision / runtime callback / real-player test
Exact named automation result and log/report paths:
Warnings investigated and disposition:
Original assets or levels changed:
Placement status / remaining unverified behaviour:
How to find and use the result:
```

Keep the report proportional to the prop. A static rock does not need an attack-state test. A hit-reactive lantern does need evidence beyond a successful FBX import. Save the exact successful settings so the next reimport does not require rediscovering them.
