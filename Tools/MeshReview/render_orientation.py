import bpy,pathlib,json,numpy as np,sys
from mathutils import Vector
HERE=pathlib.Path(__file__).resolve().parent
args=sys.argv[sys.argv.index('--')+1:]
bpy.ops.wm.open_mainfile(filepath=args[0])
if bpy.context.object and bpy.context.object.mode!='OBJECT': bpy.ops.object.mode_set(mode='OBJECT')
scene=bpy.context.scene
scene.use_nodes=False; scene.render.engine='BLENDER_EEVEE'
scene.render.resolution_x=1400;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.render.film_transparent=False
world=bpy.data.worlds.new('QA World');world.use_nodes=True;world.node_tree.nodes['Background'].inputs[0].default_value=(0.025,0.025,0.025,1);scene.world=world
meshes=[o for o in scene.objects if o.type=='MESH' and len(o.data.vertices)]
points=np.array([list(o.matrix_world@Vector(c)) for o in meshes for c in o.bound_box]);lo,hi=points.min(0),points.max(0)
if 'region' in args:
    lo=np.array([74,-25,10]);hi=np.array([119,13,40]);points=np.array([[x,y,z] for x in [lo[0],hi[0]] for y in [lo[1],hi[1]] for z in [lo[2],hi[2]]])
print('BOUNDS',lo.tolist(),hi.tolist(),flush=True)
cam=bpy.data.objects.new('QA Camera',bpy.data.cameras.new('QA Camera'));scene.collection.objects.link(cam);scene.camera=cam
cam.data.type='ORTHO';cam.data.clip_end=float(np.linalg.norm(hi-lo)*10)
if 'normals' in args:
    mat=bpy.data.materials.new('QA Face Orientation');mat.use_nodes=True;nodes=mat.node_tree.nodes;nodes.clear();links=mat.node_tree.links
    geo=nodes.new('ShaderNodeNewGeometry');mix=nodes.new('ShaderNodeMixRGB');mix.inputs[1].default_value=(0.05,0.18,0.8,1);mix.inputs[2].default_value=(0.9,0.025,0.025,1)
    emission=nodes.new('ShaderNodeEmission');output=nodes.new('ShaderNodeOutputMaterial');links.new(geo.outputs['Backfacing'],mix.inputs[0]);links.new(mix.outputs[0],emission.inputs[0]);links.new(emission.outputs[0],output.inputs[0])
    for o in meshes:
        o.data.materials.clear();o.data.materials.append(mat)
        for p in o.data.polygons:p.material_index=0
for name,direction in [('front',(1,-1,1.25)),('back',(-1,1,1.25)),('top',(0.001,0,1))]:
    direction=Vector(direction).normalized();target=Vector((lo+hi)/2);cam.location=target+direction*float(np.linalg.norm(hi-lo)*2)
    cam.rotation_euler=(-direction).to_track_quat('-Z','Y').to_euler();r=cam.rotation_euler.to_matrix().transposed();p=np.array([list(r@(Vector(v)-target)) for v in points]);cam.data.ortho_scale=float(max(np.ptp(p[:,0]),np.ptp(p[:,1])*1.4)*1.05)
    scene.render.filepath=str(HERE/f'{args[1]}_{name}.png');bpy.ops.render.render(write_still=True)
