from __future__ import annotations

import heapq
from typing import List, Optional, Sequence, Tuple

from .config import EMPTY

Coord = Tuple[int, int]

# 4-neighbors matching C++ dirs: (0,1), (1,0), (0,-1), (-1,0) as (dx, dy)
DIRS = ((0, 1), (1, 0), (0, -1), (-1, 0))
DIR_CHARS = (">", "v", "<", "^")


def _walkable(grid: Sequence[Sequence[int]], x: int, y: int) -> bool:
    if x < 0 or y < 0 or x >= len(grid) or y >= len(grid[0]):
        return False
    return grid[x][y] != EMPTY


def astar_path(
    grid: Sequence[Sequence[int]],
    start: Coord,
    goal: Coord,
) -> Optional[List[Coord]]:
    """4-connected A*; walkable iff cell != EMPTY (-1). Returns path including start and goal."""
    sx, sy = start
    gx, gy = goal
    if not _walkable(grid, sx, sy) or not _walkable(grid, gx, gy):
        return None

    def hfun(x: int, y: int) -> int:
        return abs(x - gx) + abs(y - gy)

    parent: dict[Coord, Optional[Coord]] = {start: None}
    g_score: dict[Coord, int] = {start: 0}
    heap: List[Tuple[int, int, int, int]] = []
    heapq.heappush(heap, (hfun(sx, sy), sx, sy, 0))

    while heap:
        _f, x, y, g = heapq.heappop(heap)
        cur = (x, y)
        if g != g_score[cur]:
            continue
        if cur == goal:
            path: List[Coord] = []
            c: Optional[Coord] = goal
            while c is not None:
                path.append(c)
                c = parent[c]
            path.reverse()
            return path

        for dx, dy in DIRS:
            nx, ny = x + dx, y + dy
            if not _walkable(grid, nx, ny):
                continue
            nxt = (nx, ny)
            tentative = g + 1
            if tentative < g_score.get(nxt, 10**9):
                g_score[nxt] = tentative
                parent[nxt] = cur
                heapq.heappush(heap, (tentative + hfun(nx, ny), nx, ny, tentative))

    return None


def path_to_arrow_string(path: List[Coord]) -> str:
    """Turn a path into C++-style arrows between consecutive cells."""
    if len(path) < 2:
        return ""
    out: List[str] = []
    for a, b in zip(path[:-1], path[1:]):
        dx = b[0] - a[0]
        dy = b[1] - a[1]
        for didx, (ex, ey) in enumerate(DIRS):
            if (dx, dy) == (ex, ey):
                out.append(DIR_CHARS[didx])
                break
    return "".join(out)
