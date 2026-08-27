"""Pure-Python equivalents of TOPP-RA 0.6.3's Cython constraint helpers.

The upstream package requires a C++ compiler even though these routines only
perform small NumPy-backed loops.  Keeping the public module name lets the
unmodified TOPP-RA constraint code run on Windows installations without Visual
Studio Build Tools.
"""

from __future__ import annotations

import numpy as np

from .constants import JVEL_MAXSD


def _velocity_bounds(qs: np.ndarray, vlim_grid: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    qs = np.asarray(qs, dtype=float)
    vlim_grid = np.asarray(vlim_grid, dtype=float)
    if qs.ndim != 2 or vlim_grid.shape != (qs.shape[0], qs.shape[1], 2):
        raise ValueError("expected qs=(points,dof) and vlim=(points,dof,2)")

    point_count, dof = qs.shape
    a = np.zeros((point_count, 2), dtype=float)
    b = np.ones((point_count, 2), dtype=float)
    b[:, 1] = -1.0
    c = np.zeros((point_count, 2), dtype=float)

    for i in range(point_count):
        sdmin = -float(JVEL_MAXSD)
        sdmax = float(JVEL_MAXSD)
        for k in range(dof):
            derivative = qs[i, k]
            if derivative > 0.0:
                sdmax = min(vlim_grid[i, k, 1] / derivative, sdmax)
                sdmin = max(vlim_grid[i, k, 0] / derivative, sdmin)
            elif derivative < 0.0:
                sdmax = min(vlim_grid[i, k, 0] / derivative, sdmax)
                sdmin = max(vlim_grid[i, k, 1] / derivative, sdmin)
        c[i, 0] = -(sdmax**2)
        c[i, 1] = max(sdmin, 0.0) ** 2
    return a, b, c


def _create_velocity_constraint(qs: np.ndarray, vlim: np.ndarray):
    qs_array = np.asarray(qs, dtype=float)
    limits = np.asarray(vlim, dtype=float)
    if limits.shape != (qs_array.shape[1], 2):
        raise ValueError("expected vlim=(dof,2)")
    return _velocity_bounds(qs_array, np.broadcast_to(limits, (qs_array.shape[0], *limits.shape)))


def _create_velocity_constraint_varying(qs: np.ndarray, vlim_grid: np.ndarray):
    return _velocity_bounds(qs, vlim_grid)
