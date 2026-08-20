"""Offline acceptance gate for the Whomp's Fortress rebuild and SM64 runtime.

The gate is deliberately independent of Blender and Unreal.  It reparses the
authoritative decomp C data, audits immutable export hashes, inspects actor JSON,
and scans Unreal reflection declarations directly from source.
"""

from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime, timezone
from hashlib import sha256
import json
from pathlib import Path
import re
import sys
from typing import Any, Callable, Dict, Iterable, List, Mapping, Optional, Sequence, Tuple


HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from mover_simulation import run_mover_acceptance  # noqa: E402
from wf_truth_parser import (  # noqa: E402
    PlacementTruth,
    build_placement_truth,
    parse_collision_arrays,
    parse_collision_surface_y_values,
    parse_us_mission_names,
)


DEFAULT_CONTENT_ROOT = HERE.parent.parent
DEFAULT_DECOMP_ROOT = Path(r"C:\Users\adace\Downloads\sm64-master\sm64-master")
DEFAULT_DAE_DIR = Path(
    r"C:\Users\adace\Downloads\Nintendo 64 - Super Mario 64 - Locations - Whomp's Fortress\Whomp's Fortress"
)
DEFAULT_PLUGIN_ROOT = DEFAULT_CONTENT_ROOT.parent.parent / "Plugins" / "SM64Runtime"


def read_json(path: Path) -> Dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def file_sha256(path: Path) -> str:
    digest = sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def vector_equal(left: Sequence[Any], right: Sequence[Any], tolerance: float = 1.0e-6) -> bool:
    return len(left) == len(right) and all(
        abs(float(a) - float(b)) <= tolerance for a, b in zip(left, right)
    )


def relative_fbx_path(value: str) -> str:
    normalized = value.replace("\\", "/")
    marker = "/FBX/"
    if marker not in normalized:
        raise ValueError("FBX report path has no /FBX/ segment: {}".format(value))
    return normalized.split(marker, 1)[1]


class Acceptance:
    def __init__(
        self,
        content_root: Path,
        decomp_root: Path,
        dae_dir: Path,
        plugin_root: Path,
        truth_path: Path,
    ) -> None:
        self.content_root = content_root.resolve()
        self.decomp_root = decomp_root.resolve()
        self.dae_dir = dae_dir.resolve()
        self.plugin_root = plugin_root.resolve()
        self.truth_path = truth_path.resolve()
        self.truth = read_json(self.truth_path)
        self.source_root = self.content_root / "SM64" / "Source"
        self.wf_root = self.source_root / "WhompsFortress"
        self.actor_root = self.source_root / "Actors"
        self.checks: List[Dict[str, Any]] = []
        self.mover_result: Dict[str, Any] = {}

    def add(
        self,
        name: str,
        category: str,
        passed: bool,
        expected: Any = None,
        actual: Any = None,
        detail: str = "",
    ) -> None:
        self.checks.append(
            {
                "name": name,
                "category": category,
                "passed": bool(passed),
                "expected": expected,
                "actual": actual,
                "detail": detail,
            }
        )

    def guard(self, category: str, callback: Callable[[], None]) -> None:
        try:
            callback()
        except Exception as exc:  # acceptance must report all independent groups
            self.add(
                "{}_validator_completed".format(category),
                category,
                False,
                "no exception",
                "{}: {}".format(type(exc).__name__, exc),
            )

    def validate_placements(self) -> None:
        manifest_path = self.wf_root / "Manifest" / "wf_placements.json"
        manifest = read_json(manifest_path)
        placements = manifest.get("placements", [])
        canonical = build_placement_truth(self.decomp_root)
        expected_count = int(self.truth["course"]["placement_count"])
        ids = [item.get("stable_id") for item in placements]
        duplicate_ids = sorted(name for name, count in Counter(ids).items() if count > 1)

        self.add(
            "placement_manifest_format",
            "placements",
            manifest.get("format") == "spyro64.sm64.placements" and manifest.get("fixed_step_hz") == 30,
            {"format": "spyro64.sm64.placements", "fixed_step_hz": 30},
            {"format": manifest.get("format"), "fixed_step_hz": manifest.get("fixed_step_hz")},
        )
        self.add("placement_count", "placements", len(placements) == expected_count, expected_count, len(placements))
        self.add(
            "stable_id_uniqueness",
            "placements",
            len(ids) == len(set(ids)) == expected_count,
            "{} unique IDs".format(expected_count),
            {"unique": len(set(ids)), "duplicates": duplicate_ids},
        )
        self.add(
            "independent_decomp_placement_count",
            "placements",
            len(canonical) == expected_count,
            expected_count,
            len(canonical),
            "Reconstructed without importing the generator package.",
        )

        by_id = {item.get("stable_id"): item for item in placements}
        canonical_by_id = {item.stable_id: item for item in canonical}
        missing = sorted(set(canonical_by_id) - set(by_id))
        unexpected = sorted(set(by_id) - set(canonical_by_id))
        mismatches: List[Dict[str, Any]] = []
        for stable_id in sorted(set(by_id) & set(canonical_by_id)):
            actual = by_id[stable_id]
            expected = canonical_by_id[stable_id]
            source = actual.get("source_transform", {})
            unreal = actual.get("unreal_transform", {})
            expected_ue_location = (expected.position_cm[0], expected.position_cm[2], expected.position_cm[1])
            expected_ue_rotation = (
                expected.rotation_deg[0],
                -expected.rotation_deg[1],
                expected.rotation_deg[2],
            )
            fields = {
                "model": actual.get("model") == expected.model,
                "behavior": actual.get("behavior") == expected.behavior,
                "acts": tuple(actual.get("acts", [])) == expected.acts,
                "position": vector_equal(source.get("location_cm", []), expected.position_cm),
                "rotation": vector_equal(source.get("rotation_deg", []), expected.rotation_deg),
                "behavior_param": actual.get("behavior_param", "0") == expected.behavior_param,
                "parent_id": actual.get("parent_id") == expected.parent_id,
                "ue_position": vector_equal(unreal.get("location_cm", []), expected_ue_location),
                "ue_rotation": vector_equal(unreal.get("rotation_deg", []), expected_ue_rotation),
            }
            failed_fields = [name for name, passed in fields.items() if not passed]
            if failed_fields:
                mismatches.append({"stable_id": stable_id, "fields": failed_fields})
        self.add(
            "all_placement_ids_masks_and_transforms_match_decomp",
            "placements",
            not missing and not unexpected and not mismatches,
            "exact stable ID/model/behavior/act/parameter/source+UE transform match",
            {
                "missing": missing,
                "unexpected": unexpected,
                "mismatch_count": len(mismatches),
                "first_mismatches": mismatches[:20],
            },
        )

        expected_red = int(self.truth["course"]["red_coin_count"])
        manifest_red = [item for item in placements if item.get("model") == "macro_red_coin"]
        canonical_red = [item for item in canonical if item.model == "macro_red_coin"]
        red_positions_match = sorted(tuple(item["source_transform"]["location_cm"]) for item in manifest_red) == sorted(
            tuple(item.position_cm) for item in canonical_red
        )
        self.add(
            "eight_red_coins",
            "missions",
            len(manifest_red) == len(canonical_red) == expected_red and red_positions_match,
            {"count": expected_red, "positions": "decomp exact"},
            {"manifest_count": len(manifest_red), "decomp_count": len(canonical_red), "positions_match": red_positions_match},
        )

        course_name, names = parse_us_mission_names(self.decomp_root / "text" / "us" / "courses.h")
        expected_names = [item["name"] for item in self.truth["course"]["missions"]]
        self.add(
            "canonical_course_and_mission_names",
            "missions",
            course_name == self.truth["course"]["name"] and names == expected_names,
            {"course": self.truth["course"]["name"], "missions": expected_names},
            {"course": course_name, "missions": names},
        )

        mission_mismatches: List[Dict[str, Any]] = []
        for mission in self.truth["course"]["missions"]:
            item = by_id.get(mission["stable_id"])
            if not item:
                mission_mismatches.append({"index": mission["index"], "reason": "missing stable_id"})
                continue
            source = item.get("source_transform", {})
            fields = {
                "behavior": item.get("behavior") == mission["behavior"],
                "acts": item.get("acts") == mission["acts"],
                "position": vector_equal(source.get("location_cm", []), mission["position_cm"]),
                "behavior_param": item.get("behavior_param") == mission["behavior_param"],
            }
            failed = [key for key, value in fields.items() if not value]
            if failed:
                mission_mismatches.append({"index": mission["index"], "fields": failed})
        self.add(
            "canonical_mission_targets_masks_and_positions",
            "missions",
            not mission_mismatches,
            "six exact mission target records",
            mission_mismatches,
        )

        landmark_mismatches: List[Dict[str, Any]] = []
        for name, landmark in self.truth["landmarks"].items():
            item = by_id.get(landmark["stable_id"])
            if not item:
                landmark_mismatches.append({"landmark": name, "reason": "missing stable_id"})
                continue
            source = item.get("source_transform", {})
            okay = vector_equal(source.get("location_cm", []), landmark["position_cm"])
            rotation = source.get("rotation_deg", [])
            okay = okay and len(rotation) == 3 and abs(float(rotation[1]) - float(landmark["yaw_deg"])) <= 1.0e-6
            if "behavior_param" in landmark:
                okay = okay and item.get("behavior_param") == landmark["behavior_param"]
            if not okay:
                landmark_mismatches.append({"landmark": name, "stable_id": landmark["stable_id"]})
        self.add(
            "start_and_warp_landmarks",
            "landmarks",
            not landmark_mismatches,
            "special start, entry warp, and both fading warps exact",
            landmark_mismatches,
        )

    def validate_collision(self) -> None:
        collision_path = self.decomp_root / "levels" / "wf" / "areas" / "1" / "collision.inc.c"
        arrays = parse_collision_arrays(collision_path)
        expected = self.truth["terrain"]
        terrain = next((item for item in arrays if item.name == expected["array"]), None)
        if terrain is None:
            raise ValueError("terrain array {} is missing".format(expected["array"]))
        raw_actual = {
            "vertices": terrain.vertex_count,
            "triangles": terrain.triangle_count,
            "surfaces": terrain.surfaces,
            "water_box": terrain.water_boxes[0] if len(terrain.water_boxes) == 1 else list(terrain.water_boxes),
        }
        raw_expected = {
            "vertices": expected["vertices"],
            "triangles": expected["triangles"],
            "surfaces": expected["surfaces"],
            "water_box": expected["water_box"],
        }
        self.add(
            "terrain_decomp_semantic_collision",
            "collision",
            raw_actual == raw_expected,
            raw_expected,
            raw_actual,
            "Nine semantic surface sections sum to exactly 640 triangles.",
        )

        assets = read_json(self.wf_root / "Manifest" / "wf_assets.json")
        records = [item for item in assets.get("collision_assets", []) if item.get("asset") == "Area1"]
        manifest_actual: Any = records
        manifest_passed = False
        if len(records) == 1:
            record = records[0]
            manifest_actual = {
                "name": record.get("name"),
                "vertices": record.get("vertices"),
                "triangles": record.get("triangles"),
                "surfaces": {item["surface"]: item["triangles"] for item in record.get("surface_sections", [])},
                "water_box": record.get("water_boxes", [None])[0] if len(record.get("water_boxes", [])) == 1 else record.get("water_boxes"),
                "source_sha256": record.get("sha256"),
            }
            manifest_expected = {
                "name": expected["array"],
                "vertices": expected["vertices"],
                "triangles": expected["triangles"],
                "surfaces": expected["surfaces"],
                "water_box": expected["water_box"],
                "source_sha256": file_sha256(collision_path),
            }
            manifest_passed = manifest_actual == manifest_expected
        else:
            manifest_expected = "one Area1 collision record"
        self.add(
            "terrain_manifest_matches_decomp",
            "collision",
            manifest_passed,
            manifest_expected,
            manifest_actual,
        )

    def validate_course_definition(self) -> None:
        """Cross-check the generated runtime-facing course manifest against raw decomp data."""

        path = self.wf_root / "Manifest" / "wf_course_definition.json"
        course = read_json(path)
        placements = build_placement_truth(self.decomp_root)
        by_id = {item.stable_id: item for item in placements}
        collision_path = self.decomp_root / "levels" / "wf" / "areas" / "1" / "collision.inc.c"
        terrain = next(
            item
            for item in parse_collision_arrays(collision_path)
            if item.name == self.truth["terrain"]["array"]
        )
        course_name, mission_names = parse_us_mission_names(self.decomp_root / "text" / "us" / "courses.h")

        identity_actual = {
            "format": course.get("format"),
            "course_id": course.get("course_id"),
            "sm64_course_symbol": course.get("sm64_course_symbol"),
            "sm64_course_number": course.get("sm64_course_number"),
            "spyro_level_index": course.get("spyro_level_index"),
            "level_package": course.get("level_package"),
            "return_level": course.get("return_level"),
            "fixed_step_hz": course.get("fixed_step_hz"),
        }
        identity_expected = {
            "format": "spyro64.sm64.course_definition",
            "course_id": "WF",
            "sm64_course_symbol": "COURSE_WF",
            "sm64_course_number": 2,
            "spyro_level_index": 5,
            "level_package": "/Game/Spyro64/Levels/05_Level5",
            "return_level": "/Game/Spyro64/Levels/00_Homeworld",
            "fixed_step_hz": 30,
        }
        self.add(
            "course_definition_identity",
            "course_definition",
            identity_actual == identity_expected and str(course.get("display_name", "")).upper() == course_name,
            {**identity_expected, "display_name_from_decomp": course_name},
            {**identity_actual, "display_name": course.get("display_name")},
        )

        whomp_text = (self.decomp_root / "src" / "game" / "behaviors" / "whomp.inc.c").read_text(
            encoding="utf-8"
        )
        dynamic_match = re.search(
            r"spawn_default_star\s*\(\s*(-?[0-9.]+)f?\s*,\s*(-?[0-9.]+)f?\s*,\s*(-?[0-9.]+)f?\s*\)",
            whomp_text,
        )
        if not dynamic_match:
            raise ValueError("Whomp King spawn_default_star coordinates were not found")
        dynamic_position = tuple(float(value) for value in dynamic_match.groups())
        mission_targets: List[Tuple[Tuple[int, ...], Tuple[float, float, float]]] = []
        king = by_id["wf/object/bhvwhompkingboss/000"]
        mission_targets.append((king.acts, dynamic_position))
        for index in range(2, 7):
            token = "STAR_INDEX_ACT_{}".format(index)
            target = next(item for item in placements if token in item.behavior_param)
            mission_targets.append((target.acts, target.position_cm))

        mission_mismatches: List[Dict[str, Any]] = []
        records = course.get("missions", [])
        if len(records) != 6:
            mission_mismatches.append({"reason": "mission count", "actual": len(records)})
        for index, (name, target) in enumerate(zip(mission_names, mission_targets)):
            if index >= len(records):
                break
            record = records[index]
            acts, position = target
            mask = sum(1 << (act - 1) for act in acts)
            expected_unreal = (position[0], position[2], position[1])
            fields = {
                "star_index": record.get("star_index") == index,
                "name": str(record.get("display_name", "")).upper() == name,
                "preferred_act": record.get("preferred_act") == index + 1,
                "visible_act_mask": record.get("visible_act_mask") == mask,
                "source_location": vector_equal(record.get("source_location_cm", []), position),
                "unreal_location": vector_equal(record.get("unreal_location_cm", []), expected_unreal),
                "eject": record.get("eject_on_collect") is True,
            }
            failed = [field for field, passed in fields.items() if not passed]
            if failed:
                mission_mismatches.append({"mission": index + 1, "fields": failed})
        self.add(
            "course_definition_six_missions",
            "course_definition",
            not mission_mismatches,
            "decomp names, masks, source/UE positions, and eject semantics",
            mission_mismatches,
        )

        start = by_id["wf/special/special_null_start/000"]
        entry = by_id["wf/object/bhvspinairbornewarp/000"]
        fading = [
            by_id["wf/object/bhvfadingwarp/000"],
            by_id["wf/object/bhvfadingwarp/001"],
        ]
        base_record = course.get("entrance", {}).get("mario_base_start", {})
        entry_record = course.get("entrance", {}).get("warp_0A", {})
        warp_records = course.get("warps", [])

        level_script_text = (self.decomp_root / "levels" / "wf" / "script.c").read_text(encoding="utf-8")
        mario_start_match = re.search(
            r"MARIO_POS\s*\(.*?/\*yaw\*/\s*(-?\d+)\s*,\s*/\*pos\*/\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\)",
            level_script_text,
        )
        if not mario_start_match:
            raise ValueError("WF MARIO_POS was not found")
        mario_yaw = float(mario_start_match.group(1))
        mario_position = tuple(float(value) for value in mario_start_match.groups()[1:4])

        def transform_record_matches(record: Mapping[str, Any], placement: PlacementTruth) -> bool:
            return (
                vector_equal(record.get("source_location_cm", []), placement.position_cm)
                and vector_equal(
                    record.get("unreal_location_cm", []),
                    (placement.position_cm[0], placement.position_cm[2], placement.position_cm[1]),
                )
                and abs(float(record.get("source_yaw_deg", 1000000)) - placement.rotation_deg[1]) <= 1.0e-6
                and abs(float(record.get("unreal_yaw_deg", 1000000)) + placement.rotation_deg[1]) <= 1.0e-6
            )

        warp_passed = (
            vector_equal(base_record.get("source_location_cm", []), mario_position)
            and vector_equal(
                base_record.get("unreal_location_cm", []),
                (mario_position[0], mario_position[2], mario_position[1]),
            )
            and abs(float(base_record.get("source_yaw_deg", 1000000)) - mario_yaw) <= 1.0e-6
            and abs(float(base_record.get("unreal_yaw_deg", 1000000)) + mario_yaw) <= 1.0e-6
            and transform_record_matches(entry_record, entry)
            and len(warp_records) == 2
            and all(transform_record_matches(record, placement) for record, placement in zip(warp_records, fading))
            and [(record.get("warp_id"), record.get("destination_warp_id")) for record in warp_records]
            == [("0B", "0C"), ("0C", "0B")]
        )
        self.add(
            "course_definition_entrance_and_warps",
            "course_definition",
            warp_passed,
            "decomp start/0A and reciprocal 0B/0C transforms",
            {"entrance": course.get("entrance"), "warps": warp_records},
        )

        water = course.get("water", {})
        water_box = terrain.water_boxes[0] if len(terrain.water_boxes) == 1 else {}
        surface_map = {
            item.get("surface"): item.get("triangles") for item in course.get("terrain_surface_mappings", [])
        }
        death_values = parse_collision_surface_y_values(
            collision_path, terrain.name, "SURFACE_DEATH_PLANE"
        )
        environment_passed = (
            len(terrain.water_boxes) == 1
            and water.get("source_bounds_xz_cm")
            == [water_box["min_x"], water_box["min_z"], water_box["max_x"], water_box["max_z"]]
            and water.get("unreal_bounds_xy_cm") == water.get("source_bounds_xz_cm")
            and water.get("surface_height_cm") == water_box["height_y"]
            and surface_map == terrain.surfaces
            and death_values == [course.get("death_plane_height_cm")]
        )
        self.add(
            "course_definition_water_death_and_surfaces",
            "course_definition",
            environment_passed,
            {
                "water_box": water_box,
                "death_plane_y": death_values,
                "surfaces": terrain.surfaces,
            },
            {
                "water": water,
                "death_plane_height_cm": course.get("death_plane_height_cm"),
                "surfaces": surface_map,
            },
        )

        bonus = course.get("bonus_star", {})
        rules = course.get("act_rules", {})
        progression_passed = (
            course.get("canonical_fixed_coin_value") == 100
            and course.get("red_coin_count") == 8
            and bonus.get("star_index") == 6
            and bonus.get("coin_threshold") == 100
            and bonus.get("eject_on_collect") is False
            and rules == {"whomp_king": 1, "tower_systems": 62, "buddy_cannon_hoot": 60, "all_acts": 63}
        )
        self.add(
            "course_definition_coin_totals_bonus_and_act_rules",
            "course_definition",
            progression_passed,
            {
                "canonical_fixed_coin_value": 100,
                "red_coin_count": 8,
                "bonus_threshold": 100,
                "bonus_eject": False,
                "act_rules": {"whomp_king": 1, "tower_systems": 62, "buddy_cannon_hoot": 60, "all_acts": 63},
            },
            {
                "canonical_fixed_coin_value": course.get("canonical_fixed_coin_value"),
                "red_coin_count": course.get("red_coin_count"),
                "bonus": bonus,
                "act_rules": rules,
            },
        )

    def validate_excluded_daes(self) -> None:
        assets = read_json(self.wf_root / "Manifest" / "wf_assets.json")
        expected: Mapping[str, str] = self.truth["excluded_daes"]
        excluded_records = assets.get("excluded_daes", [])
        recorded = {item.get("file"): item for item in excluded_records}
        selected = {item.get("dae") for item in assets.get("assets", [])}
        mismatches: List[Dict[str, Any]] = []
        for name, expected_hash in expected.items():
            path = self.dae_dir / name
            record = recorded.get(name)
            actual_hash = file_sha256(path) if path.is_file() else None
            if (
                record is None
                or not record.get("reason")
                or record.get("sha256") != expected_hash
                or actual_hash != expected_hash
                or name in selected
            ):
                mismatches.append(
                    {
                        "file": name,
                        "exists": path.is_file(),
                        "expected_hash": expected_hash,
                        "source_hash": actual_hash,
                        "recorded_hash": record.get("sha256") if record else None,
                        "selected": name in selected,
                    }
                )
        self.add(
            "excluded_dae_policy_and_hashes",
            "dae_policy",
            set(recorded) == set(expected) and not mismatches,
            sorted(expected),
            {"recorded": sorted(recorded), "mismatches": mismatches},
            "Area1/KickWood baked overlays, unused wide tower platform, and BridgeWhole LOD remain excluded.",
        )

    def validate_fbx(self) -> None:
        expected_count = int(self.truth["fbx"]["count"])
        expected_hashes: Mapping[str, str] = self.truth["fbx"]["sha256"]
        fbx_root = self.wf_root / "FBX"
        actual_paths = {
            path.relative_to(fbx_root).as_posix(): path for path in fbx_root.rglob("*.fbx") if path.is_file()
        }
        build = read_json(self.wf_root / "Reports" / "wf_blender_build.json")
        report_records = build.get("fbx_exports", [])
        report_by_path: Dict[str, Dict[str, Any]] = {}
        report_duplicates: List[str] = []
        for item in report_records:
            relative = relative_fbx_path(item.get("file", ""))
            if relative in report_by_path:
                report_duplicates.append(relative)
            report_by_path[relative] = item
        mismatches: List[Dict[str, Any]] = []
        for relative, expected_hash in expected_hashes.items():
            path = actual_paths.get(relative)
            record = report_by_path.get(relative)
            actual_hash = file_sha256(path) if path else None
            if (
                path is None
                or record is None
                or actual_hash != expected_hash
                or record.get("sha256") != expected_hash
                or (path is not None and record.get("bytes") != path.stat().st_size)
            ):
                mismatches.append(
                    {
                        "file": relative,
                        "exists": path is not None,
                        "expected_hash": expected_hash,
                        "actual_hash": actual_hash,
                        "reported_hash": record.get("sha256") if record else None,
                    }
                )
        exact_sets = set(actual_paths) == set(expected_hashes) == set(report_by_path)
        self.add(
            "required_fbx_inventory_and_hashes",
            "fbx",
            (
                expected_count == len(expected_hashes) == len(actual_paths) == len(report_by_path)
                and exact_sets
                and not report_duplicates
                and not mismatches
            ),
            {"count": expected_count, "files": sorted(expected_hashes)},
            {
                "actual_count": len(actual_paths),
                "reported_count": len(report_by_path),
                "sets_exact": exact_sets,
                "duplicates": report_duplicates,
                "mismatches": mismatches,
            },
        )

    def validate_actors(self) -> None:
        expected = self.truth["actors"]
        expected_catalog: Mapping[str, Dict[str, int]] = expected["catalog"]
        catalog_path = self.decomp_root / "tools" / "spyro64_actor_bridge" / "actor_catalog.json"
        catalog = read_json(catalog_path).get("actors", [])
        catalog_by_name = {item["name"]: item for item in catalog}
        manifest = read_json(self.actor_root / "manifest.json")
        records = manifest.get("actors", [])
        record_by_name = {item["actor"]: item for item in records}
        self.add(
            "actor_catalog_inventory",
            "actors",
            (
                len(catalog) == len(records) == int(expected["count"])
                and set(catalog_by_name) == set(record_by_name) == set(expected_catalog)
                and len(record_by_name) == len(records)
            ),
            {"count": expected["count"], "actors": sorted(expected_catalog)},
            {"catalog_count": len(catalog), "manifest_count": len(records), "actors": sorted(record_by_name)},
        )
        self.add(
            "actor_source_rom_provenance",
            "actors",
            manifest.get("source_rom_sha1", "").lower() == expected["source_rom_sha1"].lower(),
            expected["source_rom_sha1"],
            manifest.get("source_rom_sha1"),
        )

        mismatches: List[Dict[str, Any]] = []
        for name, count_truth in expected_catalog.items():
            record = record_by_name.get(name)
            catalog_record = catalog_by_name.get(name)
            if not record or not catalog_record:
                mismatches.append({"actor": name, "reason": "missing record"})
                continue
            json_path = self.actor_root / record["json"]
            # Current bridge manifests keep exact JSON provenance in the root
            # record and place the Blender preview at a stable conventional
            # path. Older manifests also carried explicit blend hash fields.
            blend_relative = record.get("blend", "{0}/{0}.blend".format(name))
            blend_path = self.actor_root / blend_relative
            actor_json = read_json(json_path) if json_path.is_file() else {}
            validation = actor_json.get("validation", {})
            expected_geo = catalog_record.get("geo") or "direct_display_list:{}".format(catalog_record.get("display_list"))
            fields = {
                "source_actor": record.get("source_actor") == catalog_record.get("source"),
                "geo_layout": record.get("geo_layout") == expected_geo,
                "animations": record.get("animation_count") == count_truth["animations"],
                "collision_triangles": record.get("collision_triangle_count") == count_truth["collision_triangles"],
                "render_triangles": record.get("render_triangle_count") == count_truth["render_triangles"],
                "json_validation": (
                    validation.get("animation_count") == count_truth["animations"]
                    and validation.get("collision_triangle_count") == count_truth["collision_triangles"]
                    and validation.get("render_triangle_count") == count_truth["render_triangles"]
                    and validation.get("all_collision_counts_valid") is True
                ),
                "json_hash": json_path.is_file() and file_sha256(json_path) == record.get("json_sha256"),
                "blend_artifact": (
                    blend_path.is_file()
                    and blend_path.stat().st_size > 0
                    and (
                        record.get("blend_sha256") is None
                        or file_sha256(blend_path) == record.get("blend_sha256")
                    )
                    and (
                        record.get("blend_size") is None
                        or blend_path.stat().st_size == record.get("blend_size")
                    )
                ),
            }
            failed = [key for key, value in fields.items() if not value]
            if failed:
                mismatches.append({"actor": name, "fields": failed})
        animation_total = sum(int(item.get("animation_count", 0)) for item in records)
        collision_total = sum(int(item.get("collision_triangle_count", 0)) for item in records)
        self.add(
            "actor_animation_collision_totals_and_artifact_hashes",
            "actors",
            (
                animation_total == int(expected["animation_total"])
                and collision_total == int(expected["collision_triangle_total"])
                and not mismatches
            ),
            {
                "animations": expected["animation_total"],
                "collision_triangles": expected["collision_triangle_total"],
                "artifact_hashes": "manifest exact",
            },
            {"animations": animation_total, "collision_triangles": collision_total, "mismatches": mismatches},
        )

    def validate_runtime(self) -> None:
        expected = self.truth["runtime"]
        public_dir = self.plugin_root / "Source" / "SM64Runtime" / "Public"
        private_dir = self.plugin_root / "Source" / "SM64Runtime" / "Private"
        actual_headers = sorted(path.name for path in public_dir.glob("*.h"))
        actual_sources = sorted(path.name for path in private_dir.glob("*.cpp"))
        self.add(
            "runtime_source_inventory",
            "runtime",
            set(expected["public_headers"]).issubset(actual_headers)
            and set(expected["private_sources"]).issubset(actual_sources),
            {"headers": sorted(expected["public_headers"]), "sources": sorted(expected["private_sources"])},
            {"headers": actual_headers, "sources": actual_sources},
        )

        header_text = "\n".join(path.read_text(encoding="utf-8") for path in sorted(public_dir.glob("*.h")))
        enums = sorted(set(re.findall(r"UENUM\s*\([^)]*\)\s*enum\s+class\s+(E[A-Za-z0-9_]+)", header_text, re.DOTALL)))
        structs = sorted(
            set(
                re.findall(
                    r"USTRUCT\s*\([^)]*\)\s*struct\s+(?:[A-Za-z0-9_]+\s+)*(F[A-Za-z0-9_]+)\s*\{",
                    header_text,
                    re.DOTALL,
                )
            )
        )
        uclasses = set(
            re.findall(
                r"UCLASS\s*\([^)]*\)\s*class\s+(?:[A-Za-z0-9_]+\s+)*([UA][A-Za-z0-9_]+)\s*:",
                header_text,
                re.DOTALL,
            )
        )
        uinterfaces = set(
            re.findall(
                r"UINTERFACE\s*\([^)]*\)\s*class\s+(?:[A-Za-z0-9_]+\s+)*(U[A-Za-z0-9_]+)\s*:",
                header_text,
                re.DOTALL,
            )
        )
        classes = sorted(uclasses | uinterfaces)
        native_interfaces = sorted(
            set(
                re.findall(
                    r"class\s+(?:[A-Za-z0-9_]+\s+)*(I[A-Za-z0-9_]+)\s*\{\s*GENERATED_BODY",
                    header_text,
                    re.DOTALL,
                )
            )
        )
        actual_reflection = {
            "enums": enums,
            "structs": structs,
            "classes": classes,
            "native_interfaces": native_interfaces,
        }
        expected_reflection = {
            "enums": sorted(expected["enums"]),
            "structs": sorted(expected["structs"]),
            "classes": sorted(expected["classes"]),
            "native_interfaces": sorted(expected["native_interfaces"]),
        }
        self.add(
            "runtime_reflected_type_inventory",
            "runtime",
            all(
                set(expected_reflection[key]).issubset(actual_reflection[key])
                for key in expected_reflection
            ),
            expected_reflection,
            actual_reflection,
        )

        plugin = read_json(self.plugin_root / "SM64Runtime.uplugin")
        modules = plugin.get("Modules", [])
        self.add(
            "runtime_plugin_descriptor",
            "runtime",
            (
                plugin.get("EnabledByDefault") is True
                and plugin.get("CanContainContent") is False
                and any(
                    module.get("Name") == "SM64Runtime" and module.get("Type") == "Runtime"
                    for module in modules
                )
            ),
            "enabled native Runtime module",
            plugin,
        )

        moving_source = (private_dir / "SM64MovingPlatformBase.cpp").read_text(encoding="utf-8")
        moving_header = (public_dir / "SM64MovingPlatformBase.h").read_text(encoding="utf-8")
        required_tokens = [
            "StepAccumulator += FMath::Max(0.0f, DeltaSeconds)",
            "while (StepAccumulator + SMALL_NUMBER >= StepSeconds && CatchUpSteps < 240)",
            "case ESM64PlatformMotion::Sliding:",
            "case ESM64PlatformMotion::SmallBomp:",
            "case ESM64PlatformMotion::LargeBomp:",
            "case ESM64PlatformMotion::RotatingWood:",
            "case ESM64PlatformMotion::RotatingContinuous:",
            "case ESM64PlatformMotion::TowerSliding:",
            "case ESM64PlatformMotion::TowerElevator:",
            "CurrentForwardSpeed = 30.0f",
            "CurrentForwardSpeed = 25.0f",
            "float SimulationHz = 30.0f",
            "float RotationDegreesPerFrame = 0.703125f",
        ]
        combined = moving_source + "\n" + moving_header
        missing_tokens = [token for token in required_tokens if token not in combined]
        self.add(
            "native_fixed_step_mover_contract",
            "runtime",
            not missing_tokens,
            required_tokens,
            {"missing_tokens": missing_tokens},
        )

    def validate_movers(self) -> None:
        self.mover_result = run_mover_acceptance()
        self.add(
            "fixed_step_schedule_equivalence",
            "mover_simulation",
            bool(self.mover_result.get("passed")),
            {
                "schedule_comparisons": "27/27",
                "checkpoint_assertions": "513/513",
                "motion_invariants": "9/9",
            },
            {
                "schedule_comparisons": "{}/{}".format(
                    self.mover_result.get("schedule_comparison_passes"),
                    self.mover_result.get("schedule_comparison_count"),
                ),
                "checkpoint_assertions": "{}/{}".format(
                    self.mover_result.get("checkpoint_passes"),
                    self.mover_result.get("checkpoint_assertion_count"),
                ),
                "motion_invariants": "{}/{}".format(
                    self.mover_result.get("invariant_passes"),
                    self.mover_result.get("invariant_count"),
                ),
            },
            "Nine mover scenarios are fed equal elapsed time at 30/60/120 FPS and through a repeatable hitch mix.",
        )

    def run(self) -> Dict[str, Any]:
        for category, callback in (
            ("placements", self.validate_placements),
            ("collision", self.validate_collision),
            ("course_definition", self.validate_course_definition),
            ("dae_policy", self.validate_excluded_daes),
            ("fbx", self.validate_fbx),
            ("actors", self.validate_actors),
            ("runtime", self.validate_runtime),
            ("mover_simulation", self.validate_movers),
        ):
            self.guard(category, callback)
        passed = all(item["passed"] for item in self.checks)
        manifest_checks = [item for item in self.checks if item["category"] != "mover_simulation"]
        categories: Dict[str, Dict[str, int]] = {}
        for item in self.checks:
            summary = categories.setdefault(item["category"], {"passed": 0, "count": 0})
            summary["count"] += 1
            summary["passed"] += int(bool(item["passed"]))
        return {
            "format": "spyro64.wf_acceptance_report",
            "version": 1,
            "generated_at_utc": datetime.now(timezone.utc).isoformat(),
            "passed": passed,
            "roots": {
                "content": str(self.content_root),
                "decomp": str(self.decomp_root),
                "dae": str(self.dae_dir),
                "plugin": str(self.plugin_root),
                "truth": str(self.truth_path),
            },
            "summary": {
                "manifest_artifact_passes": sum(bool(item["passed"]) for item in manifest_checks),
                "manifest_artifact_check_count": len(manifest_checks),
                "categories": categories,
                "mover_schedule_comparison_passes": self.mover_result.get("schedule_comparison_passes", 0),
                "mover_schedule_comparison_count": self.mover_result.get("schedule_comparison_count", 0),
                "mover_checkpoint_passes": self.mover_result.get("checkpoint_passes", 0),
                "mover_checkpoint_assertion_count": self.mover_result.get("checkpoint_assertion_count", 0),
                "mover_invariant_passes": self.mover_result.get("invariant_passes", 0),
                "mover_invariant_count": self.mover_result.get("invariant_count", 0),
            },
            "checks": self.checks,
            "mover_simulation": self.mover_result,
        }


def human_report(report: Mapping[str, Any]) -> str:
    summary = report["summary"]
    lines = [
        "Whomp's Fortress offline acceptance",
        "====================================",
        "Result: {}".format("PASS" if report["passed"] else "FAIL"),
        "Manifest/decomp/artifact checks: {}/{}".format(
            summary["manifest_artifact_passes"], summary["manifest_artifact_check_count"]
        ),
        "Mover schedule comparisons: {}/{}".format(
            summary["mover_schedule_comparison_passes"], summary["mover_schedule_comparison_count"]
        ),
        "Mover integer-frame checkpoints: {}/{}".format(
            summary["mover_checkpoint_passes"], summary["mover_checkpoint_assertion_count"]
        ),
        "Mover canonical invariants: {}/{}".format(
            summary["mover_invariant_passes"], summary["mover_invariant_count"]
        ),
        "",
        "Check results",
        "-------------",
    ]
    for item in report["checks"]:
        lines.append("[{}] {}/{}: {}".format("PASS" if item["passed"] else "FAIL", item["category"], item["name"], item["detail"]))
        if not item["passed"]:
            lines.append("  expected: {}".format(json.dumps(item["expected"], sort_keys=True)))
            lines.append("  actual:   {}".format(json.dumps(item["actual"], sort_keys=True)))
    lines.extend(
        [
            "",
            "Authoritative inputs",
            "--------------------",
            "Decomp: {}".format(report["roots"]["decomp"]),
            "Location DAEs: {}".format(report["roots"]["dae"]),
            "Generated source: {}".format(Path(report["roots"]["content"]) / "SM64" / "Source"),
            "Runtime plugin: {}".format(report["roots"]["plugin"]),
            "Truth snapshot: {}".format(report["roots"]["truth"]),
        ]
    )
    return "\n".join(lines) + "\n"


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--content-root", type=Path, default=DEFAULT_CONTENT_ROOT)
    parser.add_argument("--decomp-root", type=Path, default=DEFAULT_DECOMP_ROOT)
    parser.add_argument("--dae-dir", type=Path, default=DEFAULT_DAE_DIR)
    parser.add_argument("--plugin-root", type=Path, default=DEFAULT_PLUGIN_ROOT)
    parser.add_argument("--truth", type=Path, default=HERE / "expected_truth.json")
    parser.add_argument("--report-dir", type=Path, default=HERE / "Reports")
    parser.add_argument("--no-write", action="store_true", help="Run the gate without updating report files")
    parser.add_argument("--json-stdout", action="store_true", help="Print the complete JSON report")
    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = build_parser().parse_args(argv)
    acceptance = Acceptance(
        args.content_root,
        args.decomp_root,
        args.dae_dir,
        args.plugin_root,
        args.truth,
    )
    report = acceptance.run()
    text = human_report(report)
    if not args.no_write:
        args.report_dir.mkdir(parents=True, exist_ok=True)
        (args.report_dir / "wf_acceptance_report.json").write_text(
            json.dumps(report, indent=2) + "\n", encoding="utf-8"
        )
        (args.report_dir / "wf_acceptance_report.txt").write_text(text, encoding="utf-8")
    if args.json_stdout:
        print(json.dumps(report, indent=2))
    else:
        print(text, end="")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
