import unreal,json
m=unreal.load_asset('/Game/_CF_Project/Meshes/Levels/Dream_Weavers_Edit')
d=m.get_editor_property('asset_import_data');s=unreal.EditorStaticMeshLibrary.get_lod_build_settings(m,0)
print('IMPORT_TRANSFORM '+json.dumps({k:str(d.get_editor_property(k)) for k in ['import_uniform_scale','import_translation','import_rotation','transform_vertex_to_absolute','bake_pivot_in_vertex','convert_scene','force_front_x_axis','convert_scene_unit']}))
print('BUILD_SCALE '+str(s.get_editor_property('build_scale3d')))
