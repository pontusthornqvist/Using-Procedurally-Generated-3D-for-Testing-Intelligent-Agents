"""
CLI demo: generate a dungeon, optional A* path, save PNG.

Run from repo root:
  python caves_py/demo.py --out dungeon.png
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
import random

ROOT = Path(__file__).resolve().parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from caves_mapgen.config import MapGenConfig
from caves_mapgen.generator import DungeonMap
from caves_mapgen.pathfind import astar_path, path_to_arrow_string
from caves_mapgen.visualize import render_dungeon, save_figure


def main() -> None:
    p = argparse.ArgumentParser(description="Generate room+MST dungeon map (Python port of caves).")
    p.add_argument("--seed", type=int, default=random.randint(0, 1000000))
    p.add_argument("--width", type=int, default=129)
    p.add_argument("--height", type=int, default=129)
    p.add_argument("--rooms", type=int, default=15, help="Number of rooms")
    p.add_argument("--room-min", type=int, default=5)
    p.add_argument("--room-max", type=int, default=10)
    p.add_argument("--margin", type=int, default=2, help="Empty cells around each room")
    p.add_argument(
        "--slide-mult",
        type=int,
        default=40,
        help="Slide attempts = rooms * this (C++ used 100)",
    )
    p.add_argument("--cell-size", type=int, default=6, help="Pixels per cell (nearest-neighbor)")
    p.add_argument("--dpi", type=int, default=200)
    p.add_argument("--out", type=str, default="", help="Output PNG path (omit to skip save)")
    p.add_argument("--ascii", action="store_true", help="Print ASCII map to stdout")
    p.add_argument("--show-mst", action="store_true", help="Overlay MST edges on PNG")
    p.add_argument("--show-labels", action="store_true", help="Draw room indices")
    p.add_argument(
        "--path",
        type=str,
        default="",
        help='Optional A* path as "sx,sy-gx,gy" grid coords (first index, second index)',
    )
    p.add_argument("--show", action="store_true", help="Show matplotlib window")

    args = p.parse_args()

    cfg = MapGenConfig(
        seed=args.seed,
        width=args.width,
        height=args.height,
        num_rooms=args.rooms,
        room_min_size=args.room_min,
        room_max_size=args.room_max,
        room_margin=args.margin,
        slide_attempts_multiplier=args.slide_mult,
    )
    dmap = DungeonMap(cfg)

    if args.ascii:
        print(dmap.ascii_print())

    path_coords = None
    if args.path:
        try:
            a, b = args.path.split("-")
            sx, sy = map(int, a.split(","))
            gx, gy = map(int, b.split(","))
        except ValueError as e:
            p.error(f'Invalid --path (expected "sx,sy-gx,gy"): {e}')
        path_coords = astar_path(dmap.grid, (sx, sy), (gx, gy))
        if path_coords is None:
            print("No path found (check coordinates are walkable).", file=sys.stderr)
        else:
            arrows = path_to_arrow_string(path_coords)
            print(arrows)

    fig, _ax = render_dungeon(
        dmap,
        cell_size=args.cell_size,
        dpi=args.dpi,
        show_mst=args.show_mst,
        show_room_labels=args.show_labels,
        path=path_coords,
    )

    if args.out:
        out_path = Path(args.out)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        save_figure(fig, str(out_path))
        print(f"Wrote {out_path.resolve()}")

    if args.show:
        import matplotlib.pyplot as plt

        plt.show()
    else:
        from matplotlib import pyplot as plt

        plt.close(fig)


if __name__ == "__main__":
    main()
