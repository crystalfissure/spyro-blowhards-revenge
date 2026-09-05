import bpy,pathlib,json,collections,numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
HERE=pathlib.Path(__file__).resolve().parent
bpy.ops.wm.open_mainfile(filepath=json.loads((HERE/'blend_audit.json').read_text())['source'])
if bpy.context.object.mode!='OBJECT':bpy.ops.object.mode_set(mode='OBJECT')
obj=next(o for o in bpy.data.objects if o.type=='MESH');m=obj.data;m.calc_loop_triangles()
verts=[obj.matrix_world@v.co for v in m.vertices]
tris=[list(t.vertices) for t in m.loop_triangles]
bvh=BVHTree.FromPolygons(verts,tris,all_triangles=True,epsilon=1e-7)
out=[]
for i,t in enumerate(m.loop_triangles):
    a,b,c=[verts[v] for v in t.vertices];cross=(b-a).cross(c-a);area=cross.length/2
    if area<1e-10:continue
    normal=cross.normalized()
    if normal.z>=-.15:continue
    samples=[(a+b+c)/3,a*.6+b*.2+c*.2,a*.2+b*.6+c*.2,a*.2+b*.2+c*.6]
    exposed=sum(bvh.ray_cast(p+Vector((0,0,.0002)),Vector((0,0,1)),10000)[0] is None for p in samples)
    if exposed>=3:
        out.append({'triangle':i,'polygon':t.polygon_index,'area':area,'center':list(samples[0]),'normal':list(normal),'samples_visible':exposed})
(HERE/'exposed_backfaces.json').write_text(json.dumps(out,indent=2))
print('EXPOSED BACKFACES',len(out),'polygons',len(set(x['polygon'] for x in out)),'area',sum(x['area'] for x in out),flush=True)
print(json.dumps(out[:25]),flush=True)
