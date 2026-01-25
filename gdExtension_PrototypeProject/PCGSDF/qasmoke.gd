extends Node3D

@onready var voxel_terrain: VoxelTerrain = $VoxelTerrain
@onready var sunlight: DirectionalLight3D = $DirectionalLight3D
@onready var camera: Camera3D = $Camera3D

func _ready():
	# 1. DEFINE THE "RIVERWOOD" PATH
	# This represents the deterministic river curve.
	# The C++ engine will subtract density here to create a valley.
	
	# A river flowing from (0,0) to (200, 200) with width 20
	voxel_terrain.add_path(Vector3(0, 40, 0), Vector3(200, 30, 200), 20.0)
	
	# Another branch
	voxel_terrain.add_path(Vector3(200, 30, 200), Vector3(400, 10, 200), 25.0)

	# 2. Setup Camera for a "Drone View"
	camera.position = Vector3(100, 150, 100)
	camera.look_at(Vector3(100, 0, 100))
