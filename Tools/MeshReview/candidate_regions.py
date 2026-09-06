import bpy,json,pathlib
from mathutils import Vector
HERE=pathlib.Path(__file__).resolve().parent
bpy.ops.wm.open_mainfile(filepath=json.loads((HERE/'blend_audit.json').read_text())['source'])
if bpy.context.object.mode!='OBJECT':bpy.ops.object.mode_set(mode='OBJECT')
o=bpy.context.object;m=o.data
plan=json.loads((HERE/'orientation_plan.json').read_text());exposed=set(f['polygon'] for f in json.loads((HERE/'exposed_backfaces.json').read_text()))
for c in plan['islands']:
    if c['faces'] in [26,19] and exposed.intersection(c['indices']):
        centers=[o.matrix_world@m.polygons[i].center for i in c['indices']]
        print('REGION',c['faces'],'center',list(sum(centers,Vector())/len(centers)),'ids',c['indices'],flush=True)
