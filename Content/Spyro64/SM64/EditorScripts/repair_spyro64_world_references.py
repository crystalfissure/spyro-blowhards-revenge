"""Apply only reflected World-property repairs in an isolated UE process."""

from __future__ import print_function

import json
import os
import sys
import unreal


sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import repair_spyro64_foundation as foundation  # noqa: E402


def main():
    log = foundation.MutationLog()
    report = {
        "reference_results": foundation.repair_references(log),
        "mutations": log.entries,
        "warnings": log.warnings,
    }
    unreal.log_warning(
        "SM64_WORLD_REFERENCE_REPAIR=" + json.dumps(report, sort_keys=True)
    )


if __name__ == "__main__":
    main()
