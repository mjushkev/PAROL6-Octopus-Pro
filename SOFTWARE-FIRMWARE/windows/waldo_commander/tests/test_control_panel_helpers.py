from __future__ import annotations

import asyncio
import math

import pytest

from waldo_commander.components.control import ControlPanel, _ClickHoldHandler


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        (None, 1.0),
        ("", 1.0),
        (float("nan"), 1.0),
        (float("inf"), 1.0),
        (-5.0, 5.0),
        (0.01, 1.0),
        (150.0, 100.0),
        (2.5, 2.5),
    ],
)
def test_safe_step_value_never_exposes_invalid_ui_state(
    value: object, expected: float
) -> None:
    actual = ControlPanel._safe_step_value(value)
    assert math.isfinite(actual)
    assert actual == expected


@pytest.mark.asyncio
async def test_click_hold_handler_emits_one_step_per_quick_press() -> None:
    handler = _ClickHoldHandler(0.04, lambda: None)
    clicks = 0
    holds = 0
    releases: list[bool] = []

    async def on_click() -> None:
        nonlocal clicks
        clicks += 1

    def on_hold() -> None:
        nonlocal holds
        holds += 1

    for _ in range(2):
        await handler.on_change(
            "J5-",
            True,
            on_click=on_click,
            on_hold_start=on_hold,
            on_release=releases.append,
        )
        await asyncio.sleep(0.005)
        await handler.on_change(
            "J5-",
            False,
            on_click=on_click,
            on_hold_start=on_hold,
            on_release=releases.append,
        )

    assert clicks == 2
    assert holds == 0
    assert releases == [False, False]


@pytest.mark.asyncio
async def test_click_hold_handler_streams_after_threshold_and_releases() -> None:
    handler = _ClickHoldHandler(0.01, lambda: None)
    clicks = 0
    holds = 0
    releases: list[bool] = []

    async def on_click() -> None:
        nonlocal clicks
        clicks += 1

    def on_hold() -> None:
        nonlocal holds
        holds += 1

    await handler.on_change(
        "J2+",
        True,
        on_click=on_click,
        on_hold_start=on_hold,
        on_release=releases.append,
    )
    await asyncio.sleep(0.03)
    await handler.on_change(
        "J2+",
        False,
        on_click=on_click,
        on_hold_start=on_hold,
        on_release=releases.append,
    )

    assert clicks == 0
    assert holds == 1
    assert releases == [True]
