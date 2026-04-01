# caves_py

Python version of the same map idea: generates a dungeon, can run A* and save a PNG.

**Setup**

```bash
pip install -r requirements.txt
```

**Run** (from repo root is easiest):

```bash
python caves_py/demo.py --out dungeon.png
```

Add `--show` to open the image in a window, `--ascii` to print text to the terminal. `--help` lists seed, room counts, path format, etc.

Import `caves_mapgen` from your own scripts if you need the generator without the CLI.

**Example usage:**

```bash
python caves_py/demo.py --out dungeon.png --show-mst --show-labels
```

This will generate a dungeon, overlay the MST edges, and show the room labels.