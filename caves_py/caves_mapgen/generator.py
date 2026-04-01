from __future__ import annotations

import heapq
import math
import random
from typing import List, Tuple

from .config import CORRIDOR, EMPTY, MapGenConfig
from .union_find import UnionFind

# grid[x][y]: first index x (0..width-1), second y (0..height-1), matching C++ map.cpp


class DungeonMap:
    def __init__(self, config: MapGenConfig | None = None) -> None:
        self.config = config or MapGenConfig()
        self.rng = random.Random(self.config.seed)
        w, h = self.config.width, self.config.height
        self.grid: List[List[int]] = [[EMPTY] * h for _ in range(w)]
        self.rooms: List[Tuple[int, int, int, int]] = []
        self.room_connector_points: List[Tuple[int, int]] = []
        self.mst_edges: List[Tuple[int, int]] = []

        self._generate_rooms()
        self._generate_paths()

    def _check_room(self, x1: int, y1: int, x2: int, y2: int) -> bool:
        m = self.config.width
        n = self.config.height
        if x1 < 0 or y1 < 0 or x2 >= m or y2 >= n:
            return False
        for i in range(x1, x2 + 1):
            for j in range(y1, y2 + 1):
                if self.grid[i][j] != EMPTY:
                    return False
        return True

    def _place_room(self, x1: int, y1: int, x2: int, y2: int, val: int) -> None:
        for i in range(x1, x2 + 1):
            for j in range(y1, y2 + 1):
                self.grid[i][j] = val

    def _generate_rooms(self) -> None:
        cfg = self.config
        margin = cfg.room_margin
        amount_rooms = cfg.num_rooms
        min_size = cfg.room_min_size
        max_size = cfg.room_max_size
        rng = self.rng

        while len(self.rooms) < amount_rooms:
            x1 = rng.randrange(cfg.width)
            y1 = rng.randrange(cfg.height)
            x2 = x1 + rng.randrange(max_size - min_size + 1) + min_size
            y2 = y1 + rng.randrange(max_size - min_size + 1) + min_size

            if not self._check_room(
                x1 - margin, y1 - margin, x2 + margin, y2 + margin
            ):
                continue
            self._place_room(x1, y1, x2, y2, len(self.rooms))
            self.rooms.append((x1, y1, x2, y2))

        slide_n = len(self.rooms) * cfg.slide_attempts_multiplier
        half_w = cfg.width // 2
        half_h = cfg.height // 2
        for _ in range(slide_n):
            room_idx = rng.randrange(len(self.rooms))
            x1, y1, x2, y2 = self.rooms[room_idx]
            if rng.randrange(2):
                dx = 1 if (x1 + x2) // 2 < half_w else -1
                dy = 0
            else:
                dx = 0
                dy = 1 if (y1 + y2) // 2 < half_h else -1

            self._place_room(x1, y1, x2, y2, EMPTY)
            if self._check_room(
                x1 + dx - margin,
                y1 + dy - margin,
                x2 + dx + margin,
                y2 + dy + margin,
            ):
                self._place_room(x1 + dx, y1 + dy, x2 + dx, y2 + dy, room_idx)
                self.rooms[room_idx] = (x1 + dx, y1 + dy, x2 + dx, y2 + dy)
            else:
                self._place_room(x1, y1, x2, y2, room_idx)

    def _generate_paths(self) -> None:
        rng = self.rng
        rooms_pos: List[Tuple[int, int]] = []
        for x1, y1, x2, y2 in self.rooms:
            px = rng.randrange(x2 - x1 + 1) + x1
            py = rng.randrange(y2 - y1 + 1) + y1
            rooms_pos.append((px, py))

        self.room_connector_points = list(rooms_pos)

        n_rooms = len(self.rooms)
        pq: List[Tuple[float, int, int]] = []
        for i in range(n_rooms):
            for j in range(i + 1, n_rooms):
                ax, ay = rooms_pos[i]
                bx, by = rooms_pos[j]
                dist = math.hypot(ax - bx, ay - by)
                heapq.heappush(pq, (dist, i, j))

        uf = UnionFind(n_rooms)
        edges: List[Tuple[int, int]] = []
        while pq:
            _dist, i, j = heapq.heappop(pq)
            if uf.find(i) != uf.find(j):
                uf.unite(i, j)
                edges.append((i, j))

        self.mst_edges = list(edges)

        for i, j in edges:
            x1, y1 = rooms_pos[i]
            x2, y2 = rooms_pos[j]
            cx, cy = x1, y1
            tx, ty = x2, y2
            while cx != tx or cy != ty:
                if rng.randrange(2):
                    if cx < tx:
                        cx += 1
                    elif cx > tx:
                        cx -= 1
                else:
                    if cy < ty:
                        cy += 1
                    elif cy > ty:
                        cy -= 1
                if self.grid[cx][cy] == EMPTY:
                    self.grid[cx][cy] = CORRIDOR

    def ascii_print(self) -> str:
        lines = ["Rooms: " + " ".join(str(i) for i in range(len(self.rooms)))]
        for i in range(self.config.width):
            row = []
            for j in range(self.config.height):
                v = self.grid[i][j]
                if v == EMPTY:
                    row.append(" ")
                elif v == CORRIDOR:
                    row.append("#")
                else:
                    row.append(chr(ord("a") + v))
            lines.append("".join(row))
        return "\n".join(lines)
