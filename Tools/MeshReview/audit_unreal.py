import unreal,json
path='/Game/_CF_Project/Meshes/Levels/Dream_Weavers_Edit'
mesh=unreal.load_asset(path)
result={'mesh':path,'exists':bool(mesh)}
if mesh:
    result['import_source']=list(mesh.get_editor_property('asset_import_data').extract_filenames())
    result['lods']=unreal.EditorStaticMeshLibrary.get_lod_count(mesh)
    result['uv_channels']=unreal.EditorStaticMeshLibrary.get_num_uv_channels(mesh,0)
    settings=unreal.EditorStaticMeshLibrary.get_lod_build_settings(mesh,0)
    result['build_settings']={k:str(settings.get_editor_property(k)) for k in ['use_full_precision_u_vs','recompute_normals','recompute_tangents','remove_degenerates','generate_lightmap_u_vs','src_lightmap_index','dst_lightmap_index']}
    result['materials']=[]
    for slot in mesh.get_editor_property('static_materials'):
        mat=slot.get_editor_property('material_interface')
        item={'slot':str(slot.get_editor_property('material_slot_name')),'material':mat.get_path_name() if mat else None}
        if mat:
            try:
                item['texture_parameters']=[{'name':str(p.get_editor_property('parameter_info').get_editor_property('name')),'texture':p.get_editor_property('parameter_value').get_path_name() if p.get_editor_property('parameter_value') else None} for p in mat.get_editor_property('texture_parameter_values')]
            except Exception as e: item['parameter_error']=str(e)
            item['dependencies']=unreal.EditorAssetLibrary.find_package_referencers_for_asset(mat.get_path_name(),False) if False else []
            registry=unreal.AssetRegistryHelpers.get_asset_registry()
            opts=unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True)
            deps=registry.get_dependencies(mat.get_outermost().get_name(),opts) if hasattr(mat,'get_outermost') else registry.get_dependencies(mat.get_path_name().split('.')[0],opts)
            item['dependencies']=[str(x) for x in deps]
        result['materials'].append(item)
    tex=unreal.load_asset('/Game/_CF_Project/Textures/Spyro_Giga_Texture_Atlas_V2_7')
    if tex:
        result['texture']={'path':tex.get_path_name(),'source':list(tex.get_editor_property('asset_import_data').extract_filenames()),'mips':str(tex.get_editor_property('mip_gen_settings')),'filter':str(tex.get_editor_property('filter')),'size':[tex.blueprint_get_size_x(),tex.blueprint_get_size_y()]}
with open(r'C:\Users\adace\Desktop\spyro-blowhards-revenge\Tools\MeshReview\unreal_audit.json','w') as f: json.dump(result,f,indent=2)
print('UE_AUDIT '+json.dumps(result))
