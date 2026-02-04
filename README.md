Proof of concept: Implements a small procedural 3D landscape that utilizes SDFs and octrees. Heavily inspired by: https://github.com/JorisAR/GDVoxelTerrain

setup guide for godot-cpp: https://www.youtube.com/watch?v=02KJouOjQ0c&list=PL_U-ODo6Gtov0XwrDOJxC4_6Q0_MazcMA&index=2

How this fits into the larger project:
Imagine that we're essentially building a procedural "Skyrim esque" landscape. 
We're not just building a forest - we might eventually want to add caves/dungeons and perhaps other POIs, like cabins or small villages.
This idea presents a problem that I believe is solved by this architecture. 
The volumetric terrain offers a deterministic approach for placing dungeons/caves inside mountains. Moreover, using SDF booleans and Voronoi diagrams allows for "POI stamping". 

The Core Challenge (The "Riverwood Problem"): We need to generate a procedural world that supports Deterministic Points of Interest (POIs).
Scenario: We place a hand-crafted town (e.g., Riverwood) into the procedural world.
Constraint: The town has a specific river and road layout. The procedural terrain must respect this. The terrain cannot randomly block the river or bury the road.
  Also, The rivers and roads outside the POI need to connect to those inside of it.
Solution: We generate a Global Graph (Voronoi/Delaunay) for POIs first. We feed this into the C++ SDF. 
  The terrain generation subtracts density in relevant areas, guaranteeing a valley exists exactly where the town needs it, regardless of the random noise seed. 
  Roads and rivers are generateed after POI stamping and always lead to POI endpoints.


The Roadmap:

✅ Phase 1: Volumetric Core (Done: Octree, Threading, Seam-free Meshing, Basic Math).

🔲 Phase 2: Implementing a GDScript-to-C++ bridge (RiverManager) to pass deterministic Curve3D data into the C++ engine to carve specific rivers/roads that mesh with POIs.

🔲 Phase 3: Math Refinement: Tuning Analytical Derivatives as a substitute for erosion. 

🔲 Phase 4: Biome Coloring: Implementing a top-down texture synthesis for the ground texture and grass shader.

🔲 Phase 5: POI Stamping: Using Voronoi Regions (Cell Noise) and SDF Booleans to spawn dungeons/cities.
