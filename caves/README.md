# caves (C++)

Small dungeon generator + pathfinding demo. `main.cpp` builds a map and prints it plus an A* path.

**Build** (from this folder, needs a C++17 compiler):

```bash
g++ -std=c++17 -O2 main.cpp game/map/map.cpp game/pathfind/shortestpath.cpp -o caves
```

With MSVC in a developer prompt, same sources:

```text
cl /EHsc /std:c++17 main.cpp game\map\map.cpp game\pathfind\shortestpath.cpp /Fe:caves.exe
```

**Run:** `./caves` or `caves.exe` — output goes to the terminal.

There’s no CMake here; tweak `main.cpp` if you want different sizes, seed, or start/end points.
