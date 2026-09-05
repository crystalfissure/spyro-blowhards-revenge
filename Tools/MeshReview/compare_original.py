import bpy,json,pathlib,numpy as np
HERE=pathlib.Path(__file__).resolve().parent
paths=[str(HERE.parents[1]/'Blends/Dream_Weavers_GigaAtlas/Dream Weavers Edit - Giga Atlas.blend1'),json.loads((HERE/'blend_audit.json').read_text())['source']]
for index,path in enumerate(paths):
    bpy.ops.wm.open_mainfile(filepath=path)
    if bpy.context.object and bpy.context.object.mode!='OBJECT':bpy.ops.object.mode_set(mode='OBJECT')
    positions=[];normals=[];centers=[];sizes=[]
    for o in bpy.data.objects:
        if o.type!='MESH':continue
        for p in o.data.polygons:
            world=[o.matrix_world@o.data.vertices[v].co for v in p.vertices]
            positions.extend(world);centers.append(sum(world,world[0]*0)/len(world));normals.append(o.matrix_world.to_3x3().inverted().transposed()@p.normal);sizes.append(len(p.vertices))
    pos=np.array(positions);print('COMPARE',index,len(centers),'bounds',pos.min(0),pos.max(0),flush=True)
    np.savez(HERE/f'geometry_{index}.npz',positions=pos,centers=np.array(centers),normals=np.array(normals),sizes=np.array(sizes))
