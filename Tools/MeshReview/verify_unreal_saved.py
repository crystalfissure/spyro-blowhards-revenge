import unreal,json,os,math
ROOT=r'C:\Users\adace\Desktop\spyro-blowhards-revenge';OUT=os.path.join(ROOT,'Blends','Dream_Weavers_Checked')
report=json.load(open(os.path.join(OUT,'unreal_verification.json')))
mesh=unreal.load_asset(report['mesh_updated']);material=unreal.load_asset(report['material_updated']);texture=unreal.load_asset(report['texture'])
settings=unreal.EditorStaticMeshLibrary.get_lod_build_settings(mesh,0)
assert settings.get_editor_property('use_full_precision_u_vs')
assert not settings.get_editor_property('recompute_normals')
assert mesh.get_material(0)==material
assert next(p for p in material.get_editor_property('texture_parameter_values') if str(p.get_editor_property('parameter_info').get_editor_property('name'))=='Texture').get_editor_property('parameter_value')==texture
vertices,indices,normals,uvs,tangents=unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh,0,0)
assert len(indices)==89001
assert all(math.isfinite(v.x) and math.isfinite(v.y) for v in uvs)
normal_lengths=[math.sqrt(n.x*n.x+n.y*n.y+n.z*n.z) for n in normals]
assert min(normal_lengths)>.99
data=mesh.get_editor_property('asset_import_data')
data.scripted_add_filename(os.path.join(OUT,'Dream Weavers Edit - Checked.fbx'),0,'')
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
report.update({'saved_assets_reloaded_and_verified':True,'uv_corners_in_unreal':len(indices),'minimum_normal_length':min(normal_lengths),'maximum_normal_length':max(normal_lengths),'reimport_source':list(data.extract_filenames())})
with open(os.path.join(OUT,'unreal_verification.json'),'w') as f:json.dump(report,f,indent=2)
with open(os.path.join(ROOT,'Tools','MeshReview','unreal_final_geometry.json'),'w') as f:json.dump({'uv_corners':[[uvs[i].x,uvs[i].y] for i in indices]},f)
print('SAVED_ASSETS_VERIFIED '+json.dumps(report))
