"""Read-only UE4.27 Python API probe for adding save Blueprint variables."""

from __future__ import print_function

import unreal


library = getattr(unreal, "BlueprintEditorLibrary", None)
unreal.log_warning("SM64_BP_API_LIBRARY=" + repr(library))
if library is not None:
    for name in ("add_member_variable", "remove_member_variable", "compile_blueprint"):
        member = getattr(library, name, None)
        unreal.log_warning("SM64_BP_API_{}={}".format(name, getattr(member, "__doc__", repr(member))))
pin_type = getattr(unreal, "EdGraphPinType", None)
unreal.log_warning("SM64_BP_API_PIN_TYPE=" + repr(pin_type))
if pin_type is not None:
    value = pin_type()
    unreal.log_warning("SM64_BP_API_PIN_MEMBERS=" + ",".join(sorted(dir(value))))
