import unreal,json
from pathlib import Path
ROOT='/Game/OT_Ports/S1/S1_Objects/Home_00_Artisans/02_DarkHollow'
DEST=ROOT+'/Interactive_Lantern'
bp=unreal.load_asset(DEST+'/BP_DarkHollowLantern')
assert bp and unreal.MMAEditorAnimationLibrary.compile_blueprint(bp)
cdo=unreal.get_default_object(unreal.load_class(None,DEST+'/BP_DarkHollowLantern.BP_DarkHollowLantern_C'))
mesh=cdo.get_editor_property('lantern_mesh').get_editor_property('skeletal_mesh')
rest=cdo.get_editor_property('rest_animation')
reaction=cdo.get_editor_property('reaction_animation')
assert mesh==unreal.load_asset(ROOT+'/Lantern_Dark_Hollow')
assert rest.get_editor_property('skeleton')==mesh.get_editor_property('skeleton')==reaction.get_editor_property('skeleton')
report={'blueprint':bp.get_path_name(),'mesh':mesh.get_path_name(),'skeleton':mesh.get_editor_property('skeleton').get_path_name(),'rest':rest.get_path_name(),'reaction':reaction.get_path_name(),'reaction_seconds':reaction.get_editor_property('sequence_length'),'damage_class':cdo.get_editor_property('damageable').get_class().get_name(),'blueprint_compiles':True,'uses_original_mesh_and_skeleton':True,'level_placement':'Not placed; ready-to-place actor asset'}
Path(r"E:\Spyro Fangame Engine\Blowhard's Revenge docs and work\Dark_Hollow_Lantern\Inspection\production_install.json").write_text(json.dumps(report,indent=2))
print('LANTERN_VERIFIED '+json.dumps(report))
