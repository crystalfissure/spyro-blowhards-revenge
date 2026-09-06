import numpy as np,pathlib,json
from mathutils.kdtree import KDTree
HERE=pathlib.Path(__file__).resolve().parent
a=np.load(HERE/'geometry_0.npz');b=np.load(HERE/'geometry_1.npz')
tree=KDTree(len(a['centers']))
for i,c in enumerate(a['centers']):tree.insert(c,i)
tree.balance()
matches=[tree.find(c) for c in b['centers']]
indices=np.array([m[1] for m in matches]);distance=np.array([m[2] for m in matches])
an=a['normals'][indices];bn=b['normals'];an=an/np.maximum(np.linalg.norm(an,axis=1,keepdims=True),1e-9);bn=bn/np.maximum(np.linalg.norm(bn,axis=1,keepdims=True),1e-9);dot=np.sum(an*bn,axis=1)
report={'distance_max':float(distance.max()),'flipped':np.flatnonzero(dot<-.5).tolist(),'changed':np.flatnonzero(dot<.99).tolist()}
(HERE/'original_normal_comparison.json').write_text(json.dumps(report,indent=2))
print(report,flush=True)
