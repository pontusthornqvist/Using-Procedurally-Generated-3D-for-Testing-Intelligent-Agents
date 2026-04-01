from __future__ import annotations

from typing import TYPE_CHECKING, List, Optional, Sequence, Tuple

import numpy as np
from matplotlib import pyplot as plt

from .config import CORRIDOR, EMPTY

if TYPE_CHECKING:
    from .generator import DungeonMap

Coord = Tuple[int, int]


def _room_color(room_id: int) -> Tuple[float, float, float, float]:
    """Stable distinct color per room id (HSV-ish via golden ratio)."""
    import colorsys

    h = (room_id * 0.618033988749895) % 1.0
    r, g, b = colorsys.hsv_to_rgb(h, 0.55, 0.92)
    return (float(r), float(g), float(b), 1.0)


def grid_to_rgba(
    grid: Sequence[Sequence[int]],
    *,
    empty_color: Tuple[float, float, float, float] = (0.06, 0.07, 0.12, 1.0),
    corridor_color: Tuple[float, float, float, float] = (0.45, 0.38, 0.28, 1.0),
) -> np.ndarray:
    """Rasterize cell grid to float32 RGBA, shape (width, height, 4)."""
    w = len(grid)
    h = len(grid[0]) if w else 0
    out = np.zeros((w, h, 4), dtype=np.float32)
    for i in range(w):
        for j in range(h):
            v = grid[i][j]
            if v == EMPTY:
                out[i, j] = empty_color
            elif v == CORRIDOR:
                out[i, j] = corridor_color
            else:
                out[i, j] = _room_color(int(v))
    return out


def upscale_nearest(rgba: np.ndarray, cell_size: int) -> np.ndarray:
    """Repeat each cell cell_size times along both axes."""
    if cell_size < 1:
        raise ValueError("cell_size must be at least 1")
    return np.repeat(np.repeat(rgba, cell_size, axis=0), cell_size, axis=1)


def render_dungeon(
    dmap: "DungeonMap",
    *,
    cell_size: int = 6,
    dpi: int = 200,
    show_mst: bool = False,
    show_room_labels: bool = False,
    path: Optional[List[Coord]] = None,
    path_color: Tuple[float, float, float, float] = (1.0, 0.95, 0.2, 1.0),
    figsize: Optional[Tuple[float, float]] = None,
) -> Tuple[plt.Figure, plt.Axes]:
    """
    Plot dungeon map with optional MST edges and A* path.
    Image coords: x = column j, y = row i (origin upper for imshow).
    """
    rgba = grid_to_rgba(dmap.grid)
    big = upscale_nearest(rgba, cell_size)
    w, h = dmap.config.width, dmap.config.height

    if figsize is None:
        aspect = (h * cell_size) / max(w * cell_size, 1)
        figsize = (8, max(6, 8 * aspect))

    fig, ax = plt.subplots(figsize=figsize, dpi=dpi)
    # imshow expects (rows, cols, channels); rows = width dim, cols = height dim
    ax.imshow(big, origin="upper", interpolation="nearest", aspect="equal")

    # Map pixel coords to grid (i, j): cell centers in big image
    def cell_center_px(i: int, j: int) -> Tuple[float, float]:
        return j * cell_size + (cell_size - 1) / 2, i * cell_size + (cell_size - 1) / 2

    if show_mst and dmap.mst_edges:
        for a, b in dmap.mst_edges:
            i1, j1 = dmap.room_connector_points[a]
            i2, j2 = dmap.room_connector_points[b]
            x1, y1 = cell_center_px(i1, j1)
            x2, y2 = cell_center_px(i2, j2)
            ax.plot([x1, x2], [y1, y2], color=(0.2, 1.0, 0.4, 0.85), linewidth=1.2)

    if show_room_labels:
        for idx, (x1, y1, x2, y2) in enumerate(dmap.rooms):
            ci = (x1 + x2) / 2.0
            cj = (y1 + y2) / 2.0
            px, py = cell_center_px(int(round(ci)), int(round(cj)))
            ax.text(
                px,
                py,
                str(idx),
                color="white",
                fontsize=8,
                ha="center",
                va="center",
                fontweight="bold",
            )

    if path:
        xs = []
        ys = []
        for i, j in path:
            px, py = cell_center_px(i, j)
            xs.append(px)
            ys.append(py)
        ax.plot(xs, ys, color=path_color[:3], linewidth=2.0, alpha=0.95)

    ax.set_axis_off()
    fig.tight_layout(pad=0)
    return fig, ax


def save_figure(fig: plt.Figure, path: str) -> None:
    fig.savefig(path, bbox_inches="tight", pad_inches=0.02, dpi=fig.dpi)
