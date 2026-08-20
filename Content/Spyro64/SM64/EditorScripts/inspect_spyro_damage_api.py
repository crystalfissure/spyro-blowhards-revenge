"""Read-only reflection inventory for native SM64-to-Spyro damage/life handoff."""

from __future__ import print_function

import json
import unreal


BP = "/Game/SpyroContent/Global_Assets/Global_Characters/Playable_Characters/Spyro/BP_Spyro"
KEYS = ("damage", "health", "hurt", "hit", "life", "lives", "death", "die", "sparx")


def relevant_names(value):
    return sorted(name for name in dir(value) if any(key in name.lower() for key in KEYS))


generated_class = unreal.EditorAssetLibrary.load_blueprint_class(BP)
if generated_class is None:
    raise RuntimeError("Unable to load " + BP)
default_object = unreal.get_default_object(generated_class)
components = []
for component in default_object.get_components_by_class(unreal.ActorComponent):
    components.append({
        "name": component.get_name(),
        "class": component.get_class().get_path_name(),
        "relevant_api": relevant_names(component),
    })

report = {
    "class": generated_class.get_path_name(),
    "actor_relevant_api": relevant_names(default_object),
    "components": components,
}
if unreal.EditorLevelLibrary.load_level("/Game/Spyro64/Levels/05_Level5"):
    spyro = next(
        (actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()
         if actor.get_class().get_name() == "BP_Spyro_C"),
        None,
    )
    if spyro is not None:
        report["level_instance_api"] = relevant_names(spyro)
        report["level_components"] = [
            {
                "name": component.get_name(),
                "class": component.get_class().get_path_name(),
                "relevant_api": relevant_names(component),
            }
            for component in spyro.get_components_by_class(unreal.ActorComponent)
        ]
unreal.log_warning("SM64_SPYRO_DAMAGE_API=" + json.dumps(report, sort_keys=True))
