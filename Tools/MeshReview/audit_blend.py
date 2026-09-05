import bpy,bmesh,pathlib,json,collections,numpy as np
HERE=pathlib.Path(__file__).resolve().parent
SOURCE=pathlib.Path(r'C:\Users\adace\Downloads\Spyro OT Asset Dump\Spyro OT Assets 2026-01-02\S1\Levels\Home_04_Dream_Weavers\Dream Weavers Edit.blend')
bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
print('OPEN_MODE',bpy.context.mode,flush=True)
if bpy.context.object and bpy.context.object.mode!='OBJECT': bpy.ops.object.mode_set(mode='OBJECT')
report={'source':str(SOURCE),'objects':[],'images':[],'materials':[]}
for o in bpy.data.objects:
    if o.type!='MESH': continue
    m=o.data; m.calc_loop_triangles()
    bm=bmesh.new(); bm.from_mesh(m); bm.faces.ensure_lookup_table(); bm.edges.ensure_lookup_table(); bm.normal_update()
    original={f.index:f.normal.copy() for f in bm.faces}
    bad_edges=[e.index for e in bm.edges if e.is_manifold and not e.is_contiguous]
    deg=[f.index for f in bm.faces if f.calc_area()<1e-10]
    duplicates=[]; seen={}
    for f in bm.faces:
        key=tuple(sorted(tuple(round(v.co[i],6) for i in range(3)) for v in f.verts))
        if key in seen: duplicates.append([seen[key],f.index])
        else: seen[key]=f.index
    components=[]; visited=set()
    for face in bm.faces:
        if face.index in visited: continue
        stack=[face]; indices=[]
        while stack:
            f=stack.pop()
            if f.index in visited: continue
            visited.add(f.index); indices.append(f.index)
            stack.extend(n for e in f.edges for n in e.link_faces if n.index not in visited)
        components.append(indices)
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    flips=[f.index for f in bm.faces if original[f.index].dot(f.normal)<-0.5]
    uvstats=[]
    for layer in m.uv_layers:
        uv=np.array([v.uv[:] for v in layer.data])
        if not uv.size: continue
        triangles=np.array([[uv[l] for l in t.loops] for t in m.loop_triangles])
        a,b=triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0]
        area=np.abs(a[:,0]*b[:,1]-a[:,1]*b[:,0])/2
        half_error=np.abs(uv.astype(np.float16).astype(float)-uv)*[4096,8192]
        uvstats.append({'name':layer.name,'active':layer==m.uv_layers.active,'render':layer.active_render,'min':uv.min(0).tolist(),'max':uv.max(0).tolist(),'nonfinite':int((~np.isfinite(uv)).sum()),'zero_area_triangles':int((area<1e-14).sum()),'max_half_precision_error_pixels':half_error.max(0).tolist()})
    report['objects'].append({'name':o.name,'vertices':len(m.vertices),'polygons':len(m.polygons),'triangles':len(m.loop_triangles),'negative_transform':o.matrix_world.determinant()<0,'scale':list(o.scale),'smooth_faces':sum(p.use_smooth for p in m.polygons),'custom_normals':m.has_custom_normals,'modifiers':[(x.name,x.type) for x in o.modifiers],'inconsistent_edges':bad_edges,'boundary_edges':sum(e.is_boundary for e in bm.edges),'nonmanifold_edges':sum(len(e.link_faces)>2 for e in bm.edges),'zero_area_faces':deg,'zero_area_triangles':sum(t.area<1e-10 for t in m.loop_triangles),'duplicate_faces':duplicates,'components':[len(c) for c in components],'proposed_normal_flips':flips,'uv_layers':uvstats,'materials':[x.name if x else None for x in m.materials]})
    print('OBJECT',json.dumps({k:v for k,v in report['objects'][-1].items() if k not in ['proposed_normal_flips','inconsistent_edges','duplicate_faces','zero_area_faces','components']}),flush=True)
    print('ISSUES',o.name,'bad_edges',len(bad_edges),'normal_flips',len(flips),'zero_faces',len(deg),'dupes',len(duplicates),'components',len(components),flush=True)
    bm.free()
for im in bpy.data.images:
    report['images'].append({'name':im.name,'size':list(im.size),'path':im.filepath,'packed':bool(im.packed_file)})
for mat in bpy.data.materials:
    report['materials'].append({'name':mat.name,'nodes':[(n.type,n.image.name if n.type=='TEX_IMAGE' and n.image else None) for n in mat.node_tree.nodes]})
(HERE/'blend_audit.json').write_text(json.dumps(report,indent=2))
print('IMAGES',report['images'],flush=True)
