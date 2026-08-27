"""Pure-Python port of TOPP-RA 0.6.3's Seidel LP wrapper.

The calculations and active-constraint conventions mirror the upstream Cython
implementation.  It is intentionally conservative and favors portability over
the small setup-time speed benefit of the compiled extension.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from math import isnan

import numpy as np

from ..constraint import ConstraintType

TINY = 1e-10
SMALL = 1e-8
VAR_MIN = -100_000_000.0
VAR_MAX = 100_000_000.0
INF = 10_000_000_000.0


@dataclass
class _LpSol:
    result: int = 0
    optval: float = float("nan")
    optvar: np.ndarray = field(default_factory=lambda: np.full(2, np.nan, dtype=float))
    active_c: np.ndarray = field(default_factory=lambda: np.full(2, -1, dtype=np.int64))


def _solve_lp1d(v, a, b, low: float, high: float) -> _LpSol:
    v = np.asarray(v, dtype=float)
    a_array = np.asarray([] if a is None else a, dtype=float)
    b_array = np.asarray([] if b is None else b, dtype=float)
    cur_min, cur_max = float(low), float(high)
    active_min, active_max = -1, -2

    for index, coefficient in enumerate(a_array):
        if coefficient > TINY:
            bound = -b_array[index] / coefficient
            if bound < cur_max:
                cur_max, active_max = float(bound), index
        elif coefficient < -TINY:
            bound = -b_array[index] / coefficient
            if bound > cur_min:
                cur_min, active_min = float(bound), index

    result = _LpSol()
    if cur_min > cur_max:
        return result
    optimum, active = (cur_min, active_min) if abs(v[0]) < TINY or v[0] < 0.0 else (cur_max, active_max)
    result.result = 1
    result.optvar[0] = optimum
    result.optval = float(v[0] * optimum + v[1])
    result.active_c[0] = active
    return result


def solve_lp1d(v, a, b, low: float, high: float):
    solution = _solve_lp1d(v, a, b, low, high)
    return solution.result, solution.optval, solution.optvar[0], solution.active_c[0]


def _solve_lp2d(v, a, b, c, low, high, active_c) -> _LpSol:
    v = np.asarray(v, dtype=float)
    a = np.asarray(a, dtype=float)
    b = np.asarray(b, dtype=float)
    c = np.asarray(c, dtype=float)
    low = np.asarray(low, dtype=float)
    high = np.asarray(high, dtype=float)
    active_c = np.asarray(active_c, dtype=np.int64)
    nrows = a.shape[0]
    solution = _LpSol()

    if np.any(low > high):
        return solution

    current = np.where(v[:2] > TINY, high, low).astype(float)
    solution.active_c[:] = [(-2 if v[0] > TINY else -1), (-4 if v[1] > TINY else -3)]

    if (
        active_c.shape[0] >= 2
        and 0 <= active_c[0] < nrows
        and 0 <= active_c[1] < nrows
        and active_c[0] != active_c[1]
    ):
        order = [int(active_c[1]), int(active_c[0])]
        order.extend(index for index in range(nrows) if index not in active_c[:2])
    else:
        order = list(range(nrows))

    for k, row in enumerate(order):
        if a[row] * current[0] + b[row] * current[1] + c[row] < TINY:
            continue

        denominator_squared = a[row] ** 2 + b[row] ** 2
        if denominator_squared < TINY:
            return solution
        zero_projection = np.array(
            [-a[row] * c[row] / denominator_squared, -b[row] * c[row] / denominator_squared],
            dtype=float,
        )
        tangent = np.array([-b[row], a[row]], dtype=float)
        projected_objective = np.array([float(tangent @ v[:2]), 0.0], dtype=float)

        projected_a: list[float] = []
        projected_b: list[float] = []
        prior_rows = order[:k]
        constraints = [
            (-1.0, 0.0, low[0]),
            (1.0, 0.0, -high[0]),
            (0.0, -1.0, low[1]),
            (0.0, 1.0, -high[1]),
        ]
        constraints[:0] = [(a[index], b[index], c[index]) for index in prior_rows]

        for aj, bj, cj in constraints:
            denominator = tangent[0] * aj + tangent[1] * bj
            residual = cj + zero_projection[1] * bj + zero_projection[0] * aj
            if denominator > TINY:
                limit = -residual / denominator
                projected_a.append(1.0)
                projected_b.append(-limit)
            elif denominator < -TINY:
                limit = -residual / denominator
                projected_a.append(-1.0)
                projected_b.append(limit)
            else:
                if residual > SMALL:
                    return solution
                projected_a.append(0.0)
                projected_b.append(-1.0)

        one_d = _solve_lp1d(projected_objective, projected_a, projected_b, -INF, INF)
        if not one_d.result:
            return solution

        current = zero_projection + one_d.optvar[0] * tangent
        solution.active_c[0] = row
        active = int(one_d.active_c[0])
        if active < k:
            solution.active_c[1] = prior_rows[active]
        elif active == k:
            solution.active_c[1] = -1
        elif active == k + 1:
            solution.active_c[1] = -2
        elif active == k + 2:
            solution.active_c[1] = -3
        elif active == k + 3:
            solution.active_c[1] = -4
        else:
            return _LpSol()

    solution.result = 1
    solution.optvar[:] = current
    solution.optval = float(current @ v[:2] + v[2])
    return solution


def solve_lp2d(v, a, b, c, low, high, active_c):
    solution = _solve_lp2d(v, a, b, c, low, high, active_c)
    return solution.result, solution.optval, solution.optvar, solution.active_c


class seidelWrapper:
    def __init__(self, constraint_list, path, path_discretization, solve_lp1d=0):
        self.constraints = constraint_list
        self.path = path
        self.path_discretization = np.asarray(path_discretization, dtype=float)
        self.N = len(self.path_discretization) - 1
        self.deltas = np.diff(self.path_discretization)
        self._solve_lp1d = solve_lp1d
        self.nCons = len(constraint_list)
        self.nC = 2
        self.nV = 2
        self._params = []

        for item in self.constraints:
            if item.get_constraint_type() != ConstraintType.CanonicalLinear:
                raise NotImplementedError
            params = item.compute_constraint_params(self.path, self.path_discretization)
            a, _b, _c, F, _v, _ubnd, _xbnd = params
            if a is not None:
                self.nC += F.shape[0] if item.identical else F.shape[1]
            self._params.append(params)

        self.a_arr = np.zeros((self.N + 1, self.nC))
        self.b_arr = np.zeros((self.N + 1, self.nC))
        self.c_arr = np.zeros((self.N + 1, self.nC))
        self.low_arr = np.full((self.N + 1, 2), VAR_MIN)
        self.high_arr = np.full((self.N + 1, 2), VAR_MAX)
        current_index = 2

        for index, params in enumerate(self._params):
            a_j, b_j, c_j, F_j, h_j, ubound_j, xbound_j = params
            if a_j is not None:
                if self.constraints[index].identical:
                    count = F_j.shape[0]
                    ta = a_j.dot(F_j.T)
                    tb = b_j.dot(F_j.T)
                    tc = c_j.dot(F_j.T) - h_j
                    self.a_arr[:, current_index : current_index + count] = ta
                    self.b_arr[:, current_index : current_index + count] = tb
                    self.c_arr[:, current_index : current_index + count] = tc
                else:
                    count = F_j.shape[1]
                    for point in range(self.N + 1):
                        self.a_arr[point, current_index : current_index + count] = np.dot(F_j[point], a_j[point])
                        self.b_arr[point, current_index : current_index + count] = np.dot(F_j[point], b_j[point])
                        self.c_arr[point, current_index : current_index + count] = np.dot(F_j[point], c_j[point]) - h_j[point]
                current_index += count
            if ubound_j is not None:
                self.low_arr[:, 0] = np.maximum(self.low_arr[:, 0], ubound_j[:, 0])
                self.high_arr[:, 0] = np.minimum(self.high_arr[:, 0], ubound_j[:, 1])
            if xbound_j is not None:
                self.low_arr[:, 1] = np.maximum(self.low_arr[:, 1], xbound_j[:, 0])
                self.high_arr[:, 1] = np.minimum(self.high_arr[:, 1], xbound_j[:, 1])

        self.active_c_up = np.zeros(2, dtype=np.int64)
        self.active_c_down = np.zeros(2, dtype=np.int64)
        self.v = np.zeros(3)

    def get_no_vars(self):
        return self.nV

    def get_no_stages(self):
        return self.N

    def get_deltas(self):
        return self.deltas

    @property
    def params(self):
        return self._params

    def solve_stagewise_optim(self, i, H, g, x_min, x_max, x_next_min, x_next_max):
        if not 0 <= i <= self.N:
            raise IndexError(i)
        low = self.low_arr[i].copy()
        high = self.high_arr[i].copy()
        if not isnan(x_min):
            low[1] = max(low[1], x_min)
        if not isnan(x_max):
            high[1] = min(high[1], x_max)

        if i < self.N:
            if isnan(x_next_min):
                self.a_arr[i, 0], self.b_arr[i, 0], self.c_arr[i, 0] = 0.0, 0.0, -1.0
            else:
                self.a_arr[i, 0], self.b_arr[i, 0], self.c_arr[i, 0] = -2.0 * self.deltas[i], -1.0, x_next_min
            if isnan(x_next_max):
                self.a_arr[i, 1], self.b_arr[i, 1], self.c_arr[i, 1] = 0.0, 0.0, -1.0
            else:
                self.a_arr[i, 1], self.b_arr[i, 1], self.c_arr[i, 1] = 2.0 * self.deltas[i], 1.0, -x_next_max
        else:
            self.a_arr[i, :2] = 0.0
            self.b_arr[i, :2] = 0.0
            self.c_arr[i, :2] = -1.0

        self.v[:2] = -np.asarray(g, dtype=float)[:2]
        if x_min == x_max and self._solve_lp1d > 0:
            bx_c = self.b_arr[i] * x_min + self.c_arr[i]
            objective = np.array([self.v[0], -g[1] * x_min])
            solution = _solve_lp1d(objective, self.a_arr[i], bx_c, low[0], high[0])
            if not solution.result:
                return np.full(2, np.nan)
            cache = self.active_c_up if g[1] > 0 else self.active_c_down
            cache[0] = solution.active_c[0]
            return np.array([solution.optvar[0], x_min])

        cache = self.active_c_up if g[1] > 0 else self.active_c_down
        solution = _solve_lp2d(self.v, self.a_arr[i], self.b_arr[i], self.c_arr[i], low, high, cache)
        if not solution.result:
            return np.full(2, np.nan)
        cache[:] = solution.active_c
        return solution.optvar.copy()

    def setup_solver(self):
        pass

    def close_solver(self):
        pass
