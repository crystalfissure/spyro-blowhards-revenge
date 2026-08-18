import json
import unreal


ASSET_PATH = (
    "/Game/SpyroContent/Global_Assets/Global_Characters/"
    "AI_Characters/Actors/Enemies/ClubAttack_Enemy_BP"
)

blueprint = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
if not blueprint:
    raise RuntimeError("Could not load {}".format(ASSET_PATH))

if not unreal.MMAEditorAnimationLibrary.compile_blueprint(blueprint):
    raise RuntimeError("ClubAttack_Enemy_BP does not compile")

description = json.loads(
    unreal.MMAEditorAnimationLibrary.describe_blueprint_graphs(blueprint)
)
event_graph = next(graph for graph in description["graphs"] if graph["name"] == "EventGraph")
nodes = {node["guid"]: node for node in event_graph["nodes"]}
nearest = next(
    node for node in event_graph["nodes"]
    if node["title"] == "Get Nearest Player Im Alert To"
)
execute_pin = next(pin for pin in nearest["pins"] if pin["name"] == "execute")
if len(execute_pin["links"]) != 1:
    raise RuntimeError("Nearest-player call does not have one guarded exec input")

guard = nodes[execute_pin["links"][0]["node_guid"]]
condition_pin = next(pin for pin in guard["pins"] if pin["name"] == "Condition")
if guard["title"] != "Branch" or len(condition_pin["links"]) != 1:
    raise RuntimeError("Nearest-player call is not preceded by the expected guard Branch")

comparison = nodes[condition_pin["links"][0]["node_guid"]]
if comparison["title"] != "integer > integer":
    raise RuntimeError("Guard condition is not an integer greater-than comparison")

comparison_a = next(pin for pin in comparison["pins"] if pin["name"] == "A")
length_node = nodes[comparison_a["links"][0]["node_guid"]]
if length_node["title"] != "Length":
    raise RuntimeError("Guard comparison is not driven by alert-target array length")

array_pin = next(
    pin for pin in length_node["pins"]
    if pin["direction"] == "input" and len(pin["links"]) == 1
)
targets_node = nodes[array_pin["links"][0]["node_guid"]]
if targets_node["title"] != "Get Players_I_Am_Alert_To":
    raise RuntimeError("Length node is not reading Players_I_Am_Alert_To")

unreal.log_warning(
    "CLUB_ATTACK_GUARD_VERIFY_OK: {} -> {} -> {} -> {} -> {}".format(
        targets_node["title"],
        length_node["title"],
        comparison["title"],
        guard["title"],
        nearest["title"],
    )
)
