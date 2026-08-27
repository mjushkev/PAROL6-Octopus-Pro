from __future__ import annotations

import numpy as np
from scipy.optimize import linprog

from toppra._CythonUtils import _create_velocity_constraint
from toppra.solverwrapper.cy_seidel_solverwrapper import solve_lp1d, solve_lp2d


def test_velocity_constraint_matches_manual_bounds() -> None:
    qs = np.array([[1.0, -2.0], [0.5, 4.0], [0.0, -1.0]])
    limits = np.array([[-2.0, 3.0], [-8.0, 4.0]])
    a, b, c = _create_velocity_constraint(qs, limits)
    np.testing.assert_array_equal(a, np.zeros((3, 2)))
    np.testing.assert_array_equal(b, [[1.0, -1.0]] * 3)
    np.testing.assert_allclose(c[:, 0], [-9.0, -1.0, -64.0])
    np.testing.assert_allclose(c[:, 1], [0.0, 0.0, 0.0])


def test_one_dimensional_lp_bounds() -> None:
    result, value, variable, active = solve_lp1d(
        np.array([2.0, 1.0]),
        np.array([1.0, -1.0]),
        np.array([-3.0, -2.0]),
        -10.0,
        10.0,
    )
    assert result == 1
    assert variable == 3.0
    assert value == 7.0
    assert active == 0


def test_two_dimensional_lp_matches_scipy_reference() -> None:
    rng = np.random.default_rng(42)
    for _ in range(50):
        coefficients = rng.normal(size=(12, 2))
        # Every half-space contains the origin, guaranteeing feasibility.
        offsets = -rng.uniform(0.2, 3.0, size=12)
        objective = rng.normal(size=2)
        low = np.array([-2.0, -2.0])
        high = np.array([2.0, 2.0])
        result, value, variable, _active = solve_lp2d(
            np.r_[objective, 0.0],
            coefficients[:, 0],
            coefficients[:, 1],
            offsets,
            low,
            high,
            np.array([-1, -1], dtype=np.int64),
        )
        reference = linprog(
            -objective,
            A_ub=coefficients,
            b_ub=-offsets,
            bounds=list(zip(low, high, strict=True)),
            method="highs",
        )
        assert reference.success
        assert result == 1
        np.testing.assert_allclose(value, -reference.fun, atol=1e-7)
        assert np.all(coefficients @ variable + offsets <= 2e-7)
