"""Small, dependency-free parsers for the authoritative Whomp's Fortress C data.

This module intentionally does not import ``tools.spyro64_pipeline``.  Acceptance
therefore exercises a second implementation instead of asking the generator to
validate its own output.
"""

from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass
from math import cos, radians, sin
from pathlib import Path
import re
from typing import Dict, Iterable, Iterator, List, Optional, Sequence, Tuple


ALL_ACTS: Tuple[int, ...] = (1, 2, 3, 4, 5, 6)


@dataclass(frozen=True)
class PlacementTruth:
    stable_id: str
    source_kind: str
    model: str
    behavior: str
    acts: Tuple[int, ...]
    position_cm: Tuple[float, float, float]
    rotation_deg: Tuple[float, float, float]
    behavior_param: str = "0"
    parent_id: Optional[str] = None


@dataclass(frozen=True)
class CollisionTruth:
    name: str
    vertex_count: int
    surfaces: Dict[str, int]
    water_boxes: Tuple[Dict[str, int], ...]

    @property
    def triangle_count(self) -> int:
        return sum(self.surfaces.values())


def slug(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9]+", "_", value).strip("_").lower() or "placement"


def normalize_object_yaw(value: float) -> float:
    """Decode converter-style unsigned negative degrees without changing ordinary 315."""

    if value > 32767.0:
        value -= 65536.0
    elif value < -32768.0:
        value += 65536.0
    return value


def find_macro_calls(text: str, names: Iterable[str]) -> Iterator[Tuple[str, str, int]]:
    """Yield balanced macro calls while respecting comments and quoted strings."""

    ordered = sorted(set(names), key=len, reverse=True)
    pattern = re.compile(r"\b(" + "|".join(re.escape(name) for name in ordered) + r")\s*\(")
    cursor = 0
    while True:
        match = pattern.search(text, cursor)
        if match is None:
            return
        index = match.end()
        depth = 1
        quote: Optional[str] = None
        while index < len(text) and depth:
            char = text[index]
            previous = text[index - 1] if index else ""
            if quote:
                if char == quote and previous != "\\":
                    quote = None
            elif char in ('"', "'"):
                quote = char
            elif text.startswith("//", index):
                newline = text.find("\n", index + 2)
                index = len(text) if newline < 0 else newline
                continue
            elif text.startswith("/*", index):
                end = text.find("*/", index + 2)
                if end < 0:
                    raise ValueError("unterminated block comment in {}".format(match.group(1)))
                index = end + 2
                continue
            elif char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
            index += 1
        if depth:
            raise ValueError("unterminated macro call {}".format(match.group(1)))
        yield match.group(1), text[match.end() : index - 1], match.start()
        cursor = index


def parse_acts(expression: Optional[str]) -> Tuple[int, ...]:
    if not expression or "ALL_ACTS" in expression:
        return ALL_ACTS
    acts = tuple(sorted({int(value) for value in re.findall(r"\bACT_([1-6])\b", expression)}))
    return acts or ALL_ACTS


def compact_expression(value: str) -> str:
    return " ".join(value.split())


def parse_level_script(path: Path, counters: defaultdict) -> List[PlacementTruth]:
    text = path.read_text(encoding="utf-8")
    output: List[PlacementTruth] = []
    for _macro, body, _offset in find_macro_calls(text, ("OBJECT_WITH_ACTS", "OBJECT")):
        model = re.search(r"/\*model\*/\s*([A-Za-z0-9_]+)", body)
        position = re.search(r"/\*pos\*/\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)", body)
        angle = re.search(r"/\*angle\*/\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)", body)
        behavior = re.search(r"/\*bhv\*/\s*([A-Za-z0-9_]+)", body)
        if not (model and position and angle and behavior):
            continue
        model_name = model.group(1)
        behavior_name = behavior.group(1)
        identity = model_name if behavior_name == "bhvStaticObject" else behavior_name
        ordinal = counters[identity]
        counters[identity] += 1
        acts_match = re.search(r"/\*acts\*/\s*(.*)$", body, re.DOTALL)
        param_match = re.search(r"/\*bhvParam\*/\s*(.*?)\s*,\s*/\*bhv\*/", body, re.DOTALL)
        pitch, yaw, roll = (float(value) for value in angle.groups())
        output.append(
            PlacementTruth(
                stable_id="wf/object/{}/{:03d}".format(slug(identity), ordinal),
                source_kind="level_script",
                model=model_name,
                behavior=behavior_name,
                acts=parse_acts(acts_match.group(1) if acts_match else None),
                position_cm=tuple(float(value) for value in position.groups()),
                rotation_deg=(pitch, normalize_object_yaw(yaw), roll),
                behavior_param=compact_expression(param_match.group(1)) if param_match else "0",
            )
        )
    return output


def parse_macro_objects(path: Path, counters: defaultdict) -> List[PlacementTruth]:
    text = path.read_text(encoding="utf-8")
    output: List[PlacementTruth] = []
    for _macro, body, _offset in find_macro_calls(text, ("MACRO_OBJECT_WITH_BHV_PARAM", "MACRO_OBJECT")):
        preset = re.search(r"/\*preset\*/\s*([A-Za-z0-9_]+)", body)
        yaw = re.search(r"/\*yaw\*/\s*(-?\d+)", body)
        position = re.search(r"/\*pos\*/\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)", body)
        if not (preset and yaw and position):
            continue
        name = preset.group(1)
        ordinal = counters[name]
        counters[name] += 1
        param_match = re.search(r"/\*bhvParam\*/\s*([^,]+)$", body)
        output.append(
            PlacementTruth(
                stable_id="wf/macro/{}/{:03d}".format(slug(name), ordinal),
                source_kind="macro_object",
                model=name,
                behavior=name,
                acts=ALL_ACTS,
                position_cm=tuple(float(value) for value in position.groups()),
                rotation_deg=(0.0, float(yaw.group(1)), 0.0),
                behavior_param=compact_expression(param_match.group(1)) if param_match else "0",
            )
        )
    return output


def parse_special_objects(path: Path, counters: defaultdict) -> List[PlacementTruth]:
    text = path.read_text(encoding="utf-8")
    output: List[PlacementTruth] = []
    for _macro, body, _offset in find_macro_calls(text, ("SPECIAL_OBJECT_WITH_YAW", "SPECIAL_OBJECT")):
        preset = re.search(r"/\*preset\*/\s*([A-Za-z0-9_]+)", body)
        position = re.search(r"/\*pos\*/\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)", body)
        if not (preset and position):
            continue
        name = preset.group(1)
        yaw_match = re.search(r"/\*yaw\*/\s*(-?\d+)", body)
        yaw = float(int(yaw_match.group(1)) * 360.0 / 256.0) if yaw_match else 0.0
        ordinal = counters[name]
        counters[name] += 1
        output.append(
            PlacementTruth(
                stable_id="wf/special/{}/{:03d}".format(slug(name), ordinal),
                source_kind="collision_special",
                model=name,
                behavior=name,
                acts=ALL_ACTS,
                position_cm=tuple(float(value) for value in position.groups()),
                rotation_deg=(0.0, yaw, 0.0),
            )
        )
    return output


def parse_tumbling_constants(path: Path) -> Tuple[int, int, int]:
    text = path.read_text(encoding="utf-8")
    match = re.search(
        r"/\*\s*TUMBLING_BRIDGE_BP_WF\s*\*/\s*\{\s*(\d+)\s*,\s*(-?\d+)\s*,\s*(\d+)",
        text,
    )
    if not match:
        raise ValueError("WF tumbling bridge constants were not found")
    return tuple(int(value) for value in match.groups())


def parse_tower_constants(path: Path) -> Tuple[float, float, float, float, List[str]]:
    text = path.read_text(encoding="utf-8")

    def assignment(field: str) -> float:
        match = re.search(
            r"o->{}\s*=\s*(0[xX][0-9A-Fa-f]+|[0-9]+(?:\.[0-9]+)?)\s*f?\s*;".format(
                re.escape(field)
            ),
            text,
        )
        if not match:
            raise ValueError("tower constant {} was not found".format(field))
        return float(int(match.group(1), 0)) if match.group(1).lower().startswith("0x") else float(match.group(1))

    angle_units = assignment("oPlatformSpawnerUnkFC")
    yaw_step = angle_units * 360.0 / 65536.0
    radius = assignment("oPlatformSpawnerUnk100")
    distance = assignment("oPlatformSpawnerUnk104")
    speed = assignment("oPlatformSpawnerUnk108")
    function_match = re.search(r"void\s+spawn_wf_platform_group\s*\([^)]*\)\s*\{(.*?)\n\}", text, re.DOTALL)
    if not function_match:
        raise ValueError("spawn_wf_platform_group was not found")
    behaviors = re.findall(
        r"spawn_and_init_wf_platforms\s*\(\s*[^,]+,\s*(bhv[A-Za-z0-9_]+)\s*\)",
        function_match.group(1),
    )
    return yaw_step, radius, distance, speed, behaviors


def build_generated_children(decomp_root: Path, placements: Sequence[PlacementTruth]) -> List[PlacementTruth]:
    output: List[PlacementTruth] = []
    bridge = next(item for item in placements if item.behavior == "bhvTumblingBridge")
    count, start, width = parse_tumbling_constants(decomp_root / "src/game/behaviors/tumbling_bridge.inc.c")
    for index in range(count):
        x, y, z = bridge.position_cm
        output.append(
            PlacementTruth(
                stable_id="wf/generated/tumbling_bridge_piece/{:02d}".format(index),
                source_kind="behavior_generated",
                model="MODEL_WF_TUMBLING_BRIDGE_PART",
                behavior="bhvTumblingBridgePlatform",
                acts=ALL_ACTS,
                position_cm=(x, y, z + start + width * index),
                rotation_deg=bridge.rotation_deg,
                parent_id=bridge.stable_id,
            )
        )

    tower = next(item for item in placements if item.behavior == "bhvTowerPlatformGroup")
    yaw_step, radius, _distance, _speed, behaviors = parse_tower_constants(
        decomp_root / "src/game/behaviors/tower_platform.inc.c"
    )
    if len(behaviors) != 8:
        raise ValueError("expected 8 tower platform spawns, found {}".format(len(behaviors)))
    x, y, z = tower.position_cm
    for index, behavior in enumerate(behaviors):
        yaw = yaw_step * index
        output.append(
            PlacementTruth(
                stable_id="wf/generated/tower_platform/{:02d}".format(index),
                source_kind="behavior_generated",
                model=(
                    "MODEL_WF_TOWER_SQUARE_PLATORM_ELEVATOR"
                    if behavior == "bhvWFElevatorTowerPlatform"
                    else "MODEL_WF_TOWER_SQUARE_PLATORM"
                ),
                behavior=behavior,
                acts=(2, 3, 4, 5, 6),
                position_cm=(
                    x + radius * sin(radians(yaw)),
                    y + 100.0 * index,
                    z + radius * cos(radians(yaw)),
                ),
                rotation_deg=(0.0, yaw, 0.0),
                parent_id=tower.stable_id,
            )
        )
    return output


def build_placement_truth(decomp_root: Path) -> List[PlacementTruth]:
    counters: defaultdict = defaultdict(int)
    output = [
        PlacementTruth(
            stable_id="wf/render/area1",
            source_kind="render_root",
            model="WF_AREA_1",
            behavior="static_render_root",
            acts=ALL_ACTS,
            position_cm=(0.0, 0.0, 0.0),
            rotation_deg=(0.0, 0.0, 0.0),
        )
    ]
    output.extend(parse_level_script(decomp_root / "levels/wf/script.c", counters))
    output.extend(parse_macro_objects(decomp_root / "levels/wf/areas/1/macro.inc.c", counters))
    output.extend(parse_special_objects(decomp_root / "levels/wf/areas/1/collision.inc.c", counters))
    output.extend(build_generated_children(decomp_root, output))
    return output


def parse_collision_arrays(path: Path) -> List[CollisionTruth]:
    text = path.read_text(encoding="utf-8")
    array_pattern = re.compile(
        r"const\s+Collision\s+([A-Za-z0-9_]+)\s*\[\s*\]\s*=\s*\{(.*?)\n\s*\};",
        re.DOTALL,
    )
    output: List[CollisionTruth] = []
    for name, body in array_pattern.findall(text):
        declared_match = re.search(r"COL_VERTEX_INIT\s*\(\s*([^\)]+)\)", body)
        if not declared_match:
            continue
        declared = int(declared_match.group(1).strip(), 0)
        vertices = re.findall(r"COL_VERTEX\s*\(\s*-?\d+\s*,\s*-?\d+\s*,\s*-?\d+\s*\)", body)
        if len(vertices) != declared:
            raise ValueError("{} declares {} vertices but contains {}".format(name, declared, len(vertices)))
        token = re.compile(
            r"COL_TRI_INIT\s*\(\s*([A-Za-z0-9_]+)\s*,\s*([^\)]+)\)"
            r"|COL_TRI\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)"
        )
        surfaces: Dict[str, int] = {}
        expected: Dict[str, int] = {}
        current: Optional[str] = None
        for match in token.finditer(body):
            if match.group(1):
                current = match.group(1)
                expected[current] = int(match.group(2).strip(), 0)
                surfaces[current] = 0
            else:
                if current is None:
                    raise ValueError("{} has a triangle before its surface header".format(name))
                surfaces[current] += 1
        if surfaces != expected:
            raise ValueError("{} surface declarations do not match parsed triangles".format(name))
        water = tuple(
            {
                key: int(value)
                for key, value in zip(
                    ("box_id", "min_x", "min_z", "max_x", "max_z", "height_y"),
                    match,
                )
            }
            for match in re.findall(
                r"COL_WATER_BOX\s*\(\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\)",
                body,
            )
        )
        output.append(CollisionTruth(name, declared, surfaces, water))
    if not output:
        raise ValueError("no collision arrays found in {}".format(path))
    return output


def parse_collision_surface_y_values(path: Path, array_name: str, surface_name: str) -> List[int]:
    """Return the distinct source Y coordinates referenced by one collision surface."""

    text = path.read_text(encoding="utf-8")
    match = re.search(
        r"const\s+Collision\s+{}\s*\[\s*\]\s*=\s*\{{(.*?)\n\s*\}};".format(
            re.escape(array_name)
        ),
        text,
        re.DOTALL,
    )
    if not match:
        raise ValueError("collision array {} was not found".format(array_name))
    body = match.group(1)
    vertices = [
        tuple(int(value) for value in item)
        for item in re.findall(
            r"COL_VERTEX\s*\(\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\)", body
        )
    ]
    token = re.compile(
        r"COL_TRI_INIT\s*\(\s*([A-Za-z0-9_]+)\s*,\s*[^\)]+\)"
        r"|COL_TRI\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)"
    )
    current: Optional[str] = None
    indices = set()
    for item in token.finditer(body):
        if item.group(1):
            current = item.group(1)
        elif current == surface_name:
            indices.update(int(value) for value in item.groups()[1:4])
    if not indices:
        raise ValueError("surface {} was not found in {}".format(surface_name, array_name))
    if max(indices) >= len(vertices):
        raise ValueError("surface {} references an invalid vertex".format(surface_name))
    return sorted({vertices[index][1] for index in indices})


def parse_us_mission_names(path: Path) -> Tuple[str, List[str]]:
    text = path.read_text(encoding="utf-8")
    for _macro, body, _offset in find_macro_calls(text, ("COURSE_ACTS",)):
        if not re.match(r"\s*COURSE_WF\b", body):
            continue
        strings = re.findall(r'_\(\s*"([^"\\]*(?:\\.[^"\\]*)*)"\s*\)', body)
        if len(strings) != 7:
            raise ValueError("COURSE_WF contains {} translated strings, expected 7".format(len(strings)))
        course_name = re.sub(r"^\s*\d+\s*", "", strings[0])
        return course_name, strings[1:]
    raise ValueError("COURSE_WF was not found in {}".format(path))
