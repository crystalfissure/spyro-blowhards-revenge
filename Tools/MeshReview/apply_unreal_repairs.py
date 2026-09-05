import unreal,json,os,math
ROOT=r'C:\Users\adace\Desktop\spyro-blowhards-revenge'
OUT=os.path.join(ROOT,'Blends','Dream_Weavers_Checked')
original=unreal.load_asset('/Game/_CF_Project/Meshes/Levels/Dream_Weavers_Edit')
checked=unreal.load_asset('/Game/_CF_Project/Meshes/Levels/Dream_Weavers_Edit_Checked')
material=unreal.load_asset('/Game/_CF_Project/Materials/Dream_Weavers_Material')
assert original and checked and material
def geometry(m):
    return unreal.ProceduralMeshLibrary.get_section_from_static_mesh(m,0,0)
def bounds(vertices):
    return [[min(getattr(v,a) for v in vertices) for a in ['x','y','z']],[max(getattr(v,a) for v in vertices) for a in ['x','y','z']]]
before=geometry(original);new=geometry(checked)
old_bounds=bounds(before[0]);new_bounds=bounds(new[0])
scales=[(old_bounds[1][i]-old_bounds[0][i])/(new_bounds[1][i]-new_bounds[0][i]) for i in range(3)]
scale=sum(scales)/3
assert max(scales)-min(scales)<1e-5
assert max(abs(new_bounds[j][i]*scale-old_bounds[j][i]) for j in range(2) for i in range(3))<.02
assert len(new[1])//3==29667
assert os.path.isfile(os.path.join(OUT,'Unreal_Backup','Dream_Weavers_Edit.uasset'))
assert os.path.isfile(os.path.join(OUT,'Unreal_Backup','Dream_Weavers_Material.uasset'))
body=original.get_editor_property('body_setup')
collision_before={'simple':unreal.EditorStaticMeshLibrary.get_simple_collision_count(original),'convex':unreal.EditorStaticMeshLibrary.get_convex_collision_count(original),'trace':str(body.get_editor_property('collision_trace_flag'))}
task=unreal.AssetImportTask();task.set_editor_property('filename',os.path.join(OUT,'Spyro_Giga_Texture_Atlas_V2.7_Dream_Weavers_Extended.png'));task.set_editor_property('destination_path','/Game/_CF_Project/Textures');task.set_editor_property('destination_name','Spyro_Giga_Texture_Atlas_V2_7_DreamWeavers_Extended');task.set_editor_property('automated',True);task.set_editor_property('replace_existing',False);task.set_editor_property('save',False)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
tex=unreal.load_asset('/Game/_CF_Project/Textures/Spyro_Giga_Texture_Atlas_V2_7_DreamWeavers_Extended');assert tex
old_tex=unreal.load_asset('/Game/_CF_Project/Textures/Spyro_Giga_Texture_Atlas_V2_7')
for prop in ['compression_settings','srgb','lod_group']:
    tex.set_editor_property(prop,old_tex.get_editor_property(prop))
tex.set_editor_property('filter',unreal.TextureFilter.TF_NEAREST)
tex.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
tex.set_editor_property('lod_bias',0)
parameters=list(material.get_editor_property('texture_parameter_values'))
matched=0
for parameter in parameters:
    if str(parameter.get_editor_property('parameter_info').get_editor_property('name'))=='Texture':
        parameter.set_editor_property('parameter_value',tex);matched+=1
assert matched==1
material.set_editor_property('texture_parameter_values',parameters)
assert next(p for p in material.get_editor_property('texture_parameter_values') if str(p.get_editor_property('parameter_info').get_editor_property('name'))=='Texture').get_editor_property('parameter_value')==tex
unreal.MaterialEditingLibrary.update_material_instance(material)
checked.set_material(0,material)
settings=unreal.EditorStaticMeshLibrary.get_lod_build_settings(original,0)
result=unreal.EditorStaticMeshLibrary.set_lod_from_static_mesh(original,0,checked,0,True)
assert result==0
settings.set_editor_property('use_full_precision_u_vs',True)
settings.set_editor_property('recompute_normals',False)
settings.set_editor_property('remove_degenerates',False)
settings.set_editor_property('build_scale3d',unreal.Vector(scale,scale,scale))
unreal.EditorStaticMeshLibrary.set_lod_build_settings(original,0,settings)
original.set_material(0,material)
checked_settings=unreal.EditorStaticMeshLibrary.get_lod_build_settings(checked,0)
checked_settings.set_editor_property('build_scale3d',unreal.Vector(scale,scale,scale))
unreal.EditorStaticMeshLibrary.set_lod_build_settings(checked,0,checked_settings)
after=geometry(original)
after_bounds=bounds(after[0])
assert len(after[1])//3==29667
assert max(abs(after_bounds[j][i]-old_bounds[j][i]) for j in range(2) for i in range(3))<.03
collision_after={'simple':unreal.EditorStaticMeshLibrary.get_simple_collision_count(original),'convex':unreal.EditorStaticMeshLibrary.get_convex_collision_count(original),'trace':str(original.get_editor_property('body_setup').get_editor_property('collision_trace_flag'))}
assert collision_after==collision_before
for asset in [tex,material,checked,original]:assert unreal.EditorAssetLibrary.save_loaded_asset(asset,False)
report={'mesh_updated':original.get_path_name(),'checked_copy':checked.get_path_name(),'material_updated':material.get_path_name(),'texture':tex.get_path_name(),'full_precision_uvs':True,'triangles':len(after[1])//3,'scale_to_preserve_existing_unreal_bounds':scale,'bounds_before':old_bounds,'bounds_after':after_bounds,'collision_preserved':collision_after,'backups':os.path.join(OUT,'Unreal_Backup')}
with open(os.path.join(OUT,'unreal_verification.json'),'w') as f:json.dump(report,f,indent=2)
with open(os.path.join(ROOT,'Tools','MeshReview','unreal_final_geometry.json'),'w') as f:json.dump({'uv_corners':[[after[3][i].x,after[3][i].y] for i in after[1]],'normals':[[n.x,n.y,n.z] for n in after[2]]},f)
print('REPAIRS_APPLIED '+json.dumps(report))
