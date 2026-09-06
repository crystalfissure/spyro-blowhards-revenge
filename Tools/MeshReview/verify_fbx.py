import bpy,pathlib,json,numpy as np
from mathutils.kdtree import KDTree
HERE=pathlib.Path(__file__).resolve().parent
OUT=HERE.parents[1]/'Blends/Dream_Weavers_Checked'
expected=np.load(HERE/'expected_export.npz')
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(OUT/'Dream Weavers Edit - Checked.fbx'),use_custom_normals=True)
obj=next(o for o in bpy.data.objects if o.type=='MESH');m=obj.data;m.calc_loop_triangles()
actual=np.array([obj.matrix_world@m.vertices[l.vertex_index].co for l in m.loops]);uv=np.array([l.uv[:] for l in m.uv_layers[0].data])
print('ROUNDTRIP SHAPES',expected['world'].shape,actual.shape,'bounds',actual.min(0),actual.max(0),flush=True)
if actual.shape!=expected['world'].shape:
    np.savez(HERE/'roundtrip_actual.npz',world=actual,uv=uv)
    aa=actual.reshape(-1,3,3).mean(1);ee=expected['world'].reshape(-1,3,3).mean(1)
    tree=KDTree(len(aa))
    for i,c in enumerate(aa):tree.insert(c,i)
    tree.balance()
    matches=[tree.find(c) for c in ee]
    print('MISSING TRIANGLES',[(i,m[2],ee[i].tolist()) for i,m in enumerate(matches) if m[2]>.001],flush=True)
assert actual.shape==expected['world'].shape
position_error=float(np.max(np.abs(expected['world']-actual)))
uv_error=float(np.max(np.abs(expected['uv']-uv)))
normals=np.array([(obj.matrix_world.to_3x3().inverted().transposed()@p.normal).normalized()[:] for p in m.polygons])
dot=np.sum(normals*expected['normals'],axis=1)
print('ROUNDTRIP',position_error,uv_error,'normal dot',float(dot.min()),flush=True)
assert position_error<.001
assert uv_error<1e-7
assert np.isfinite(normals).all() and dot.min()>.999
report=json.loads((OUT/'verification.json').read_text())
report['fbx_roundtrip']={'triangles':len(m.polygons),'uv_channels':len(m.uv_layers),'max_position_error_blender_units':position_error,'max_uv_error':uv_error,'minimum_normal_dot':float(dot.min()),'all_corners_preserved':True}
(OUT/'verification.json').write_text(json.dumps(report,indent=2))
bpy.ops.wm.open_mainfile(filepath=str(OUT/'Dream Weavers Edit - Checked.blend'))
obj=next(o for o in bpy.data.objects if o.type=='MESH')
im=next(n.image for mat in obj.data.materials for n in mat.node_tree.nodes if n.type=='TEX_IMAGE')
assert im.packed_file and tuple(im.size)==(4096,8192)
print('BLEND PACKED ATLAS VERIFIED',flush=True)
