from __future__ import annotations

import json
from pathlib import Path
import sys
import unittest


VALIDATION_ROOT = Path(__file__).resolve().parents[1]
if str(VALIDATION_ROOT) not in sys.path:
    sys.path.insert(0, str(VALIDATION_ROOT))

from mover_simulation import CHECKPOINTS, SPECS, run_mover_acceptance
from validate_wf_acceptance import (
    Acceptance,
    DEFAULT_CONTENT_ROOT,
    DEFAULT_DAE_DIR,
    DEFAULT_DECOMP_ROOT,
    DEFAULT_PLUGIN_ROOT,
)
from wf_truth_parser import (
    build_placement_truth,
    find_macro_calls,
    parse_collision_arrays,
    parse_collision_surface_y_values,
    parse_us_mission_names,
)


class TruthParserTests(unittest.TestCase):
    def test_balanced_macro_parser_keeps_nested_behavior_parameters(self) -> None:
        source = "OBJECT_WITH_ACTS(MODEL_X, BPARAM1(4) | BPARAM2(F(1, 2)), bhvX, ACT_2 | ACT_3)"
        calls = list(find_macro_calls(source, ("OBJECT_WITH_ACTS", "OBJECT")))
        self.assertEqual(len(calls), 1)
        self.assertEqual(calls[0][0], "OBJECT_WITH_ACTS")
        self.assertIn("BPARAM2(F(1, 2))", calls[0][1])

    def test_decomp_reconstructs_exact_stable_placement_set(self) -> None:
        placements = build_placement_truth(DEFAULT_DECOMP_ROOT)
        self.assertEqual(len(placements), 127)
        self.assertEqual(len({item.stable_id for item in placements}), 127)
        self.assertEqual(sum(item.model == "macro_red_coin" for item in placements), 8)
        self.assertEqual(sum(item.source_kind == "behavior_generated" for item in placements), 17)

    def test_terrain_semantics_and_death_height(self) -> None:
        path = DEFAULT_DECOMP_ROOT / "levels/wf/areas/1/collision.inc.c"
        terrain = parse_collision_arrays(path)[0]
        self.assertEqual(terrain.vertex_count, 422)
        self.assertEqual(terrain.triangle_count, 640)
        self.assertEqual(terrain.surfaces["SURFACE_DEATH_PLANE"], 2)
        self.assertEqual(
            parse_collision_surface_y_values(path, terrain.name, "SURFACE_DEATH_PLANE"),
            [-3071],
        )

    def test_us_course_has_six_canonical_missions(self) -> None:
        course, missions = parse_us_mission_names(DEFAULT_DECOMP_ROOT / "text/us/courses.h")
        self.assertEqual(course, "WHOMP'S FORTRESS")
        self.assertEqual(len(missions), 6)
        self.assertEqual(missions[0], "CHIP OFF WHOMP'S BLOCK")
        self.assertEqual(missions[-1], "BLAST AWAY THE WALL")


class MoverSimulationTests(unittest.TestCase):
    def test_all_frame_rate_and_hitch_schedules_are_equivalent(self) -> None:
        result = run_mover_acceptance()
        self.assertTrue(result["passed"])
        self.assertEqual(result["scenario_count"], len(SPECS))
        self.assertEqual(result["schedule_comparison_passes"], 27)
        self.assertEqual(result["checkpoint_passes"], len(SPECS) * 3 * len(CHECKPOINTS))
        self.assertEqual(result["invariant_passes"], len(SPECS))


class FullAcceptanceTests(unittest.TestCase):
    def test_current_generated_delivery_passes_offline_gate(self) -> None:
        acceptance = Acceptance(
            DEFAULT_CONTENT_ROOT,
            DEFAULT_DECOMP_ROOT,
            DEFAULT_DAE_DIR,
            DEFAULT_PLUGIN_ROOT,
            VALIDATION_ROOT / "expected_truth.json",
        )
        report = acceptance.run()
        failures = [item for item in report["checks"] if not item["passed"]]
        self.assertTrue(report["passed"], json.dumps(failures, indent=2))
        self.assertEqual(report["summary"]["mover_checkpoint_passes"], 513)


if __name__ == "__main__":
    unittest.main()
