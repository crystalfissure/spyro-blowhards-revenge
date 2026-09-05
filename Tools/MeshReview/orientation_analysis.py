import bpy,bmesh,pathlib,json,collections
from mathutils import Vector
HERE=pathlib.Path(__file__).resolve().parent
SOURCE=json.loads((HERE/'blend_audit.json').read_text())['source']
bpy.ops.wm.open_mainfile(filepath=SOURCE)
if bpy.context.object.mode!='OBJECT': bpy.ops.object.mode_set(mode='OBJECT')
obj=next(o for o in bpy.data.objects if o.type=='MESH')
bm=bmesh.new(); bm.from_mesh(obj.data); bm.faces.ensure_lookup_table(); bm.normal_update()
adj=collections.defaultdict(list)
for e in bm.edges:
    if not e.is_manifold: continue
    a,b=e.link_faces
    adj[a.index].append((b.index,int(not e.is_contiguous)))
    adj[b.index].append((a.index,int(not e.is_contiguous)))
visited={}; islands=[]; conflicts=set(); flips=[]
for root in bm.faces:
    if root.index in visited: continue
    visited[root.index]=0; stack=[root.index]; component=[]
    while stack:
        i=stack.pop(); component.append(i)
        for j,p in adj[i]:
            wanted=visited[i]^p
            if j not in visited: visited[j]=wanted; stack.append(j)
            elif visited[j]!=wanted: conflicts.add(tuple(sorted([i,j])))
    area=[sum(bm.faces[i].calc_area() for i in component if visited[i]==p) for p in [0,1]]
    invert=int(area[1]>area[0])
    changed=[i for i in component if visited[i]^invert]
    closed=all(e.is_manifold for i in component for e in bm.faces[i].edges)
    if closed:
        volume=sum((f.verts[0].co.dot(f.verts[j].co.cross(f.verts[j+1].co))/6)*(-1 if f.index in changed else 1) for i in component for f in [bm.faces[i]] for j in range(1,len(f.verts)-1))
        if volume<0: changed=[i for i in component if i not in changed]
    else: volume=None
    flips.extend(changed)
    islands.append({'faces':len(component),'closed':closed,'volume':volume,'flip_count':len(changed),'flip_area':sum(bm.faces[i].calc_area() for i in changed),'total_area':sum(area),'indices':component,'flips':changed})
result={'flips':sorted(flips),'conflicts':list(conflicts),'islands':islands}
(HERE/'orientation_plan.json').write_text(json.dumps(result,indent=2))
print('ORIENTATION',len(flips),'flips;',len(conflicts),'conflicts;',len(islands),'islands',flush=True)
print('CHANGED ISLANDS',json.dumps([{k:v for k,v in i.items() if k not in ['indices','flips']} for i in islands if i['flip_count']]),flush=True)
for i in flips[:20]:
    f=bm.faces[i]; print('FLIP',i,'area',f.calc_area(),'center',list(f.calc_center_median()),'normal',list(f.normal),flush=True)
