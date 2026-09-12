"""Import only lantern animation assets; create an unplaced gameplay Blueprint."""
import unreal,json,hashlib
from pathlib import Path
ROOT='/Game/OT_Ports/S1/S1_Objects/Home_00_Artisans/02_DarkHollow'
DEST=ROOT+'/Interactive_Lantern'
W=Path(r"E:\Spyro Fangame Engine\Blowhard's Revenge docs and work\Dark_Hollow_Lantern\Inspection")
source=r'E:\Spyro Fangame Engine\Spaghetti_2026-04-22\Content\Spyro_OT_Assets_June\S1\Objects\Home_00_Artisans\02_DarkHollow\Lantern_Dark_Hollow.fbx'
mesh=unreal.load_asset(ROOT+'/Lantern_Dark_Hollow')
assert mesh
skeleton=mesh.get_editor_property('skeleton')
ui=unreal.FbxImportUI()
ui.automated_import_should_detect_type=False
ui.import_as_skeletal=True
ui.mesh_type_to_import=unreal.FBXImportType.FBXIT_ANIMATION
ui.import_mesh=False
ui.import_animations=True
ui.import_materials=False
ui.import_textures=False
ui.skeleton=skeleton
ui.anim_sequence_import_data.set_editor_property('import_meshes_in_bone_hierarchy',False)
ui.anim_sequence_import_data.set_editor_property('import_rotation',unreal.Rotator(pitch=0,yaw=0,roll=90))
task=unreal.AssetImportTask()
task.filename=source
task.destination_path=DEST
task.destination_name='Lantern'
task.options=ui
task.automated=True
task.save=False
task.replace_existing=False
tools=unreal.AssetToolsHelpers.get_asset_tools()
assert not unreal.EditorAssetLibrary.does_directory_exist(DEST),'Destination already exists: inspect before rerunning'
tools.import_asset_tasks([task])
animations=[unreal.load_asset(p) for p in unreal.EditorAssetLibrary.list_assets(DEST)]
animations=[a for a in animations if isinstance(a,unreal.AnimSequence)]
assert len(animations)==2, [a.get_path_name() for a in animations]
rest=next(a for a in animations if 'Anim0' in a.get_name())
reaction=next(a for a in animations if 'Anim1' in a.get_name())
for a in animations:
 assert a.get_editor_property('skeleton')==skeleton
 assert unreal.EditorAssetLibrary.save_loaded_asset(a)
factory=unreal.BlueprintFactory()
factory.set_editor_property('parent_class',unreal.MMALantern)
bp=tools.create_asset('BP_DarkHollowLantern',DEST,unreal.Blueprint,factory)
assert bp
cls=unreal.load_class(None,DEST+'/BP_DarkHollowLantern.BP_DarkHollowLantern_C')
cdo=unreal.get_default_object(cls)
cdo.set_editor_property('rest_animation',rest)
cdo.set_editor_property('reaction_animation',reaction)
component=cdo.get_editor_property('lantern_mesh')
component.set_skeletal_mesh(mesh)
component.set_editor_property('animation_mode',unreal.AnimationMode.ANIMATION_SINGLE_NODE)
assert unreal.MMAEditorAnimationLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
row={'blueprint':bp.get_path_name(),'mesh':mesh.get_path_name(),'skeleton':skeleton.get_path_name(),'rest':rest.get_path_name(),'reaction':reaction.get_path_name(),'reaction_seconds':reaction.get_editor_property('sequence_length'),'damage_component_class':cdo.get_editor_property('damageable').get_class().get_name()}
(W/'production_install.json').write_text(json.dumps(row,indent=2))
print('LANTERN_INSTALLED '+json.dumps(row))
