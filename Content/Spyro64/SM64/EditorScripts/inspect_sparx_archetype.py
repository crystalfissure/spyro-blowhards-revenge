"""Read-only discovery of Sparx's Capsule component-template chain."""

from __future__ import print_function

import json
import unreal


MAP = "/Game/Spyro64/Levels/05_Level5_PreWhomps"
SPARX_BLUEPRINT = "/Game/SpyroContent/Global_Assets/Global_Characters/Sparx/Sparx_BP"


def path(value):
    try:
        return value.get_path_name() if value else None
    except Exception:
        return None


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    sparx = next(
        actor
        for actor in unreal.EditorLevelLibrary.get_all_level_actors()
        if actor and actor.get_actor_label() == "Sparx_BP"
    )
    capsule = next(
        component
        for component in sparx.get_components_by_class(unreal.SceneComponent)
        if component.get_name() == "Capsule"
    )
    blueprint = unreal.EditorAssetLibrary.load_asset(SPARX_BLUEPRINT)
    generated_class = unreal.EditorAssetLibrary.load_blueprint_class(SPARX_BLUEPRINT)
    default_object = unreal.get_default_object(generated_class)
    template_path = generated_class.get_path_name() + ":Capsule_GEN_VARIABLE"
    template = unreal.find_object(None, template_path)
    if not template:
        template = unreal.load_object(None, template_path)
    report = {
        "instance": {
            "path": path(capsule),
            "outer": path(capsule.get_outer()),
            "absolute_location": capsule.get_editor_property("absolute_location"),
        },
        "template_path": template_path,
        "template": path(template),
        "template_absolute_location": (
            template.get_editor_property("absolute_location") if template else None
        ),
        "component_members": [
            name
            for name in dir(capsule)
            if any(term in name.lower() for term in ("archetype", "outer", "template"))
        ],
        "blueprint_members": [
            name
            for name in dir(blueprint)
            if any(term in name.lower() for term in ("component", "construction", "template"))
        ],
        "class_members": [
            name
            for name in dir(generated_class)
            if any(term in name.lower() for term in ("component", "construction", "template"))
        ],
        "cdo_members": [
            name
            for name in dir(default_object)
            if any(term in name.lower() for term in ("capsule", "component"))
        ],
    }
    unreal.log_warning("SM64_SPARX_ARCHETYPE=" + json.dumps(report, sort_keys=True))


if __name__ == "__main__":
    main()
