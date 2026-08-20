"""Pure 30 Hz acceptance model for the native SM64 moving-platform families."""

from __future__ import annotations

from dataclasses import dataclass, replace
from fractions import Fraction
from math import floor
from typing import Dict, Iterable, List, Mapping, Sequence, Tuple


STEP_HZ = 30
TOTAL_FRAMES = 420
CHECKPOINTS: Tuple[int, ...] = (
    1,
    30,
    60,
    61,
    62,
    100,
    101,
    102,
    117,
    118,
    150,
    180,
    204,
    240,
    270,
    300,
    346,
    360,
    420,
)


@dataclass(frozen=True)
class MotionSpec:
    name: str
    motion: str
    speed: float = 15.0
    travel: float = 510.0
    rotation_per_frame: float = 0.703125
    trigger_on_start: bool = False


@dataclass
class MotionState:
    simulation_frame: int = 0
    action: int = 0
    timer: int = 0
    x: float = 0.0
    z: float = 0.0
    yaw: float = 0.0
    pitch: float = 0.0
    roll: float = 0.0
    vertical_velocity: float = 0.0
    pitch_velocity: float = 0.0
    roll_velocity: float = 0.0
    current_forward_speed: float = 0.0


SPECS: Tuple[MotionSpec, ...] = (
    MotionSpec("sliding_10", "Sliding", speed=10.0),
    MotionSpec("sliding_15", "Sliding", speed=15.0),
    MotionSpec("sliding_20", "Sliding", speed=20.0),
    MotionSpec("small_bomp", "SmallBomp"),
    MotionSpec("large_bomp", "LargeBomp"),
    MotionSpec("rotating_wood", "RotatingWood"),
    MotionSpec("rotating_continuous", "RotatingContinuous"),
    MotionSpec("tower_sliding", "TowerSliding", speed=3.0, travel=380.0),
    MotionSpec("tower_elevator", "TowerElevator", trigger_on_start=True),
)


def normalize_angle(value: float) -> float:
    while value >= 180.0:
        value -= 360.0
    while value < -180.0:
        value += 360.0
    return value


def set_action(state: MotionState, action: int, inside_step: bool = True) -> None:
    state.action = action
    state.timer = -1 if inside_step else 0


def initial_state(spec: MotionSpec) -> MotionState:
    state = MotionState()
    if spec.motion == "Sliding":
        # ResetMotion permanently applies the decomp's +2 X initialization.
        state.x = 2.0
    if spec.trigger_on_start:
        set_action(state, 1, inside_step=False)
    return state


def step_motion(spec: MotionSpec, state: MotionState) -> None:
    motion = spec.motion
    if motion == "Sliding":
        home_x = 2.0
        if state.action == 0:
            if state.timer > 100:
                set_action(state, 1)
        elif state.action == 1:
            state.x += spec.speed
            if state.timer >= floor(500.0 / max(1.0, spec.speed)):
                state.x = home_x + spec.travel
            if state.timer == 60:
                set_action(state, 2)
        else:
            state.x -= spec.speed
            if state.timer >= floor(500.0 / max(1.0, spec.speed)):
                state.x = home_x
            if state.timer == 90:
                set_action(state, 1)

    elif motion in ("SmallBomp", "LargeBomp"):
        if state.action == 0:
            if state.timer > 100:
                state.current_forward_speed = 30.0
                set_action(state, 1)
        elif state.action == 1:
            state.x = min(state.x + state.current_forward_speed, 150.0)
            if state.timer == 15:
                set_action(state, 2)
        elif state.action == 2:
            extend_speed = 40.0 if motion == "SmallBomp" else 10.0
            state.x = min(state.x + extend_speed, 530.0)
            if state.timer == 60:
                set_action(state, 3)
        else:
            state.x = max(state.x - 10.0, 30.0)
            if state.timer == 90:
                state.current_forward_speed = 25.0
                set_action(state, 1)

    elif motion == "RotatingWood":
        if state.action == 0:
            if state.timer > 60:
                set_action(state, 1)
        else:
            state.yaw += 0x100 * (360.0 / 65536.0)
            if state.timer > 126:
                set_action(state, 0)

    elif motion == "RotatingContinuous":
        state.yaw += spec.rotation_per_frame

    elif motion == "TowerSliding":
        half_cycle = floor(spec.travel / max(1.0, spec.speed))
        state.x += spec.speed * (-1.0 if state.action == 0 else 1.0)
        if state.timer > half_cycle:
            set_action(state, 1 if state.action == 0 else 0)

    elif motion == "TowerElevator":
        if state.action == 1:
            if state.timer > 140:
                set_action(state, 2)
            else:
                state.z += 5.0
        elif state.action == 2:
            if state.timer > 60:
                set_action(state, 3)
        elif state.action == 3:
            if state.timer > 140:
                state.z = 0.0
                set_action(state, 0)
            else:
                state.z -= 5.0

    else:
        raise ValueError("unsupported motion {}".format(motion))

    state.yaw = normalize_angle(state.yaw)
    state.pitch = normalize_angle(state.pitch)
    state.roll = normalize_angle(state.roll)
    state.timer += 1
    state.simulation_frame += 1


def state_key(state: MotionState) -> Tuple[object, ...]:
    return (
        state.simulation_frame,
        state.action,
        state.timer,
        round(state.x, 6),
        round(state.z, 6),
        round(state.yaw, 6),
        round(state.pitch, 6),
        round(state.roll, 6),
    )


def uniform_schedule(fps: int, total_frames: int = TOTAL_FRAMES) -> List[float]:
    count = total_frames * fps // STEP_HZ
    return [1.0 / float(fps)] * count


def hitch_schedule(total_frames: int = TOTAL_FRAMES) -> List[float]:
    """A 120/60/30 mix with repeatable 233 ms and 50 ms hitches."""

    total = Fraction(total_frames, STEP_HZ)
    pattern = (
        Fraction(1, 120),
        Fraction(1, 60),
        Fraction(7, 30),
        Fraction(1, 120),
        Fraction(1, 20),
        Fraction(1, 30),
    )
    values: List[Fraction] = []
    elapsed = Fraction(0, 1)
    index = 0
    while elapsed < total:
        value = min(pattern[index % len(pattern)], total - elapsed)
        values.append(value)
        elapsed += value
        index += 1
    return [float(value) for value in values]


def simulate_schedule(
    spec: MotionSpec,
    deltas: Iterable[float],
    total_frames: int = TOTAL_FRAMES,
) -> Dict[int, Tuple[object, ...]]:
    state = initial_state(spec)
    accumulator = 0.0
    step_seconds = 1.0 / STEP_HZ
    history: Dict[int, Tuple[object, ...]] = {}
    for delta in deltas:
        accumulator += max(0.0, delta)
        catch_up = 0
        while accumulator + 1.0e-8 >= step_seconds and catch_up < 240:
            accumulator -= step_seconds
            step_motion(spec, state)
            if state.simulation_frame > total_frames:
                raise AssertionError("schedule produced more than {} simulation frames".format(total_frames))
            history[state.simulation_frame] = state_key(state)
            catch_up += 1
    if state.simulation_frame != total_frames:
        raise AssertionError(
            "{} produced {} simulation frames, expected {}".format(
                spec.name, state.simulation_frame, total_frames
            )
        )
    return history


def run_mover_acceptance() -> Dict[str, object]:
    schedules: Mapping[str, Sequence[float]] = {
        "30_fps": uniform_schedule(30),
        "60_fps": uniform_schedule(60),
        "120_fps": uniform_schedule(120),
        "hitch_mix": hitch_schedule(),
    }
    comparisons: List[Dict[str, object]] = []
    invariants: List[Dict[str, object]] = []
    checkpoint_assertions = 0
    checkpoint_passes = 0
    for spec in SPECS:
        histories = {name: simulate_schedule(spec, deltas) for name, deltas in schedules.items()}
        baseline = histories["30_fps"]
        for schedule_name in ("60_fps", "120_fps", "hitch_mix"):
            actual = histories[schedule_name]
            mismatches = []
            for frame in CHECKPOINTS:
                checkpoint_assertions += 1
                if actual[frame] == baseline[frame]:
                    checkpoint_passes += 1
                else:
                    mismatches.append(
                        {"frame": frame, "expected": baseline[frame], "actual": actual[frame]}
                    )
            comparisons.append(
                {
                    "scenario": spec.name,
                    "schedule": schedule_name,
                    "passed": not mismatches and actual == baseline,
                    "checkpoint_count": len(CHECKPOINTS),
                    "mismatches": mismatches,
                    "full_history_equal": actual == baseline,
                }
            )

        states = list(baseline.values())
        if spec.motion == "Sliding":
            passed = min(item[3] for item in states) >= 2.0 and max(item[3] for item in states) == 512.0
            detail = "bounded at source-init X=2 and source-init+510"
        elif spec.motion in ("SmallBomp", "LargeBomp"):
            passed = max(item[3] for item in states) == 530.0 and min(item[3] for item in states) == 0.0
            detail = "reaches the exact 530-unit extension without crossing home"
        elif spec.motion == "RotatingContinuous":
            passed = baseline[128][5] == 90.0
            detail = "0x80 angle units/frame reaches 90 degrees at frame 128"
        elif spec.motion == "RotatingWood":
            passed = any(item[1] == 1 for item in states) and any(item[5] != 0.0 for item in states)
            detail = "enters the 0x100 angle-unit rotation action after the 60-frame wait"
        elif spec.motion == "TowerSliding":
            passed = min(item[3] for item in states) < -spec.travel and max(item[3] for item in states) <= 0.0
            detail = "alternates deterministically using the decomp 380/3 half-cycle"
        else:
            passed = max(item[4] for item in states) == 705.0 and states[345][4] == 0.0
            detail = "rider-triggered elevator rises 141x5 and returns to home"
        invariants.append({"scenario": spec.name, "passed": passed, "detail": detail})

    passed_comparisons = sum(bool(item["passed"]) for item in comparisons)
    passed_invariants = sum(bool(item["passed"]) for item in invariants)
    return {
        "passed": (
            passed_comparisons == len(comparisons)
            and checkpoint_passes == checkpoint_assertions
            and passed_invariants == len(invariants)
        ),
        "simulation_hz": STEP_HZ,
        "simulation_frames": TOTAL_FRAMES,
        "scenario_count": len(SPECS),
        "schedule_comparison_passes": passed_comparisons,
        "schedule_comparison_count": len(comparisons),
        "checkpoint_passes": checkpoint_passes,
        "checkpoint_assertion_count": checkpoint_assertions,
        "invariant_passes": passed_invariants,
        "invariant_count": len(invariants),
        "checkpoints": list(CHECKPOINTS),
        "comparisons": comparisons,
        "invariants": invariants,
    }
