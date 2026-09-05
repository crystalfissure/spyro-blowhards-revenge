import unreal,json,os
ROOT=r'C:\Users\adace\Desktop\spyro-blowhards-revenge'
OUT=os.path.join(ROOT,'Blends','Dream_Weavers_Checked')
def import_asset(filename,dest,name,options=None):
    t=unreal.AssetImportTask();t.set_editor_property('filename',filename);t.set_editor_property('destination_path',dest);t.set_editor_property('destination_name',name);t.set_editor_property('automated',True);t.set_editor_property('replace_existing',True);t.set_editor_property('save',False)
    if options:t.set_editor_property('options',options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    a=unreal.load_asset(dest+'/'+name);assert a,'Import failed';return a
options=unreal.FbxImportUI()
options.set_editor_property('import_mesh',True);options.set_editor_property('import_as_skeletal',False);options.set_editor_property('import_materials',False);options.set_editor_property('import_textures',False);options.set_editor_property('automated_import_should_detect_type',False);options.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
sm=options.get_editor_property('static_mesh_import_data')
sm.set_editor_property('combine_meshes',True);sm.set_editor_property('generate_lightmap_u_vs',False);sm.set_editor_property('auto_generate_collision',False);sm.set_editor_property('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS);sm.set_editor_property('vertex_color_import_option',unreal.VertexColorImportOption.REPLACE)
checked=import_asset(os.path.join(OUT,'Dream Weavers Edit - Checked.fbx'),'/Game/_CF_Project/Meshes/Levels','Dream_Weavers_Edit_Checked',options)
settings=unreal.EditorStaticMeshLibrary.get_lod_build_settings(checked,0)
settings.set_editor_property('use_full_precision_u_vs',True);settings.set_editor_property('recompute_normals',False);settings.set_editor_property('remove_degenerates',False)
unreal.EditorStaticMeshLibrary.set_lod_build_settings(checked,0,settings)
original=unreal.load_asset('/Game/_CF_Project/Meshes/Levels/Dream_Weavers_Edit')
def dump(mesh):
    sections=[]
    for i in range(mesh.get_num_sections(0)):
        vertices,triangles,normals,uvs,tangents=unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh,0,i)
        sections.append({'vertices':[[v.x,v.y,v.z] for v in vertices],'indices':list(triangles),'uv':[[v.x,v.y] for v in uvs],'normals':[[n.x,n.y,n.z] for n in normals]})
    return sections
data={'original':dump(original),'checked':dump(checked)}
with open(os.path.join(ROOT,'Tools','MeshReview','unreal_geometry.json'),'w') as f:json.dump(data,f)
unreal.EditorAssetLibrary.save_loaded_asset(checked,False)
print('STAGED_CHECKED_MESH '+checked.get_path_name())
