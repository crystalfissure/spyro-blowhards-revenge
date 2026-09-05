import bpy,pathlib,json,numpy as np,hashlib
from mathutils import Matrix
HERE=pathlib.Path(__file__).resolve().parent
OUT=HERE.parents[1]/'Blends/Dream_Weavers_Checked'
source=pathlib.Path(json.loads((HERE/'blend_audit.json').read_text())['source'])
source_hash=hashlib.sha256(source.read_bytes()).hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(source))
if bpy.context.object.mode!='OBJECT':bpy.ops.object.mode_set(mode='OBJECT')
obj=next(o for o in bpy.data.objects if o.type=='MESH');old=obj.data;old.calc_loop_triangles()
print('TRANSFORM',obj.matrix_world,'parent',obj.parent,'unit',bpy.context.scene.unit_settings.scale_length,flush=True)
triangles=[];loop_ids=[];source_polys=[];removed=[];split_duplicates=[];seen_triangles=set()
vertex_coords=[v.co[:] for v in old.vertices]
source_vertex_ids=list(range(len(old.vertices)))
for i,t in enumerate(old.loop_triangles):
    verts=[obj.matrix_world@old.vertices[v].co for v in t.vertices]
    if (verts[1]-verts[0]).cross(verts[2]-verts[0]).length<1e-10:
        removed.append(i);continue
    key=tuple(sorted(t.vertices));indices=list(t.vertices)
    if key in seen_triangles:
        indices=[]
        for v in t.vertices:
            indices.append(len(vertex_coords));vertex_coords.append(old.vertices[v].co[:]);source_vertex_ids.append(v)
        split_duplicates.append(i)
    seen_triangles.add(key)
    triangles.append(indices);loop_ids.extend(t.loops);source_polys.append(t.polygon_index)
new=bpy.data.meshes.new('DreamWeavers_Checked_Mesh')
new.from_pydata(vertex_coords,[],triangles)
for mat in old.materials:new.materials.append(mat)
for layer in old.uv_layers:
    target=new.uv_layers.new(name=layer.name)
    values=np.array([layer.data[i].uv[:] for i in loop_ids],np.float32)
    target.data.foreach_set('uv',values.ravel());target.active_render=layer.active_render
    assert np.isfinite(values).all()
for layer in old.color_attributes:
    target=new.color_attributes.new(name=layer.name,type=layer.data_type,domain=layer.domain)
    ids=loop_ids if layer.domain=='CORNER' else source_vertex_ids
    values=np.array([layer.data[i].color[:] for i in ids],np.float32)
    target.data.foreach_set('color',values.ravel())
if old.color_attributes.active_color:
    new.color_attributes.active_color=new.color_attributes[old.color_attributes.active_color.name]
for p,source_index in zip(new.polygons,source_polys):
    p.use_smooth=old.polygons[source_index].use_smooth
    p.material_index=old.polygons[source_index].material_index
obj.data=new;new.update();new.calc_loop_triangles()
assert all(len(p.vertices)==3 and p.normal.length>.99 for p in new.polygons)
assert len(new.loop_triangles)==len(triangles)
atlas=next(n.image for m in new.materials for n in m.node_tree.nodes if n.type=='TEX_IMAGE')
atlas_path=OUT/'Spyro_Giga_Texture_Atlas_V2.7_Dream_Weavers_Extended.png'
atlas_path.write_bytes(bytes(atlas.packed_file.data))
fresh_atlas=bpy.data.images.load(str(atlas_path),check_existing=False)
fresh_atlas.colorspace_settings.name=atlas.colorspace_settings.name
fresh_atlas.pack()
for mat in bpy.data.materials:
    if not mat.node_tree:continue
    for node in mat.node_tree.nodes:
        if node.type=='TEX_IMAGE' and node.image==atlas:node.image=fresh_atlas
bpy.data.images.remove(atlas,do_unlink=True)
blend=OUT/'Dream Weavers Edit - Checked.blend'
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(blend),compress=True)
bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
fbx=OUT/'Dream Weavers Edit - Checked.fbx'
bpy.ops.export_scene.fbx(filepath=str(fbx),use_selection=True,object_types={'MESH'},use_mesh_modifiers=True,mesh_smooth_type='FACE',use_tspace=False,add_leaf_bones=False,bake_anim=False,path_mode='STRIP',axis_forward='-Y',axis_up='Z',apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE')
report={'source':str(source),'source_sha256':source_hash,'blend':str(blend),'fbx':str(fbx),'atlas':str(atlas_path),'faces_before':len(old.polygons),'triangles_before':len(old.loop_triangles),'triangles_after':len(triangles),'removed_zero_area_triangles':removed,'split_coincident_triangles_to_preserve_both_uv_mappings':split_duplicates,'uv_corners_after':len(loop_ids),'face_winding_preserved':True,'normals':'Flat face normals regenerated from explicit triangles; all finite and unit length. Inward cave ceilings and cliff walls retained.','uv_coordinates_and_vertex_colors_copied_per_corner':True}
(OUT/'verification.json').write_text(json.dumps(report,indent=2))
uv=np.array([v.uv[:] for v in new.uv_layers[0].data])
world=np.array([obj.matrix_world@new.vertices[l.vertex_index].co for l in new.loops])
normals=np.array([(obj.matrix_world.to_3x3().inverted().transposed()@p.normal).normalized()[:] for p in new.polygons])
np.savez(HERE/'expected_export.npz',uv=uv,world=world,normals=normals)
assert hashlib.sha256(source.read_bytes()).hexdigest()==source_hash
print('PREPARED',json.dumps(report),flush=True)
