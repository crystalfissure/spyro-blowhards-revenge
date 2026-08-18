"""Print compact UE Python API metadata for automation development."""

from __future__ import print_function

import sys
import unreal


def resolve(name):
    value = unreal
    for part in name.split("."):
        value = getattr(value, part)
    return value


for symbol in sys.argv[1:]:
    value = resolve(symbol)
    unreal.log_warning(
        "SM64_API {}\nDOC={}\nMEMBERS={}".format(
            symbol,
            getattr(value, "__doc__", None),
            [name for name in dir(value) if not name.startswith("_")],
        )
    )
