import pathlib,json,numpy as np
from mathutils.kdtree import KDTree
HERE=pathlib.Path(__file__).resolve().parent;OUT=HERE.parents[1]/'Blends/Dream_Weavers_Checked'
expected=np.load(HERE/'expected_export.npz')['uv'].copy();expected[:,1]=1-expected[:,1]
actual=np.array(json.loads((HERE/'unreal_final_geometry.json').read_text())['uv_corners'])
assert len(actual)==len(expected)
tree=KDTree(len(expected))
for i,uv in enumerate(expected):tree.insert((uv[0],uv[1],0),i)
tree.balance()
errors=np.array([tree.find((uv[0],uv[1],0))[2] for uv in actual])
assert errors.max()<1e-7
report=json.loads((OUT/'unreal_verification.json').read_text());report['maximum_uv_distance_to_blender']=float(errors.max());report['maximum_uv_distance_in_atlas_pixels_upper_bound']=float(errors.max()*8192)
(OUT/'unreal_verification.json').write_text(json.dumps(report,indent=2))
print('UNREAL_UVS_VERIFIED',len(actual),'max error',errors.max(),'pixel bound',errors.max()*8192,flush=True)
