extends JarVoxelTerrain

# Keep stamps alive so the C++ background thread can use them!
var active_stamps: Array = []

# Tracker for our fly-cam collision hack
var cave_trigger_pos: Vector3 = Vector3.INF
var cave_trigger_radius: float = 0.0

var time_since_last_print: float = 0.0

func _ready():
	if not sdf or not sdf.has_method("sample_height"):
		return

	# 1. Hunt for a Cave on the SLOPES (Influence between 0.3 and 0.6)
	var cave_coords = find_valid_poi_location(sdf.get_ridge_map(), 0.05, 0.4) 
	if cave_coords != Vector2.INF:
		var cave_y = sdf.sample_height(cave_coords)
		
		# Carve into the slope. We use a larger box and sink it slightly.
		var cave_pos = Vector3(cave_coords.x, cave_y - 5.0, cave_coords.y)
		stamp_cave(cave_pos, 15.0) 

	# 2. Hunt for a City in the Meadow
	var city_coords = find_meadow_location() 
	if city_coords != Vector2.INF:
		var city_y = sdf.sample_height(city_coords)
		stamp_city(Vector3(city_coords.x, city_y, city_coords.y), 20.0)

	# 3. Generate trees
	generate_forest_trees(4000.0, 0.2)

func _process(delta):
	# 1. Print Camera Position every 5 seconds to help you debug
	time_since_last_print += delta
	if time_since_last_print >= 5.0:
		if player_node:
			print("--- Camera World Pos: ", player_node.global_position, " ---")
		time_since_last_print = 0.0

	# 2. Fly-cam Collision Trigger Check
	if cave_trigger_pos != Vector3.INF and player_node:
		var dist = player_node.global_position.distance_to(cave_trigger_pos)
		if dist < cave_trigger_radius*5:
			print("Agent entered the cave! Transitioning to interior...")
			#cave_trigger_pos = Vector3.INF # Set to INF so it only triggers once!

# --- POI HUNTING ---

func find_valid_poi_location(biome_map: Resource, min_influence: float, max_influence: float = 1.0, max_attempts: int = 100) -> Vector2:
	if not biome_map: return Vector2.INF
	var search_radius = 2000.0 
	for i in range(max_attempts):
		var rand_x = randf_range(-search_radius, search_radius)
		var rand_z = randf_range(-search_radius, search_radius)
		
		var influence = biome_map.sample_influence(rand_x, rand_z)
		if influence >= min_influence and influence <= max_influence:
			return Vector2(rand_x, rand_z)
	return Vector2.INF

# Hunts for pure meadow (no forest, no mountain)
func find_meadow_location(max_attempts: int = 100) -> Vector2:
	var search_radius = 2000.0 
	var ridge_map = sdf.get_ridge_map()
	var forest_map = sdf.get_forest_map()
	for i in range(max_attempts):
		var rand_x = randf_range(-search_radius, search_radius)
		var rand_z = randf_range(-search_radius, search_radius)
		if ridge_map.sample_influence(rand_x, rand_z) < 0.05 and forest_map.sample_influence(rand_x, rand_z) < 0.05:
			return Vector2(rand_x, rand_z)
	return Vector2.INF

# --- POI STAMPING ---

@warning_ignore("int_as_enum_without_cast", "int_as_enum_without_match")
func stamp_city(world_pos: Vector3, poi_size: float):
	var city_box = JarBoxSdf.new()
	city_box.set_extent(Vector3(poi_size, 2.0, poi_size)) 
	# Prevent garbage collection!
	active_stamps.append(city_box) 
	# We now cast the constant to an int so the C++ signature is happy
	self.modify(city_box, JarVoxelTerrain.SDF_OPERATION_SMOOTH_UNION as int, world_pos, poi_size + 5.0)
	print("Stamped City at: ", world_pos)

@warning_ignore("int_as_enum_without_cast", "int_as_enum_without_match")
func stamp_cave(world_pos: Vector3, poi_size: float):
	var cave_box = JarBoxSdf.new()
	cave_box.set_extent(Vector3(poi_size, poi_size, poi_size * 2.0)) 
	active_stamps.append(cave_box) 
	self.modify(cave_box, JarVoxelTerrain.SDF_OPERATION_SMOOTH_SUBTRACTION as int, world_pos, poi_size + 5.0)
	create_scene_transition_trigger(world_pos, poi_size)
	print("Stamped Cave at: ", world_pos)

func create_scene_transition_trigger(pos: Vector3, poi_size: float):
	var trigger_area = Area3D.new()
	# NOTE: Make sure this layer mask matches your Agent's collision layer!
	trigger_area.collision_mask = 1 
	var collision = CollisionShape3D.new()
	var box_shape = BoxShape3D.new()
	box_shape.size = Vector3(poi_size, poi_size, poi_size)
	collision.shape = box_shape
	trigger_area.add_child(collision)
	trigger_area.body_entered.connect(_on_cave_entered)
	add_child(trigger_area)
	trigger_area.global_position = pos

func _on_cave_entered(body: Node3D):
	if body.is_in_group("agent"):
		print("Agent entered the cave! Transitioning to interior...")

# --- FOREST DECORATION ---

func generate_forest_trees(area_size: float = 1000.0, density: float = 0.2):
	var forest_map = sdf.get_forest_map()
	if not forest_map:
		return
		
	var tree_transforms = []
	var step = 1.0 / density
	var half_area = area_size / 2.0
	
	# 1. Find Valid Tree Positions
	for x in range(int(-half_area), int(half_area), int(step)):
		for z in range(int(-half_area), int(half_area), int(step)):
			var px = x + randf_range(-step * 0.4, step * 0.4)
			var pz = z + randf_range(-step * 0.4, step * 0.4)
			
			# Mask check: Only place trees where forest influence is high
			var influence = forest_map.sample_influence(px, pz)
			if influence > 0.1: # lowered from 0.6 
				var py = sdf.sample_height(Vector2(px, pz))
				if py > (sdf.get_height_scale() * 0.15): # tree line at 30% height scale
					continue
				var tree_basis = Basis().rotated(Vector3.UP, randf_range(0, TAU))
				var random_scale = randf_range(0.8, 1.3)
				tree_basis = tree_basis.scaled(Vector3(random_scale, random_scale, random_scale))
				tree_transforms.append(Transform3D(basis, Vector3(px, py, pz)))
				
	if tree_transforms.is_empty():
		return

	# 2. Populate the MultiMesh
	var multimesh = MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.instance_count = tree_transforms.size()
	# Replace this BoxMesh with Tree Mesh resource!
	var debug_mesh = BoxMesh.new()
	debug_mesh.size = Vector3(1, 4, 1)
	multimesh.mesh = debug_mesh
	for i in range(tree_transforms.size()):
		multimesh.set_instance_transform(i, tree_transforms[i])

	# 3. Add to Scene
	var mmi = MultiMeshInstance3D.new()
	mmi.multimesh = multimesh
	add_child(mmi)
	print("Spawned ", tree_transforms.size(), " trees in the forest biome!")
